#pragma once

#include "UsageState.h"

#include <QString>

namespace claudedial::core {

/// Human-readable strings shared by the tray tooltip, the popup and the CLI.
/// `now` is injectable so the formatting is testable without waiting for a clock.
namespace format {

/// "resets in 1h 52m", "resets in 4m", "resets now".
QString resetRelative(const QDateTime& resetAt, const QDateTime& now = QDateTime::currentDateTimeUtc());

/// The reset wording each window uses: a countdown for both, which is what
/// Claude Code shows. Lower case, for use after a separator in the tooltip.
/// Empty when there is no timestamp.
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

/// One window as a line for the tray menu: "Session 63% · resets in 2h".
///
/// The menu carries these because a tooltip cannot be relied on. AppIndicator,
/// which is how GNOME has a tray at all, supports no tooltips whatsoever - so
/// on that desktop the middle tier of the information hierarchy is simply
/// absent unless the menu carries it. DBusMenu renders plain entries fine,
/// which is what makes this work where a tooltip does not.
///
/// Empty when that window has no data, so the caller can leave the entry out
/// rather than show a placeholder.
QString menuEntry(PeriodKind kind, const UsageState& state,
                  const QDateTime& now = QDateTime::currentDateTimeUtc());

/// The title of the notification announcing that `threshold` was crossed.
///
/// Takes the user's own critical threshold rather than assuming a number: the
/// two configurable stops are theirs to move, and a banner calling their
/// critical threshold a mere warning tells them the wrong thing about their own
/// setting.
[[nodiscard]] QString thresholdTitle(int threshold, int criticalThreshold);

} // namespace format
} // namespace claudedial::core
