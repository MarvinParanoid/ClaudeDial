#pragma once

#include <QObject>

namespace claudometer::core {
class Config;
}

namespace claudometer::ui {

/// Settings, as QML sees them. A thin pass-through to core::Config so QML never
/// touches QSettings directly.
class SettingsViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool startOnLogin READ startOnLogin WRITE setStartOnLogin NOTIFY changed)
    Q_PROPERTY(int refreshMinutes READ refreshMinutes WRITE setRefreshMinutes NOTIFY changed)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY changed)
    Q_PROPERTY(int warningThreshold READ warningThreshold WRITE setWarningThreshold NOTIFY changed)
    Q_PROPERTY(int criticalThreshold READ criticalThreshold WRITE setCriticalThreshold NOTIFY changed)
    Q_PROPERTY(bool showPercentageInTray READ showPercentageInTray WRITE setShowPercentageInTray NOTIFY changed)
    Q_PROPERTY(int themeIndex READ themeIndex WRITE setThemeIndex NOTIFY changed)

public:
    explicit SettingsViewModel(core::Config* config, QObject* parent = nullptr);

    [[nodiscard]] bool startOnLogin() const;
    void setStartOnLogin(bool enabled);

    /// Minutes rather than seconds, because that is the only unit the UI offers.
    [[nodiscard]] int refreshMinutes() const;
    void setRefreshMinutes(int minutes);

    [[nodiscard]] bool notificationsEnabled() const;
    void setNotificationsEnabled(bool enabled);

    [[nodiscard]] int warningThreshold() const;
    void setWarningThreshold(int percent);

    [[nodiscard]] int criticalThreshold() const;
    void setCriticalThreshold(int percent);

    [[nodiscard]] bool showPercentageInTray() const;
    void setShowPercentageInTray(bool show);

    /// 0 = System, 1 = Light, 2 = Dark - matching the order of the UI selector.
    [[nodiscard]] int themeIndex() const;
    void setThemeIndex(int index);

Q_SIGNALS:
    void changed();

private:
    core::Config* m_config;
};

} // namespace claudometer::ui
