#pragma once

#include <QObject>

namespace claudometer::tray {

/// Emits resumed() after the machine wakes from suspend.
///
/// Qt has no cross-platform API for this, so we use the standard Linux
/// mechanism: the PrepareForSleep signal on org.freedesktop.login1, which fires
/// with `true` before suspending and `false` after resuming. If logind is not
/// present this class simply never fires, which is the correct degradation.
class SleepWatcher : public QObject
{
    Q_OBJECT

public:
    explicit SleepWatcher(QObject* parent = nullptr);

Q_SIGNALS:
    void resumed();

private Q_SLOTS:
    void onPrepareForSleep(bool aboutToSleep);
};

} // namespace claudometer::tray
