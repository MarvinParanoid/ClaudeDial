#include "RefreshSchedule.h"

#include <algorithm>
#include <iterator>

namespace claudedial::core {
namespace {

/// Backoff ladder for 429s, in minutes, capped at the last entry.
constexpr int kBackoffMinutes[] = { 3, 6, 12, 15 };

/// How long after a window's reset to poll, so the new window is reflected.
constexpr int kPostResetDelayMs = 5000;

} // namespace

int rateLimitBackoffMinutes(int strikes)
{
    if (strikes <= 0)
        return 0;
    const int index = std::min<int>(strikes - 1, static_cast<int>(std::size(kBackoffMinutes)) - 1);
    return kBackoffMinutes[index];
}

qint64 nextRefreshMs(int refreshIntervalSeconds, int rateLimitStrikes, int retryAfterSeconds,
                     const UsageState& state, const QDateTime& now)
{
    qint64 intervalMs = static_cast<qint64>(refreshIntervalSeconds) * 1000;

    if (rateLimitStrikes > 0) {
        intervalMs = std::max<qint64>(intervalMs, rateLimitBackoffMinutes(rateLimitStrikes) * 60LL * 1000);
        // Never come back sooner than the server asked, whatever our own ladder
        // says. The ladder is a guess for when it does not tell us.
        return std::max<qint64>(intervalMs, static_cast<qint64>(retryAfterSeconds) * 1000LL);
    }

    // Align to the nearest upcoming reset so the first poll of a new window is
    // prompt, instead of showing a full interval of stale percentages.
    for (const auto kind : { PeriodKind::FiveHour, PeriodKind::SevenDay }) {
        const auto& period = state.period(kind);
        if (!period || !period->resetAt)
            continue;
        const qint64 untilReset = now.msecsTo(*period->resetAt) + kPostResetDelayMs;
        if (untilReset > 0 && untilReset < intervalMs)
            intervalMs = untilReset;
    }

    return intervalMs;
}

} // namespace claudedial::core
