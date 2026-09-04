#pragma once

#include <QString>

namespace claudedial::core {

/// How alarming a percentage is.
///
/// One definition, used by the tray icon, the popup and `--json`. It lived in
/// three places before and would have drifted the moment the ramp changed.
enum class UsageLevel {
    Normal,        ///< quiet; no colour
    Warning,       ///< accent
    Critical,      ///< orange
    Severe,        ///< red
    LimitReached,  ///< red
};

/// Where the fixed high-water marks sit. These are also the notification
/// thresholds, so the colour a user sees and the alert they get agree.
constexpr int kSevereThreshold = 95;
constexpr int kLimitThreshold = 100;

/// Monotonic by construction, including when the configured thresholds are
/// unusual (a critical threshold above 95, or above the warning one).
[[nodiscard]] UsageLevel levelFor(double percentage, int warningThreshold, int criticalThreshold);

/// The stable string used in `--json`'s `class` field and by the QML theme.
[[nodiscard]] QString levelName(UsageLevel level);

} // namespace claudedial::core
