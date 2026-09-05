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

#ifdef Q_OS_MACOS
#include <QProcess>
#include <QStringList>
#endif

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

#ifdef Q_OS_MACOS
/// The keychain item is filed under the login name, as Claude Code writes it.
QString keychainAccount()
{
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString user = env.value(QStringLiteral("USER"));
    return user.isEmpty() ? QDir::home().dirName() : user;
}

/// Long enough for a cold keychain, short enough that the tray never looks hung.
constexpr int kKeychainTimeoutMs = 3000;
#endif

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
        m_watched = true; // it cannot change under a running process
        m_status = Status::Ok;
        return m_status;
    }

    // 2. Claude Code's credential file.
    const QString path = credentialFilePath();
    watch(path);
    m_source = path;
    m_watched = true;
    bool loaded = loadFromFile(path);

#ifdef Q_OS_MACOS
    // 3. On macOS Claude Code normally keeps the same JSON in the login
    // keychain instead, and there is nothing there to watch.
    if (!loaded) {
        loaded = loadFromKeychain();
        if (loaded) {
            m_source = QStringLiteral("macOS keychain (Claude Code-credentials)");
            m_watched = false;
        }
    }
#endif

    if (!loaded) {
        m_status = Status::Missing;
        return m_status;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_refreshExpiresAt.isValid() && m_refreshExpiresAt.toMSecsSinceEpoch() <= now) {
        m_status = Status::RefreshExpired;
    } else if (m_expiresAt.isValid() && m_expiresAt.toMSecsSinceEpoch() - kExpiryBufferMs <= now) {
        // Do not refresh. Claude Code will, next time it runs; a watched file
        // announces that, and an unwatched source is re-read on the next poll.
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
    QByteArray raw = file.readAll();
    file.close();

    const bool loaded = loadFromJson(raw);
    secureClear(raw);
    return loaded;
}

bool Credentials::loadFromJson(const QByteArray& raw)
{
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

#ifdef Q_OS_MACOS
bool Credentials::loadFromKeychain()
{
    // Two queries, and both are needed because the prior art disagrees about
    // the item's account attribute: claudometer passes -a $USER, so-agentbar - a
    // native Mac application - matches on the service alone. Nobody here can
    // settle which is right without a Mac running Claude Code, so try the
    // narrow one and fall back to the broad one rather than pick a side.
    const QStringList service { QStringLiteral("find-generic-password"),
                                QStringLiteral("-s"),
                                QStringLiteral("Claude Code-credentials") };
    QStringList withAccount = service;
    withAccount << QStringLiteral("-a") << keychainAccount();

    for (QStringList arguments : { withAccount, service }) {
        arguments << QStringLiteral("-w"); // print the secret, and only the secret
        if (runSecurity(arguments))
            return true;
    }
    return false;
}

bool Credentials::runSecurity(const QStringList& arguments)
{
    // Read the item through /usr/bin/security rather than SecItemCopyMatching.
    // Claude Code puts that binary in the item's access control list, so it is
    // let through silently; this process is not, and would make macOS ask the
    // user for permission on every single poll.
    QProcess security;
    security.setProgram(QStringLiteral("/usr/bin/security"));
    security.setArguments(arguments);
    security.setStandardErrorFile(QProcess::nullDevice());
    security.start(QIODevice::ReadOnly);

    // If it does prompt after all, do not freeze the tray waiting for an answer.
    if (!security.waitForFinished(kKeychainTimeoutMs)) {
        security.kill();
        security.waitForFinished(kKeychainTimeoutMs);
        return false;
    }
    if (security.exitStatus() != QProcess::NormalExit || security.exitCode() != 0)
        return false;

    QByteArray raw = security.readAllStandardOutput();
    const bool loaded = loadFromJson(raw);
    secureClear(raw);
    return loaded;
}
#endif

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
