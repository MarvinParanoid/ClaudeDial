#include "AppIcon.h"

namespace claudedial {

QIcon applicationIcon()
{
    // The installed icon first: the desktop is already using it for the
    // .desktop entry, and matching it keeps the window, the launcher and the
    // task manager showing one thing.
    QIcon themed = QIcon::fromTheme(QStringLiteral("claudedial"));
    if (!themed.isNull() && !themed.availableSizes().isEmpty())
        return themed;

    // Not installed - running from a build tree. Same artwork, from the binary.
    return QIcon(QStringLiteral(":/icons/claudedial.svg"));
}

} // namespace claudedial
