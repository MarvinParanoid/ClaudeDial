#pragma once

#include "UsageState.h"

#include <QString>

namespace claudedial::core {

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

/// "Usage 63% · window 60%", or empty when the window position is unknown.
///
/// Deliberately two bare numbers with no interpretation. A percentage alone
/// answers "how much is spent" but not "is that a lot": 63% with four hours to
/// go is heavy, 63% with forty minutes left is fine. Putting the two side by
/// side lets the reader see 63 against 60 and draw their own conclusion, which
/// is as far as ClaudeDial goes - no pace multiplier, no projection, no verdict.
///
/// Experimental. If it turns out nobody's eye ever catches this line, it should
/// go; it is one line and one function, deliberately easy to remove.
QString pace(PeriodKind kind, const UsagePeriod& period,
             const QDateTime& now = QDateTime::currentDateTimeUtc());

/// "just now", "2 minutes ago", "1 hour ago".
QString updatedAgo(const QDateTime& updatedAt, const QDateTime& now = QDateTime::currentDateTimeUtc());

/// The multi-line tray tooltip.
QString tooltip(const UsageState& state, const QDateTime& now = QDateTime::currentDateTimeUtc());

} // namespace format
} // namespace claudedial::core
