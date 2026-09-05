#include "Application.h"

#include "AppIcon.h"
#include "Brand.h"
#include "core/Config.h"
#include "core/Credentials.h"
#include "core/Format.h"
#include "core/PanelTheme.h"
#include "core/UsageJson.h"
#include "core/UsageLevel.h"
#include "core/UsageService.h"
#include "tray/IconRenderer.h"
#include "tray/Notifier.h"
#include "tray/SleepWatcher.h"
#include "tray/SystemTrayBackend.h"
#include "ui/Colors.h"
#include "ui/PopupWindow.h"
#include "ui/SettingsViewModel.h"
#include "ui/UsageViewModel.h"

#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPalette>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTextStream>
#include <QTimer>

namespace claudedial {

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

    // The static identity mark - not the tray indicator, which is a different
    // job with different constraints. Without this the compositor shows a
    // placeholder for our app id until the .desktop entry and hicolor icon are
    // installed, which looks like a foreign application in the title bar.
    QGuiApplication::setWindowIcon(applicationIcon());

    m_tray = new tray::SystemTrayBackend(this);

    m_engine = new QQmlEngine(this);
    m_usage = new ui::UsageViewModel(m_service, m_config, this);
    m_settings = new ui::SettingsViewModel(m_config, this);

    m_engine->rootContext()->setContextProperty(QStringLiteral("usage"), m_usage);
    m_engine->rootContext()->setContextProperty(QStringLiteral("settings"), m_settings);
    m_colors = new ui::Colors(this);
    m_engine->rootContext()->setContextProperty(QStringLiteral("colors"), m_colors);

    m_popup = new ui::PopupWindow(m_engine, QUrl(QStringLiteral("qrc:/qml/Popup.qml")));
    // Exposed only so the header can act as a drag handle. Nothing sensitive
    // reaches QML through it - the window knows nothing about credentials.
    m_engine->rootContext()->setContextProperty(QStringLiteral("popupWindow"), m_popup);

    // --- tray -----------------------------------------------------------------
    connect(m_tray, &tray::TrayBackend::activated, this, [this] {
        m_popup->toggle(m_tray->iconGeometry());
    });
    connect(m_tray, &tray::TrayBackend::showRequested, this, &Application::showPopup);
    connect(m_tray, &tray::TrayBackend::refreshRequested, m_service, &UsageService::refreshNow);
    connect(m_tray, &tray::TrayBackend::settingsRequested, this, &Application::showSettings);
    connect(m_tray, &tray::TrayBackend::quitRequested, qApp, &QCoreApplication::quit);

    // Only ever emitted where there is no notification bus; connected
    // unconditionally because a signal nobody emits costs nothing, and one
    // #ifdef fewer is worth more than the connection.
    connect(m_notifier, &tray::Notifier::messageRequested, this,
            [this](const QString& title, const QString& body, bool critical, const QImage& icon) {
                if (m_tray)
                    m_tray->showMessage(title, body, icon, critical);
            });

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

    // The tooltip contains "resets in 1h 52m", which decays on its own.
    m_tooltipTick->setInterval(60 * 1000);
    connect(m_tooltipTick, &QTimer::timeout, this, &Application::updateTray);
    m_tooltipTick->start();

    qApp->installEventFilter(this);

    applyTheme();
    m_tray->show();
    m_service->start();

    verifyTrayVisible(0);

    return true;
}

void Application::verifyTrayVisible(int attempt)
{
    // A panel can register its host after we start, so give it time and one
    // second chance before saying anything.
    constexpr int kDelaysMs[] = {3000, 9000};
    constexpr int kAttempts = 2;

    if (attempt >= kAttempts) {
        QTextStream err(stderr);
        err << "claudedial: this desktop reports a system tray, but our icon did "
               "not appear in one.\n"
               "That happens when Qt's tray implementation and the panel "
               "disagree - typically a\n"
               "D-Bus platform theme with no StatusNotifierHost on the session "
               "bus.\n"
               "ClaudeDial keeps running, and `claudedial --json` still reports "
               "usage from this\n"
               "instance without spending an API request of its own.\n";
        return;
    }

    QTimer::singleShot(kDelaysMs[attempt], this, [this, attempt] {
        if (m_tray && !m_tray->hasVisibleIcon())
            verifyTrayVisible(attempt + 1);
    });
}

void Application::showPopup()
{
    if (m_popup)
        m_popup->present(m_tray ? m_tray->iconGeometry() : QRect());
}

QByteArray Application::statusJson() const
{
    return core::json::status(m_service->state(), m_config->warningThreshold(),
                              m_config->criticalThreshold());
}

bool Application::eventFilter(QObject* watched, QEvent* event)
{
    // Guarded against our own setColorScheme(), which changes the palette and
    // would otherwise bring us straight back here.
    if (event->type() == QEvent::ApplicationPaletteChange && !m_applyingTheme)
        applyTheme();
    return QObject::eventFilter(watched, event);
}

void Application::updateTray()
{
    if (!m_tray)
        return;

    const auto& state = m_service->state();

    IconRenderer::Options options;
    options.style = m_config->trayStyle();
    options.foreground = trayNeutral();
    options.warningThreshold = m_config->warningThreshold();
    options.criticalThreshold = m_config->criticalThreshold();
    options.stale = state.stale;

    const std::optional<double> percentage = state.fiveHour
        ? std::optional<double>(state.fiveHour->percentage)
        : std::nullopt;

    QIcon icon = IconRenderer::render(percentage, options);

    // A template image, which macOS tints itself to match the menu bar. That is
    // the only correct answer on a platform where the panel's background is the
    // wallpaper: reported from a real Mac that the neutral looked washed out on
    // Auto while the explicit tones were legible, and no fixed grey can fix
    // that, because the background changes when the wallpaper does.
    //
    // Only for the neutral state, and only on Auto. A warning must keep its
    // colour - amber and red are the reading, not decoration - and an explicit
    // tone is the user having overridden us. setIsMask does nothing off macOS.
    const bool neutral = !percentage
        || core::levelFor(*percentage, options.warningThreshold, options.criticalThreshold)
            == core::UsageLevel::Normal;
    if (neutral && m_config->trayTone() == Config::TrayTone::Auto)
        icon.setIsMask(true);

    m_tray->setIcon(icon);

    const QString tooltip = state.isValid() ? core::format::tooltip(state)
                                            : QStringLiteral("ClaudeDial\n%1").arg(
                                                  m_service->unavailableReason());
    m_tray->setToolTip(tooltip);

    // The same two readings again, for a desktop whose tray has no tooltips.
    m_tray->setSummary(core::format::menuEntry(PeriodKind::FiveHour, state),
                       core::format::menuEntry(PeriodKind::SevenDay, state));
}

namespace {

/// The panel background Plasma declares, or nullopt off Plasma and when Plasma
/// says nothing.
///
/// Three small file reads, repeated whenever the icon is redrawn - a minute
/// apart at worst. Caching would mean watching both plasmarc and kdeglobals to
/// stay correct through a look-and-feel change, which is more machinery than
/// the reads cost.
/// What could be learned about the panel.
///
/// `plasma` without a `background` is not ignorance: it is Plasma saying the
/// panel follows the application colour scheme, which the stock "default" theme
/// does by shipping no colours at all. That is the one case where QPalette is
/// the right answer, and it has to be told apart from there being no Plasma to
/// ask.
struct PanelKnowledge {
    bool plasma = false;
    std::optional<core::Rgb> background;
};

PanelKnowledge panelKnowledge()
{
    const auto read = [](const QString& path) -> QString {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(file.readAll());
    };
    const QString config =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);

    QString lookAndFeel;
    const QString package =
        core::lookAndFeelPackage(read(config + QStringLiteral("/kdeglobals")));
    if (!package.isEmpty()) {
        const QString relative =
            QStringLiteral("plasma/look-and-feel/%1/contents/defaults").arg(package);
        for (const QString& path :
             QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, relative)) {
            lookAndFeel = read(path);
            if (!lookAndFeel.isEmpty())
                break;
        }
    }

    const QString theme =
        core::plasmaThemeName(read(config + QStringLiteral("/plasmarc")), lookAndFeel);
    if (theme.isEmpty())
        return PanelKnowledge{}; // not Plasma, or Plasma has not written it down

    PanelKnowledge knowledge;
    knowledge.plasma = true;

    const QString relative = QStringLiteral("plasma/desktoptheme/%1/colors").arg(theme);
    for (const QString& path :
         QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, relative)) {
        if (const auto background = core::plasmaPanelBackground(read(path))) {
            knowledge.background = background;
            break;
        }
    }
    return knowledge;
}

} // namespace

QColor Application::trayNeutral() const
{
    switch (m_config->trayTone()) {
    case Config::TrayTone::Light:
        return brand::kTrayNeutralLight;
    case Config::TrayTone::Dark:
        return brand::kTrayNeutralDark;
    case Config::TrayTone::Auto:
        break;
    }

    const PanelKnowledge panel = panelKnowledge();

    // Plasma writes the panel's colour down, and it is exact rather than a
    // guess: breeze-dark declares BackgroundNormal=32,35,38, which is the
    // #202326 measured off the panel itself. This is what makes Auto right on
    // Breeze Twilight, where the applications are light and the panel is not.
    if (panel.background)
        return panel.background->isDark() ? brand::kTrayNeutralLight : brand::kTrayNeutralDark;

    // Plasma, but with a theme that declares no colours - the stock "default"
    // theme, which follows the application colour scheme. Here the palette is
    // not a proxy for the panel, it *is* the panel.
    if (panel.plasma)
        return QGuiApplication::palette().color(QPalette::WindowText);

    // And here nothing is known. The palette is not evidence: on a desktop with
    // no Qt integration it is a built-in default, and it has now been wrong in
    // both directions - #000000 where the panel was black, and light where a
    // Debian i3bar was light. Guessing a side is what produced both reports, so
    // the fallback does not guess: kTrayNeutral is the grey that maximises the
    // worse of the two cases, visible against either at the cost of being crisp
    // against neither. Anyone who wants crisp says which, with the Tray icon
    // setting.
    return brand::kTrayNeutral;
}

void Application::applyTheme()
{
    auto* hints = QGuiApplication::styleHints();
    const Config::Theme theme = m_config->theme();

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
    m_colors->setDark(dark);
    // The desktop's accent can change with the scheme.
    m_colors->refresh();

    updateTray();
}

void Application::showSettings()
{
    if (!m_settingsWindow) {
        m_settingsWindow = new QQuickView(m_engine, nullptr);
        m_settingsWindow->setTitle(tr("ClaudeDial Settings"));
        // A dialog, which is what it is - and it is also the flag a tiling
        // window manager reads. Measured under xcb: without it the window
        // declares only _NET_WM_WINDOW_TYPE_NORMAL, so i3 tiles the settings
        // form into a workspace column; with it the type is
        // _NET_WM_WINDOW_TYPE_DIALOG, which i3 floats by default. The popup
        // already declares UTILITY first for the same reason.
        m_settingsWindow->setFlag(Qt::Dialog);
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
    options.foreground = trayNeutral();
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

} // namespace claudedial
