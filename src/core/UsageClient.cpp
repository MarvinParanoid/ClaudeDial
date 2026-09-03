#include "UsageClient.h"

#include "Credentials.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QTimeZone>

#include <algorithm>

namespace claudometer::core {
namespace {

/// Pinned at compile time. A configurable endpoint would be a credential
/// exfiltration feature waiting to be social-engineered.
constexpr auto kUsageUrl = "https://api.anthropic.com/api/oauth/usage?skip_spend=1";

/// Matches Claude Code's own timeout for this request.
constexpr int kTimeoutMs = 5000;

/// Qt can log request headers, and ours carry the bearer token. A user's
/// ~/.config/QtProject/qtlogging.ini must not be able to switch that on for
/// Claudometer, so the rules are forced off here - in the constructor of the
/// only class that ever sends the token, rather than in an entry point that a
/// future one could forget to mirror. Rules set this way take precedence over
/// the configuration file.
void silenceNetworkLogging()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    QLoggingCategory::setFilterRules(QStringLiteral("qt.network.*.debug=false\n"
                                                    "qt.network.*.info=false"));
}

/// Reads CLAUDOMETER_SIMULATE, e.g. "96" or "96,41" - the five-hour and
/// seven-day percentages to report instead of asking the server.
std::optional<UsageState> simulatedState()
{
    const QString raw =
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("CLAUDOMETER_SIMULATE"));
    if (raw.isEmpty())
        return std::nullopt;

    const QStringList parts = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return std::nullopt;

    const auto window = [](const QString& text, int hoursUntilReset) -> std::optional<UsagePeriod> {
        bool ok = false;
        const double value = text.trimmed().toDouble(&ok);
        if (!ok)
            return std::nullopt;
        UsagePeriod period;
        period.percentage = std::clamp(value, 0.0, 100.0);
        period.resetAt = QDateTime::currentDateTimeUtc().addSecs(hoursUntilReset * 3600);
        return period;
    };

    UsageState state;
    state.fiveHour = window(parts.at(0), 2);
    state.sevenDay = parts.size() > 1 ? window(parts.at(1), 4 * 24) : std::nullopt;
    state.updatedAt = QDateTime::currentDateTimeUtc();

    if (!state.isValid())
        return std::nullopt;
    return state;
}

} // namespace

bool UsageClient::isSimulating()
{
    return simulatedState().has_value();
}

UsageClient::UsageClient(Credentials* credentials, QObject* parent)
    : QObject(parent)
    , m_credentials(credentials)
    , m_network(new QNetworkAccessManager(this))
{
    silenceNetworkLogging();
    m_network->setTransferTimeout(kTimeoutMs);
}

void UsageClient::fetch()
{
    if (m_inFlight)
        return;

    // Simulation short-circuits the network entirely, so it costs no request
    // and works offline.
    if (const auto simulated = simulatedState()) {
        Q_EMIT succeeded(*simulated);
        return;
    }

    switch (m_credentials->status()) {
    case Credentials::Status::Ok:
        break;
    case Credentials::Status::Expired:
        Q_EMIT failed(FetchError::TokenExpired);
        return;
    case Credentials::Status::Missing:
    case Credentials::Status::RefreshExpired:
        Q_EMIT failed(FetchError::NoCredentials);
        return;
    }

    QNetworkRequest request { QUrl(QString::fromLatin1(kUsageUrl)) };
    request.setRawHeader(QByteArrayLiteral("Content-Type"), QByteArrayLiteral("application/json"));
    request.setRawHeader(QByteArrayLiteral("anthropic-beta"), QByteArrayLiteral("oauth-2025-04-20"));
    request.setRawHeader(QByteArrayLiteral("User-Agent"),
                         QByteArrayLiteral("claudometer/" CLAUDOMETER_VERSION));
    // Never follow a redirect while carrying a bearer token: an unexpected
    // redirect would replay the Authorization header at another host.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);

    if (!m_credentials->authorize(request)) {
        Q_EMIT failed(FetchError::NoCredentials);
        return;
    }

    m_inFlight = true;
    QNetworkReply* reply = m_network->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_inFlight = false;

        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // Error strings are built from the status code and a fixed set of
        // messages - never by interpolating the request, which carries the token.
        if (http == 401 || http == 403) {
            Q_EMIT failed(FetchError::Unauthorized);
            return;
        }
        if (http == 429) {
            Q_EMIT failed(FetchError::RateLimited);
            return;
        }
        if (reply->error() != QNetworkReply::NoError || http != 200) {
            Q_EMIT failed(FetchError::Network);
            return;
        }

        if (const auto state = parseResponse(reply->readAll()))
            Q_EMIT succeeded(*state);
        else
            Q_EMIT failed(FetchError::BadResponse);
    });
}

std::optional<UsagePeriod> UsageClient::parseWindow(const QJsonValue& value)
{
    if (!value.isObject())
        return std::nullopt; // null, absent, or renamed - simply no data

    const QJsonObject object = value.toObject();

    const QJsonValue utilization = object.value(QStringLiteral("utilization"));
    if (!utilization.isDouble())
        return std::nullopt; // present but unusable

    UsagePeriod period;
    period.percentage = std::clamp(utilization.toDouble(), 0.0, 100.0);

    if (const QJsonValue resets = object.value(QStringLiteral("resets_at")); resets.isString()) {
        // ISO 8601 with fractional seconds and a +00:00 offset.
        QDateTime parsed = QDateTime::fromString(resets.toString(), Qt::ISODateWithMs);
        if (parsed.isValid())
            period.resetAt = parsed.toUTC();
        // Left unset when null or malformed - the UI then omits the reset line.
    }

    return period;
}

std::optional<UsageState> UsageClient::parseResponse(const QByteArray& body)
{
    QJsonParseError error {};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;

    const QJsonObject root = doc.object();

    UsageState state;
    state.fiveHour = parseWindow(root.value(QStringLiteral("five_hour")));
    state.sevenDay = parseWindow(root.value(QStringLiteral("seven_day")));
    state.updatedAt = QDateTime::currentDateTimeUtc();

    if (!state.isValid())
        return std::nullopt;

    return state;
}

} // namespace claudometer::core
