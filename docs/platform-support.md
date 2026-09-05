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
| macOS | not attempted — four things missing, see below |

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

### Windows, as far as it has got

Personal use, so: no installer, no signing, no Store. The release workflow
assembles the Qt runtime with `windeployqt` and zips it; a manual run leaves
that zip as an artefact, which is how to get a build without spending a version
number.

The expensive part turned out not to exist. Claude Code keeps the token in a
plain file there as on Linux — `%USERPROFILE%\.claude\.credentials.json`,
honouring `CLAUDE_CONFIG_DIR` — corroborated by the native Windows tray
surveyed in [usage-api.md](usage-api.md). `QDir::homePath()` resolves to
`%USERPROFILE%`, so `core::Credentials` is untouched and nothing is written
against DPAPI or the Credential Manager. macOS is the opposite case, and is why
it stays unattempted: the Keychain would mean new credential code in the one
place that must not be got wrong.

Three things a Linux desktop answers through D-Bus needed another answer:
notifications go out through the tray icon itself, which is native there and
loses only `replaces_id`; suspend and resume simply never fire, already the
documented degradation where logind is absent; and autostart is a value under
the registry's `Run` key, reached through `QSettings`.

One Windows-specific wrinkle: Qt links this as a GUI binary, which there means
no console at all, so `claudedial --json` from a terminal would print into
nothing. It borrows the parent console when given any flag.

**The macOS build does not compile yet**, contrary to what the section below
first claimed on the strength of the D-Bus guards alone. CI says the build step
fails; the reason is not yet known, because Actions logs need admin rights to
read and there is no cross compiler here. Recorded rather than explained.

**Run on a real Windows desktop, and it works.** Confirmed there: the tray icon
appears, the CLI prints to the terminal that launched it, and the small-size
mark renders — Windows asks for a 16-pixel notification-area icon, which trips
the same level of detail added for GNOME and i3, so Percentage shows the number
with a bar rather than the dial. Selecting the Gauge style keeps the dial, which
survives 16 px with no number inside it.

Not exercised there: notifications, autostart, and reading real credentials.
The last one has a trap worth repeating — if Claude Code runs inside WSL its
credentials are in the WSL filesystem, where a native build will not find them.

One unexplained symptom, kept because it is real: exiting prints
`QObject::killTimer: Timers cannot be stopped from another thread`. It does not
reproduce on Linux, on either the primary or the secondary-instance path, and
diagnosing it needs a Windows machine — there is no cross compiler on any
machine working on this. Harmless as far as anyone can tell, and it stays
written down rather than guessed at.

It is a row of its own rather than a supported tier because "works when tried
once" is not the same as tested.

### macOS, and what it would actually take

Worth writing down because the Windows port made the build portable in passing,
which makes macOS look closer than it is. `if (UNIX AND NOT APPLE)` already
excludes D-Bus and the XDG install rules, and every use of it is behind
`CLAUDEDIAL_HAVE_DBUS`, from which this document concluded the tree would
compile there. **It does not** — the first CI run on macOS failed at the build
step. So even the free-looking part was not free, which is the second time in
this file that "the mechanism implies it works" has been wrong.

**The token is in the Keychain**, service `Claude Code-credentials`, so the file
reader finds nothing. This is the one that decides the whole thing, and it
carries an unknown that cannot be settled from here: a Keychain item has an
access control list, and whether a *different* binary may read the one Claude
Code created depends on that ACL and on the user answering a prompt. Reading it
is perhaps forty lines of `SecItemCopyMatching`; finding out whether it can be
read at all needs a Mac. And it is new code in the one place the security rules
say to be most careful, which is the opposite of the Windows case, where the
token turned out to be a plain file and `core::Credentials` was never touched.

**Autostart currently lies there.** `startOnLogin` branches on `Q_OS_WIN` and
everything else writes an XDG `.desktop` entry — on macOS into
`~/Library/Preferences/autostart/`, where nothing will ever read it. The toggle
would appear to work and do nothing. A `LaunchAgent` plist in
`~/Library/LaunchAgents` is the real answer, and until it exists the setting
should refuse rather than pretend.

**Notifications need a bundle.** `QSystemTrayIcon::showMessage` goes through the
system notification centre, which in practice ignores a bare binary; it wants a
signed, bundled `.app`. Our fallback path is already in place from the Windows
work, but it would be delivering into nothing.

**A signature has to survive deployment, or the kernel kills the process.**
`macdeployqt` rewrites library load paths with `install_name_tool` after the
linker has already ad-hoc signed everything, which invalidates those
signatures. On Apple Silicon that is fatal rather than a warning, and the shell
reports it as nothing more than:

```console
$ ./claudedial.app/Contents/MacOS/claudedial --demo 96,41
zsh: killed
```

No dialog, no message, no exit code worth reading. CI re-signs ad-hoc after
deploying — `codesign --force --deep --sign -`, which needs no identity and no
keychain — and verifies the result, so this cannot regress quietly.

**Gatekeeper, and it bites before anything else does.** An unsigned build is
blocked on first run. From a terminal the symptom is not a dialog and not
"permission denied" — it is:

```console
$ ./claudedial.app/Contents/MacOS/claudedial --demo 96,41
operation not permitted
```

Which looks like a file-permission problem and is not: the executable bit is
set, and the cause is `com.apple.quarantine` on the whole bundle. Observed with
the attribute reading `0087;6a9c2f30;Telegram;` — it is stamped by whatever
delivered the file, a messenger as readily as a browser, so it will be there
however the artefact travels. One command clears it, and it has to be recursive
because the attribute is on the tree rather than on the binary alone:

```console
$ xattr -dr com.apple.quarantine claudedial.app
```

 Fine for personal use, no Developer ID needed — but ad-hoc signing changes
the binary's identity on every rebuild, which is exactly what a Keychain ACL
keys on, so the prompt may return after every update.

None of this is hard in isolation. It is unverifiable without a Mac, and the
credential path is the wrong place to ship code nobody has run.

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

### And then KDE's own Breeze Twilight did it differently

The first fix keyed on `colorScheme() == Unknown` — treating the palette as
uninformative rather than wrong. A second report killed that idea: on Breeze
Twilight, a look-and-feel Plasma ships, the icon was invisible below 75% too.

Measured live on that desktop: `WindowText #232629`, `Window #eff0f1`,
`colorScheme() = Light`. The palette is *honest*. It is simply about the wrong
thing, because on Plasma the panel is a separate setting — the look-and-feel
package says so outright:

```
# org.kde.breezetwilight.desktop/contents/defaults
[kdeglobals][General] ColorScheme=BreezeLight    # applications light
[plasmarc][Theme]     name=breeze-dark           # panel dark
```

So the proxy is not merely uninformative sometimes; it can be accurate and
useless. Chasing it further would mean reading `plasmarc`, resolving an unset
value through the look-and-feel package's defaults, and deciding whether a
Plasma theme name implies a dark panel — four fragile hops, for one desktop.

**A fixed mid-tone was tried next, and rejected on a report.** Contrast ratios
against a dark panel, a light panel and i3bar's black:

| mark | dark | light | i3bar |
| --- | --- | --- | --- |
| near-black `#232629` | **1.0** | 13.3 | **1.4** |
| light grey `#dcdcdc` | 11.1 | **1.2** | 15.3 |
| mid grey `#9a9a9a` | 5.4 | 2.47 | 7.5 |
| best possible grey `#7c7c7c` | 3.64 | 3.66 | 5.0 |
| `kUsageWarning` | 5.8 | 2.3 | 8.0 |

Either extreme is invisible somewhere, so `#9a9a9a` shipped — and came back as
faint on a light panel, then faint on a dark one too. That is not a bad choice
of grey. `#7c7c7c` is the grey that maximises the worse of the two Plasma
panels, and 3.64 is the ceiling for *any* single colour: a fixed neutral can be
balanced or crisp, never both. Terracotta scores comparably (4.9 / 2.7) and also
shares a hue with the warning step, which would blunt the escalation.

**And then Auto still had to be right by default.** Leaving Breeze Twilight to
a setting was not good enough: it is a look-and-feel KDE ships, on the platform
most of these users are on, and a default that draws an invisible icon there is
a bug rather than a configuration question.

Plasma does write the panel's colour down, and it is exact. The chain, verified
against a screenshot of the panel:

1. `~/.config/plasmarc` `[Theme] name` — absent on the machine in question,
   while the desktop was plainly using breeze-dark, so this cannot be the only
   source;
2. `kdeglobals LookAndFeelPackage` — then that package's
   `contents/defaults`, whose groups read `[plasmarc][Theme]` rather than plain
   INI. Twilight's says `name=breeze-dark`;
3. `plasma/desktoptheme/breeze-dark/colors` `[Colors:Window] BackgroundNormal`
   = `32,35,38`.

`#202326`. Sampling the panel's own pixels from a screenshot measured
`#202326`, luminance 34.6. Not a heuristic — the panel's actual background.

The stock `default` theme ships no `colors` file at all, which is how it says it
follows the application colour scheme, and that is exactly the case where
QPalette was the right answer all along. So Auto now reads the declared panel
colour where there is one, and falls back to the palette where there is not.
The parsing lives in `core::PanelTheme` with the real config strings as its
tests, since core takes only Qt Core and Network and stays testable headlessly.

**The tone stays a setting too.** `core::Config::TrayTone` — Auto, Light, Dark —
with `brand::kTrayNeutralLight` (`#dcdcdc`, 11.1 on a dark panel) and
`kTrayNeutralDark` (`#232629`, 13.3 on a light one). Both crisp, because each is
chosen for a known background rather than hedged across two.

Auto keeps the palette, which is right wherever the panel follows the
applications — stock Breeze and Breeze Dark — and assumes a dark panel where
the platform reports no colour scheme at all. Auto is wrong wherever the panel
is themed independently, which is precisely what the explicit tones are for.

Thresholds and the warning/critical/severe steps were never touched by any of
this.

Two notes for whoever tests this next. The notification icon takes the same
tone, on the assumption that a notification daemon and a panel are themed
together; nobody has checked that. And redirecting `XDG_CONFIG_HOME` to isolate
a test config also hides `kdeglobals` from Qt, so Auto measured that way
reports a palette the desktop is not using — the explicit tones are unaffected,
which is how that was noticed.

### What clean Debian 13 VMs added

Tested on v0.1.4 artefacts, i3 on X11 and GNOME on Wayland, and the headline is
that the portability bet holds: `QSystemTrayIcon` works on both, so no
desktop-specific tray backend is needed anywhere. What came back were detection
and polish faults, and two of them corrected things written above.

**i3bar was light, not black.** The fallback here assumed panels are
overwhelmingly dark — "i3bar, polybar and waybar all default that way" — and
on this Debian i3 the bar was very light, so a light mark was invisible. The
palette had also reported light on that machine, where forcing
`XDG_CURRENT_DESKTOP=i3` inside a Plasma session reported black. So the palette
is unreliable in both directions on a desktop with no Qt integration, and so is
any assumption about which way panels lean. The fallback no longer picks a side:
`brand::kTrayNeutral` is the grey that maximises the worse case, and anyone who
wants crisp uses the Tray icon setting.

Plasma is unaffected by that — the declared panel colour still wins, and the
stock `default` theme, which declares nothing because it follows the
application colour scheme, still uses the palette. Ignorance and
"Plasma says it follows the apps" are now told apart rather than sharing a
branch.

**The tray verification produced a false alarm on GNOME.** With the
AppIndicator extension the icon appeared in the top bar and worked, while
ClaudeDial printed that it had not appeared. The check looked itself up in the
watcher's `RegisteredStatusNotifierItems` and matched by pid, which Plasma
answers and that extension does not. It now asks only whether a
`StatusNotifierWatcher` exists at all: registration is asynchronous and hosts
disagree about what they report, so anything more specific is guesswork
dressed as a check — and a false warning is worse than no warning. It still
catches what it was built for, a desktop claiming a tray with no host on the bus.

**Tiling layout, measured.** Under xcb the popup already declares
`_NET_WM_WINDOW_TYPE_UTILITY` first, which i3 floats; the settings window
declared only `NORMAL`, so i3 tiled a form into a workspace column. It is now a
`Qt::Dialog` and declares `DIALOG`, which i3 floats by default. If the popup is
still tiled, that is i3 reading the type list differently and the remedy is a
rule:

```
for_window [class="claudedial" title="ClaudeDial"] floating enable
```

**The 15x15 icon has a level of detail now.** The first measurement was right
— there is no outer padding to reclaim, the ink already spans the full pixmap
width at every size. What was actually wrong is that the *design* does not
survive the pixels: a number that has to fit inside a 240-degree arc ends up at
a fraction of the box, reported from GNOME as "an icon inside an icon" and from
i3 as the arc turning to grey noise.

Enlarging the digits inside the arc was tried and is worse, not better: at 15 px
the two collide and both go muddy. Rendered side by side at 15 px, magnified
without interpolation, the only variant that reads is the one that gives up the
dial: the number at 0.86 of the box with a two-pixel usage bar beneath it,
pixel-aligned and unantialiased. For the gauge style, which has no number
competing for the space, the answer is the opposite — keep the dial and make
the stroke heavier, 0.55 against 0.32, because weight is what survives few
pixels. A one-pixel arc with antialiasing off was also tried, and turns visibly
polygonal.

So below 17 px both styles draw something simpler, and that is chosen **by the
size of the pixmap, never by the desktop**. Which needs one caveat about the
protocols: a StatusNotifierItem host is never asked what size it wants. We
publish every size in `kSizes` — verified on the bus: 16, 22, 24, 32, 48, 64,
now with 15 added for i3bar's exact request — and the host picks one and scales
it as it likes. Baking the simpler mark into the small entries is therefore the
only way to reach a host with a small panel at all, GNOME's included.

Plasma is untouched, and by proof rather than by inspection: rendering both
styles at 22, 24, 32, 48 and 64 hashes byte-for-byte identically before and
after the change.

*Superseded, kept because the measurement stands:* the ink already spans the
full pixmap width at every size — 15x12 with margins L0 R0 T0 B3 at 15 px —
and the gap at the bottom is the dial's own opening.

**Packaging, now documented:** the AppImage needs FUSE on the host unless run
with `--appimage-extract-and-run`, and the tarball is an install tree that
links the system Qt rather than a portable bundle — it stops at
`libQt6QuickControls2.so.6` on a Debian box with no Qt.

**Confirmed as supported:** GNOME on Wayland with the AppIndicator extension —
tray, popup, notifications and the icon inside the notification all work. Stock
GNOME without the extension correctly reports no tray and points at `--json`.

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

### Suspend and resume

Reported working on Arch with Plasma: nothing breaks across a suspend, which
was the real risk — a dropped socket, a request left hanging, a timer that
never fires again.

What that does *not* establish is which mechanism refreshed the numbers. The
logind path exists for promptness, not correctness: `PrepareForSleep(false)`
triggers a refresh on wake, and without it the ordinary interval catches up
within five minutes anyway. Both look like "fine after waking".

Distinguishing them costs one reading. Note `updated_at` from `claudedial
--json` before suspending, and read it again immediately after waking: within
seconds of the wake means logind fired; up to a refresh interval old means the
timer did it.

It cannot be tested from a script. Injecting the signal with `dbus-send
--system` is accepted by the bus and then not delivered — verified with a
listener subscribed exactly as SleepWatcher is, which reported
`connected=1 subscription=1` and received nothing. An unprivileged sender may
not broadcast on that interface, so an app that fails to react proves nothing.
That mistake was made here once already.

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
