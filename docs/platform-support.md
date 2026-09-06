# Platform support — the contract

What ClaudeDial guarantees, what it attempts, and what it declines to do.

This is a contract rather than a wish list. Its purpose is to make the answer to
"does ClaudeDial work on X?" a matter of reading rather than of testing twenty
desktops — and to stop the codebase from slowly acquiring a `if (gnome &&
wayland && version >= …)` in every method.

The rule it encodes: **information must never exist in only one place.** Every
tier below can be missing on somebody's desktop, so nothing may depend on the
tier above it being present.

This document says what is true now. How each answer was arrived at — the
measurements, the three rounds it took to work out how Qt picks a tray backend,
the conclusions that were stated confidently and then disproved — is in
[research/platform-testing.md](research/platform-testing.md). Keeping the two
apart is deliberate: a reader asking "does this work on my desktop?" should not
have to reconstruct the answer from a chronology.

---

## Which desktops, and how far

Two axes, and keeping them apart is what stops this document contradicting
itself. The tiers below are about **features**: the CLI, then the tray, then the
popup, and which of them may be missing. This table is about **desktops**: how
much care each one gets when a choice cannot serve them all.

| Desktop | Level |
| --- | --- |
| KDE Plasma / Wayland | primary — polished and tested, and where a visual decision is settled |
| GNOME / Wayland + AppIndicator | supported — must work and look reasonable |
| i3 / X11 with a tray | supported, best effort — must work; cosmetic compromises accepted |
| Xfce, Cinnamon, MATE, LXQt, COSMIC | expected to work; unverified |
| No tray | the CLI is the interface, not a fallback |
| Windows | works, lightly tested; personal use |
| macOS | runs; one session on one machine, personal use |

ClaudeDial is made of exactly what Plasma is built around — a permanently
visible tray icon, a small popup on click, settings, notifications, autostart —
so that is where it is taken to the pixel. The rest is portability, not equal
ambition.

**The operative rule: Plasma is not made worse to make another desktop better.**
Where one choice cannot serve both, Plasma gets the precise answer and the others
get a safe one. That is not a preference, it decides code: the tray neutral
reads the panel colour Plasma declares, and falls back to a grey that is merely
never invisible everywhere else. Balancing that grey across all desktops was
tried first, and it made Plasma worse for no one's benefit.

Note what the rule does *not* license. GNOME reporting a false failure, or i3
tiling a settings form into a column, are bugs at "supported" level and were
fixed as such — the licence is for polish, not for defects.

### Windows and macOS, in one paragraph each

**Windows** builds, tests and packages on every push, and has been run on a real
desktop: the tray icon appears, the small-size mark renders, the CLI prints to
the terminal that launched it. Not exercised there: notifications, autostart and
reading real credentials — which, since the tray and the CLI both work, is most
of what a supported tier would have to promise. One unexplained symptom survives:
exiting prints `QObject::killTimer: Timers cannot be stopped from another
thread`. It does not reproduce on Linux and diagnosing it needs a Windows
machine.

**macOS** builds, tests and packages on every push, and ships as a release
asset. Seen working on one machine: the menu bar item, the popup anchored under
it, settings, no Dock icon, a LaunchAgent, a notification permission prompt.
The one open question is credentials — the token lives in the login Keychain and
`Credentials::loadFromKeychain()` reads it through `security`, but that has never
run against a real subscription, so `CLAUDE_CODE_OAUTH_TOKEN` remains the
supported way in. Also untried by anyone: an Intel Mac; CI builds arm64 only.

Both are personal-use scaffolding rather than supported tiers. What was measured
to get here — Gatekeeper, quarantine, a kernel killing an unsigned binary, a
deployment that silently carried no QML — is in
[research/platform-testing.md](research/platform-testing.md).

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
| GNOME | needs the [AppIndicator extension][appind] | n/a — reached from the menu | **no** | **yes** — Debian 13, Wayland |
| COSMIC | SNI, menu only reported elsewhere | unknown | unknown | no |
| i3 + an XEmbed tray | XEmbed | **anchored to the icon** — *measured* | n/a | **yes** — Debian 13, i3bar |
| Sway, Hyprland, Awesome | Waybar/i3blocks via `--json` | n/a | n/a | no |
| Windows, macOS | Qt supports it; anchoring works | anchored to the icon | — | **yes** — one machine each |

### Which tray protocol, and who decides

**Qt picks its tray implementation from the platform theme, not from the
platform.** A D-Bus platform theme — which is what `XDG_CURRENT_DESKTOP=KDE`
loads — takes the StatusNotifierItem path on X11 exactly as on Wayland, and SNI
has no concept of icon geometry, so `QSystemTrayIcon::geometry()` is empty and
the popup cannot be anchored. A non-D-Bus theme with an XEmbed tray docks and
reports exact geometry.

That is what makes the anchored branch in `PopupWindow::placeAndShow` live on
i3-with-a-tray, on Windows and on macOS, and unreachable on Plasma either way.
It took three rounds to establish and the first two answers were wrong; the
measurements are in
[research/platform-testing.md](research/platform-testing.md#linux-desktops).

One consequence is a guard rather than a fact: where a D-Bus platform theme is
active but no `StatusNotifierHost` exists, `isSystemTrayAvailable()` returns
**true** and no icon ever appears. `TrayBackend::hasVisibleIcon()` measures the
outcome afterwards instead of trusting that answer.

### Suspend and resume

Nothing breaks across a suspend — reported on Arch with Plasma. Which mechanism
refreshed the numbers, logind or the ordinary interval, is not established, and
[research/platform-testing.md](research/platform-testing.md#suspend-and-resume)
says how to tell them apart and why a script cannot.

### Living with the popup's position on Wayland

A Wayland client cannot place its own top-level windows, so `setPosition()` is
ignored and the compositor decides — typically the middle of the screen.
ClaudeDial asks the panel where its icon is and anchors to it when it gets an
answer, which on Wayland it never does; that path is reached on X11 with an
XEmbed tray, and was seen working on i3.

Two things that do work, and belong here rather than in a README:

**Drag the popup by its header.** That goes through the compositor, so it moves
where `setPosition()` cannot.

**On Plasma, a window rule makes it stick.** System Settings → Window
Management → Window Rules, match window class `claudedial` *and* window title
`ClaudeDial`, then force Position to the corner you want. Matching the title as
well as the class matters: on class alone the rule also catches the settings
window, which is why the popup carries a title at all.

And one that does not: the popup may not receive keyboard focus, and then it
will not dismiss itself when you click elsewhere. Clicking the tray icon again,
or the close button, always works. This is the reason focus-out dismissal was
removed rather than fixed.

## Degradation

```
tray + activation + reliable placement   →  compact popup, anchored
tray + activation                        →  compact popup, wherever it lands
tray, menu only                          →  native menu; "Show usage" opens the popup
no usable tray                           →  --json / --once
```

Implemented today: all four. Rung 1 is unreachable on Plasma but does occur on
an X11 desktop whose platform theme is not D-Bus-based and whose tray speaks
XEmbed, where `geometry()` was measured exact.

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

The list is empty. What used to be here is below, named rather than deleted so
the reasoning stays checkable.

**A late tray was a real bug, and is fixed.** A panel registers its
StatusNotifierHost some way into the login and an autostart entry can easily run
first; `isSystemTrayAvailable()` was false at that moment, the application
exited, and the user had no icon for the session with nothing on screen saying
why. `--wait` covers it, and every autostart entry this application writes
passes the flag; a manual launch does not, so a session with genuinely no tray
still says so in fifteen milliseconds rather than after a minute. Found by
reading [Syncthing Tray][st], which answers the same question the same way.

That flag arrived with a bug of its own, worth keeping because it is the sort
that hides: the XDG entry named the bare `claudedial`, which is on the
*session's* PATH only for an installed copy. A build run from its own tree is
not, so the entry pointed at nothing and the session started nothing — reported
from Arch, and measured: that `Exec` line exits 127 under a login PATH. Every
entry now records the real binary, as Windows and macOS always did.

Two more, closed earlier:

- *The menu does not carry the numbers.* It does now — `format::menuEntry()`
  puts both percentages at the top of the tray menu as disabled items, which is
  what AppIndicator's missing tooltip needed.
- *Autostart is XDG-specific code inside `core`.* The condition attached to that
  entry was "only once there is a second implementation to put behind it", and
  the Windows and macOS ports supplied a second and a third. It now lives in
  `core/Autostart.{h,cpp}` — a three-function namespace, not a class hierarchy,
  because there is nothing to polymorph over — and `Config` delegates to it. The
  move also fixed a quiet lie: a write that failed used to leave the toggle
  looking enabled, and `changed()` is now emitted either way so the UI re-reads
  the truth.

---

## Non-goals

Declining these is a decision, not an omission:

- **A window instead of a tray icon where there is no tray.** Syncthing Tray's
  other flag is `--windowed`, and the code for it is already here: the popup
  renders perfectly well as an ordinary window. Declined anyway, because the
  contract above already answers that case and answers it differently — on Sway,
  on Hyprland, in a bare session, `claudedial --json` is *the* interface for
  that user rather than a consolation. Shipping a window would say the opposite
  of what this document says, for a case that has a better answer already. A
  tray that is merely late is a different thing, and `--wait` handles it.
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
| Native context menu | done, and it carries the numbers |
| `--json` / `--once` as public interface | done, documented here and in the README |
| Absence of a tray does not kill the application | `--wait` for a late tray; no tray at all is the CLI, by decision |
| Autostart behind an interface | done — `core/Autostart.{h,cpp}` |
| AppImage | done — built and attached to every tag |
| `claudedial-bin` in the AUR | written, not published — AUR registration was closed when it was tried |
| Compatibility matrix in the README | done — a short version, linking here |

And then the part no amount of preparation replaces: release it, and let real
reports from real desktops replace guesses. Even Syncthing Tray, with a far
longer list than ours, does not claim to test every combination.

[appind]: https://github.com/ubuntu/gnome-shell-extension-appindicator
[st]: https://github.com/Martchus/syncthingtray
