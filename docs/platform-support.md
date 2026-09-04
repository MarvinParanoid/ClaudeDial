# Platform support — the contract

What Claudometer guarantees, what it attempts, and what it declines to do.

This is a contract rather than a wish list. Its purpose is to make the answer to
"does Claudometer work on X?" a matter of reading rather than of testing twenty
desktops — and to stop the codebase from slowly acquiring a `if (gnome &&
wayland && version >= …)` in every method.

The rule it encodes: **information must never exist in only one place.** Every
tier below can be missing on somebody's desktop, so nothing may depend on the
tier above it being present.

---

## Three tiers of guarantee

### Tier 1 — the core, and the CLI. Guaranteed.

```
claudometer --json     machine-readable, stable
claudometer --once     human-readable, one shot
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
| KDE Plasma / X11 | SNI | may anchor to the icon | yes | no |
| Xfce | SNI | may anchor to the icon | yes | no |
| Cinnamon, MATE, LXQt, DDE | SNI | unknown | likely | no |
| GNOME | needs the [AppIndicator extension][appind] | n/a — reached from the menu | **no** | no |
| COSMIC | SNI, menu only reported elsewhere | unknown | unknown | no |
| Sway, Hyprland, i3, Awesome | Waybar/i3blocks via `--json` | n/a | n/a | no |
| Windows, macOS | Qt supports it; we have not tried | — | — | no |

The X11 rows matter more than they look. Qt 6 still implements the XEmbed tray
protocol, so `QSystemTrayIcon::geometry()` may return a real rectangle there —
which would make the anchored popup work on X11 even though it cannot on
Wayland. Untested, and the single most valuable thing to test next.

GNOME's missing tray is not a bug for us to fix. GNOME removed the system tray;
applications reach it through an extension. Claudometer's job is to say so
clearly and to keep working through Tier 2, not to install anything on the
user's behalf.

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

## Degradation

```
tray + activation + reliable placement   →  compact popup, anchored
tray + activation                        →  compact popup, wherever it lands
tray, menu only                          →  native menu; "Show usage" opens the popup
no usable tray                           →  --json / --once
```

Implemented today: rungs 2, 3 and 4. Rung 1 needs X11 to confirm it exists at
all.

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
   writes `~/.config/autostart/claudometer.desktop` directly. Core is otherwise
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
| `claudometer-bin` in the AUR | not done — the PKGBUILD is a source package |
| Compatibility matrix in the README | done — a short version, linking here |

And then the part no amount of preparation replaces: release it, and let real
reports from real desktops replace guesses. Even Syncthing Tray, with a far
longer list than ours, does not claim to test every combination.

[appind]: https://github.com/ubuntu/gnome-shell-extension-appindicator
[lc]: https://github.com/leonardocouy/claudometer
