#include "SleepWatcher.h"

#ifdef CLAUDEDIAL_HAVE_DBUS
#include <QDBusConnection>
#endif

namespace claudedial::tray {

SleepWatcher::SleepWatcher(QObject* parent)
    : QObject(parent)
{
#ifdef CLAUDEDIAL_HAVE_DBUS
    QDBusConnection::systemBus().connect(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"),
        QStringLiteral("PrepareForSleep"),
        this,
        SLOT(onPrepareForSleep(bool)));
#endif
}

void SleepWatcher::onPrepareForSleep(bool aboutToSleep)
{
    // false means "we just came back", which is the edge worth refreshing on.
    if (!aboutToSleep)
        Q_EMIT resumed();
}

} // namespace claudedial::tray
