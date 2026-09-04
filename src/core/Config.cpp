#include "Config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace claudedial::core {
namespace {

constexpr auto kRefreshInterval = "refreshIntervalSeconds";
constexpr auto kNotifications = "notificationsEnabled";
constexpr auto kWarning = "warningThreshold";
constexpr auto kCritical = "criticalThreshold";
constexpr auto kTrayStyle = "trayStyle";
constexpr auto kTrayTone = "trayTone";
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
        + QStringLiteral("/autostart/claudedial.desktop");
}

} // namespace

Config::Config(QObject* parent)
    : QObject(parent)
    // NativeFormat rather than IniFormat: on Linux it is the same INI text file,
    // but at the idiomatic ~/.config/claudedial/claudedial.conf instead of a
    // .ini. Nothing binary or registry-like is involved.
    , m_settings(new QSettings(QSettings::NativeFormat, QSettings::UserScope,
                               QStringLiteral("claudedial"), QStringLiteral("claudedial"), this))
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

Config::TrayStyle Config::trayStyle() const
{
    // Percentage by default: it is readable at 16 px and answers the question
    // with no hover and nothing to interpret. The needle is the nicer mark, and
    // it stays the logotype in the popup header regardless of this setting.
    const QString value = m_settings->value(QLatin1String(kTrayStyle),
                                            QStringLiteral("percentage")).toString();
    return value == QLatin1String("gauge") ? TrayStyle::Gauge : TrayStyle::Percentage;
}

Config::TrayTone Config::trayTone() const
{
    const QString value =
        m_settings->value(QLatin1String(kTrayTone), QStringLiteral("auto")).toString();
    if (value == QLatin1String("light"))
        return TrayTone::Light;
    if (value == QLatin1String("dark"))
        return TrayTone::Dark;
    return TrayTone::Auto;
}

void Config::setTrayTone(TrayTone tone)
{
    m_settings->setValue(QLatin1String(kTrayTone),
                         tone == TrayTone::Light ? QStringLiteral("light")
                             : tone == TrayTone::Dark ? QStringLiteral("dark")
                                                      : QStringLiteral("auto"));
    Q_EMIT changed();
}

void Config::setTrayStyle(TrayStyle style)
{
    m_settings->setValue(QLatin1String(kTrayStyle),
                         style == TrayStyle::Gauge ? QStringLiteral("gauge")
                                                   : QStringLiteral("percentage"));
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
        "Name=ClaudeDial\n"
        "Comment=Claude Code usage at a glance\n"
        "Exec=claudedial\n"
        "Icon=claudedial\n"
        "Terminal=false\n"
        "Categories=Utility;\n"
        "X-GNOME-Autostart-enabled=true\n");
    file.close();
    Q_EMIT changed();
}

} // namespace claudedial::core
