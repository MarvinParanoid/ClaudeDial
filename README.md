# ClaudeDial

[![CI](https://img.shields.io/github/actions/workflow/status/MarvinParanoid/ClaudeDial/ci.yml?branch=main&label=CI)](https://github.com/MarvinParanoid/ClaudeDial/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/MarvinParanoid/ClaudeDial?label=release)](https://github.com/MarvinParanoid/ClaudeDial/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
![Platforms](https://img.shields.io/badge/platforms-Linux%20%7C%20Windows-lightgrey)

**Claude Code usage at a glance.**

A minimal system tray indicator for Claude Code usage limits. Glance at the
icon, hover for details, click for a small popup, forget about it.

Built for KDE Plasma first. Also runs on other Linux desktops and on Windows -
[Desktop support](#desktop-support) says how far each one is taken.

<img src="docs/images/tray-styles.png" alt="The tray icon at 8, 23, 75, 88, 99 and 100 percent, in both styles" width="336">

That is the tray icon at 8%, 23%, 75%, 88%, 99% and at the limit - the exact
figure on top, the needle below. Same dial, two styles. Click it:

<img src="docs/images/popup-dark.png" alt="The ClaudeDial popup" width="340">

It is not an analytics dashboard, and is not going to become one. No graphs, no
token history, no accounts, no telemetry, no cloud backend.

## Please read this before installing


Two things you are entitled to know up front.

**1. ClaudeDial reads Claude Code's credential file.** It needs the OAuth access
token Claude Code stores at `~/.claude/.credentials.json` in order to ask
Anthropic what your usage is. It opens that file read-only, holds the token in
exactly one class, sends it only to `api.anthropic.com` over HTTPS - an address
hard-coded at build time, not configurable - and never logs it,
copies it, exposes it to the UI layer, or writes it anywhere.

It also **never refreshes the token**, which is a deliberate limitation rather
than an oversight: Anthropic rotates the refresh token on use, so refreshing
here would race Claude Code and could sign you out of it. Instead, ClaudeDial
watches the file and picks up whatever Claude Code refreshes. In practice, if
you use Claude Code daily the token stays alive for free; if you have not run it
in a while, ClaudeDial shows you stale data and says so.

**2. The usage endpoint is undocumented.** ClaudeDial reads
`GET api.anthropic.com/api/oauth/usage`, which is not part of Anthropic's public
API and is annotated *"Experimental - the response shape may change"* by Claude
Code's own internal schema. It can change or disappear without notice. When it
does, ClaudeDial degrades to showing stale data rather than crashing or lying.

ClaudeDial is an unofficial tool. It is not endorsed by or affiliated with
Anthropic.

Full write-up, including the exact request, the response shape, and the security
rules the code follows: [docs/usage-api.md](docs/usage-api.md). How the code is
put together: [docs/architecture.md](docs/architecture.md).

## Installation

Three artefacts per release, plus a source build.

### AppImage

Carries Qt and OpenSSL, so it needs nothing from your distribution but FUSE.

```console
$ chmod +x ClaudeDial-x86_64.AppImage
$ ./ClaudeDial-x86_64.AppImage
```

If that fails with `No suitable fusermount binary found on the $PATH`, install
FUSE (`fuse3` on Debian) or skip the mount, which needs nothing at all:
`./ClaudeDial-x86_64.AppImage --appimage-extract-and-run`.

Take v0.1.2 or later: the v0.1.0 and v0.1.1 AppImages shipped without the Qt
Wayland plugin, so their popup never appears.

### Tarball

`claudedial-<version>-linux-x86_64.tar.gz` is a hundred times smaller because
it links your distribution's Qt. Unpack it over `/usr` or `~/.local`.

**It is not standalone** - it needs Qt 6.8 or newer installed, including
QtQuick Controls. Take the AppImage if you would rather not care;
[packaging/README.md](packaging/README.md) lists what to install if you would.

### Arch Linux

Not in the AUR yet: registration there is paused while they handle a wave of
automated signups. The package is ready, so build it from this repository:

```console
$ curl -fLo PKGBUILD https://raw.githubusercontent.com/MarvinParanoid/ClaudeDial/main/packaging/PKGBUILD-bin
$ makepkg -si
```

That is the AUR package unchanged. It unpacks the tarball rather than
compiling, so it takes seconds, and pacman owns the files afterwards -
`pacman -R claudedial-bin` removes it cleanly. Repeat the two commands for a
new version until `yay -S claudedial-bin` starts working.

### Windows

`ClaudeDial-windows-x86_64.zip` ships with each release from v0.1.6. Run
`bin\claudedial.exe`; Qt sits beside it and nothing needs installing. Flags
work from a terminal - the binary borrows the console it was launched from.
Between releases the same zip is on every green [Actions
run](https://github.com/MarvinParanoid/ClaudeDial/actions), under
**Artifacts**.

It reads the credentials Claude Code writes, at
`%USERPROFILE%\.claude\.credentials.json`. **If your Claude Code runs inside
WSL those are in the WSL filesystem** and a native build will not find them;
point `CLAUDE_CONFIG_DIR` at the WSL path or set `CLAUDE_CODE_OAUTH_TOKEN`.

The tray icon looks different here by design: Windows asks for a 16-pixel icon,
which is where the mark switches to its small-size form. See **Tray style**.

### From source

See [Building](#building).

## Status bar integration


ClaudeDial is also a one-shot CLI, so Waybar, Polybar, i3blocks, Conky and
plain scripts can use it without the tray.

```console
$ claudedial --json
{
    "five_hour": { "usage": 10, "reset_at": "2026-09-03T21:30:00Z" },
    "seven_day": { "usage": 27, "reset_at": "2026-09-08T04:00:00Z" },
    "updated_at": "2026-09-03T18:17:54Z",
    "stale": false,
    "text": "10%",
    "tooltip": "ClaudeDial\n5h  10% · resets in 3h 12m\n7d  27% · resets in 4d 9h",
    "class": "normal"
}
```

`text`, `tooltip` and `class` are what Waybar's `return-type: json` expects, so a
module needs no wrapper script:

```jsonc
"custom/claude": {
    "exec": "claudedial --json",
    "return-type": "json",
    "interval": 300,
    "on-click": "claudedial"
}
```

`class` is one of `normal`, `warning`, `critical`, `severe`, `limit`, `stale`,
`unavailable` - style them in your Waybar CSS. The output carries percentages
and timestamps only: no token, no organisation id, nothing identifying, because
status-bar configs end up in public dotfiles repos.

`claudedial --once` prints the same thing for humans. Both run without a
display, so they work over SSH and from a startup script before the compositor
is up.

**Polling `--json` is free while the tray is running.** The rate-limit bucket is
per access token, so a status bar on its own timer and the tray on its would
otherwise consume it twice over. A one-shot invocation asks the running instance
for its last result over a local socket and only reaches for the network when
nothing is running - so a Waybar `interval` costs nothing, and returns instantly.

## Desktop support

**ClaudeDial is a KDE Plasma application that works elsewhere.** Everything it
is made of - a permanently visible tray icon, a small popup on click, settings,
notifications, autostart - is what Plasma is built around, so that is where it
is taken to the pixel. The rest is deliberate portability rather than equal
ambition, and saying so is more useful than implying every desktop gets the
same care.

| Desktop | Level | What that means |
| --- | --- | --- |
| **KDE Plasma / Wayland** | primary | Polished and tested. Appearance is settled here, and this is the desktop a visual decision is made on. |
| **GNOME / Wayland** + [AppIndicator][appind] | supported | Must work, without bugs, and look reasonable. Verified: tray, popup, notifications. A left click opens the menu, so use **Show usage** or double-click, and AppIndicator has no tooltips at all. |
| **i3 / X11** with a tray | supported, best effort | Tray, popup, settings and CLI must work. i3bar asks for a 15x15 icon, where the mark switches to its small-size form - legible rather than faithful. Verified on Debian 13. |
| Xfce, Cinnamon, MATE, LXQt, COSMIC | expected to work | Same StatusNotifierItem path as Plasma. Nobody has run it; the protocol says it should. |
| **No tray at all** - Sway, Hyprland, a bare session | `claudedial --json` | Not a fallback but the interface for that user. Stable, and free while the tray runs. |
| **Windows** | works, lightly tested | Run on a real desktop: the tray icon appears, the small-size mark renders, and the CLI works from a terminal. Notifications, autostart and reading real credentials have not been exercised there. Personal-use scaffolding rather than a supported target. |
| macOS | not attempted | Compiles and packages, untried. The token is reported to live in the login Keychain there, which nothing here has verified - see [usage-api.md](docs/usage-api.md) for how to check and why it decides the difficulty. |

The operative rule, when two of those conflict: **Plasma is not made worse to
make another desktop better.** Where a choice cannot serve both, Plasma gets the
precise answer and the others get one that is safe - which is exactly how the
tray icon's colour works, since Plasma is the only desktop that writes down what
its panel looks like.

[docs/platform-support.md](docs/platform-support.md) is the long version: what
is guaranteed, what is attempted, what is declined, and the measurements behind
each. Note that the tiers there are about *features* - the CLI, then the tray,
then the popup - which is a different axis from this table.

[appind]: https://github.com/ubuntu/gnome-shell-extension-appindicator

The tray uses StatusNotifierItem over D-Bus, via `QSystemTrayIcon` - which means
it works on Plasma out of the box, and on GNOME with the AppIndicator extension.
Notifications use `org.freedesktop.Notifications`. Autostart is a normal XDG
entry in `~/.config/autostart`. Refresh-on-resume uses logind's `PrepareForSleep`.

Two Wayland limitations ClaudeDial cannot work around, because a client there
cannot place its own windows:

- **The compositor chooses where the popup appears** - typically the middle of
  the screen, not next to the icon. **Drag it by its header** to move it; on
  Plasma a window rule can make that stick.
- **The popup may not receive keyboard focus**, and then it will not dismiss
  itself when you click elsewhere. Clicking the tray icon again, or the close
  button, always works.

[platform-support.md](docs/platform-support.md) has the window rule, and why
neither of these is a bug we can fix.

## Settings

<img src="docs/images/settings.png" alt="The ClaudeDial settings window" width="381">

Right-click the tray icon for the two readings and then **Show usage**,
**Refresh now**, **Settings** and **Quit**, or use the gear in the popup. The
numbers are in the menu because a tooltip cannot be relied on - AppIndicator,
which is how GNOME has a tray at all, supports none. On GNOME a left click
opens that menu instead of the popup, so **Show usage** is the way in there; on
Plasma a single click toggles it.

Configurable: start on login, refresh interval, notifications, warning and
critical thresholds, tray style, tray icon tone, and theme. Settings live in
`~/.config/claudedial/claudedial.conf`, and on Windows under
`HKEY_CURRENT_USER\Software\claudedial` - the `[state]` group in the file is
ClaudeDial's own bookkeeping, not settings.

**Tray style** picks what goes inside the arc: **Percentage** (default) shows
the exact 5-hour figure, `!` at 100%; **Gauge** shows the needle. The arc is
the same in both and fills with usage either way. Below 17 px the mark changes
shape, because the dial with a number inside it is not legible that small -
chosen by the size the panel asks for, never by the desktop.

**Tray icon** sets how light or dark the mark is while usage is below the
warning threshold. **Auto** reads the panel's colour where Plasma declares one,
which covers Breeze Twilight, and follows your application colours elsewhere;
pick **Light** for a dark panel or **Dark** for a light one when it guesses
wrong. Above the warning threshold the icon is amber, orange or red regardless.
When the data is stale the whole mark fades.

Under the 5-hour row sits one more line, `Usage 63% · window 60%`. A percentage
alone says how much is spent but not whether that is a lot: 63% with four hours
to run is heavy, 63% with forty minutes left is fine. Two numbers side by side,
no projection and no verdict. Experimental, and only for the 5-hour window.

<img src="docs/images/popup-light.png" alt="The popup in the light theme" width="340">

Why any of it looks the way it does - the colour roles, the ramp, the tray
icon's proportions, and what was tried and rejected:
[docs/design.md](docs/design.md). Where each desktop lands and the measurements
behind it: [docs/platform-support.md](docs/platform-support.md).

## Notifications


<img src="docs/images/notification.png" alt="A ClaudeDial notification" width="343">

Once per window at 75%, 90%, 95% and 100%, reset when the window does - and not
re-announced after a restart. The title says how bad it is, the first line says
which threshold was crossed, and the second gives the reset time, which is the
part you do not already have from the title and the icon. (The icon shows the
*current* figure, so it can read 96 on a banner about crossing 95%.)

The icon travels with the notification as pixels rather than as an icon name.
A name only resolves once ClaudeDial is installed into an icon theme, and until
then the daemon draws a blank document, which makes the banner look broken.

## Refresh behaviour


Every 5 minutes by default, and on start, on manual refresh, when Claude Code
refreshes its token, and after waking from suspend. The interval floor is 60
seconds on purpose: the rate-limit bucket is per access token and shared with
any other usage monitor you run, and this endpoint does return 429 if you push
it. A failure never clears good data - the last known usage stays on screen,
marked stale.

## If TLS fails from the AppImage


The AppImage bundles OpenSSL, because distributions ship incompatible versions
of it and ClaudeDial makes one HTTPS request. On a host whose own OpenSSL
configuration disagrees with the bundled library, that can break instead of
help; the escape hatch is to neutralise the host configuration for this process:

```console
$ OPENSSL_CONF= ./ClaudeDial-x86_64.AppImage
```

## Building


Needs Qt 6.8+, CMake 3.21+ and a C++20 compiler. No other runtime dependencies -
no Electron, no webview, no Python, no Node.

6.8 rather than something older because `QStyleHints::setColorScheme()`, which
is the whole Light/Dark setting, arrived there; `QPalette::Accent`, which is how
the settings window follows your desktop's accent colour, arrived in 6.6. Qt Svg
is needed for the application icon.

```console
$ cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
$ cmake --build build
$ ctest --test-dir build
$ sudo cmake --install build
```

On Arch: `pacman -S qt6-base qt6-declarative qt6-svg cmake ninja`.

## Development

`--demo` runs the whole application on invented numbers - no network request,
no credentials read, so it works on a machine that has never seen Claude Code:

```console
$ claudedial --demo            # tray, popup, settings, notifications
$ claudedial --demo 96,41      # one or two percentages, five-hour first
$ claudedial --demo --json
```

It takes a single-instance lock of its own, so it starts a second tray icon
beside a live ClaudeDial rather than poking that one. That makes it the way to
check a package on another distribution or desktop, and the only way to reach
the top of the scale: the 95% and 100% steps cannot otherwise be exercised
short of spending a limit. Simulated numbers are never recorded as announced,
so a run at 100% will not silence the real notification later.

`CLAUDEDIAL_SIMULATE=96,41` is the same mechanism as an environment variable.
The screenshots in this README were taken with it.

## License


MIT. See [LICENSE](LICENSE).
