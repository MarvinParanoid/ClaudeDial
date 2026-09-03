# Claudometer

**Claude Code usage at a glance.**

A minimal system tray indicator for Claude Code usage limits, for Linux.
Glance at the icon, hover for details, click for a small popup, forget about it.

<img src="docs/images/tray-styles.png" alt="The tray icon at 8, 23, 75, 88, 99 and 100 percent, in both styles" width="368">

That is the tray icon at 8%, 23%, 75%, 88%, 99% and at the limit - the exact
figure on top, the needle below. Same arc, two readings. Click it:

<img src="docs/images/popup-dark.png" alt="The Claudometer popup" width="340">

It is not an analytics dashboard, and is not going to become one. No graphs, no
token history, no accounts, no telemetry, no cloud backend.

## Please read this before installing

Two things you are entitled to know up front.

**1. Claudometer reads Claude Code's credential file.** It needs the OAuth access
token Claude Code stores at `~/.claude/.credentials.json` in order to ask
Anthropic what your usage is. It opens that file read-only, holds the token in
exactly one class, sends it to exactly one pinned HTTPS host, and never logs it,
copies it, exposes it to the UI layer, or writes it anywhere.

It also **never refreshes the token**, which is a deliberate limitation rather
than an oversight: Anthropic rotates the refresh token on use, so refreshing
here would race Claude Code and could sign you out of it. Instead, Claudometer
watches the file and picks up whatever Claude Code refreshes. In practice, if
you use Claude Code daily the token stays alive for free; if you have not run it
in a while, Claudometer shows you stale data and says so.

**2. The usage endpoint is undocumented.** Claudometer reads
`GET api.anthropic.com/api/oauth/usage`, which is not part of Anthropic's public
API and is annotated *"Experimental - the response shape may change"* by Claude
Code's own internal schema. It can change or disappear without notice. When it
does, Claudometer degrades to showing stale data rather than crashing or lying.

Claudometer is an unofficial tool. It is not endorsed by or affiliated with
Anthropic.

Full write-up, including the exact request, the response shape, and the security
rules the code follows: [docs/usage-api.md](docs/usage-api.md). How the code is
put together: [docs/architecture.md](docs/architecture.md).

## Status bar integration

Claudometer is also a one-shot CLI, so Waybar, Polybar, i3blocks, Conky and
plain scripts can use it without the tray.

```console
$ claudometer --json
{
    "five_hour": { "usage": 10, "reset_at": "2026-09-03T21:30:00Z" },
    "seven_day": { "usage": 27, "reset_at": "2026-09-08T04:00:00Z" },
    "updated_at": "2026-09-03T18:17:54Z",
    "stale": false,
    "text": "10%",
    "tooltip": "Claudometer\n5h  10% · resets in 3h 12m\n7d  27% · resets Tue 06:00",
    "class": "normal"
}
```

`text`, `tooltip` and `class` are what Waybar's `return-type: json` expects, so a
module needs no wrapper script:

```jsonc
"custom/claude": {
    "exec": "claudometer --json",
    "return-type": "json",
    "interval": 300,
    "on-click": "claudometer"
}
```

`class` is one of `normal`, `warning`, `critical`, `severe`, `limit`, `stale`,
`unavailable` - style them in your Waybar CSS. The output carries percentages
and timestamps only: no token, no organisation id, nothing identifying, because
status-bar configs end up in public dotfiles repos.

`claudometer --once` prints the same thing for humans. Both run without a
display, so they work over SSH and from a startup script before the compositor
is up.

**Polling `--json` is free while the tray is running.** The rate-limit bucket is
per access token, so a status bar on its own timer and the tray on its would
otherwise consume it twice over. A one-shot invocation asks the running instance
for its last result over a local socket and only reaches for the network when
nothing is running - so a Waybar `interval` costs nothing, and returns instantly.

## Development

`CLAUDOMETER_SIMULATE` reports a usage figure instead of asking the server:

```console
$ CLAUDOMETER_SIMULATE=99 claudometer          # tray, popup and notifications
$ CLAUDOMETER_SIMULATE=63,41 claudometer --json
```

One or two percentages, five-hour first. It makes no request, so it works
offline and costs no quota, and it is the only way to reach the top of the scale:
the 95% and 100% steps are fixed, so without it their colours, the `!` glyph and
their notifications cannot be exercised short of actually spending a limit.
Simulated numbers are never recorded as announced, so a run at 100% will not
silence the real notification later.

Screenshots in this README were taken with it.

## Building

Needs Qt 6.5+, CMake 3.21+ and a C++20 compiler. No other runtime dependencies -
no Electron, no webview, no Python, no Node.

```console
$ cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
$ cmake --build build
$ ctest --test-dir build
$ sudo cmake --install build
```

On Arch: `pacman -S qt6-base qt6-declarative cmake ninja`.

## Desktop support

Developed on Arch with KDE Plasma on Wayland; built to work beyond it.

The tray uses StatusNotifierItem over D-Bus, via `QSystemTrayIcon` - which means
it works on Plasma out of the box, and on GNOME with the AppIndicator extension.
Notifications use `org.freedesktop.Notifications`. Autostart is a normal XDG
entry in `~/.config/autostart`. Refresh-on-resume uses logind's `PrepareForSleep`.

Two known Wayland limitations, neither of which Claudometer can fix from inside
the sandbox:

- **The popup's position is chosen by the compositor.** A Wayland client cannot
  place its own top-level windows, so `setPosition()` is ignored and the popup
  appears wherever the compositor decides - typically near the middle of the
  screen. Claudometer asks the panel where its icon is and anchors to it when it
  gets an answer, but StatusNotifierItem hosts generally do not report icon
  geometry; Plasma does not. (The anchoring path is exercised only by a legacy
  XEmbed tray on X11, and is untested.)

  **Drag the popup by its header** to move it - that goes through the compositor
  and works on Wayland. On Plasma you can make the position stick with a window
  rule: System Settings → Window Management → Window Rules, match window class
  `claudometer` and window title `Claudometer`, then set Position to force the
  corner you want. Match the title as well as the class, or the rule will also
  catch the settings window.
- **The popup may not receive keyboard focus**, in which case it will not
  dismiss itself when you click elsewhere. Clicking the tray icon again, or the
  close button, always works.

## Settings

<img src="docs/images/settings.png" alt="The Claudometer settings window" width="381">

Right-click the tray icon, or use the gear in the popup. Start on login, refresh
interval, notifications, warning and critical thresholds, tray style, and theme
(system / light / dark). Stored in `~/.config/claudometer/claudometer.conf`;
the `[state]` group in that file is Claudometer's own bookkeeping, not settings.

**Tray style** picks what goes inside the Claudometer arc:

- **Percentage** (default) - the exact 5-hour figure. Answers the question with
  no hover and nothing to interpret, which is what a tray indicator is for.
  At 100% it shows `!`: three digits are not legible at 16 px, and "at the
  limit" is better said than counted.
- **Gauge** - the needle. The nicer mark, and a reading you take in at a glance
  as little / half / nearly all, with the exact figure a hover away.

The arc is identical in both, and fills with usage either way, so they are two
variants of one mark rather than two icons. The popup header always shows the
needle, as the constant logotype. A full ring was tried first and read as a
notification badge.

At 100% the icon shows `!`. Three digits were tried, shrunk to fit, and in a real
panel they came out weaker than `99` - the one reading that most needs to carry.
A full red arc around an exclamation mark says it more firmly, and the tooltip
still gives the figure.

When the data is stale the whole mark fades, so a glance at the panel tells you
the number is the last one Claudometer managed to fetch.

Colour steps at the warning threshold, the critical threshold, 95% and 100% -
the same points the notifications fire on, so what you see and what you are told
agree. The icon stays monochrome below the warning threshold: a panel icon is on
screen permanently and should not be a standing splash of colour.

Both themes, and both are the popup's own colours rather than the desktop
palette - which is what lets the Light and Dark settings work on a desktop whose
platform theme declines to switch:

<img src="docs/images/popup-light.png" alt="The popup in the light theme" width="340">

The tray icon stays monochrome until the warning threshold, then goes amber, then
red - a panel icon is on screen permanently and should not be a standing splash
of colour. The popup uses a longer ramp with an accent step, because it is only
visible while you are reading it.

Notification thresholds fire once per window at 75%, 90%, 95% and 100%, and
reset when the window does.

## Refresh behaviour

Every 5 minutes by default, and on start, on manual refresh, when Claude Code
refreshes its token, and after waking from suspend. The interval floor is 60
seconds on purpose: the rate-limit bucket is per access token and shared with
any other usage monitor you run, and this endpoint does return 429 if you push
it. A failure never clears good data - the last known usage stays on screen,
marked stale.

## License

MIT. See [LICENSE](LICENSE).
