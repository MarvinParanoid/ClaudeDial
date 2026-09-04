#pragma once

#include "Credentials.h"
#include "UsageClient.h"
#include "UsageState.h"

#include <QDateTime>
#include <QObject>
#include <QSet>

#include <optional>

class QTimer;

namespace claudometer::core {

class Config;

/// Owns the polling schedule, the staleness flag and the notification
/// thresholds. Everything above this class only observes state.
class UsageService : public QObject
{
    Q_OBJECT

public:
    UsageService(Credentials* credentials, Config* config, QObject* parent = nullptr);

    [[nodiscard]] const UsageState& state() const { return m_state; }
    [[nodiscard]] Credentials::Status credentialStatus() const;

    /// True while a request is outstanding - the popup shows a spinner.
    [[nodiscard]] bool isFetching() const { return m_fetching; }

    /// Human-readable reason there is no data, or empty when there is.
    [[nodiscard]] QString unavailableReason() const;

    /// Why the most recent attempt failed, if it did. Drives the wording shown
    /// to the user, so "rate limited" does not read as "not signed in".
    [[nodiscard]] std::optional<FetchError> lastError() const { return m_lastError; }

    /// Reads credentials and issues the first fetch.
    void start();

    /// Manual refresh (popup button) and post-resume refresh. Ignores backoff.
    void refreshNow();

Q_SIGNALS:
    void stateChanged();
    void fetchingChanged();
    void fetchFailed(claudometer::core::FetchError error);

    /// A usage threshold was crossed for the first time in this window.
    void thresholdCrossed(claudometer::core::PeriodKind kind, int threshold, double percentage);

private:
    void scheduleNext();
    void onSucceeded(const UsageState& state);
    void onFailed(FetchError error, int retryAfterSeconds);
    void evaluateThresholds(const UsageState& previous, const UsageState& current);
    void resetFiredThresholds(PeriodKind kind);
    void persistFiredThresholds(PeriodKind kind);
    [[nodiscard]] QList<int> thresholdsFor() const;

    Credentials* m_credentials;
    Config* m_config;
    UsageClient* m_client;
    QTimer* m_timer;
    QTimer* m_credentialDebounce;

    UsageState m_state;
    bool m_fetching = false;
    std::optional<FetchError> m_lastError;

    /// Consecutive rate-limit responses, driving the 3/6/12/15 minute backoff.
    int m_rateLimitStrikes = 0;

    /// What the server's last Retry-After asked for, in seconds. 0 when it did
    /// not ask.
    int m_retryAfterSeconds = 0;

    QSet<int> m_firedFiveHour;
    QSet<int> m_firedSevenDay;
    QDateTime m_lastFiveHourReset;
    QDateTime m_lastSevenDayReset;
};

} // namespace claudometer::core
