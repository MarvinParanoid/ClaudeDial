#pragma once

#include "core/UsageClient.h"
#include "core/UsageState.h"

#include <QByteArray>
#include <QColor>
#include <QObject>

class QQmlEngine;
class QQuickView;
class QTimer;

namespace claudometer::core {
class Config;
class Credentials;
class UsageService;
}

namespace claudometer::tray {
class Notifier;
class SleepWatcher;
class TrayBackend;
}

namespace claudometer::ui {
class Colors;
class PopupWindow;
class SettingsViewModel;
class UsageViewModel;
}

namespace claudometer {

/// Wires the pieces together and owns nothing else. All the behaviour lives in
/// core; this is the assembly.
class Application : public QObject
{
    Q_OBJECT

public:
    explicit Application(QObject* parent = nullptr);

    /// False when there is no system tray to live in - the caller should report
    /// that and exit rather than run invisibly.
    bool initialize();

    /// Show the popup - used when a second launch asks the running instance to
    /// surface itself.
    void showPopup();

    /// The current state as `--json` would print it, for answering another
    /// process over the local socket.
    [[nodiscard]] QByteArray statusJson() const;

private:
    void updateTray();
    void applyTheme();
    void showSettings();
    void onThresholdCrossed(core::PeriodKind kind, int threshold, double percentage);

    core::Config* m_config;
    core::Credentials* m_credentials;
    core::UsageService* m_service;

    tray::TrayBackend* m_tray = nullptr;
    tray::Notifier* m_notifier;
    tray::SleepWatcher* m_sleepWatcher;

    QQmlEngine* m_engine = nullptr;
    ui::Colors* m_colors = nullptr;
    ui::UsageViewModel* m_usage = nullptr;
    ui::SettingsViewModel* m_settings = nullptr;
    ui::PopupWindow* m_popup = nullptr;
    QQuickView* m_settingsWindow = nullptr;

    /// Keeps the tooltip's relative times honest without refetching.
    QTimer* m_tooltipTick;

    /// Guards the re-entrancy of setColorScheme -> colorSchemeChanged.
    bool m_applyingTheme = false;

    /// What the desktop's text colour is, which is the best proxy available for
    /// what the panel looks like. Held separately from the application palette
    /// because the Theme setting may override that, and the tray icon must not
    /// follow it - see applyTheme().
    QColor m_panelForeground;
};

} // namespace claudometer
