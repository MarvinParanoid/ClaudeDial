#include "SystemTrayBackend.h"

#include <QAction>
#include <QCoreApplication>
#ifdef CLAUDEDIAL_HAVE_DBUS
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#endif
#include <QMenu>
#include <QStringList>
#include <QPixmap>
#include <QSystemTrayIcon>

namespace claudedial::tray {

SystemTrayBackend::SystemTrayBackend(QObject* parent)
    : TrayBackend(parent)
    , m_tray(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
{
    // The context menu is rendered by the panel itself over DBusMenu, so it must
    // stay plain actions - a custom widget would simply not appear.

    // First, and deliberately: on desktops where a left click opens this menu
    // instead of activating the item, this is the only way to reach the popup.
    auto* show = m_menu->addAction(tr("Show usage"));
    connect(show, &QAction::triggered, this, &TrayBackend::showRequested);

    m_menu->addSeparator();

    auto* refresh = m_menu->addAction(tr("Refresh now"));
    connect(refresh, &QAction::triggered, this, &TrayBackend::refreshRequested);

    auto* settings = m_menu->addAction(tr("Settings..."));
    connect(settings, &QAction::triggered, this, &TrayBackend::settingsRequested);

    m_menu->addSeparator();

    auto* quit = m_menu->addAction(tr("Quit"));
    connect(quit, &QAction::triggered, this, &TrayBackend::quitRequested);

    m_tray->setContextMenu(m_menu);

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                switch (reason) {
                case QSystemTrayIcon::Trigger:
                    Q_EMIT activated();
                    break;
                case QSystemTrayIcon::DoubleClick:
                    // Show rather than toggle: a double click arrives as Trigger
                    // followed by DoubleClick, so the pair must be idempotent.
                    // GNOME's AppIndicator extension reaches an application's UI
                    // only this way.
                    Q_EMIT showRequested();
                    break;
                default:
                    break;
                }
            });
}

SystemTrayBackend::~SystemTrayBackend()
{
    // A QMenu is a widget and cannot take a QObject parent.
    delete m_menu;
}

bool SystemTrayBackend::isAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void SystemTrayBackend::showMessage(const QString& title, const QString& body,
                                   const QImage& icon, bool critical)
{
    constexpr int kOrdinaryMs = 8000;
    constexpr int kCriticalMs = 20000;
    m_tray->showMessage(title, body,
                        icon.isNull() ? m_tray->icon() : QIcon(QPixmap::fromImage(icon)),
                        critical ? kCriticalMs : kOrdinaryMs);
}

bool SystemTrayBackend::hasVisibleIcon() const
{
    // Positive evidence only, and deliberately weak.
    //
    // This exists to catch one situation: the desktop reports a tray while
    // nothing on the system can actually display our item. It must never
    // contradict a panel that is plainly showing the icon, and the first version
    // did exactly that. It looked our own item up in the watcher's
    // RegisteredStatusNotifierItems and matched it by pid, which works on
    // Plasma; on GNOME with the AppIndicator extension the icon was visible and
    // working in the top bar while this reported "did not appear". A false
    // warning is worse than no warning, so the bar is now: is there anything at
    // all that could show an item?
    //
    // A docked XEmbed window has a geometry.
    if (!m_tray->geometry().isEmpty())
        return true;

    // Otherwise the item travels over StatusNotifierItem, and the one thing that
    // can be established without trusting a host's bookkeeping is whether a host
    // exists. Registration is asynchronous and hosts differ in what they report
    // about it, so anything more specific than this is guesswork dressed as a
    // check.
#ifdef CLAUDEDIAL_HAVE_DBUS
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;

    QDBusConnectionInterface* daemon = bus.interface();
    return daemon != nullptr
        && daemon->isServiceRegistered(QStringLiteral("org.kde.StatusNotifierWatcher"));
#else
    // No bus to ask. Where Qt has a native tray - Windows, macOS - an empty
    // geometry is not evidence of anything, so claim nothing rather than warn.
    return true;
#endif
}

void SystemTrayBackend::setIcon(const QIcon& icon)
{
    m_tray->setIcon(icon);
}

void SystemTrayBackend::setToolTip(const QString& tooltip)
{
    m_tray->setToolTip(tooltip);
}

void SystemTrayBackend::show()
{
    m_tray->show();
}

QRect SystemTrayBackend::iconGeometry() const
{
    return m_tray->geometry();
}

} // namespace claudedial::tray
