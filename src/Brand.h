#pragma once

#include "core/UsageLevel.h"

#include <QColor>

namespace claudometer::brand {

/// The colours, in one place, with one role each.
///
/// Three roles, kept apart deliberately:
///
///   identity  - Claudometer itself. The application icon and the popup's
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

} // namespace claudometer::brand
