#pragma once

#include "core/UsageState.h"

#include <QObject>
#include <QString>

namespace claudometer::tray {

/// Desktop notifications over the standard freedesktop.org D-Bus interface.
///
/// Talking to org.freedesktop.Notifications directly rather than shelling out to
/// notify-send buys the one thing that matters here: `replaces_id`, so a second
/// warning updates the first instead of stacking another banner on the user's
/// screen.
class Notifier : public QObject
{
    Q_OBJECT

public:
    explicit Notifier(QObject* parent = nullptr);

    void notifyThreshold(core::PeriodKind kind, int threshold);

private:
    void send(core::PeriodKind kind, const QString& title, const QString& body,
              const QString& urgencyHint);

    /// One id per window, not one overall. With a single id the 7-day banner
    /// replaced the 5-hour one whenever both crossed a threshold on the same
    /// poll, and the first was silently lost.
    quint32 m_fiveHourId = 0;
    quint32 m_sevenDayId = 0;
};

} // namespace claudometer::tray
