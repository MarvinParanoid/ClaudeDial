#pragma once

#include "core/UsageState.h"

#include <QImage>
#include <QObject>
#include <QString>

namespace claudedial::tray {

/// Desktop notifications.
///
/// On a Linux desktop, over the standard freedesktop.org D-Bus interface.
/// Everywhere else there is no such bus, so the same text and icon go out
/// through messageRequested() for the tray icon to show - which is what
/// QSystemTrayIcon::showMessage does natively on Windows and macOS. The wording,
/// the thresholds and the icon are decided here either way; only the delivery
/// differs.
///
/// Talking to org.freedesktop.Notifications directly rather than shelling out to
/// notify-send buys two things that matter: `replaces_id`, so a second warning
/// updates the first instead of stacking another banner, and the `image-data`
/// hint, which carries the icon as pixels.
///
/// Pixels rather than an icon name, because a name only resolves once the
/// application is installed into an icon theme - and until then the daemon draws
/// a blank document, which makes the notification look unfinished. Sending the
/// mark itself also lets the banner show the state it is announcing.
class Notifier : public QObject
{
    Q_OBJECT

public:
    explicit Notifier(QObject* parent = nullptr);

    /// `resetText` is the already-formatted reset line, e.g. "Resets in 42m" -
    /// more use in a notification than repeating the percentage, which the title
    /// and the icon have both already given.
    void notifyThreshold(core::PeriodKind kind, int threshold, int criticalThreshold,
                         const QString& resetText, const QImage& icon);

Q_SIGNALS:
    /// Emitted instead of sending, where there is no notification bus. Nothing
    /// connects to it on Linux, and nothing emits it there.
    void messageRequested(const QString& title, const QString& body, bool critical,
                          const QImage& icon);

private:
    void send(core::PeriodKind kind, const QString& title, const QString& body,
              bool critical, const QImage& icon);

    /// One id per window, not one overall. With a single id the 7-day banner
    /// replaced the 5-hour one whenever both crossed a threshold on the same
    /// poll, and the first was silently lost.
    quint32 m_fiveHourId = 0;
    quint32 m_sevenDayId = 0;
};

} // namespace claudedial::tray
