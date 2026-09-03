#include "Application.h"
#include "SingleInstance.h"
#include "ui/GaugeItem.h"
#include "cli/Cli.h"

#include <QApplication>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QTextStream>

#include <cstring>

namespace {

void printUsage(QTextStream& out)
{
    out << "claudometer " CLAUDOMETER_VERSION " - Claude Code usage at a glance\n\n"
           "Usage:\n"
           "  claudometer            Run in the system tray\n"
           "  claudometer --once     Print current usage and exit\n"
           "  claudometer --json     Print current usage as JSON and exit\n"
           "  claudometer --help     Show this help\n"
           "  claudometer --version  Show the version\n\n"
           "The JSON output includes Waybar's text/tooltip/class keys, so a\n"
           "custom module needs no wrapper script.\n";
}

} // namespace

int main(int argc, char** argv)
{
    // Flags are handled before any application object exists, because --json has
    // to work over SSH and from a status-bar startup script - that is, with no
    // display and no compositor.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--json") == 0)
            return claudometer::cli::run(argc, argv, /*jsonOutput=*/true);
        if (std::strcmp(argv[i], "--once") == 0)
            return claudometer::cli::run(argc, argv, /*jsonOutput=*/false);
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            QTextStream out(stdout);
            printUsage(out);
            return 0;
        }
        if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            QTextStream out(stdout);
            out << "claudometer " CLAUDOMETER_VERSION "\n";
            return 0;
        }
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("claudometer"));
    QApplication::setApplicationVersion(QStringLiteral(CLAUDOMETER_VERSION));
    QApplication::setOrganizationName(QStringLiteral("claudometer"));
    // Lets the compositor and the notification daemon associate us with our
    // .desktop entry - which is what gives notifications the right name and icon.
    QApplication::setDesktopFileName(QStringLiteral("claudometer"));

    // A tray application must not exit when its popup closes.
    QApplication::setQuitOnLastWindowClosed(false);

    // Basic is the unstyled template set. Every control Claudometer uses
    // supplies its own visuals from Theme.qml, so a style that derives colours
    // from QPalette would only reintroduce the mismatch that made light-theme
    // labels invisible: the platform theme can keep a dark palette while the
    // user has asked Claudometer for Light.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // The popup header's gauge is drawn by the same function as the tray icon.
    qmlRegisterType<claudometer::ui::GaugeItem>("Claudometer", 1, 0, "Gauge");

    // A second launch should surface the running instance, not add a second
    // tray icon.
    claudometer::SingleInstance instance;
    if (!instance.acquire())
        return 0;

    claudometer::Application application;
    if (!application.initialize()) {
        QTextStream err(stderr);
        err << "claudometer: no system tray is available on this desktop.\n"
               "Try `claudometer --json` for status-bar integration instead.\n";
        return 1;
    }

    QObject::connect(&instance, &claudometer::SingleInstance::activationRequested,
                     &application, &claudometer::Application::showPopup);

    return QApplication::exec();
}
