#include "UsageService.h"

#include "RefreshSchedule.h"

#include "Config.h"
#include "UsageLevel.h"

#include <QCoreApplication>
#include <QTimer>

#include <algorithm>

namespace claudedial::core {
namespace {

/// Coalesce the burst of file-watcher events a credential rewrite produces.
constexpr int kCredentialDebounceMs = 1000;

/// Keys under which the already-announced thresholds are remembered.
constexpr auto kFiveHourKey = "five_hour";
constexpr auto kSevenDayKey = "seven_day";

QString windowKey(PeriodKind kind)
{
    return QString::fromLatin1(kind == PeriodKind::FiveHour ? kFiveHourKey : kSevenDayKey);
}

} // namespace

UsageService::UsageService(Credentials* credentials, Config* config, QObject* parent)
    : QObject(parent)
    , m_credentials(credentials)
    , m_config(config)
    , m_client(new UsageClient(credentials, this))
    , m_timer(new QTimer(this))
    , m_credentialDebounce(new QTimer(this))
{
    m_timer->setSingleShot(true);
    // Named so a test can fire it without waiting out a poll interval.
    m_timer->setObjectName(QStringLiteral("refresh"));
    m_credentialDebounce->setSingleShot(true);
    m_credentialDebounce->setInterval(kCredentialDebounceMs);

    // The scheduled poll goes through refreshNow rather than straight to the
    // client. It used to call fetch() and only then set m_fetching, which is
    // backwards whenever fetch() answers synchronously - simulation does, and so
    // does any bad credential status - because the handler had already cleared
    // the flag by the time it was set. The service was then stuck believing a
    // request was in flight, and every manual refresh after that returned at the
    // guard. Going through refreshNow also re-reads a credential source that
    // cannot be watched, which the direct call skipped entirely.
    connect(m_timer, &QTimer::timeout, this, &UsageService::refreshNow);

    connect(m_client, &UsageClient::succeeded, this, &UsageService::onSucceeded);
    connect(m_client, &UsageClient::failed, this, &UsageService::onFailed);

    // Claude Code refreshed its token: re-read and try again straight away.
    connect(m_credentials, &Credentials::changed, m_credentialDebounce, [this] {
        m_credentialDebounce->start();
    });
    connect(m_credentialDebounce, &QTimer::timeout, this, [this] {
        const auto before = m_credentials->status();
        const auto after = m_credentials->reload();
        if (after != before)
            Q_EMIT stateChanged();
        if (after == Credentials::Status::Ok)
            refreshNow();
    });

    // Pick up where the previous run left off, so a restart does not re-announce
    // a threshold for the window the user is still in.
    for (const auto kind : { PeriodKind::FiveHour, PeriodKind::SevenDay }) {
        const QString key = windowKey(kind);
        const auto thresholds = m_config->firedThresholds(key);
        QSet<int>& fired = kind == PeriodKind::FiveHour ? m_firedFiveHour : m_firedSevenDay;
        fired = QSet<int>(thresholds.begin(), thresholds.end());
        QDateTime& lastReset = kind == PeriodKind::FiveHour ? m_lastFiveHourReset
                                                            : m_lastSevenDayReset;
        lastReset = m_config->firedWindowReset(key);
    }

    // A changed refresh interval should take effect without a restart.
    connect(m_config, &Config::changed, this, [this] {
        if (m_timer->isActive())
            scheduleNext();
    });
}

Credentials::Status UsageService::credentialStatus() const
{
    return m_credentials->status();
}

QString UsageService::unavailableReason() const
{
    if (m_state.isValid())
        return {};

    // A transport-level failure is more informative than the credential state
    // when the credentials are fine and the request simply did not land.
    if (m_credentials->status() == Credentials::Status::Ok && m_lastError) {
        switch (*m_lastError) {
        case FetchError::RateLimited:
            return QCoreApplication::translate("UsageService",
                "Rate limited by the usage endpoint. Retrying shortly.");
        case FetchError::Network:
            return QCoreApplication::translate("UsageService",
                "Could not reach api.anthropic.com.");
        case FetchError::Unauthorized:
            return QCoreApplication::translate("UsageService",
                "The server rejected the token. Run Claude Code to sign in again.");
        case FetchError::BadResponse:
            return QCoreApplication::translate("UsageService",
                "The usage endpoint returned something unexpected.");
        case FetchError::NoCredentials:
        case FetchError::TokenExpired:
            break; // handled by the credential status below
        }
    }

    switch (m_credentials->status()) {
    case Credentials::Status::Missing:
        return QCoreApplication::translate("UsageService",
            "No Claude subscription credentials found. Sign in with Claude Code.");
    case Credentials::Status::RefreshExpired:
        return QCoreApplication::translate("UsageService",
            "Claude Code sign-in has expired. Run Claude Code to sign in again.");
    case Credentials::Status::Expired:
        return QCoreApplication::translate("UsageService",
            "Claude Code's token has expired. Run Claude Code to refresh it.");
    case Credentials::Status::Ok:
        return QCoreApplication::translate("UsageService", "Usage data is not available yet.");
    }
    return {};
}

void UsageService::start()
{
    m_credentials->reload();
    refreshNow();
}

void UsageService::refreshNow()
{
    if (m_fetching)
        return;

    m_timer->stop();

    // Re-read before every attempt: the token may have been refreshed by Claude
    // Code since the last poll, and it costs one small file read. A source that
    // cannot be watched has to be re-read even when the last read was good.
    if (m_credentials->status() != Credentials::Status::Ok || !m_credentials->watchesForChanges())
        m_credentials->reload();

    m_fetching = true;
    Q_EMIT fetchingChanged();
    m_client->fetch();
}

void UsageService::onSucceeded(const UsageState& state)
{
    m_fetching = false;
    m_rateLimitStrikes = 0;
    m_lastError.reset();
    m_retryAfterSeconds = 0;

    m_state = state;
    m_state.stale = false;

    evaluateThresholds(m_state);

    Q_EMIT fetchingChanged();
    Q_EMIT stateChanged();
    scheduleNext();
}

void UsageService::onFailed(FetchError error, int retryAfterSeconds)
{
    m_fetching = false;
    m_lastError = error;
    m_retryAfterSeconds = retryAfterSeconds;

    // A failure never clears good data - it only marks it stale.
    if (m_state.isValid())
        m_state.stale = true;

    if (error == FetchError::RateLimited)
        ++m_rateLimitStrikes;
    else
        m_rateLimitStrikes = 0;

    Q_EMIT fetchingChanged();
    Q_EMIT fetchFailed(error);
    Q_EMIT stateChanged();

    // Nothing to poll for until the credentials change. Where they are watched
    // the watcher wakes us, so stop; where they are not, polling is the only way
    // to notice Claude Code signing in again.
    if ((error == FetchError::NoCredentials || error == FetchError::TokenExpired)
        && m_credentials->watchesForChanges()) {
        m_timer->stop();
        return;
    }

    scheduleNext();
}

void UsageService::scheduleNext()
{
    // The decision itself lives in RefreshSchedule, where it can be tested.
    m_timer->start(static_cast<int>(nextRefreshMs(m_config->refreshIntervalSeconds(),
                                                  m_rateLimitStrikes, m_retryAfterSeconds,
                                                  m_state, QDateTime::currentDateTimeUtc())));
}

QList<int> UsageService::thresholdsFor() const
{
    // The configured warning/critical points, plus the two fixed high-water
    // marks that matter regardless of configuration. These are the same stops
    // the colour ramp uses, so what the user sees and what they are told agree.
    QList<int> thresholds { m_config->warningThreshold(), m_config->criticalThreshold(),
                            kSevereThreshold, kLimitThreshold };
    std::sort(thresholds.begin(), thresholds.end());
    thresholds.erase(std::unique(thresholds.begin(), thresholds.end()), thresholds.end());
    return thresholds;
}

void UsageService::resetFiredThresholds(PeriodKind kind)
{
    if (kind == PeriodKind::FiveHour)
        m_firedFiveHour.clear();
    else
        m_firedSevenDay.clear();
}

void UsageService::persistFiredThresholds(PeriodKind kind)
{
    // Simulated numbers must not be written down: a run at 100% would otherwise
    // record that step as already announced and silence the real notification
    // when the user genuinely reaches it.
    if (UsageClient::isSimulating())
        return;

    const QSet<int>& fired = kind == PeriodKind::FiveHour ? m_firedFiveHour : m_firedSevenDay;
    const QDateTime& lastReset = kind == PeriodKind::FiveHour ? m_lastFiveHourReset
                                                              : m_lastSevenDayReset;
    m_config->setFiredThresholds(windowKey(kind), QList<int>(fired.begin(), fired.end()),
                                 lastReset);
}

void UsageService::evaluateThresholds(const UsageState& current)
{
    for (const auto kind : { PeriodKind::FiveHour, PeriodKind::SevenDay }) {
        const auto& period = current.period(kind);
        if (!period)
            continue;

        QDateTime& lastReset = kind == PeriodKind::FiveHour ? m_lastFiveHourReset : m_lastSevenDayReset;
        QSet<int>& fired = kind == PeriodKind::FiveHour ? m_firedFiveHour : m_firedSevenDay;

        // A window began when its reset timestamp moves forward. That is the
        // definitive signal; a falling percentage is not one, because the
        // seven-day figure also drops mid-window as older usage ages out.
        bool windowChanged = false;
        if (period->resetAt) {
            if (lastReset.isValid() && *period->resetAt > lastReset) {
                resetFiredThresholds(kind);
                windowChanged = true;
            }
            if (lastReset != *period->resetAt) {
                lastReset = *period->resetAt;
                windowChanged = true;
            }
        }

        bool changed = false;
        for (const int threshold : thresholdsFor()) {
            if (period->percentage + 1e-9 < threshold)
                continue;
            if (fired.contains(threshold))
                continue;
            fired.insert(threshold);
            changed = true;
            Q_EMIT thresholdCrossed(kind, threshold, period->percentage);
        }

        if (changed || windowChanged)
            persistFiredThresholds(kind);
    }
}

} // namespace claudedial::core
