#pragma once

#include <QObject>

namespace claudedial::core {
class Config;
}

namespace claudedial::ui {

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
    Q_PROPERTY(int trayStyleIndex READ trayStyleIndex WRITE setTrayStyleIndex NOTIFY changed)
    Q_PROPERTY(int themeIndex READ themeIndex WRITE setThemeIndex NOTIFY changed)
    /// Read-only, and the only reason the settings window shows it: a user who
    /// installed a package has no other way to see which build is running, and
    /// that is the first thing any bug report needs. --version serves only the
    /// people already in a terminal.
    Q_PROPERTY(QString version READ version CONSTANT)

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

    /// 0 = Gauge, 1 = Percentage - matching the order of the UI selector.
    [[nodiscard]] int trayStyleIndex() const;
    void setTrayStyleIndex(int index);

    /// 0 = System, 1 = Light, 2 = Dark - matching the order of the UI selector.
    [[nodiscard]] int themeIndex() const;
    [[nodiscard]] QString version() const;
    void setThemeIndex(int index);

Q_SIGNALS:
    void changed();

private:
    core::Config* m_config;
};

} // namespace claudedial::ui
