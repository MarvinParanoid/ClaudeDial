#pragma once

#include <QDateTime>
#include <optional>

namespace claudometer::core {

/// Which quota window a value belongs to. The two windows Claudometer tracks.
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

} // namespace claudometer::core
