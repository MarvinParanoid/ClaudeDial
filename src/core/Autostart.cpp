#include "Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace claudedial::core::autostart {
namespace {

#ifdef Q_OS_WIN
/// Where Windows keeps per-user startup entries. QSettings reaches the registry
/// natively, so this needs no platform API of its own.
constexpr auto kRunKey = R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)";
constexpr auto kRunValue = "ClaudeDial";
#else

QString entryPath()
{
#ifdef Q_OS_MACOS
    // A LaunchAgent, which is how macOS starts something at login.
    //
    // Not the XDG autostart entry the branch below writes: on macOS that would
    // land in ~/Library/Preferences/autostart, where nothing reads it, and the
    // toggle would appear to work while doing nothing at all.
    return QDir::homePath()
        + QStringLiteral("/Library/LaunchAgents/io.github.marvinparanoid.claudedial.plist");
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/claudedial.desktop");
#endif
}

QByteArray entryContents()
{
#ifdef Q_OS_MACOS
    // Dropped into ~/Library/LaunchAgents, this takes effect at the next login
    // without launchctl. The path is the running binary's, which inside a
    // bundle is Contents/MacOS/claudedial - launching that directly is correct
    // for an LSUIElement app.
    return QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
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
        .toUtf8();
#else
    return QByteArrayLiteral("[Desktop Entry]\n"
                             "Type=Application\n"
                             "Name=ClaudeDial\n"
                             "Comment=Claude Code usage at a glance\n"
                             "Exec=claudedial\n"
                             "Icon=claudedial\n"
                             "Terminal=false\n"
                             "Categories=Utility;\n"
                             "X-GNOME-Autostart-enabled=true\n");
#endif
}

#endif // !Q_OS_WIN

} // namespace

bool isEnabled()
{
#ifdef Q_OS_WIN
    return QSettings(QLatin1String(kRunKey), QSettings::NativeFormat)
        .contains(QLatin1String(kRunValue));
#else
    return QFile::exists(entryPath());
#endif
}

bool setEnabled(bool enabled)
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
    run.sync();
    return run.status() == QSettings::NoError;
#else
    const QString path = entryPath();

    // Removing something that was never there is a success, not a failure.
    if (!enabled)
        return !QFile::exists(path) || QFile::remove(path);

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    const QByteArray contents = entryContents();
    if (file.write(contents) != contents.size())
        return false;
    file.close();
    return file.error() == QFileDevice::NoError;
#endif
}

QString location()
{
#ifdef Q_OS_WIN
    return QStringLiteral("%1\\%2").arg(QLatin1String(kRunKey), QLatin1String(kRunValue));
#else
    return entryPath();
#endif
}

} // namespace claudedial::core::autostart
