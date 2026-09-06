# Portability and distribution — what to take from the field

**Scope:** research only. No ClaudeDial code was changed to write this.

Surveyed for the things a tray utility gets wrong on desktops its author does
not run: [Syncthing Tray][st] (C++/Qt/CMake, the closest match to our stack and
purpose), [dorkbox/SystemTray][dorkbox] (a long catalogue of per-desktop
breakage), [gogpu/systray][gogpu] (platform split and icon requirements),
[gatus-monitor][gatus] (a package matrix a small project actually ships), and
[Qt's own documentation][qtdoc].

Two findings break ClaudeDial's primary interaction outside Plasma. Everything
else is either confirmation that our current choices are right, or a list of
traps for later.

---

## 1. Two things that break our design on GNOME

### Left click does not open anything

ClaudeDial's whole interaction model is *glance → hover → click*. Two sources
agree that the click half does not survive GNOME:

- Qt: "since GNOME Shell 3.26, not all activation reasons are supported without
  shell extensions."
- Syncthing Tray, of the same UI we have: "when using the mentioned GNOME
  extension the Syncthing Tray UI shown in the screenshots is only accessible by
  **double**-clicking the icon."

So on GNOME with the AppIndicator extension, a single left click gives the menu,
not our popup.

Our menu had no way to open the popup — it was Refresh / Settings / Quit — so on
GNOME the popup would have been unreachable rather than merely awkward. That was
the one finding in this survey that made the product not work on a whole
desktop, so it is **fixed**:

1. **"Show usage" is now the first menu item.** ✅ It emits `showRequested()`
   rather than `activated()`, because a menu item by that name must not close
   the popup when it is already open.
2. **`DoubleClick` is handled as well as `Trigger`.** ✅ Mapped to *show* rather
   than toggle, because a double click arrives as `Trigger` then `DoubleClick`
   and the pair has to be idempotent - otherwise it would open the popup and
   immediately shut it again. Verified: single click still toggles both ways,
   and "Show usage" twice in a row leaves it open.
3. **Done.** The two percentages are disabled entries at the top of the menu,
   which is the actual replacement for the missing tooltip: DBusMenu renders
   plain items fine where tooltips do not exist at all. Verified on the bus
   rather than by reading the code — the panel is handed `Session 63% · resets
   in 2h` and `Weekly 41% · resets in 4d`, and when there is no data those two
   entries and their separator come across as `visible=false`, so the menu looks
   exactly as it did before rather than growing two blank rows.

### The tooltip does not exist on AppIndicator

dorkbox, flatly: "AppIndicators do not support tooltips at all", where
GtkStatusIcon and Swing support them up to 64 characters.

Our tooltip carries the whole summary — both windows, both reset times. On any
desktop served by AppIndicator, that middle tier of the hierarchy is simply
gone. The popup does carry the same information, which is why fixing
reachability above matters more than the tooltip itself.

**Conclusion:** the tooltip is a Plasma-and-friends nicety, not a tier we can
rely on. It must never be the only place something lives. It currently is not,
but nothing enforces that.

---

## 2. What the field confirms we already got right

Worth writing down explicitly, so none of it gets "improved" later.

**Wayland cannot position client windows.** Syncthing Tray: "The tray menu can
not be positioned correctly under Wayland because the protocol does not allow
setting window positions from the client-side." We found this empirically. It is
not a bug in our code and not something a newer Qt fixes.

**A frameless normal window is the right shape for the popup.** Syncthing Tray
again: "It is also not possible to use a popup window under Wayland. Therefore a
normal window without title bar is used." That is exactly what `PopupWindow`
does — `Qt::Tool | Qt::FramelessWindowHint`. Independent arrival at the same
answer.

**Compositor rules are the accepted user-side fix.** They ship Sway and KWin
rules for exactly this. Our README already tells Plasma users to pin the popup
with a window rule, matching on class `claudedial` and title `ClaudeDial`.

**Do not build a portability layer.** `QSystemTrayIcon` in Qt 6 covers Windows,
macOS, D-Bus StatusNotifierItem (KDE, GNOME, Xfce, LXQt, DDE) *and* the legacy
X11 XEmbed protocol. A three-way `PlatformTray` split like [gogpu/systray][gogpu]
is what you write when you have no toolkit; we have one. Our `TrayBackend` is a
single virtual worth keeping as an escape hatch — and worth not growing until
something concrete forces it.

**One correction to my own earlier note.** I had suspected Qt 6 dropped XEmbed;
it did not. The documentation lists both protocols. So `geometry()` may well
return a usable rectangle on X11 under an XEmbed tray, which makes X11 testing
more valuable than I implied: the anchored-popup path could genuinely work there.

---

## 3. The real fix for popup placement on Plasma is a Plasmoid, not layer-shell

Syncthing Tray ships two front-ends: a Qt Widgets application, and a native
Plasmoid for Plasma 5 and 6. The reason is precisely our problem — "the Plasmoid
is not affected by this", where *this* is Wayland positioning. A Plasmoid lives
in the panel, so Plasma positions its popup correctly by construction.

This is a better answer than `layer-shell-qt`, which I had been treating as the
option:

| | Plasmoid | layer-shell |
| --- | --- | --- |
| Placement on Plasma | correct by construction | correct |
| Other compositors | n/a | KWin and wlroots only, never GNOME |
| New C++ dependency | none | yes |
| Touches our core | no | yes |

And it is unusually cheap for us, because a Plasmoid is QML and ClaudeDial
already has two machine-readable interfaces it could read: `claudedial --json`,
or the local socket that answers `status` without spending an API request. A
Plasmoid would be a separate, optional package that consumes those — no changes
to the core at all.

Not now. But it is the thing to build when Plasma users complain about placement,
and it should displace layer-shell in the plan.

---

## 4. Graceful degradation, made concrete

The ladder, with what each rung actually needs:

| Situation | Behaviour | Needs |
| --- | --- | --- |
| Plasma, left click works | popup | works today |
| GNOME, only double click | popup on double click, and from the menu | done |
| AppIndicator, no tooltip | numbers in the menu | done - `format::menuEntry()`, two disabled entries |
| Frameless popup misbehaves | a normal decorated window | an appearance setting — Syncthing Tray has exactly this |

Not to be confused with a window standing in for a missing tray, which is
declined outright — see the non-goals in
[platform-support.md](platform-support.md). This rung is for a desktop that
*has* a tray and renders our frameless popup badly.
| No tray at all | `--json` / `--once` | works today: we exit with a message pointing at them |

Four of the five rungs behave today. The bottom one always did:
`SystemTrayBackend::isAvailable()` is checked and the failure message names
`--json` - and where the tray is merely late rather than absent, `--wait` now
covers it, which is what every autostart entry passes. What is left is a single
appearance option for desktops where the frameless popup misbehaves, which
cannot be verified from here because it only matters on desktops this was not
developed on.

---

## 5. Packaging traps, aimed at our AppImage

Syncthing Tray's generic self-contained Linux build is the most directly useful
artefact in this survey, because it documents what actually went wrong.

- **Bundle OpenSSL.** "This build bundles OpenSSL because different GNU/Linux
  distributions come with different incompatible versions of that library", with
  `OPENSSL_CONF=` documented as the escape hatch when a host config breaks TLS.
  We use `QtNetwork` over HTTPS to exactly one host, so we walk into this trap
  head-first. An AppImage that does not bundle OpenSSL will fail to fetch on some
  distro and the report will look like our bug.
- **State a glibc floor.** They say `glibc>=2.26` and name the oldest distro
  releases that satisfy it. Build on the oldest practical image in CI, not on
  Arch, and put the number in the README.
- **They also require OpenGL and libX11.** So do we: Qt Quick needs a GL stack.
  Worth saying out loud before someone tries it on a headless box.
- **Ship a scalable icon and a `.desktop` entry inside the AppImage**, since the
  desktop resolves `Icon=claudedial` by name. We already install both.

On channels, what a one-person project sustains: **AUR** (their model is a
PKGBUILD plus an optional prebuilt binary repository), **Flathub**, and a generic
tarball or AppImage. openSUSE's Build Service is worth knowing about — it builds
many distributions' packages from one spec, which is the cheapest way to reach
Debian and Fedora without maintaining either.

Our Flatpak-specific problem stands: the sandbox needs a filesystem override to
read `~/.claude/`. That is already noted in `packaging/README.md`, and §5(b) of
`usage-api.md` now offers a way out — the status-line source needs no
credentials at all, which would make a sandboxed build far less awkward.

[gatus-monitor][gatus] shows the full matrix (.deb, .rpm, AppImage, .dmg, .msi)
is achievable by a small project, but it is Go and Fyne, so only the ambition
transfers, not the tooling.

---

## 6. Icons, if ClaudeDial ever leaves Linux

From [gogpu/systray][gogpu], which states the requirements plainly:

| Platform | Sizes | Requirement |
| --- | --- | --- |
| Linux / SNI | 22×22, 24×24 | spec recommends 22×22 |
| Windows | 16×16, 32×32 | both standard and HiDPI |
| macOS | 22×22, 44×44 @2x | **must be a monochrome template image** |

We publish 16, 22, 24, 32, 48 and 64, so Linux and Windows are covered.

**macOS is a design problem, not a packaging one — and it already has an answer,
which is not ours.** A template image is alpha-only: macOS recolours it for the
light or dark menu bar and discards our colour, so the entire usage ramp would
vanish and with it the reason the icon reads at a glance.

The macOS implementations of this same idea sidestep the problem entirely by
**putting text in the menu bar rather than an icon** — `5h 46% · 7d 12%`, or
just `46`. That is idiomatic there, it needs no template image, and severity
moves to notifications instead of colour. Worth knowing before anyone tries to
port the dial: on macOS the dial is probably the wrong artefact, not a
differently-coloured one.

Distribution friction to expect there too: those projects ship unnotarised, and
tell users to right-click on first launch. Notarisation needs a paid Apple
Developer account.

Windows needs less thought: Qt 6.7+ follows the system dark mode automatically,
per Syncthing Tray's notes; earlier versions need the Fusion style or a manual
palette. A native Windows implementation of this same design - a ring that fills
with usage, accent then amber then red - reports two further requirements worth
noting: **per-monitor DPI awareness**, and **surviving Explorer restarts**, since
tray icons must re-register when `explorer.exe` comes back. Qt handles both, but
they are the kind of thing that is only discovered by a Windows user filing a
bug. Unsigned binaries also trip SmartScreen on first run.

---

## 7. HiDPI

- **Plasma on X11**: Syncthing Tray reports "High-DPI scaling of Plasmoid is
  broken under X11", worked around with `PLASMA_USE_QT_SCALING=1`. Only relevant
  if we build a Plasmoid.
- **Windows**: PMv2 scaling works as of Qt 6.
- **Us**: `IconRenderer` publishes a fixed ladder of sizes and never looks at the
  device pixel ratio. On a 2× display a panel asking for 22 logical pixels wants
  44 physical; we publish 48, so Qt scales down from a real pixmap rather than up
  from a small one. Probably fine, untested, and cheap to check on a scaled
  output.

---

## What does not transfer

dorkbox/SystemTray's catalogue is invaluable but half of it is Java: JavaFX and
GTK3 conflicts, `libappindicator1` on Ubuntu, Java 8's lack of DPI awareness.
Its *desktop* observations are what matter, and they are in §1. Its matrix also
notes WSL and ChromeOS as unsupported outright — worth knowing if anyone asks.

gatus-monitor is Go and Fyne. Only its release matrix is of interest.

[st]: https://github.com/Martchus/syncthingtray
[dorkbox]: https://github.com/dorkbox/SystemTray
[gogpu]: https://github.com/gogpu/systray
[gatus]: https://github.com/kartoza/gatus-monitor
[qtdoc]: https://doc.qt.io/qt-6/qsystemtrayicon.html
