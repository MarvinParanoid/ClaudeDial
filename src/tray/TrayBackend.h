#pragma once

#include <QIcon>
#include <QObject>
#include <QRect>
#include <QString>

namespace claudometer::tray {

/// What the rest of the application needs from a tray, and nothing more.
///
/// Deliberately the only abstraction in the project. It exists because the
/// StatusNotifierItem path may need a fallback (AppIndicator, or a hand-rolled
/// SNI implementation) on desktops where Qt's own backend misbehaves - and one
/// virtual call is a cheap price for being able to add one later without
/// touching anything else.
class TrayBackend : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~TrayBackend() override = default;

    virtual void setIcon(const QIcon& icon) = 0;
    virtual void setToolTip(const QString& tooltip) = 0;
    virtual void show() = 0;

    /// Empty when the host does not report it - which is the normal case under
    /// StatusNotifierItem, and always the case on Wayland.
    [[nodiscard]] virtual QRect iconGeometry() const = 0;

Q_SIGNALS:
    /// Primary activation: a left click on the icon. Toggles the popup.
    void activated();

    /// Show the popup, without toggling it.
    ///
    /// Kept separate from activated() because a menu item named "Show usage"
    /// must not close the popup when it is already open - and because a double
    /// click arrives as Trigger *then* DoubleClick, so mapping both to a toggle
    /// would open the popup and immediately shut it again.
    ///
    /// This is not a nicety. On GNOME, where the tray comes from the
    /// AppIndicator extension, a single left click opens the menu rather than
    /// activating the item, so the menu is the only route to the popup at all.
    void showRequested();

    void refreshRequested();
    void settingsRequested();
    void quitRequested();
};

} // namespace claudometer::tray
