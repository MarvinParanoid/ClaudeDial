#include "Config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace claudometer::core {
namespace {

constexpr auto kRefreshInterval = "refreshIntervalSeconds";
constexpr auto kNotifications = "notificationsEnabled";
constexpr auto kWarning = "warningThreshold";
constexpr auto kCritical = "criticalThreshold";
constexpr auto kShowPercentage = "showPercentageInTray";
constexpr auto kTheme = "theme";

/// Grouped separately from the settings proper - see Config.h.
QString firedKey(const QString& windowKey)
{
    return QStringLiteral("state/fired_%1").arg(windowKey);
}

QString firedResetKey(const QString& windowKey)
{
    return QStringLiteral("state/fired_%1_reset").arg(windowKey);
}

QString autostartFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/claudometer.desktop");
}

} // namespace

Config::Config(QObject* parent)
    : QObject(parent)
    // NativeFormat rather than IniFormat: on Linux it is the same INI text file,
    // but at the idiomatic ~/.config/claudometer/claudometer.conf instead of a
    // .ini. Nothing binary or registry-like is involved.
    , m_settings(new QSettings(QSettings::NativeFormat, QSettings::UserScope,
                               QStringLiteral("claudometer"), QStringLiteral("claudometer"), this))
{
}

QList<int> Config::firedThresholds(const QString& windowKey) const
{
    QList<int> thresholds;
    const QStringList raw = m_settings->value(firedKey(windowKey)).toStringList();
    thresholds.reserve(raw.size());
    for (const QString& entry : raw) {
        bool ok = false;
        const int value = entry.toInt(&ok);
        if (ok)
            thresholds.append(value);
    }
    return thresholds;
}

QDateTime Config::firedWindowReset(const QString& windowKey) const
{
    const QString raw = m_settings->value(firedResetKey(windowKey)).toString();
    if (raw.isEmpty())
        return {};
    return QDateTime::fromString(raw, Qt::ISODate);
}

void Config::setFiredThresholds(const QString& windowKey, const QList<int>& thresholds,
                                const QDateTime& windowReset)
{
    QStringList raw;
    raw.reserve(thresholds.size());
    for (const int value : thresholds)
        raw.append(QString::number(value));

    m_settings->setValue(firedKey(windowKey), raw);
    m_settings->setValue(firedResetKey(windowKey),
                         windowReset.isValid() ? windowReset.toUTC().toString(Qt::ISODate)
                                               : QString());
    // Deliberately no changed() signal: this is bookkeeping, and anything
    // listening for a settings change has no interest in it.
}

QString Config::filePath() const
{
    return m_settings->fileName();
}

int Config::refreshIntervalSeconds() const
{
    const int value = m_settings->value(QLatin1String(kRefreshInterval), kDefaultRefreshSeconds).toInt();
    return std::max(value, kMinimumRefreshSeconds);
}

void Config::setRefreshIntervalSeconds(int seconds)
{
    m_settings->setValue(QLatin1String(kRefreshInterval), std::max(seconds, kMinimumRefreshSeconds));
    Q_EMIT changed();
}

bool Config::notificationsEnabled() const
{
    return m_settings->value(QLatin1String(kNotifications), true).toBool();
}

void Config::setNotificationsEnabled(bool enabled)
{
    m_settings->setValue(QLatin1String(kNotifications), enabled);
    Q_EMIT changed();
}

int Config::warningThreshold() const
{
    return std::clamp(m_settings->value(QLatin1String(kWarning), 75).toInt(), 1, 100);
}

void Config::setWarningThreshold(int percent)
{
    m_settings->setValue(QLatin1String(kWarning), std::clamp(percent, 1, 100));
    Q_EMIT changed();
}

int Config::criticalThreshold() const
{
    return std::clamp(m_settings->value(QLatin1String(kCritical), 90).toInt(), 1, 100);
}

void Config::setCriticalThreshold(int percent)
{
    m_settings->setValue(QLatin1String(kCritical), std::clamp(percent, 1, 100));
    Q_EMIT changed();
}

bool Config::showPercentageInTray() const
{
    // On by default: the number is readable at 16 px and tells the user the
    // state with no hover and nothing to interpret. Clearing it gives the dial.
    return m_settings->value(QLatin1String(kShowPercentage), true).toBool();
}

void Config::setShowPercentageInTray(bool show)
{
    m_settings->setValue(QLatin1String(kShowPercentage), show);
    Q_EMIT changed();
}

Config::Theme Config::theme() const
{
    const QString value = m_settings->value(QLatin1String(kTheme), QStringLiteral("system")).toString();
    if (value == QLatin1String("light"))
        return Theme::Light;
    if (value == QLatin1String("dark"))
        return Theme::Dark;
    return Theme::System;
}

void Config::setTheme(Theme theme)
{
    const QString value = theme == Theme::Light ? QStringLiteral("light")
        : theme == Theme::Dark                  ? QStringLiteral("dark")
                                                : QStringLiteral("system");
    m_settings->setValue(QLatin1String(kTheme), value);
    Q_EMIT changed();
}

bool Config::startOnLogin() const
{
    return QFile::exists(autostartFilePath());
}

void Config::setStartOnLogin(bool enabled)
{
    const QString path = autostartFilePath();

    if (!enabled) {
        QFile::remove(path);
        Q_EMIT changed();
        return;
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Claudometer\n"
        "Comment=Claude Code usage at a glance\n"
        "Exec=claudometer\n"
        "Icon=claudometer\n"
        "Terminal=false\n"
        "Categories=Utility;\n"
        "X-GNOME-Autostart-enabled=true\n");
    file.close();
    Q_EMIT changed();
}

} // namespace claudometer::core
