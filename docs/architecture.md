# Architecture

One binary, three build targets, fifteen classes. The shape exists to keep one
promise: **the token lives in one class, and the UI layer cannot reach it.**

```
claudometer_core        Qt6::Core + Qt6::Network only. No QML, no widgets, no D-Bus.
  Credentials             finds and validates the token; authorizes a request
  UsageClient             the one HTTP GET; lenient parsing
  UsageState              UsagePeriod{ double, optional<QDateTime> } x2
  UsageService            polling, backoff, staleness, notification thresholds
  Config                  QSettings
  UsageLevel              the one threshold ramp: normal/warning/critical/severe/limit
  Format                  human strings (tooltip, "Resets in 1h 52m")
  UsageJson               --json output

claudometer             the assembly
  Application             wires it together; owns nothing interesting
  SingleInstance          one tray icon; a second launch shows the popup
  GaugePainter            the speedometer mark - one drawing, two hosts
  AppIcon                 the static identity mark, resolved from the icon theme
  cli/Cli                 --json / --once under QCoreApplication
  tray/TrayBackend        interface (the project's only abstraction)
  tray/SystemTrayBackend  QSystemTrayIcon -> StatusNotifierItem over D-Bus
  tray/IconRenderer       QPainter ring gauge, six sizes
  tray/Notifier           org.freedesktop.Notifications
  tray/SleepWatcher       logind PrepareForSleep
  ui/UsageViewModel       display values, and only display values
  ui/SettingsViewModel    pass-through to Config
  ui/PopupWindow          frameless QQuickView; header drag via startSystemMove
  ui/GaugeItem            QQuickPaintedItem wrapping GaugePainter for QML
  ui/qml/*.qml            Popup, Settings, and two small components

claudometer_tests       core only, headless
```

## Data flow

```
Credentials ──authorize()──> UsageClient ──UsageState──> UsageService
                                                              │
                                              ┌───────────────┼───────────────┐
                                              ▼               ▼               ▼
                                         tray icon      UsageViewModel     --json
                                         + tooltip        → QML popup
```

State flows one way. Nothing below `UsageService` writes to it.

## Decisions worth knowing about

**Core links only Core and Network.** That is what makes the tests headless -
no Xvfb in CI, no display in a container. It is also the boundary that keeps
credential handling auditable: everything that touches the token is in a library
that physically cannot construct a QML engine.

**`Credentials` has no `Q_INVOKABLE` and no `Q_PROPERTY`.** The view models
expose formatted numbers and level names. There is no code path from QML to the
token, so there is nothing to accidentally print in the QML debugger.

**`UsageClient` never refreshes.** Anthropic rotates the refresh token on use;
refreshing here would race Claude Code and could sign the user out of it.
`Credentials` watches the file instead and `UsageService` retries when it
changes. This is a deliberate capability we decline. See
[usage-api.md](usage-api.md) §7.

**Failures mark, never clear.** `UsageService::onFailed` sets `stale` and keeps
the previous `UsageState`. There is no code path that replaces good data with
zeros, which is the failure mode that makes a usage indicator actively harmful.

**Notification thresholds reset on `resets_at` moving forward**, not on the
percentage dropping. Both windows are *rolling*, so their percentages fall
mid-window as old usage ages out; watching the number would refire warnings.

**The application icon is not the tray icon.** They were briefly the same
generated `QIcon`, and it went badly: the window decoration renders a window
icon through a different path than the panel renders a StatusNotifierItem, and
tray-weight strokes came out looking hollow in the title bar - two thin arcs
instead of a dial. They are also different jobs. The tray mark is regenerated
per poll, depends on usage, and is tuned stroke by stroke for 16-24 px against
unknown panel colours; the application icon is static, bolder, carries no
reading, and has to hold up in a launcher at 256 px. It is now one hand-drawn
SVG, installed into `hicolor` for the desktop to find by name and embedded in
the binary so a build tree looks right too - `Qt6::Svg` rasterises it, and
`AppIcon` prefers the installed copy because on Wayland that is how the
compositor resolves a window's icon at all.

**One drawing of the gauge, not two.** The popup header's mark and the tray icon
are the same `GaugePainter::paint` call, reached from a `QQuickPaintedItem` and
from `QPainter` respectively. The header was a QML `Canvas` reimplementation
first; that was a second copy of the drawing and would have drifted the first
time the mark changed. What the two hosts supply separately is colour and stroke
weight: the tray must follow the panel's palette and use a shorter ramp, and its
panel-tuned weight reads as heavy on the popup's calm card. The geometry - the
one thing that makes it the same mark - is not adjustable.

**The tray icon was designed in the panel, not in a mock-up.** Proportions in
`GaugePainter` were settled by registering four candidate icons as real
StatusNotifierItems and screenshotting them at 22 px beside Chrome and Teams. A
magnified mock-up is actively misleading at this size: the first version read as
a speedometer when enlarged and as a grey loading spinner in the panel.

**`TrayBackend` is the only abstraction.** One interface, one implementation,
because StatusNotifierItem may need an AppIndicator fallback later. Everything
else is concrete on purpose.

**`QCoreApplication` for the CLI, `QApplication` for the tray.** `--json` has to
work over SSH and from a status-bar startup script before a compositor exists,
so `main` dispatches on argv before any application object is constructed.

## Where Qt already did the work

Checked before writing anything, per the project's own rules:

| Need | Qt provides |
| --- | --- |
| StatusNotifierItem over D-Bus | `QSystemTrayIcon` (routes via `QDBusTrayIcon` when a watcher is present) |
| Tray context menu on Wayland | `QMenu` on the tray icon, exported as DBusMenu |
| XDG config file | `QSettings(NativeFormat, UserScope, …)` |
| Autostart directory | `QStandardPaths::GenericConfigLocation` |
| Light/dark/system theme | `QStyleHints::setColorScheme()` (Qt 6.8+) - moves QML too |
| Custom-painted QML item | `QQuickPaintedItem` |
| Credential-file watching | `QFileSystemWatcher` |
| Single instance | `QLocalServer` / `QLocalSocket` |
| ISO 8601 with fractional seconds | `QDateTime::fromString(…, Qt::ISODateWithMs)` |
| HTTP with timeout | `QNetworkAccessManager::setTransferTimeout` |

Nothing was hand-rolled that Qt already had. The only things written from
scratch are the icon drawing, the D-Bus notification call (three lines more than
shelling out to `notify-send`, and it buys `replaces_id`), and the logind sleep
signal.

## Simulation

`CLAUDOMETER_SIMULATE=<5h>[,<7d>]` makes `UsageClient::fetch()` emit a synthetic
state and return without touching the network. It exists because the 95% and
100% steps of the ramp are fixed, so their colours, the `!` glyph and their
notifications are otherwise unreachable without actually spending a limit - and
because screenshots need stable numbers.

`UsageService` checks `UsageClient::isSimulating()` before persisting which
thresholds it has announced: a simulated run at 100% must not record that step as
already announced and silence the real notification later. The CLI checks it too,
so an explicit override beats the shared-socket shortcut rather than being
silently ignored while a tray instance is running.

## What is deliberately absent

No plugin system, no database, no telemetry, no crash reporting, no update
check, no accounts, no embedded web view, no dependency injection framework, no
event bus. One outbound host, pinned at compile time.
