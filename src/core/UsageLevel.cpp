#include "UsageLevel.h"

#include <algorithm>

namespace claudometer::core {

UsageLevel levelFor(double percentage, int warningThreshold, int criticalThreshold)
{
    if (percentage >= kLimitThreshold)
        return UsageLevel::LimitReached;
    // max() keeps the ramp ordered even if the critical threshold is set above
    // the fixed severe mark, in which case severe simply starts later.
    if (percentage >= std::max(criticalThreshold, kSevereThreshold))
        return UsageLevel::Severe;
    if (percentage >= criticalThreshold)
        return UsageLevel::Critical;
    if (percentage >= warningThreshold)
        return UsageLevel::Warning;
    return UsageLevel::Normal;
}

QString levelName(UsageLevel level)
{
    switch (level) {
    case UsageLevel::Normal:
        return QStringLiteral("normal");
    case UsageLevel::Warning:
        return QStringLiteral("warning");
    case UsageLevel::Critical:
        return QStringLiteral("critical");
    case UsageLevel::Severe:
        return QStringLiteral("severe");
    case UsageLevel::LimitReached:
        return QStringLiteral("limit");
    }
    return QStringLiteral("normal");
}

} // namespace claudometer::core
