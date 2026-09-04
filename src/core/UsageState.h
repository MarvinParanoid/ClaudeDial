#pragma once

#include <QDateTime>

#include <algorithm>
#include <optional>

namespace claudedial::core {

/// Which quota window a value belongs to. The two windows ClaudeDial tracks.
enum class PeriodKind {
    FiveHour,
    SevenDay,
};

/// One quota window.
///
/// `resetAt` is independently optional: the API can report a valid utilization
/// with a null `resets_at` (observed on some buckets), in which case the UI must
/// show the percentage and omit the "resets in ..." line rather than invent one.
struct UsagePeriod {
    double percentage = 0.0;
    std::optional<QDateTime> resetAt;
};

/// A snapshot of usage.
///
/// Both windows are optional because the API returns `null` for windows that do
/// not apply to an account, and because we parse leniently: a renamed or removed
/// key yields no data rather than a parse failure.
/// Length of the session window, which is what makes "how far through it are we"
/// answerable at all.
///
/// Claude Code's own code uses the same 18000 seconds. Measured to be a *fixed*
/// boundary rather than a sliding one: two readings 75 s apart reported the same
/// `resets_at` to within sub-second jitter, where a sliding window would have
/// moved it forward by 75 s.
constexpr int kFiveHourWindowSeconds = 5 * 60 * 60;

/// How far through its window a period is, as a percentage, or nullopt when
/// that cannot honestly be said.
///
/// Only the five-hour window. For the seven-day one the idea stops being
/// useful: consumption across days is naturally uneven - weekends, days off -
/// so "40% through the week" says nothing about whether 40% spent is a lot.
///
/// Returns nullopt when there is no reset time, and when the arithmetic lands
/// outside the window at all. That last guard matters: the window length is an
/// assumption about an undocumented endpoint, and if it ever stops holding this
/// should go quiet rather than start lying.
[[nodiscard]] inline std::optional<double> windowProgress(PeriodKind kind,
                                                          const UsagePeriod& period,
                                                          const QDateTime& now)
{
    if (kind != PeriodKind::FiveHour || !period.resetAt)
        return std::nullopt;

    const double remaining = now.secsTo(*period.resetAt);
    const double elapsed = kFiveHourWindowSeconds - remaining;
    const double fraction = elapsed / kFiveHourWindowSeconds;

    // A little slack for clock skew and the endpoint's sub-second drift.
    if (fraction < -0.01 || fraction > 1.01)
        return std::nullopt;

    return std::clamp(fraction, 0.0, 1.0) * 100.0;
}

struct UsageState {
    std::optional<UsagePeriod> fiveHour;
    std::optional<UsagePeriod> sevenDay;
    QDateTime updatedAt;

    /// True when the last fetch failed and these numbers are from an earlier one.
    /// A display flag only - a failure never clears good data.
    bool stale = false;

    [[nodiscard]] bool isValid() const { return fiveHour.has_value() || sevenDay.has_value(); }

    [[nodiscard]] const std::optional<UsagePeriod>& period(PeriodKind k) const
    {
        return k == PeriodKind::FiveHour ? fiveHour : sevenDay;
    }
};

} // namespace claudedial::core
