#include "Credentials.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QTimeZone>

namespace claudedial::core {
namespace {

/// Never let a token linger in a freed heap page.
void secureClear(QByteArray& buffer)
{
    if (!buffer.isEmpty()) {
        volatile char* p = buffer.data();
        for (qsizetype i = 0; i < buffer.size(); ++i)
            p[i] = 0;
    }
    buffer.clear();
}

/// Claude Code treats a token as expired slightly early; match that.
constexpr qint64 kExpiryBufferMs = 60 * 1000;

QString credentialFilePath()
{
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString configDir = env.value(QStringLiteral("CLAUDE_CONFIG_DIR"));
    if (!configDir.isEmpty())
        return QDir(configDir).filePath(QStringLiteral(".credentials.json"));
    return QDir::homePath() + QStringLiteral("/.claude/.credentials.json");
}

} // namespace

Credentials::Credentials(QObject* parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
{
    // Claude Code replaces the file rather than editing in place, so watching the
    // file alone loses the notification after the first refresh. Watch the
    // directory too and re-arm on every event.
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { Q_EMIT changed(); });
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { Q_EMIT changed(); });
}

Credentials::~Credentials()
{
    clearToken();
}

void Credentials::clearToken()
{
    secureClear(m_token);
    m_expiresAt = QDateTime();
    m_refreshExpiresAt = QDateTime();
}

void Credentials::watch(const QString& path)
{
    const QString dir = QFileInfo(path).absolutePath();
    const auto files = m_watcher->files();
    const auto dirs = m_watcher->directories();
    if (!files.contains(path) && QFile::exists(path))
        m_watcher->addPath(path);
    if (!dirs.contains(dir))
        m_watcher->addPath(dir);
}

Credentials::Status Credentials::reload()
{
    clearToken();

    // 1. An explicit token in the environment wins, and carries no expiry.
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString envToken = env.value(QStringLiteral("CLAUDE_CODE_OAUTH_TOKEN"));
    if (!envToken.isEmpty()) {
        m_token = envToken.toUtf8();
        m_source = QStringLiteral("environment (CLAUDE_CODE_OAUTH_TOKEN)");
        m_status = Status::Ok;
        return m_status;
    }

    // 2. Claude Code's credential file.
    const QString path = credentialFilePath();
    watch(path);
    m_source = path;

    if (!loadFromFile(path)) {
        m_status = Status::Missing;
        return m_status;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_refreshExpiresAt.isValid() && m_refreshExpiresAt.toMSecsSinceEpoch() <= now) {
        m_status = Status::RefreshExpired;
    } else if (m_expiresAt.isValid() && m_expiresAt.toMSecsSinceEpoch() - kExpiryBufferMs <= now) {
        // Do not refresh. Claude Code will, next time it runs, and changed() fires.
        m_status = Status::Expired;
    } else {
        m_status = Status::Ok;
    }
    return m_status;
}

bool Credentials::loadFromFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    // Read-only, always. This file belongs to Claude Code.
    const QByteArray raw = file.readAll();
    file.close();

    QJsonParseError error {};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    const QJsonObject oauth = doc.object().value(QStringLiteral("claudeAiOauth")).toObject();
    const QString token = oauth.value(QStringLiteral("accessToken")).toString();
    if (token.isEmpty())
        return false;

    m_token = token.toUtf8();
    if (const auto v = oauth.value(QStringLiteral("expiresAt")); v.isDouble())
        m_expiresAt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(v.toDouble()), QTimeZone::UTC);
    if (const auto v = oauth.value(QStringLiteral("refreshTokenExpiresAt")); v.isDouble())
        m_refreshExpiresAt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(v.toDouble()), QTimeZone::UTC);

    return true;
}

bool Credentials::authorize(QNetworkRequest& request) const
{
    if (m_status != Status::Ok || m_token.isEmpty())
        return false;

    QByteArray header;
    header.reserve(7 + m_token.size());
    header.append("Bearer ", 7);
    header.append(m_token);
    request.setRawHeader(QByteArrayLiteral("Authorization"), header);
    return true;
}

} // namespace claudedial::core
