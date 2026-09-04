#include "SystemTrayBackend.h"

#include <QAction>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QMenu>
#include <QStringList>
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

bool SystemTrayBackend::hasVisibleIcon() const
{
    // The XEmbed path docks a real window into the panel, so any geometry at
    // all means the icon is on screen.
    if (!m_tray->geometry().isEmpty())
        return true;

    // Otherwise Qt should have registered a StatusNotifierItem for us. Match by
    // process rather than by bus name: Qt registers the item on a connection of
    // its own, so our default session bus name is not the one in the list.
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;

    QDBusInterface watcher(QStringLiteral("org.kde.StatusNotifierWatcher"),
                           QStringLiteral("/StatusNotifierWatcher"),
                           QStringLiteral("org.kde.StatusNotifierWatcher"), bus);
    if (!watcher.isValid())
        return false;

    const QStringList items = watcher.property("RegisteredStatusNotifierItems").toStringList();
    if (items.isEmpty())
        return false;

    QDBusInterface daemon(QStringLiteral("org.freedesktop.DBus"),
                          QStringLiteral("/org/freedesktop/DBus"),
                          QStringLiteral("org.freedesktop.DBus"), bus);
    const auto self = static_cast<uint>(QCoreApplication::applicationPid());
    for (const QString& entry : items) {
        // Entries are "<bus name>/<object path>".
        const QString name = entry.left(entry.indexOf(QLatin1Char('/')));
        if (name.isEmpty())
            continue;
        const QDBusReply<uint> owner =
            daemon.call(QStringLiteral("GetConnectionUnixProcessID"), name);
        if (owner.isValid() && owner.value() == self)
            return true;
    }
    return false;
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
