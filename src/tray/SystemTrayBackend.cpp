#include "SystemTrayBackend.h"

#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>

namespace claudometer::tray {

SystemTrayBackend::SystemTrayBackend(QObject* parent)
    : TrayBackend(parent)
    , m_tray(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
{
    // The context menu is rendered by the panel itself over DBusMenu, so it must
    // stay plain actions - a custom widget would simply not appear.
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
                if (reason == QSystemTrayIcon::Trigger)
                    Q_EMIT activated();
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

} // namespace claudometer::tray
