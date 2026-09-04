#include "Application.h"
#include "SingleInstance.h"
#include "ui/GaugeItem.h"
#include "cli/Cli.h"
#include "core/UsageClient.h"

#include <QApplication>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QTextStream>

#include <cstring>

#ifdef Q_OS_WIN
// After the Qt headers, and with the two macros Windows insists on, or its
// min/max and its ANSI/Unicode aliases collide with everything.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#endif

namespace {

#ifdef Q_OS_WIN
/// Qt links this as a GUI binary, which on Windows means no console at all - so
/// `claudedial --json` run from a terminal would print into nothing. Borrow the
/// console that launched us, if there is one. A double-click has none, and then
/// this does nothing, which is right.
void borrowParentConsole()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
}
#endif

void printUsage(QTextStream& out)
{
    out << "claudedial " CLAUDEDIAL_VERSION " - Claude Code usage at a glance\n\n"
           "Usage:\n"
           "  claudedial            Run in the system tray\n"
           "  claudedial --once     Print current usage and exit\n"
           "  claudedial --json     Print current usage as JSON and exit\n"
           "  claudedial --demo     Run on invented numbers, with no credentials\n"
           "  claudedial --help     Show this help\n"
           "  claudedial --version  Show the version\n\n"
           "The JSON output includes Waybar's text/tooltip/class keys, so a\n"
           "custom module needs no wrapper script.\n\n"
           "--demo makes no network request and reads no credentials, which is\n"
           "how to check packaging and appearance on a desktop that has never\n"
           "run Claude Code. It takes optional percentages, either way round:\n"
           "  claudedial --demo 96,41\n"
           "  claudedial --demo=96,41\n";
}

} // namespace

int main(int argc, char** argv)
{
#ifdef Q_OS_WIN
    // Any flag at all means somebody typed this in a terminal and expects to
    // read the answer there.
    if (argc > 1)
        borrowParentConsole();
#endif

    // Demo mode is resolved first, so that it composes with every other flag:
    // `--demo --json` has to print invented numbers too. It reuses the same
    // environment variable the client already honours rather than threading a
    // second switch through the core.
    int demoValueIndex = -1; // consumed, so it is not reported as a stray later
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--demo", 6) != 0)
            continue;
        const char tail = argv[i][6];
        if (tail == '=') {
            qputenv("CLAUDEDIAL_SIMULATE", argv[i] + 7);
        } else if (tail == '\0') {
            // Accept `--demo 96,41` as well as `--demo=96,41`. Only the equals
            // form used to work, and the other silently ran the default 62 while
            // dropping the number the user typed - which looks like the demo
            // percentages being ignored rather than the argument being.
            const bool hasValue = i + 1 < argc && argv[i + 1][0] != '-';
            if (hasValue) {
                qputenv("CLAUDEDIAL_SIMULATE", argv[i + 1]);
                demoValueIndex = i + 1;
            } else {
                qputenv("CLAUDEDIAL_SIMULATE", "62,41");
            }
        } else {
            continue;
        }

        // Refuse an unparseable --demo=... rather than silently falling through
        // to a real request: someone asking for demo numbers has no intention
        // of reaching the network, and may have no credentials at all.
        if (!claudedial::core::UsageClient::isSimulating()) {
            QTextStream err(stderr);
            err << "claudedial: cannot read demo percentages from '"
                << (demoValueIndex > 0 ? argv[demoValueIndex] : argv[i])
                << "'.\nExpected one or two numbers, five-hour first: "
                   "--demo 96 or --demo=96,41\n";
            return 2;
        }
    }

    // ClaudeDial takes no positional arguments, so one is always a mistake -
    // and until this said so, `--demo 10,10` ran happily at 62% with the
    // number silently discarded. Qt's own switches all begin with a dash, so
    // this cannot swallow one of those.
    for (int i = 1; i < argc; ++i) {
        if (i == demoValueIndex || argv[i][0] == '-')
            continue;
        QTextStream err(stderr);
        err << "claudedial: ignoring unexpected argument '" << argv[i] << "'\n";
    }

    // Flags are handled before any application object exists, because --json has
    // to work over SSH and from a status-bar startup script - that is, with no
    // display and no compositor.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--json") == 0)
            return claudedial::cli::run(argc, argv, /*jsonOutput=*/true);
        if (std::strcmp(argv[i], "--once") == 0)
            return claudedial::cli::run(argc, argv, /*jsonOutput=*/false);
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            QTextStream out(stdout);
            printUsage(out);
            return 0;
        }
        if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            QTextStream out(stdout);
            out << "claudedial " CLAUDEDIAL_VERSION "\n";
            return 0;
        }
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("claudedial"));
    QApplication::setApplicationVersion(QStringLiteral(CLAUDEDIAL_VERSION));
    QApplication::setOrganizationName(QStringLiteral("claudedial"));
    // Lets the compositor and the notification daemon associate us with our
    // .desktop entry - which is what gives notifications the right name and icon.
    QApplication::setDesktopFileName(QStringLiteral("claudedial"));

    // A tray application must not exit when its popup closes.
    QApplication::setQuitOnLastWindowClosed(false);

    // Basic is the unstyled template set. Every control ClaudeDial uses
    // supplies its own visuals from Theme.qml, so a style that derives colours
    // from QPalette would only reintroduce the mismatch that made light-theme
    // labels invisible: the platform theme can keep a dark palette while the
    // user has asked ClaudeDial for Light.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // The popup header's gauge is drawn by the same function as the tray icon.
    qmlRegisterType<claudedial::ui::GaugeItem>("ClaudeDial", 1, 0, "Gauge");

    // A second launch should surface the running instance, not add a second
    // tray icon.
    claudedial::SingleInstance instance;
    if (!instance.acquire())
        return 0;

    claudedial::Application application;
    if (!application.initialize()) {
        QTextStream err(stderr);
        err << "claudedial: no system tray is available on this desktop.\n"
               "Try `claudedial --json` for status-bar integration instead.\n";
        return 1;
    }

    QObject::connect(&instance, &claudedial::SingleInstance::activationRequested,
                     &application, &claudedial::Application::showPopup);

    // Lets `claudedial --json` from a status bar read this instance's last
    // result instead of spending an API request of its own.
    instance.setStatusProvider([&application] { return application.statusJson(); });

    return QApplication::exec();
}
