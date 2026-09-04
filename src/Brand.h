#pragma once

#include "core/UsageLevel.h"

#include <QColor>

namespace claudedial::brand {

/// The colours, in one place, with one role each.
///
/// Three roles, kept apart deliberately:
///
///   identity  - ClaudeDial itself. The application icon and the popup's
///               header mark, and nothing else. Never a control, never a usage
///               level: painting the whole application in it would cost the
///               usage ramp its meaning and stop the controls matching the
///               desktop they sit on.
///   usage     - how much of a limit is spent. The same four steps in the tray
///               icon, the popup and --json, because a user who sees amber in
///               one and blue in the other has learned nothing.
///   accent    - interaction. Not defined here at all: it is the user's own
///               Plasma accent, read from the palette at run time.
///
/// The usage steps were duplicated between the tray renderer and the QML theme
/// until they had to match exactly, at which point the duplication became a bug
/// waiting to happen.

/// Claude's terracotta.
inline const QColor kIdentity { 0xd9, 0x77, 0x57 };

/// The tray mark's neutral, while nothing needs attention.
///
/// A fixed mid-tone, and deliberately not the panel's own foreground. There is
/// no way to learn what a panel looks like: not from a StatusNotifierItem host,
/// not from an XEmbed one, and not from QPalette - which describes the
/// *application* colours and is a separate setting from the panel's. Two
/// independent reports proved the proxy wrong in mainstream configurations, and
/// in opposite ways. On i3, QPalette::WindowText is #000000 with colorScheme
/// Unknown, against i3bar's black. On KDE's shipped Breeze Twilight, the
/// palette is honest and still useless: ColorScheme=BreezeLight for
/// applications, plasmarc Theme=breeze-dark for the panel, so a correctly
/// sampled near-black landed on a dark panel. In both, the icon was invisible
/// below 75% and appeared above it - exactly where these usage colours take
/// over from the neutral.
///
/// Contrast ratios decided the value. Against a dark panel, a light panel and
/// i3bar's black: near-black gives 1.0 / 13.3 / 1.4 and light grey 11.1 / 1.2 /
/// 15.3 - each invisible somewhere. This grey gives 5.4 / 2.5 / 7.5, never
/// invisible anywhere, and its worst case is no worse than kUsageWarning's 2.3,
/// which has shipped without complaint. Terracotta scores similarly but shares
/// a hue with the warning step and would blunt the escalation.
inline const QColor kTrayNeutral { 0x9a, 0x9a, 0x9a };

inline const QColor kUsageWarning { 0xfd, 0xbc, 0x4b };
inline const QColor kUsageCritical { 0xf0, 0x84, 0x2c };
inline const QColor kUsageSevere { 0xda, 0x44, 0x53 };

/// The colour for a level.
///
/// `neutral` comes from the caller because "not worth colouring yet" means
/// different things in different places: the panel's own foreground in a tray
/// icon, a quiet grey on the popup's card.
[[nodiscard]] inline QColor usageColour(core::UsageLevel level, const QColor& neutral)
{
    switch (level) {
    case core::UsageLevel::Normal:
        return neutral;
    case core::UsageLevel::Warning:
        return kUsageWarning;
    case core::UsageLevel::Critical:
        return kUsageCritical;
    case core::UsageLevel::Severe:
    case core::UsageLevel::LimitReached:
        return kUsageSevere;
    }
    return neutral;
}

} // namespace claudedial::brand
