#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>

class QNetworkRequest;
class QFileSystemWatcher;

namespace claudometer::core {

/// Reads the OAuth access token Claude Code stores on disk, and nothing else.
///
/// This class is the *only* place a token exists in the process. It is never
/// logged, never copied into Claudometer's own configuration, never exposed to
/// QML, and never written back. In particular this class does NOT refresh the
/// token: Anthropic rotates the refresh token on use, so refreshing here would
/// race Claude Code and could log the user out of it. See docs/usage-api.md.
class Credentials : public QObject
{
    Q_OBJECT

public:
    enum class Status {
        Ok,             ///< usable token
        Missing,        ///< no OAuth credentials found (API key / Bedrock / not signed in)
        Expired,        ///< access token expired; wait for Claude Code to refresh it
        RefreshExpired, ///< refresh token expired too; the user must sign in again
    };

    explicit Credentials(QObject* parent = nullptr);
    ~Credentials() override;

    /// Re-reads the credential source. Safe to call often.
    Status reload();

    [[nodiscard]] Status status() const { return m_status; }

    /// Where the token came from - a path or "environment". Never the token.
    [[nodiscard]] QString sourceDescription() const { return m_source; }

    /// Sets the Authorization header. Returns false when there is no usable token.
    [[nodiscard]] bool authorize(QNetworkRequest& request) const;

Q_SIGNALS:
    /// Claude Code rewrote its credential file - most likely a refresh.
    void changed();

private:
    void watch(const QString& path);
    bool loadFromFile(const QString& path);
    void clearToken();

    QByteArray m_token;
    QDateTime m_expiresAt;
    QDateTime m_refreshExpiresAt;
    QString m_source;
    Status m_status = Status::Missing;
    QFileSystemWatcher* m_watcher = nullptr;
};

} // namespace claudometer::core
