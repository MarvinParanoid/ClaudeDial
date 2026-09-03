#pragma once

#include "UsageState.h"

#include <QObject>
#include <optional>

class QNetworkAccessManager;
class QJsonValue;

namespace claudometer::core {

class Credentials;

/// Why a fetch did not produce a new state.
enum class FetchError {
    NoCredentials,  ///< not signed in with OAuth at all
    TokenExpired,   ///< local token expired; we refuse to refresh it ourselves
    Unauthorized,   ///< server rejected the token (401/403)
    RateLimited,    ///< 429 - back off
    Network,        ///< timeout, offline, DNS, 5xx
    BadResponse,    ///< 200 but nothing we could parse
};

/// Performs the single HTTP request Claudometer makes, and parses it.
///
/// The endpoint is undocumented and marked experimental by Claude Code's own
/// schema, so parsing is deliberately lenient: unknown keys, new quota buckets
/// and null fields are non-events. See docs/usage-api.md.
class UsageClient : public QObject
{
    Q_OBJECT

public:
    explicit UsageClient(Credentials* credentials, QObject* parent = nullptr);

    /// Issues one request. Emits exactly one of succeeded()/failed().
    void fetch();

    /// Exposed for tests. Returns nullopt when neither window could be read.
    [[nodiscard]] static std::optional<UsageState> parseResponse(const QByteArray& body);

    /// Exposed for tests. nullopt for null/absent/unusable windows.
    [[nodiscard]] static std::optional<UsagePeriod> parseWindow(const QJsonValue& value);

    /// True when CLAUDOMETER_SIMULATE is set, in which case no request is made.
    ///
    /// A development aid, and the only way to reach the upper end of the scale:
    /// the 95% and 100% steps are fixed, so without this the colours, the "!"
    /// glyph and the notifications for those steps cannot be exercised short of
    /// actually spending a month's limit.
    [[nodiscard]] static bool isSimulating();

Q_SIGNALS:
    void succeeded(const claudometer::core::UsageState& state);
    void failed(claudometer::core::FetchError error);

private:
    Credentials* m_credentials;
    QNetworkAccessManager* m_network;
    bool m_inFlight = false;
};

} // namespace claudometer::core
