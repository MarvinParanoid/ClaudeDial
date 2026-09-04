#include "Config.h"

#include <QCoreApplication>
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

#ifdef Q_OS_WIN
/// Where Windows keeps per-user startup entries. QSettings reaches the registry
/// natively, so this needs no platform API of its own.
constexpr auto kRunKey = R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)";
constexpr auto kRunValue = "ClaudeDial";
#elif defined(Q_OS_MACOS)
/// A LaunchAgent, which is how macOS starts something at login.
///
/// Not the XDG autostart entry the branch below writes: on macOS that would
/// land in ~/Library/Preferences/autostart, where nothing reads it, and the
/// toggle would appear to work while doing nothing at all.
QString autostartFilePath()
{
    return QDir::homePath()
        + QStringLiteral("/Library/LaunchAgents/io.github.marvinparanoid.claudedial.plist");
}
#else
QString autostartFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/claudedial.desktop");
}
#endif

} // namespace

Config::Config(QObject* parent)
    : Config(QStringLiteral("claudedial"), QStringLiteral("claudedial"), parent)
{
}

Config::Config(const QString& organisation, const QString& application, QObject* parent)
    : QObject(parent)
    // NativeFormat rather than IniFormat. On Linux it is the same INI text file,
    // but at the idiomatic ~/.config/claudedial/claudedial.conf instead of a
    // .ini; on Windows it is the registry and on macOS a plist, which is the
    // right answer on each and the reason the documented path is asserted per
    // platform rather than assumed.
    , m_settings(new QSettings(QSettings::NativeFormat, QSettings::UserScope,
                               organisation, application, this))
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
#ifdef Q_OS_WIN
    return QSettings(QLatin1String(kRunKey), QSettings::NativeFormat)
        .contains(QLatin1String(kRunValue));
#else
    return QFile::exists(autostartFilePath());
#endif
}

void Config::setStartOnLogin(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings run(QLatin1String(kRunKey), QSettings::NativeFormat);
    if (enabled) {
        // Quoted: the path contains spaces on any normal install.
        run.setValue(QLatin1String(kRunValue),
                     QStringLiteral("\"%1\"").arg(
                         QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    } else {
        run.remove(QLatin1String(kRunValue));
    }
    Q_EMIT changed();
    return;
#else
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

#ifdef Q_OS_MACOS
    // Dropped into ~/Library/LaunchAgents, this takes effect at the next login
    // without launchctl. The path is the running binary's, which inside a
    // bundle is Contents/MacOS/claudedial - launching that directly is correct
    // for an LSUIElement app.
    file.write(QStringLiteral(
                   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                   "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                   "<plist version=\"1.0\">\n"
                   "<dict>\n"
                   "    <key>Label</key>\n"
                   "    <string>io.github.marvinparanoid.claudedial</string>\n"
                   "    <key>ProgramArguments</key>\n"
                   "    <array>\n"
                   "        <string>%1</string>\n"
                   "    </array>\n"
                   "    <key>RunAtLoad</key>\n"
                   "    <true/>\n"
                   "</dict>\n"
                   "</plist>\n")
                   .arg(QCoreApplication::applicationFilePath())
                   .toUtf8());
#else
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
#endif
    file.close();
    Q_EMIT changed();
#endif
}

} // namespace claudedial::core
