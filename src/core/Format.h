#pragma once

#include "UsageState.h"

#include <QString>

namespace claudometer::core {

/// Human-readable strings shared by the tray tooltip, the popup and the CLI.
/// `now` is injectable so the formatting is testable without waiting for a clock.
namespace format {

/// "resets in 1h 52m", "resets in 4m", "resets now".
QString resetRelative(const QDateTime& resetAt, const QDateTime& now = QDateTime::currentDateTimeUtc());

/// "resets Mon 09:00" within the next week, "resets 14 Sep 09:00" beyond it.
QString resetAbsolute(const QDateTime& resetAt, const QDateTime& now = QDateTime::currentDateTimeUtc());

/// The reset wording each window uses: relative for the rolling 5-hour window,
/// absolute for the 7-day one. Lower case, for use after a separator in the
/// tooltip. Empty when there is no timestamp.
QString resetFor(PeriodKind kind, const UsagePeriod& period,
                 const QDateTime& now = QDateTime::currentDateTimeUtc());

/// The same wording as its own line in the popup, where it starts a sentence.
/// A separate translatable string rather than capitalising resetFor()'s result,
/// which would not survive translation into languages with different casing
/// rules. Empty when there is no timestamp.
QString resetSentence(PeriodKind kind, const UsagePeriod& period,
                      const QDateTime& now = QDateTime::currentDateTimeUtc());

/// "just now", "2 minutes ago", "1 hour ago".
QString updatedAgo(const QDateTime& updatedAt, const QDateTime& now = QDateTime::currentDateTimeUtc());

/// The multi-line tray tooltip.
QString tooltip(const UsageState& state, const QDateTime& now = QDateTime::currentDateTimeUtc());

} // namespace format
} // namespace claudometer::core
