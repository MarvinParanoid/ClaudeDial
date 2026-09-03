#include "Application.h"

#include "core/Config.h"
#include "core/Credentials.h"
#include "core/Format.h"
#include "core/UsageJson.h"
#include "core/UsageService.h"
#include "tray/IconRenderer.h"
#include "tray/Notifier.h"
#include "tray/SleepWatcher.h"
#include "tray/SystemTrayBackend.h"
#include "ui/PopupWindow.h"
#include "ui/SettingsViewModel.h"
#include "ui/UsageViewModel.h"

#include <QGuiApplication>
#include <QImage>
#include <QPalette>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QStyleHints>
#include <QTimer>

namespace claudometer {

using core::Config;
using core::Credentials;
using core::PeriodKind;
using core::UsageService;
using tray::IconRenderer;

Application::Application(QObject* parent)
    : QObject(parent)
    , m_config(new Config(this))
    , m_credentials(new Credentials(this))
    , m_service(new UsageService(m_credentials, m_config, this))
    , m_notifier(new tray::Notifier(this))
    , m_sleepWatcher(new tray::SleepWatcher(this))
    , m_tooltipTick(new QTimer(this))
{
}

bool Application::initialize()
{
    if (!tray::SystemTrayBackend::isAvailable())
        return false;

    // The window icon, drawn from the same mark. Without this the compositor
    // falls back to a placeholder for our app id until the .desktop file and
    // hicolor icon are installed, which looks like a foreign application in the
    // settings window's title bar.
    QGuiApplication::setWindowIcon(
        IconRenderer::logo(QGuiApplication::palette().color(QPalette::WindowText)));

    m_tray = new tray::SystemTrayBackend(this);

    m_engine = new QQmlEngine(this);
    m_usage = new ui::UsageViewModel(m_service, m_config, this);
    m_settings = new ui::SettingsViewModel(m_config, this);

    m_engine->rootContext()->setContextProperty(QStringLiteral("usage"), m_usage);
    m_engine->rootContext()->setContextProperty(QStringLiteral("settings"), m_settings);
    m_engine->rootContext()->setContextProperty(QStringLiteral("isDark"), false);

    m_popup = new ui::PopupWindow(m_engine, QUrl(QStringLiteral("qrc:/qml/Popup.qml")));
    // Exposed only so the header can act as a drag handle. Nothing sensitive
    // reaches QML through it - the window knows nothing about credentials.
    m_engine->rootContext()->setContextProperty(QStringLiteral("popupWindow"), m_popup);

    // --- tray -----------------------------------------------------------------
    connect(m_tray, &tray::TrayBackend::activated, this, [this] {
        m_popup->toggle(m_tray->iconGeometry());
    });
    connect(m_tray, &tray::TrayBackend::refreshRequested, m_service, &UsageService::refreshNow);
    connect(m_tray, &tray::TrayBackend::settingsRequested, this, &Application::showSettings);
    connect(m_tray, &tray::TrayBackend::quitRequested, qApp, &QCoreApplication::quit);

    connect(m_usage, &ui::UsageViewModel::settingsRequested, this, [this] {
        m_popup->hide();
        showSettings();
    });
    connect(m_usage, &ui::UsageViewModel::closeRequested, m_popup, &QQuickView::hide);

    // --- state ----------------------------------------------------------------
    connect(m_service, &UsageService::stateChanged, this, &Application::updateTray);
    connect(m_service, &UsageService::thresholdCrossed, this, &Application::onThresholdCrossed);
    connect(m_config, &Config::changed, this, &Application::applyTheme);
    connect(m_sleepWatcher, &tray::SleepWatcher::resumed, m_service, &UsageService::refreshNow);

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (!m_applyingTheme)
            applyTheme();
    });

    // Sampled before any Theme override is applied, so it really is the
    // desktop's colour and not one we chose.
    m_panelForeground = QGuiApplication::palette().color(QPalette::WindowText);

    // The tooltip contains "resets in 1h 52m", which decays on its own.
    m_tooltipTick->setInterval(60 * 1000);
    connect(m_tooltipTick, &QTimer::timeout, this, &Application::updateTray);
    m_tooltipTick->start();

    applyTheme();
    m_tray->show();
    m_service->start();

    return true;
}

void Application::showPopup()
{
    if (m_popup && !m_popup->isVisible())
        m_popup->toggle(m_tray ? m_tray->iconGeometry() : QRect());
}

QByteArray Application::statusJson() const
{
    return core::json::status(m_service->state(), m_config->warningThreshold(),
                              m_config->criticalThreshold());
}

void Application::updateTray()
{
    if (!m_tray)
        return;

    const auto& state = m_service->state();

    IconRenderer::Options options;
    options.style = m_config->trayStyle();
    // There is no way to ask a StatusNotifierItem host what its panel looks
    // like, so the application palette is the best available proxy. It is right
    // on Plasma with a matching panel theme and can be wrong on a panel that is
    // themed independently.
    options.foreground = m_panelForeground;
    options.warningThreshold = m_config->warningThreshold();
    options.criticalThreshold = m_config->criticalThreshold();
    options.stale = state.stale;

    const std::optional<double> percentage = state.fiveHour
        ? std::optional<double>(state.fiveHour->percentage)
        : std::nullopt;

    m_tray->setIcon(IconRenderer::render(percentage, options));

    const QString tooltip = state.isValid() ? core::format::tooltip(state)
                                            : QStringLiteral("Claudometer\n%1").arg(
                                                  m_service->unavailableReason());
    m_tray->setToolTip(tooltip);
}

void Application::applyTheme()
{
    auto* hints = QGuiApplication::styleHints();
    const Config::Theme theme = m_config->theme();

    // While we are not overriding anything, the application palette *is* the
    // desktop's, so this is the moment to note what the panel looks like. The
    // tray icon has to keep following the desktop even when the user forces
    // Light or Dark for Claudometer's own windows: forcing Light on a dark panel
    // would otherwise paint a dark icon onto a dark panel and lose it entirely.
    if (theme == Config::Theme::System)
        m_panelForeground = QGuiApplication::palette().color(QPalette::WindowText);

    Qt::ColorScheme requested = Qt::ColorScheme::Unknown; // Unknown = follow the system
    switch (theme) {
    case Config::Theme::Light:
        requested = Qt::ColorScheme::Light;
        break;
    case Config::Theme::Dark:
        requested = Qt::ColorScheme::Dark;
        break;
    case Config::Theme::System:
        break;
    }

    m_applyingTheme = true;
    hints->setColorScheme(requested);
    m_applyingTheme = false;

    // Derive this from the setting rather than reading colorScheme() back.
    // setColorScheme() is applied asynchronously by the platform theme, so
    // immediately after the call it still reports the previous value - which
    // silently made the Light and Dark settings do nothing at all. Only the
    // System case has to ask, and a late colorSchemeChanged will re-run this.
    const bool dark = theme == Config::Theme::Dark
        || (theme == Config::Theme::System && hints->colorScheme() == Qt::ColorScheme::Dark);
    m_engine->rootContext()->setContextProperty(QStringLiteral("isDark"), dark);

    updateTray();
}

void Application::showSettings()
{
    if (!m_settingsWindow) {
        m_settingsWindow = new QQuickView(m_engine, nullptr);
        m_settingsWindow->setTitle(tr("Claudometer Settings"));
        m_settingsWindow->setResizeMode(QQuickView::SizeRootObjectToView);
        m_settingsWindow->setSource(QUrl(QStringLiteral("qrc:/qml/Settings.qml")));
        if (QQuickItem* root = m_settingsWindow->rootObject()) {
            m_settingsWindow->resize(static_cast<int>(root->implicitWidth()),
                                     static_cast<int>(root->implicitHeight()));
        }
    }
    m_settingsWindow->show();
    m_settingsWindow->raise();
    m_settingsWindow->requestActivate();
}

void Application::onThresholdCrossed(PeriodKind kind, int threshold, double percentage)
{
    if (!m_config->notificationsEnabled())
        return;

    // The banner carries the mark in the state it is announcing, drawn at a size
    // where the number is comfortable to read.
    constexpr int kNotificationIconSize = 48;
    IconRenderer::Options options;
    // Always the number here, whatever the tray is set to: a notification is
    // about one specific figure, and at this size it is plainly legible.
    options.style = Config::TrayStyle::Percentage;
    options.foreground = m_panelForeground;
    options.warningThreshold = m_config->warningThreshold();
    options.criticalThreshold = m_config->criticalThreshold();

    const QImage icon = IconRenderer::render(percentage, options)
                            .pixmap(kNotificationIconSize, kNotificationIconSize)
                            .toImage();

    // The reset time is the part the user does not already have from the title
    // and the icon.
    const auto& period = m_service->state().period(kind);
    const QString reset = period ? core::format::resetSentence(kind, *period) : QString();

    m_notifier->notifyThreshold(kind, threshold, reset, icon);
}

} // namespace claudometer
