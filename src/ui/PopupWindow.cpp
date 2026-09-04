#include "PopupWindow.h"

#include <QCursor>
#include <QGuiApplication>
#include <QQuickItem>
#include <QScreen>

#include <cmath>

namespace claudedial::ui {
namespace {

/// Gap between the popup and the panel edge it sits against.
constexpr int kMargin = 8;

} // namespace

PopupWindow::PopupWindow(QQmlEngine* engine, const QUrl& source)
    : QQuickView(engine, nullptr)
{
    // Tool rather than Popup: a Popup without a parent window behaves
    // inconsistently across compositors, while Tool reliably stays undecorated
    // and out of the task switcher.
    setFlags(Qt::Tool | Qt::FramelessWindowHint);
    // Never shown - the window has no decorations - but it is how a KWin window
    // rule can pin this window without also matching the settings window, which
    // shares the application's window class.
    setTitle(QStringLiteral("ClaudeDial"));
    setResizeMode(QQuickView::SizeRootObjectToView);
    setColor(Qt::transparent); // let the QML rectangle's rounded corners show

    setSource(source);

    if (QQuickItem* root = rootObject()) {
        connect(root, &QQuickItem::implicitWidthChanged, this, &PopupWindow::syncSize);
        connect(root, &QQuickItem::implicitHeightChanged, this, &PopupWindow::syncSize);
    }
    syncSize();

    // Deliberately no dismiss-on-focus-loss.
    //
    // A tray popup conventionally vanishes when it loses focus, and that is what
    // this did. It is the wrong behaviour here: the compositor decides where the
    // window lands (a Wayland client cannot place its own top-levels), so the
    // user has to be able to drag it somewhere useful and leave it there while
    // working in another window. Closing is explicit instead - the tray icon
    // toggles it, the close button and Escape shut it.
}

void PopupWindow::syncSize()
{
    QQuickItem* root = rootObject();
    if (!root)
        return;

    const QSize wanted(static_cast<int>(std::ceil(root->implicitWidth())),
                       static_cast<int>(std::ceil(root->implicitHeight())));
    if (wanted.isValid() && wanted != size())
        resize(wanted);
}

void PopupWindow::toggle(const QRect& anchor)
{
    if (isVisible()) {
        hide();
        return;
    }
    placeAndShow(anchor);
}

void PopupWindow::present(const QRect& anchor)
{
    if (isVisible()) {
        raise();
        requestActivate();
        return;
    }
    placeAndShow(anchor);
}

void PopupWindow::beginMove()
{
    startSystemMove();
}

void PopupWindow::placeAndShow(const QRect& anchor)
{
    // The content may have changed since the popup was last shown.
    syncSize();

    const QPoint cursor = QCursor::pos();
    QScreen* screen = QGuiApplication::screenAt(cursor);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    const QSize popup = size();
    QPoint position;

    if (!anchor.isEmpty()) {
        // Anchor to the icon. Reached where Qt uses its XEmbed tray - an X11
        // desktop whose platform theme is not D-Bus-based - and on Windows
        // and macOS. Never on Plasma, which takes the SNI path on X11 as
        // well as Wayland and reports no geometry at all. Measured; see
        // docs/platform-support.md.
        position.setX(anchor.center().x() - popup.width() / 2);
        position.setY(anchor.center().y() < available.center().y()
                          ? anchor.bottom() + kMargin
                          : anchor.top() - popup.height() - kMargin);
    } else {
        // No geometry from the host. The cursor was just on the tray icon, so
        // the nearest corner is almost always the right corner.
        position.setX(cursor.x() < available.center().x()
                          ? available.left() + kMargin
                          : available.right() - popup.width() - kMargin);
        position.setY(cursor.y() < available.center().y()
                          ? available.top() + kMargin
                          : available.bottom() - popup.height() - kMargin);
    }

    position.setX(qBound(available.left() + kMargin, position.x(),
                         available.right() - popup.width() - kMargin));
    position.setY(qBound(available.top() + kMargin, position.y(),
                         available.bottom() - popup.height() - kMargin));

    setPosition(position);
    show();
    raise();
    requestActivate();
}

} // namespace claudedial::ui
