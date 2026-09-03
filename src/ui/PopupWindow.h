#pragma once

#include <QQuickView>

namespace claudometer::ui {

/// The small frameless window the tray icon opens.
///
/// Placement is the awkward part on Linux. Under StatusNotifierItem the panel
/// owns the icon and reports no geometry, and a Wayland client cannot position
/// its own top-level windows at all - so anchoring the popup to the icon works
/// on X11 and degrades to a sensible screen corner elsewhere. See
/// docs/usage-api.md's sibling note in the README.
class PopupWindow : public QQuickView
{
    Q_OBJECT

public:
    PopupWindow(QQmlEngine* engine, const QUrl& source);

    /// Show anchored to `anchor` if the host gave us one, otherwise near the
    /// cursor's corner of the screen. Hides again if already visible.
    void toggle(const QRect& anchor);

    /// Hands an interactive move to the compositor, so the header acts as a
    /// drag handle. This is the only way to move the window on Wayland, where a
    /// client cannot position its own top-levels - and it is why the popup would
    /// otherwise be stuck wherever the compositor first put it.
    Q_INVOKABLE void beginMove();

private:
    void placeAndShow(const QRect& anchor);

    /// Resize the window to the QML content's implicit size.
    ///
    /// The content height changes at runtime - rows appear once data arrives,
    /// and the "unavailable" message comes and goes - and QQuickView's
    /// SizeViewToRootObject does not follow those changes once the window has
    /// been created, which silently clips the bottom of the popup. Driving the
    /// size explicitly from the root item's implicit size is the reliable way.
    void syncSize();
};

} // namespace claudometer::ui
