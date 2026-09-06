#include "Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace claudedial::core::autostart {
namespace {

// Every entry below launches with --wait, and that is the only reason the flag
// exists. A panel registers its tray host some way into the login and an
// autostart entry can easily run first; without this the application finds no
// tray, exits, and leaves the user with no icon for the session and nothing on
// screen saying why. Spelled out in each entry rather than shared, because the
// three formats quote it differently and a constant would not survive any of
// them intact.

/// The path the running binary actually has.
///
/// Not the bare name `claudedial`: an autostart entry saying that only works if
/// the binary is on the *session's* PATH, and it frequently is not - a build
/// run from its own tree never is, and a session's PATH is not the shell's. The
/// entry then points at nothing and the session silently starts nothing, which
/// is exactly what happened. Windows and macOS have always recorded the real
/// path; this is the branch that guessed.
QString launchPath()
{
    // Inside an AppImage, applicationFilePath() is a mount point that stops
    // existing the moment the process does. APPIMAGE is the file the user
    // actually launched, and the only one still there at the next login.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty())
        return appImage;
    return QCoreApplication::applicationFilePath();
}

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
                          "        <string>--wait</string>\n"
                          "    </array>\n"
                          "    <key>RunAtLoad</key>\n"
                          "    <true/>\n"
                          "</dict>\n"
                          "</plist>\n")
        .arg(launchPath())
        .toUtf8();
#else
    // Quoted and escaped as the Desktop Entry specification asks, so a path with
    // a space in it is one argument rather than two.
    QString executable = launchPath();
    executable.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    for (const QChar special : { QLatin1Char('"'), QLatin1Char('`'), QLatin1Char('$') })
        executable.replace(special, QLatin1Char('\\') + special);

    return QStringLiteral("[Desktop Entry]\n"
                          "Type=Application\n"
                          "Name=ClaudeDial\n"
                          "Comment=Claude Code usage at a glance\n"
                          "Exec=\"%1\" --wait\n"
                          "Icon=claudedial\n"
                          "Terminal=false\n"
                          "Categories=Utility;\n"
                          "X-GNOME-Autostart-enabled=true\n")
        .arg(executable)
        .toUtf8();
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
                     QStringLiteral("\"%1\" --wait")
                         .arg(QDir::toNativeSeparators(launchPath())));
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
