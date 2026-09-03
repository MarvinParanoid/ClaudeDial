#include "SettingsViewModel.h"

#include "core/Config.h"

#include <algorithm>

namespace claudometer::ui {

using core::Config;

SettingsViewModel::SettingsViewModel(Config* config, QObject* parent)
    : QObject(parent)
    , m_config(config)
{
    connect(m_config, &Config::changed, this, &SettingsViewModel::changed);
}

bool SettingsViewModel::startOnLogin() const
{
    return m_config->startOnLogin();
}

void SettingsViewModel::setStartOnLogin(bool enabled)
{
    if (enabled != m_config->startOnLogin())
        m_config->setStartOnLogin(enabled);
}

int SettingsViewModel::refreshMinutes() const
{
    return std::max(1, m_config->refreshIntervalSeconds() / 60);
}

void SettingsViewModel::setRefreshMinutes(int minutes)
{
    m_config->setRefreshIntervalSeconds(std::max(1, minutes) * 60);
}

bool SettingsViewModel::notificationsEnabled() const
{
    return m_config->notificationsEnabled();
}

void SettingsViewModel::setNotificationsEnabled(bool enabled)
{
    if (enabled != m_config->notificationsEnabled())
        m_config->setNotificationsEnabled(enabled);
}

int SettingsViewModel::warningThreshold() const
{
    return m_config->warningThreshold();
}

void SettingsViewModel::setWarningThreshold(int percent)
{
    if (percent != m_config->warningThreshold())
        m_config->setWarningThreshold(percent);
}

int SettingsViewModel::criticalThreshold() const
{
    return m_config->criticalThreshold();
}

void SettingsViewModel::setCriticalThreshold(int percent)
{
    if (percent != m_config->criticalThreshold())
        m_config->setCriticalThreshold(percent);
}

int SettingsViewModel::trayStyleIndex() const
{
    return m_config->trayStyle() == Config::TrayStyle::Gauge ? 0 : 1;
}

void SettingsViewModel::setTrayStyleIndex(int index)
{
    const auto style = index == 0 ? Config::TrayStyle::Gauge : Config::TrayStyle::Percentage;
    if (style != m_config->trayStyle())
        m_config->setTrayStyle(style);
}

int SettingsViewModel::themeIndex() const
{
    switch (m_config->theme()) {
    case Config::Theme::System:
        return 0;
    case Config::Theme::Light:
        return 1;
    case Config::Theme::Dark:
        return 2;
    }
    return 0;
}

void SettingsViewModel::setThemeIndex(int index)
{
    const auto theme = index == 1 ? Config::Theme::Light
        : index == 2              ? Config::Theme::Dark
                                  : Config::Theme::System;
    if (theme != m_config->theme())
        m_config->setTheme(theme);
}

} // namespace claudometer::ui
