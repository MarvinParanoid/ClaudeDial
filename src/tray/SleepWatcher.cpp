#include "SleepWatcher.h"

#include <QDBusConnection>

namespace claudometer::tray {

SleepWatcher::SleepWatcher(QObject* parent)
    : QObject(parent)
{
    QDBusConnection::systemBus().connect(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"),
        QStringLiteral("PrepareForSleep"),
        this,
        SLOT(onPrepareForSleep(bool)));
}

void SleepWatcher::onPrepareForSleep(bool aboutToSleep)
{
    // false means "we just came back", which is the edge worth refreshing on.
    if (!aboutToSleep)
        Q_EMIT resumed();
}

} // namespace claudometer::tray
