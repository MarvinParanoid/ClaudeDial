#pragma once

#include "TrayBackend.h"

class QMenu;
class QSystemTrayIcon;

namespace claudedial::tray {

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
    ///
    /// Necessary but not sufficient: it can report a tray where no icon of ours
    /// can appear. Confirm with hasVisibleIcon() once the icon has been shown.
    [[nodiscard]] static bool isAvailable();

    /// Whether our icon actually reached a panel - measured, not predicted.
    ///
    /// Qt chooses its tray implementation from the platform theme, and the two
    /// paths fail differently. A D-Bus theme with no StatusNotifierHost on the
    /// bus registers nothing and shows nothing, while the XEmbed tracker still
    /// answers isAvailable() with true. So ask the two mechanisms directly: a
    /// docked XEmbed window has a geometry, and an SNI item appears in the
    /// watcher's list owned by this process. See docs/platform-support.md for
    /// the measurement this is built on.
    [[nodiscard]] bool hasVisibleIcon() const override;

    void setIcon(const QIcon& icon) override;
    void setToolTip(const QString& tooltip) override;
    void show() override;
    [[nodiscard]] QRect iconGeometry() const override;

private:
    QSystemTrayIcon* m_tray;
    QMenu* m_menu;
};

} // namespace claudedial::tray
