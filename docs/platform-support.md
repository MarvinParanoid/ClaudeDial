# Platform support — the contract

What ClaudeDial guarantees, what it attempts, and what it declines to do.

This is a contract rather than a wish list. Its purpose is to make the answer to
"does ClaudeDial work on X?" a matter of reading rather than of testing twenty
desktops — and to stop the codebase from slowly acquiring a `if (gnome &&
wayland && version >= …)` in every method.

The rule it encodes: **information must never exist in only one place.** Every
tier below can be missing on somebody's desktop, so nothing may depend on the
tier above it being present.

---

## Three tiers of guarantee

### Tier 1 — the core, and the CLI. Guaranteed.

```
claudedial --json     machine-readable, stable
claudedial --once     human-readable, one shot
```

Runs with no display, no compositor and no tray: `QCoreApplication` only, chosen
for exactly this reason. Works over SSH, from a status-bar startup script, and
in a container.

**These two commands are a public interface, not a convenience.** For Sway,
Hyprland, i3 and anyone else with a text status bar, they are *the* interface —
that user never sees our tray at all. The JSON shape is therefore something we
own and keep stable, and it already carries Waybar's `text`, `tooltip` and
`class` keys so no wrapper script is needed.

`--json` is also free while the tray is running: it asks the running instance
over a local socket rather than spending an API request.

Guaranteed: the numbers, the reset times, the staleness flag, the exit codes.

### Tier 2 — the tray icon and its menu. The lowest common denominator.

```
icon            5-hour usage, at a glance
context menu    Show usage · Refresh now · Settings… · Quit
```

`QSystemTrayIcon` over StatusNotifierItem, or XEmbed where that is what exists.
Qt covers Windows, macOS, KDE, GNOME, Xfce, LXQt and DDE from one call, which is
why there is no portability layer here and should not be one.

**The menu is the floor.** Where a left click opens the menu instead of
activating the icon — GNOME with the AppIndicator extension does exactly this —
the menu is the only route to anything. It must therefore be sufficient on its
own, which is why "Show usage" is its first item.

Guaranteed *where a tray exists at all*: the icon and every action in the menu.

### Tier 3 — the popup, the tooltip, placement. Best effort.

| | Why it can be missing |
| --- | --- |
| Compact popup | needs the icon's activation to reach us |
| Popup **placement** near the icon | impossible on Wayland: a client cannot position its own top-levels, and SNI hosts do not report icon geometry |
| Hover tooltip | AppIndicator supports no tooltips at all |
| Dragging the popup | needs the compositor to honour `startSystemMove()` |

None of these may be the only home for a piece of information. The popup
duplicates everything the tooltip says; the menu must eventually duplicate the
numbers for the same reason (see [Gaps](#gaps-between-this-contract-and-the-code)).

---

## Desktops

Honest columns. "Verified" means someone ran it and looked; everything else is
inference from the protocols involved, and inference has already been wrong once
in this project.

| Desktop | Tray | Popup placement | Tooltip | Verified |
| --- | --- | --- | --- | --- |
| KDE Plasma / Wayland | SNI | compositor's choice | yes | **yes** — the development desktop |
| KDE Plasma / X11 | SNI | compositor's choice | yes | **partly** — the xcb path, under XWayland |
| Xfce | SNI | compositor's choice | yes | no |
| Cinnamon, MATE, LXQt, DDE | SNI | unknown | likely | no |
| GNOME | needs the [AppIndicator extension][appind] | n/a — reached from the menu | **no** | no |
| COSMIC | SNI, menu only reported elsewhere | unknown | unknown | no |
| i3 + an XEmbed tray | XEmbed | **anchored to the icon** — *measured* | n/a | **yes** — Debian 13, i3bar |
| Sway, Hyprland, Awesome | Waybar/i3blocks via `--json` | n/a | n/a | no |
| Windows, macOS | Qt supports it; we have not tried | anchoring should work | — | no |

**The X11 question took three rounds to answer, and the first two answers were
both wrong.** Worth writing down as it happened, because the wrong turns are
where the useful mechanism is.

Round one. The claim was that `geometry()` might return a real rectangle on
X11. Under `QT_QPA_PLATFORM=xcb` on Plasma it does not: both the app and a
standalone probe get an empty rect, exactly as on Wayland. D-Bus introspection
showed why — the probe publishes `/StatusNotifierItem` and the watcher
registers it, so Qt chose the SNI backend even on xcb, and SNI has no concept of
icon geometry. Conclusion drawn: the deciding factor is not X11 versus Wayland
but whether a `StatusNotifierHost` is on the session bus.

Round two, and the mistake. From that it seemed to follow that anchoring would
work on an XEmbed-only tray, and a test said otherwise: on an isolated
`Xwayland :77` with a purpose-built spec-compliant XEmbed tray — owning
`_NET_SYSTEM_TRAY_S0`, advertising `_NET_SYSTEM_TRAY_VISUAL` and
`_NET_SYSTEM_TRAY_ORIENTATION` — and a private D-Bus session with no watcher,
Qt never sent a dock request. The harness was sound: a bare XEmbed client docked
in the same run and was reported at its true `22x22+110+204`. So the conclusion
became "Qt 6 has no XEmbed tray at all". That was wrong, and one detail
contradicted it: `libQt6XcbQpa` exports `QXcbWindow::requestSystemTrayWindowDock()`,
which exists for no other purpose.

Round three, the actual answer. **Qt chooses its tray implementation from the
platform theme, not from the platform.** Same display, same tray, only
`XDG_CURRENT_DESKTOP` varied:

| `XDG_CURRENT_DESKTOP` | docked | `geometry()` |
| --- | --- | --- |
| `KDE` | no | empty |
| unset | no | empty |
| `i3` | **yes** | **`22x22+110+204`** |

The i3 row matches the tray's ground truth exactly. So Qt 6's XEmbed tray works;
a D-Bus platform theme shadows it. Which means the anchored branch in
`PopupWindow::placeAndShow` is **live** on an X11 desktop that loads a non-D-Bus
platform theme and runs an XEmbed tray — the i3-with-`stalonetray` case — and
unreachable on Plasma, where the KDE theme takes the SNI path on both X11 and
Wayland. It is also live on Windows and macOS, where `geometry()` is
implemented.

**The same rounds turned up a real defect, now guarded.** Where a D-Bus platform
theme is active but no `StatusNotifierHost` exists, `isSystemTrayAvailable()`
returns **true** and no icon ever appears. Measured on one display: no tray plus
a private bus gave `false`; the XEmbed tray plus a private bus plus the KDE
theme gave `true` with no icon; the same tray with the i3 theme gave `true` with
a working icon. ClaudeDial would have started, reported a tray, shown nothing,
and never dropped to the rung below — because that rung is keyed on the same
check. A silent no-op is worse than a clean failure.

The guard is `TrayBackend::hasVisibleIcon()`, asked 3 s and again 9 s after
`show()` rather than predicted in advance: a docked XEmbed window has a
geometry, and an SNI item appears in the watcher's list owned by this process
(matched by pid, because Qt registers the item on a connection of its own, so
our session-bus name is not the one listed). If neither holds, ClaudeDial says
so on stderr and keeps running, since `--json` still serves from the socket.
Verified three ways: the warning fires in the failing case, stays silent when
the icon really docks under the i3 theme, and stays silent on Plasma/Wayland
where the SNI item is found by pid.

Two caveats belong with these results. Everything was measured under XWayland
inside a Plasma Wayland session rather than a real X11 login, so it establishes
the mechanism rather than the whole environment — though the mechanism is what
decides the outcome, since a Plasma X11 session registers the same SNI host, and
the XEmbed test ran against a tray built for the purpose rather than against
`stalonetray` itself. And separately: the popup opened on most activations but
failed to appear once, against a longer-running instance. That single
non-appearance was never reproduced and remains unexplained.

GNOME's missing tray is not a bug for us to fix. GNOME removed the system tray;
applications reach it through an extension. ClaudeDial's job is to say so
clearly and to keep working through Tier 2, not to install anything on the
user's behalf.

**And GNOME has a first-class answer that is not a tray at all.** Someone has
already built a *GNOME Shell extension* for this problem, rendering two compact
meters straight into the top panel and never touching the system tray - so it
inherits none of the tray's limitations. It needs GNOME Shell 49+ and ships as a
`.shell-extension.zip`.

That generalises the Plasmoid conclusion in `portability.md` §3 into a pattern
worth stating plainly: **where a desktop has its own panel-widget API, the
native widget beats an SNI tray icon** - correct placement, no activation
guessing, no missing tooltip. Both a Plasmoid and a Shell extension are
front-ends that ClaudeDial could grow *without touching its core*, because
`--json` and the local socket already give them everything they need. Neither is
work for now; both are the right shape when a desktop's tray proves too thin.

**A third toolkit agrees about that row.** [leonardocouy/claudometer][lc], a
Tauri application solving the same problem, builds its Linux tray on
`libappindicator3` — GTK AppIndicator, not StatusNotifierItem directly. It
therefore inherits the whole AppIndicator restriction set: no tooltips, and a
left click that opens the menu rather than activating the item. Qt, dorkbox's
Java catalogue and a Rust/GTK application arriving at the same limitations from
three different directions is about as much confirmation as this contract is
going to get without a GNOME machine.

It also shows what the WebView route costs on Linux: that project needs
`libwebkit2gtk-4.1` at build *and* run time. The brief's "no embedded browser"
constraint turns out to have had a concrete portability basis and not only an
aesthetic one — WebKitGTK's 4.0/4.1 split has broken Tauri applications across
distributions repeatedly. Our runtime dependencies are three Qt packages.

---

### What a real i3 session added

Run on Debian 13 with i3 on X11, and it confirmed the isolated-display result:
Qt's XEmbed path docks correctly, `xwininfo` showing the 15x15 tray window in
the right place in i3bar. No separate i3 support is needed.

It also found a bug that no synthetic harness would have — the icon was
invisible below 75%, and visible at or above it. 75% is the warning threshold,
which is precisely where the brand colours take over from the neutral, so the
neutral was the invisible part.

The neutral came from `QPalette::WindowText`. Measured under xcb:

| `XDG_CURRENT_DESKTOP` | `WindowText` | `colorScheme()` |
| --- | --- | --- |
| `KDE` | `#fcfcfc` | Dark |
| `GNOME` | `#fcfcfc` | Dark |
| `i3` | `#000000` | **Unknown** |
| `sway` | `#000000` | **Unknown** |

Pure black is not a light desktop. It is Qt's built-in default with no desktop
integration to fill it in — and i3bar's background is black, so ClaudeDial was
drawing black on black.

The panel itself stays unknowable: neither a StatusNotifierItem host nor an
XEmbed one can be asked what it looks like. So the rule is not about the colour
but about whether the proxy carries information at all. Where `colorScheme()` is
Unknown, `Application::trayForeground()` assumes a dark panel — the default for
i3bar, polybar and waybar alike. That is a guess, chosen because it fails
visibly rather than invisibly: a light mark on a light panel is faint, a black
mark on a black panel is nothing.

**Still open:** a user whose panel is light and whose desktop tells Qt nothing
gets a faint icon and no way to say so. An explicit override would settle it —
a setting, or an environment variable for the crowd this affects, who are the
same people editing status-bar configs. Not added yet, because no such report
exists and the guess covers every default in the list above.

## Degradation

```
tray + activation + reliable placement   →  compact popup, anchored
tray + activation                        →  compact popup, wherever it lands
tray, menu only                          →  native menu; "Show usage" opens the popup
no usable tray                           →  --json / --once
```

Implemented today: all four. Rung 1 is unreachable on Plasma but does occur on
an X11 desktop whose platform theme is not D-Bus-based and whose tray speaks
XEmbed, where `geometry()` was measured exact; see above.

Rung 4 used to be unreachable in the one case that needed it most, because it
was selected by `isSystemTrayAvailable()`, which answers true where no icon can
appear. `TrayBackend::hasVisibleIcon()` now measures the outcome after the fact
instead.

**On a capability struct.** A `PlatformCapabilities { trayAvailable,
trayActivation, reliablePopupPositioning, notifications, autostart }` is the
right shape for this eventually, and better than scattered environment sniffing.
It should arrive *with the first fallback that needs it* rather than before —
three of its five fields are knowable today, but a struct that only ever holds
one value on the one platform we ship is the premature abstraction this project
set out to avoid. The moment a "use a normal window instead" option exists,
build the struct.

---

## Gaps between this contract and the code

Written down rather than quietly tolerated.

1. **No tray means the application exits.** `Application::initialize()` returns
   false and `main` exits 1 with a message pointing at `--json`. That honours
   Tier 1 but contradicts the spirit of the ladder: with no tray it could still
   show the popup as an ordinary window. Not decided, not implemented.
2. **The menu does not carry the numbers.** On AppIndicator there is no tooltip,
   so the two percentages exist only inside the popup. Disabled menu entries are
   the fix; DBusMenu renders plain items fine.
3. **Autostart is XDG-specific code inside `core`.** `Config::setStartOnLogin`
   writes `~/.config/autostart/claudedial.desktop` directly. Core is otherwise
   free of platform assumptions, and this is the one place a second platform
   would force a change. It belongs behind an interface — but only once there is
   a second implementation to put behind it.

---

## Non-goals

Declining these is a decision, not an omission:

- **A hand-rolled StatusNotifierItem implementation.** Qt has one.
- **A Plasmoid.** It would fix popup placement on Plasma perfectly, and it is
  the right answer eventually (see `portability.md` §3), but not before there is
  a released application to hang it off.
- **A GTK or libappindicator backend.** Qt's SNI covers the same desktops.
- **Desktop detection.** No sniffing `XDG_CURRENT_DESKTOP` to branch behaviour.
  Capabilities, or nothing.
- **Installing anything on the user's behalf** — no GNOME extensions, no
  changes to their Claude Code configuration.
- **Hand-maintained packages for ten distributions.** One tag, one pipeline, and
  the formats that pipeline can actually produce.

Qt, then a native fallback, then the CLI. That is most of the portability for a
small fraction of the work.

---

## Before v0.1.0

Against the tiers above, what is done and what is not:

| | State |
| --- | --- |
| `QSystemTrayIcon` the only tray backend | done |
| No KDE Frameworks | done — core links `Qt6::Core` and `Qt6::Network`, nothing else |
| `UsageState` separated from tray and QML | done |
| Native context menu | done, minus the numbers (gap 2) |
| `--json` / `--once` as public interface | done, documented here and in the README |
| Absence of a tray does not kill the application | **not done** (gap 1) |
| Autostart behind an interface | **not done** (gap 3) |
| AppImage | not done |
| `claudedial-bin` in the AUR | not done — the PKGBUILD is a source package |
| Compatibility matrix in the README | done — a short version, linking here |

And then the part no amount of preparation replaces: release it, and let real
reports from real desktops replace guesses. Even Syncthing Tray, with a far
longer list than ours, does not claim to test every combination.

[appind]: https://github.com/ubuntu/gnome-shell-extension-appindicator
[lc]: https://github.com/leonardocouy/claudometer
