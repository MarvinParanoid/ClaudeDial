#pragma once

#include "TrayBackend.h"

class QMenu;
class QSystemTrayIcon;

namespace claudometer::tray {

/// QSystemTrayIcon-backed tray.
///
/// On a desktop with a StatusNotifierWatcher on the bus - Plasma, and GNOME with
/// the AppIndicator extension - Qt routes this over D-Bus as a
/// StatusNotifierItem automatically, which is the modern mechanism we want. Qt
/// already does this, so there is nothing to implement by hand.
class SystemTrayBackend : public TrayBackend
{
    Q_OBJECT

public:
    explicit SystemTrayBackend(QObject* parent = nullptr);
    ~SystemTrayBackend() override;

    /// False when no tray is available at all; the caller should say so and exit
    /// rather than running invisibly.
    [[nodiscard]] static bool isAvailable();

    void setIcon(const QIcon& icon) override;
    void setToolTip(const QString& tooltip) override;
    void show() override;
    [[nodiscard]] QRect iconGeometry() const override;

private:
    QSystemTrayIcon* m_tray;
    QMenu* m_menu;
};

} // namespace claudometer::tray
