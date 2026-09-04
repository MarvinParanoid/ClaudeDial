# Claude Code usage data — technical report

**Status:** research complete. A reliable, read-only path to the required data exists.
**Date of investigation:** 2026-09-03
**Investigated on:** Arch Linux, KDE Plasma (Wayland), Claude Code `2.1.255` / VS Code extension `2.1.259`

> **Summary:** all four values Claudometer needs (5-hour %, 5-hour reset, 7-day %, 7-day reset)
> are available from a single authenticated `GET` request to an **undocumented** Anthropic
> endpoint, using the OAuth access token Claude Code already stores on disk.
> No unsafe credential handling is required, **provided Claudometer never refreshes the token.**
> That constraint is the single most important design decision in this document — see
> [Token lifetime](#token-lifetime-the-main-limitation).

---

## 1. Where Claude Code stores credentials

### Primary location (Linux)

```
$CLAUDE_CONFIG_DIR/.credentials.json     # when CLAUDE_CONFIG_DIR is set
~/.claude/.credentials.json              # default
```

Mode `0600`, owned by the user. Verified present on the test machine.

Structure (**all secret values replaced with their string lengths — never printed verbatim**):

```json
{
  "claudeAiOauth": {
    "accessToken":            "<str len=108>",
    "refreshToken":           "<str len=108>",
    "expiresAt":              1788480781215,
    "refreshTokenExpiresAt":  1790783201215,
    "scopes": [
      "user:file_upload", "user:inference", "user:mcp_servers",
      "user:profile", "user:sessions:claude_code"
    ],
    "subscriptionType": "team",
    "rateLimitTier":    "default_claude_max_5x"
  },
  "organizationUuid": "<uuid>"
}
```

`expiresAt` / `refreshTokenExpiresAt` are **milliseconds** since the Unix epoch.

The `user:profile` scope is what gates the usage endpoint. Sessions authenticated with an
API key (`ANTHROPIC_API_KEY`) or via Bedrock/Vertex have **no** plan rate limits and this
file will be absent or lack `claudeAiOauth`. Claude Code models this internally as
`rate_limits_available: false`.

### Alternative sources, in the order Claudometer should try them

1. `$CLAUDE_CODE_OAUTH_TOKEN` — environment variable. Claude Code honours it
   (129 references in the binary); a bare access token, no refresh token.
2. `$CLAUDE_CONFIG_DIR/.credentials.json`
3. `~/.claude/.credentials.json`

### Not applicable on Linux

- **macOS** stores this in the login Keychain under service `Claude Code-credentials`.
  That string does **not** appear in the Linux binary. Corroborated by a shipping
  application: [leonardocouy/claudometer][lc] reads
  `~/.claude/.credentials.json` on Linux and the system Keychain on macOS.
- `libsecret` appears twice in the binary but is not used for these credentials on Linux —
  Claude Code writes the plain `0600` JSON file. Claudometer therefore does not need a
  keyring dependency.

### Can these credentials safely be reused?

**Yes, for reading — with one hard rule: do not refresh, do not write.**

- Reading the file is a read of a file the user owns, in-process, by a tool the user
  installed. This is the same thing every existing monitor does.
- **Refreshing is not safe.** The refresh endpoint is
  `https://platform.claude.com/v1/oauth/token`. Anthropic rotates the refresh token on
  use, so a refresh performed by Claudometer would (a) require writing
  `.credentials.json`, and (b) race Claude Code doing the same thing. Losing that race
  invalidates the stored refresh token and **logs the user out of Claude Code.**
  Claudometer must never do this.

---

## 2. The endpoint

```
GET https://api.anthropic.com/api/oauth/usage
```

Two variants exist in the Claude Code binary:

| Request | Used by Claude Code for |
| --- | --- |
| `/api/oauth/usage` | normal refresh |
| `/api/oauth/usage?at_wall=1&skip_spend=1` | when a request was just rejected for hitting a limit; skips the billing/spend lookup |

`skip_spend=1` returns a smaller payload. Claudometer does not need spend data, so
**`?skip_spend=1` is the better default** — less work for the server, less data we hold.

The corresponding call site in Claude Code (`fetchUtilization`, deobfuscated names):

```js
const path = atWall ? "/api/oauth/usage?at_wall=1&skip_spend=1" : "/api/oauth/usage";
const res  = await http.get(path, {
  timeout: 5000,
  headers: { "Content-Type": "application/json" },
  refreshOAuth: true,          // retries once after refreshing on 401
  credentials,
});
```

Note the **5 second timeout** and the single 401→refresh→retry. Claudometer keeps the
timeout and drops the refresh.

### Authentication

```http
GET /api/oauth/usage?skip_spend=1 HTTP/1.1
Host: api.anthropic.com
Authorization: Bearer <accessToken from .credentials.json>
Content-Type: application/json
anthropic-beta: oauth-2025-04-20
User-Agent: claudometer/0.1 (+https://github.com/<owner>/claudometer)
```

Bearer token OAuth. No signing, no other secret.

- `anthropic-beta: oauth-2025-04-20` is the beta flag Claude Code uses for its
  OAuth-scoped API surface. Community tools send it. **It was not required** in testing.
- `anthropic-version: 2023-06-01` was sent during testing and accepted; it is not
  referenced for this endpoint and appears unnecessary.

#### The User-Agent question — read this before choosing one

Community reports ([Claude-Code-Usage-Monitor#202][issue202]) state that a
`User-Agent: claude-code/<version>` is **mandatory** and that requests without it get
aggressive, persistent `429`s.

**This was not reproduced.** The verification request in §3 was sent with
`User-Agent: claudometer-research/0.1` and returned `200` immediately.

Interpretation: there is likely UA-keyed rate-limit bucketing rather than an allowlist.
An unrecognised UA plausibly gets a smaller bucket, which would look exactly like
"mandatory" to a tool that polls quickly.

**Recommendation: send an honest `claudometer/<version>` UA and poll conservatively.**
Do not impersonate `claude-code/<version>`. Impersonation makes Claudometer
indistinguishable from the official client in Anthropic's telemetry, which is both
dishonest and actively harmful — it would pollute the signal Anthropic uses to decide
whether this endpoint is being abused, and it is the fastest way to get the endpoint
locked down for everyone. If honest-UA rate limits turn out to be too tight in practice,
the correct response is to poll less often, not to lie.

### Rate limiting

**Observed during this work:** roughly ten requests within ten minutes - the
verification request in §3, plus repeated `claudometer --once` / `--json` runs
while testing - produced a `429`. That is a burst, not a poll, but it establishes
that the limit is real and not generous, and that it applies to an honest
`claudometer/*` User-Agent.

- Buckets are **per access token**, not per account. Two monitors on one machine
  share one bucket.
- Community guidance: 180 s between polls is comfortably safe.
- Claudometer's 5-minute default is well inside that. **Do not offer an interval
  below 60 s**, and expect `--json` invocations from a status bar to count
  against the same bucket as the tray.
- On `429`: **honour `Retry-After` when the server sends it**, and fall back to
  our own ladder (3 → 6 → 12 → 15 min cap) when it does not. Guessing when the
  server has told you is both less correct and less polite. Both forms the
  specification allows are handled - a delay in seconds and an HTTP date - and
  the value is capped at an hour, so a header asking for three days cannot
  freeze the indicator until the user restarts it.
- Keep last known state, do not notify.

That the endpoint is unofficial is not only our reading of it. A Windows
implementation of the same idea warns its users in the same terms: "Both usage
endpoints are **unofficial**. Anthropic/OpenAI can change them any time, at
which point the flyout will tell you it can't parse the response until this app
is updated." Independent arrival at the same disclosure, and at the same
lenient-parsing consequence.

---

## 3. Verified request / response

Live request made once during research, read-only, with no token refresh.
Organisation and workspace identifiers redacted.

**Response headers of interest**

```
anthropic-organization-id: <uuid>
anthropic-workspace-id: wrkspc_<redacted>
```

**Body** (`HTTP 200`)

```json
{
  "five_hour": {
    "utilization": 6.0,
    "resets_at": "2026-09-03T21:30:00.382338+00:00",
    "limit_dollars": null,
    "used_dollars": null,
    "remaining_dollars": null,
    "locked_reason": null
  },
  "seven_day": {
    "utilization": 26.0,
    "resets_at": "2026-09-08T04:00:00.382358+00:00",
    "limit_dollars": null,
    "used_dollars": null,
    "remaining_dollars": null,
    "locked_reason": null
  },
  "seven_day_oauth_apps": null,
  "seven_day_opus": null,
  "seven_day_sonnet": null,
  "seven_day_cowork": null,
  "nimbus_quill": { "utilization": 0.0, "resets_at": null, "...": null },
  "cinder_cove": null,
  "extra_usage": {
    "is_enabled": false,
    "monthly_limit": null,
    "used_credits": null,
    "utilization": null,
    "currency": null,
    "user_disabled": true,
    "spend_limit_reached": false
  },
  "limits": [
    { "kind": "session",       "group": "session", "percent": 6,  "severity": "normal",
      "resets_at": "2026-09-03T21:30:00.382338+00:00", "scope": null, "is_active": false },
    { "kind": "weekly_all",    "group": "weekly",  "percent": 26, "severity": "normal",
      "resets_at": "2026-09-08T04:00:00.382358+00:00", "scope": null, "is_active": true },
    { "kind": "weekly_scoped", "group": "weekly",  "percent": 0,  "severity": "normal",
      "resets_at": null,
      "scope": { "model": { "id": null, "display_name": "Fable" }, "surface": null },
      "is_active": false }
  ],
  "spend": { "used": { "amount_minor": 0, "currency": "USD", "exponent": 2 },
             "limit": null, "percent": 0, "enabled": false },
  "member_dashboard_available": false
}
```

Several top-level keys are evidently internal codenames for unreleased or per-account
quota buckets (`nimbus_quill`, `cinder_cove`, `tangelo`, `iguana_necktie`,
`amber_ladder`, `juniper_tide`, `seven_day_omelette`, `omelette_promotional`). They are
`null` for this account.

**Claudometer must ignore every key it does not explicitly understand.** New buckets
appear without warning; a strict parser would break on a Tuesday.

### Window object shape

Confirmed against Claude Code's own schema, which describes it as
*"Experimental — the response shape may change"*:

```js
{
  utilization: number | null,   // "Percentage of the window used, 0-100."
  resets_at:   string | null,   // "ISO 8601 timestamp when the window resets."
}
```

Both fields are nullable. `resets_at` carries fractional seconds and a `+00:00` offset —
`QDateTime::fromString(s, Qt::ISODateWithMs)` handles it.

### Known window keys

| Key | Meaning | Claudometer |
| --- | --- | --- |
| `five_hour` | rolling 5-hour session window | **primary** — drives the tray icon |
| `seven_day` | 7-day, all models | **primary** — second popup row |
| `seven_day_opus` | 7-day, Opus only | ignore for now |
| `seven_day_sonnet` | 7-day, Sonnet only | ignore for now |
| `seven_day_oauth_apps` | 7-day, third-party OAuth apps | ignore |
| `extra_usage` | paid overage credits | ignore |
| `limits[]` | same data, array form, with `severity` + `is_active` | ignore; redundant |
| `spend` | billing | ignore; suppressed by `skip_spend=1` |

`limits[]` is a parallel representation of the same numbers. Reading the named keys is
simpler and is the shape Claude Code's own public-facing schema exposes.

### Redundant source: response headers

Claude Code also reads `anthropic-ratelimit-unified-*` response headers from **ordinary
inference requests** (`status`, `resetsAt`, `utilization`, and a `unifiedWindows` object
with `five_hour` / `seven_day` / `seven_day_overage_included`). This is how it updates
usage without polling. Not usable by Claudometer — we do not make inference requests —
but it is confirmation that the two windows are the canonical, first-class pair.

---

## 4. Is there an official API?

**No.**

- `/api/oauth/usage` is not in Anthropic's published API documentation.
- Claude Code's own internal schema for this data is annotated
  *"Experimental — the response shape may change."*
- The `anthropic-beta: oauth-2025-04-20` flag marks the whole OAuth surface as beta.
- The documented, supported Anthropic APIs (`/v1/messages` etc.) are for API-key billing
  and expose no subscription-plan window state at all.

There is one semi-supported alternative, described in §5.

### Consequences to state plainly in Claudometer's README

1. The endpoint can change shape or disappear in any Anthropic deployment, with no notice
   and no deprecation window.
2. Response fields are nullable and the key set grows.
3. Claudometer is an unofficial tool. It is not endorsed by or affiliated with Anthropic.
4. It reads credentials belonging to another application. That must be stated up front,
   not buried.

---

## 5. Alternative data sources considered

### (a) Claude Code SDK control protocol — `get_usage`

Claude Code exposes a `{"subtype": "get_usage"}` control request returning a **normalised**
structure: session cost totals, `subscription_type`, `rate_limits_available`, and
`rate_limits: { five_hour, seven_day, seven_day_opus, seven_day_sonnet, model_scoped[], extra_usage }`
where each window is the `{utilization, resets_at}` pair above.

- **Upside:** the most stable contract available. Handles auth, refresh and endpoint
  changes internally. Never touches raw credentials.
- **Downside, fatal for this project:** requires spawning the 217 MB `claude` binary per
  poll. That contradicts the "tiny native utility, no Node runtime" requirement, and costs
  more RAM and startup time than the entire rest of the application.

**Rejected as the primary mechanism.** Worth revisiting only if the HTTP endpoint breaks
and the user already has `claude` on `PATH`.

### (b) Claude Code's status-line hook — a *documented* source

Found by surveying more of the field (see §6). Claude Code renders its status
line by running a command from `~/.claude/settings.json` and handing it a JSON
payload on stdin. That payload carries the same two windows:

```json
"rate_limits": {
  "five_hour": { "used_percentage": 0-100, "resets_at": 1788480781 },
  "seven_day": { "used_percentage": 0-100, "resets_at": 1788480781 },
  "spend_limit": { "used_percentage": 0-100, "resets_at": 1788480781 }
}
```

`resets_at` is Unix epoch **seconds** here, not the ISO 8601 string the HTTP
endpoint returns. The payload is documented inside the Claude Code binary
itself, which says the block is "Only present for subscribers, or behind a
gateway that sets a spend limit for you, after first API response, while at
least one window is present", and that each window is "present only while the
API reports it and its resets_at has not passed".

Wiring, per the settings schema:

```json
"statusLine": { "type": "command", "command": "...", "refreshInterval": 30 }
```

`refreshInterval` re-runs the command every N seconds in addition to the
event-driven update after each turn.

**This changes the risk picture, and it is worth being clear about why.**
Everything in §2–§4 rests on an endpoint Anthropic does not document, and that
Claude Code's own schema calls experimental. This payload *is* documented, by
the tool that produces it. It also needs no credentials at all — the single
largest security concern in this document simply does not arise — and it costs
no API request, so it cannot consume the rate-limit bucket or earn a 429.

The costs are equally real:

- **It requires editing the user's `~/.claude/settings.json`.** If they already
  have a status line, Claudometer would have to chain into it without breaking
  it. That is the user's file, and the restraint that keeps us out of
  `.credentials.json` applies to writing this one for them too.
- **It only updates while Claude Code is running.** The HTTP endpoint answers on
  demand; this answers when a turn happens. Someone who has not run Claude Code
  since this morning gets this morning's numbers.
- **It sees only Claude Code.** claude.ai and the Desktop app spend the same
  limits, and this payload will not know about them.
- Nothing arrives until the first API response of a session.

**Recommendation: keep the HTTP endpoint as the primary source, and offer this
as an opt-in credential-free mode later.** It is a genuinely better answer for
anyone unwilling to let a tray widget read another application's OAuth token,
and for an active user it would cut Claudometer's API traffic to nothing. It is
not a replacement, because "current on demand" is the whole point of a tray
indicator. Two data paths are only worth keeping honest once the first one hurts.

If it is built: throttle the writes (cctop uses ~30 s across all sessions), write
atomically, and never emit anything on stdout — the tap must not be able to
disturb the user's status line.


### (c) `~/.config/Claude/plan-usage-history.json` — Claude Desktop's local cache

Written by the Claude **Desktop** app (not Claude Code):

```json
{"version":2,"samples":[
  {"t":1788457650340,"org":"<uuid>","u":{"fh":4,"sd":26}}
]}
```

`t` = ms epoch, `fh` = five-hour %, `sd` = seven-day %. Sampled roughly every 15 min.
Cross-checked against the live API response in §3 (`fh` 4→6, `sd` 26) — **the numbers
agree.** Useful independent confirmation that `fh`/`sd` are the same two windows.

- **Upside:** zero authentication, zero network, integer percentages.
- **Downsides:** no reset timestamps (the whole "resets in 1h 52m" line is impossible);
  only updates while Claude Desktop is running; another app's private cache format;
  absent for users who do not install Desktop.

**Rejected.** Missing reset timestamps alone disqualifies it.

### (d) Reading local transcripts and estimating

What some monitors do: scan `~/.claude/projects/**/*.jsonl`, sum tokens, guess. Claude
Code itself does this only for its "what's contributing to usage" breakdown, never for
the percentages.

**Rejected.** Estimated, not authoritative; wrong across multiple machines; the weighting
is unpublished; a tray indicator that lies about how close you are to a limit is worse
than no tray indicator.

---

## 6. How existing open-source monitors do it

Surveyed, not copied. No code from any of these is reused.

| Project | Approach |
| --- | --- |
| [jens-duttke/usage-monitor-for-claude][jd] | Closest prior art: native Windows/Linux tray. `~/.claude/.credentials.json` (honours `CLAUDE_CONFIG_DIR`), token used in the `Authorization` header only, credentials + API isolated in one auditable file. Adaptive polling, aligned to quota resets, backs off on errors. |
| [Claude-Code-Usage-Monitor#202][issue202] | The most complete public write-up of the endpoint: headers, per-token rate-limit bucketing, 180 s cache TTL, 3→6→12→15 min backoff, `expiresAt` minus 60 s expiry buffer. Notes Linux paths were untested by the author. |
| [xikxp1/claude-monitor][xik] | Tray app; 5-hour / 7-day / Sonnet / Opus, threshold notifications, 1–30 min configurable refresh. |
| Bortlesboat/claude-usage-monitor | Tray + GUI + CLI, colour-coded icon, notifications at 80 % / 90 %. |
| [StaticB1/claude_ai_usage_widget][sb1] | Linux GTK tray widget + CLI. |
| [stefanprodan/cctop][cctop] | Where the status-line source in §5(b) came from. Captures `rate_limits` from the payload via a `--capture-usage` tap, persists it to `~/.claude/cctop/usage.json`, throttles writes to ~30 s, writes atomically, and marks snapshots older than an hour with their age. Reads nothing else — "only `~/.claude` and the process table". |
| [Maciek-roboblog/Claude-Code-Usage-Monitor][ccum] | Estimates from JSONL transcripts, *but* treats the official `rate_limits` from the status line as the source of truth when fresh, and labels every number `official`, `local_estimate`, `experimental` or `unknown`. |
| [minchenlee/c9watch][c9], [sverrirsig/claude-control][cc], [m1ckc3s/claude-status-bar][csb], [gmr/claude-status][cs] | Session state and cost, from transcripts, hooks and the process table. No quota data, no credentials. |
| [sotthang/so-agentbar][soa] | Reads official quota over OAuth. Its README names `/api/oauth/user`; **that endpoint does not exist** in Claude Code 2.1.255 — only `/api/oauth/usage` and `/api/oauth/profile` do. A slip, not a second endpoint. |

**Two conclusions from the wider survey.** Every tool that reports authoritative
percentages gets them from exactly one of two places: `GET /api/oauth/usage` with
the OAuth token, or the status-line payload. Nothing else exists. And the tools
that do estimate from transcripts say plainly that the official numbers win when
they have them — the same conclusion §5(d) reaches independently.

Claudometer is therefore doing nothing novel or riskier than the field. The one
thing the field has that we do not is the credential-free path.

Two patterns worth adopting: **isolating all credential and network code in one small,
auditable unit**, and **aligning polls to reset boundaries**.

One pattern worth explicitly rejecting: several of these tools auto-run `claude update` or
otherwise force a token refresh when the token expires. That is the credential-rotation
race described in §1. Claudometer will degrade visibly instead.

---

## 7. Limitations

### Token lifetime — the main limitation

Because Claudometer never refreshes, it is only as fresh as Claude Code's own token.

Observed on the test machine: access token had **~6.3 h** remaining, refresh token
**~26 days**. (The `#202` write-up claims ~60 min access-token lifetime; not what was
observed. Treat lifetime as unknown and always test `expiresAt` rather than assuming.)

Behaviour when `now >= expiresAt - 60s`:

1. Do **not** send the request. Do **not** refresh.
2. Keep and display the last known usage, marked stale.
3. Watch `.credentials.json` with `QFileSystemWatcher`; when Claude Code refreshes the
   token the file changes, and Claudometer re-reads and polls again immediately.
4. Only if the **refresh** token has also expired is this genuinely unrecoverable — then
   show "sign in with Claude Code" and stop polling.

In practice anyone using Claude Code daily keeps the token alive for free. Someone who
has not run it in a day opens Claudometer to stale data and a clear reason why. That is
an acceptable trade for never being able to log the user out.

### Other limitations

- **Undocumented and explicitly experimental.** May break without notice.
- **Unknown key set.** New quota buckets appear; parse leniently.
- **Nullable everywhere.** `utilization` and `resets_at` are both nullable even on a
  present window — hence `optional<>` in the core model.
- **Not applicable to every session type.** API key / Bedrock / Vertex users have no plan
  limits; detect and say so rather than showing 0 %.
- **`utilization` is a weighted percentage**, not tokens or dollars. Its exact composition
  is unpublished. Do not attempt to derive absolute token counts from it.
- **Quantisation.** Server sends floats (`6.0`), Desktop's cache stores integers. Round
  once, at the display layer.
- **Per-token rate limiting.** Multiple monitors on the same machine share the bucket.
- **`five_hour` is a rolling window.** `resets_at` moves as the window slides; it is not
  a fixed schedule. Recompute "resets in …" from the timestamp, never cache the duration.
- **`seven_day.resets_at` drifts between calls, by fractions of a second.**
  Observed: consecutive responses reporting the same weekly boundary as
  `03:59:59.6…` and `04:00:00.4…`. It is a rolling window too, so its reset moves
  as the oldest usage ages out. Consequence for the UI: an absolute time rendered
  by truncation flickers between two adjacent minutes on successive polls. **Round
  to the nearest minute before formatting** - Claudometer does this in
  `core::format::resetAbsolute`, with a regression test.
- **Team/Enterprise accounts** may report `member_dashboard_available` and org-level
  restrictions (`org_level_disabled_until`, `org_spend_cap_reached`) that change what
  limits mean. Out of scope; ignore.

---

## 8. Security implications and rules

The threat here is not an exotic exploit. It is a small utility carelessly turning a
`0600` file into a `0644` file, a log line, or a crash report attachment. Rules:

1. **Never log the token.** Not at any verbosity, not truncated, not "first 8 chars".
   Prefixes are still secret material and still identify the account.
2. **Never copy the token anywhere.** Not into `QSettings`, not into Claudometer's own
   config, not into a cache file, not into an environment variable for a child process.
   Read it, use it, drop it.
3. **Never expose it to QML.** QML is a scripting surface with introspection; a token in
   a `Q_PROPERTY` is a token in the QML debugger. Nothing under `core/Credentials` gets a
   `Q_INVOKABLE` or a property.
4. **Never write to `.credentials.json`.** Open read-only. No refresh, no repair, no
   "helpful" reformat. This is Claude Code's file.
5. **Keep the token in one class.** `core::Credentials` owns it. `UsageClient` asks it to
   authorise a `QNetworkRequest`; the token does not pass through any other type.
6. **Scrub before erroring.** `QNetworkReply::errorString()` and Qt's network debug
   logging can include request headers. Build error messages from status code and a fixed
   string set — never by interpolating the request.
7. **Disable Qt network logging in release.** Ship with `QT_LOGGING_RULES` for
   `qt.network.*` forced off, so a user's `~/.config/QtProject/qtlogging.ini` cannot turn
   on header logging under Claudometer.
8. **No crash reporting.** No Sentry, no breakpad, no core-dump upload. Nothing that
   could ship process memory off the machine. This is the simplest way to satisfy
   "credentials must not appear in crash reports": have no crash reports.
9. **HTTPS only, verify always.** Never disable peer verification, never accept a
   user-supplied CA override, never allow the base URL to be configured — a configurable
   endpoint is a credential-exfiltration feature waiting to be social-engineered.
   Pin the host to `api.anthropic.com` at compile time.
10. **`--json` must be safe to pipe into a status bar.** It carries percentages and
    timestamps only: no token, no org UUID, no workspace ID, no email. Waybar configs end
    up in public dotfiles repos.
11. **Zero the buffer.** Overwrite the token's `QByteArray` before it is freed
    (`SecureZeroMemory`-equivalent: `std::fill` through a `volatile` pointer, or Qt's
    `QByteArray::fill('\0')` before clear). Cheap; prevents the token lingering in a heap
    page that ends up swapped or in a core dump.
12. **Redact in any diagnostic output.** If a `--debug` mode is ever added, it must route
    through one redaction helper that strips anything matching the token, UUIDs, and
    `Bearer\s+\S+`. The research probe used for this document did exactly that.

### Data Claudometer must never persist or transmit

The response in §3 also contains the organisation UUID, workspace ID, subscription type
and spend figures. **None of it is needed.** Extract the four numbers, discard the rest —
do not hold the parsed response after mapping it into `UsageState`.

### Not sending anything anywhere

Claudometer makes exactly one kind of outbound request: `GET api.anthropic.com/api/oauth/usage`.
No telemetry, no update check, no analytics, no error reporting. That should be a stated,
testable property of the project — one host in the allowlist, verifiable with `strace`
or by reading `UsageClient`.

---

## 9. Recommended implementation

### Data flow

```
Credentials            reads $CLAUDE_CODE_OAUTH_TOKEN, else
  │                    $CLAUDE_CONFIG_DIR/.credentials.json, else
  │                    ~/.claude/.credentials.json
  │                    · validates expiresAt (60 s buffer)
  │                    · QFileSystemWatcher → credentialsChanged
  │                    · authorize(QNetworkRequest&) — token never leaves this class
  ▼
UsageClient            GET https://api.anthropic.com/api/oauth/usage?skip_spend=1
  │                    QNetworkAccessManager, 5 s timeout, lenient JSON parse
  ▼
UsageState             { fiveHour, sevenDay, updatedAt, stale }
  │                    UsagePeriod { double percentage; optional<time_point> resetAt; }
  ├─────────────► tray icon + tooltip
  ├─────────────► QML popup (via a view-model that exposes only these values)
  └─────────────► --json / --once
```

### Request

```cpp
QNetworkRequest req{QUrl("https://api.anthropic.com/api/oauth/usage?skip_spend=1")};
req.setRawHeader("Content-Type",   "application/json");
req.setRawHeader("anthropic-beta", "oauth-2025-04-20");
req.setRawHeader("User-Agent",     "claudometer/" CLAUDOMETER_VERSION);
credentials.authorize(req);                  // sets Authorization: Bearer …
req.setTransferTimeout(5000);
req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                 QNetworkRequest::ManualRedirectPolicy);   // never follow a redirect
                                                           // while carrying a bearer token
```

That last line matters: an unexpected redirect with automatic following would replay the
`Authorization` header at whatever host the redirect names.

### Parsing — lenient by design

```cpp
// Reads only what it understands. Unknown keys, extra buckets, and nulls are non-events.
std::optional<UsagePeriod> parseWindow(const QJsonValue& v)
{
    if (!v.isObject()) return std::nullopt;          // null / absent / renamed → no data
    const auto o = v.toObject();
    UsagePeriod p;
    const auto u = o.value("utilization");
    if (!u.isDouble()) return std::nullopt;          // present but unusable
    p.percentage = std::clamp(u.toDouble(), 0.0, 100.0);
    if (const auto r = o.value("resets_at"); r.isString())
        if (auto dt = QDateTime::fromString(r.toString(), Qt::ISODateWithMs); dt.isValid())
            p.resetAt = dt;                          // stays nullopt when null or malformed
    return p;
}
```

Note `resetAt` is independently optional: a window can have a valid percentage and no
reset timestamp (see `nimbus_quill` in §3). The tray must render "63%" with no
"resets in …" line rather than showing a bogus time.

### Failure handling

| Condition | Behaviour |
| --- | --- |
| `200`, parsed | update state, clear stale, evaluate notification thresholds |
| `200`, both windows unparseable | keep previous state, mark stale, no notification |
| `401` | mark stale, **do not refresh**, wait for `.credentials.json` to change |
| `429` | mark stale, back off 3 → 6 → 12 → 15 min |
| `5xx`, timeout, offline | mark stale, retry at next normal interval |
| token expired locally | skip the request entirely, mark stale |
| refresh token also expired | stop polling, show "sign in with Claude Code" |
| no OAuth credentials at all | show "not signed in"; never show 0 % |

A failure never clears a good `UsageState`. `stale` is a display flag, not a data reset.

### Polling

- Default 5 min; floor 60 s; align to `resets_at` so the first poll after a reset is prompt.
- Poll on start, on manual refresh, on `.credentials.json` change.
- Suspend/resume: subscribe to `PrepareForSleep` on
  `org.freedesktop.login1` via `QDBusConnection::systemBus()` and refresh on wake.
  Qt has no cross-platform API for this; the D-Bus signal is the standard Linux mechanism
  and `Qt6::DBus` is already a dependency.
- Pause polling while the session is idle or locked, if that turns out to be easy —
  a tray icon nobody is looking at does not need to be current.

### Notifications

Thresholds 75 / 90 / 95 / 100 %, fired once per window per crossing. Reset the fired-flags
when `resets_at` moves forward — that is the reliable signal a new window began, and it is
more robust than watching the percentage drop (which also happens mid-window as a rolling
window slides).

Use `QDBusConnection::sessionBus()` against `org.freedesktop.Notifications` directly rather
than shelling out to `notify-send`, so notifications can be replaced in place by
`replaces_id` instead of stacking up.

---

## 10. Verdict

**Proceed to implementation.**

The data is authoritative, complete (both percentages *and* both reset timestamps), and
reachable with one small `GET`. Nothing about it requires unsafe credential handling: the
token is read from a file the user already owns, held in one class, sent to exactly one
pinned HTTPS host, never written, never logged, never given to QML, and never refreshed.

The two real risks are both honest-documentation problems rather than engineering ones:

1. **The endpoint is undocumented and marked experimental by its own authors.** It will
   break eventually. The README must say so, the parser must be lenient, and a broken
   endpoint must degrade to visible staleness rather than a crash or a wrong number.
2. **Claudometer reads another application's credential file.** Users are entitled to know
   this before they install a tray widget. State it in the README, keep the credential
   path auditable in a single short file, and keep the outbound host list at one entry.

---

---

## Appendix: desktop integration, verified on the target platform

Confirmed empirically on Arch / KDE Plasma 6 / Wayland while building the first
prototype, since these were the open questions the research phase could not
settle from documentation.

**StatusNotifierItem works, with no extra code.** `QSystemTrayIcon` registered
over D-Bus as expected; the item shows up in
`org.kde.StatusNotifierWatcher.RegisteredStatusNotifierItems` as
`":1.903/StatusNotifierItem"`. Note the *unique* bus name - a modern SNI item
does not take a well-known `org.kde.StatusNotifierItem-<pid>-<n>` name, so
looking for one on the bus is a misleading way to check whether the tray works.
All six icon sizes are published as `IconPixmap` ARGB data.

**The popup renders correctly and the compositor places it.** A frameless
translucent `QQuickView` with rounded corners displays fine. `setPosition()` is
ignored, as expected for a Wayland top-level, so the popup appears where KWin
decides rather than next to the tray icon. Nothing in the client can change
this; layer-shell would be required, and is not worth a dependency yet.

**Focus-out dismissal is unreliable.** A popup opened from a D-Bus `Activate`
call may never receive keyboard focus, so `QWindow::activeChanged` never fires
and the window does not dismiss itself. The tray toggle and an explicit close
button are the dependable paths, and are what Claudometer ships; `Escape` is
wired up for the cases where focus is granted.

**A tray icon cannot be designed in a mock-up.** Four candidate marks were
registered as real StatusNotifierItems and screenshotted in the panel at 22 px.
The version that looked like a clear speedometer when magnified read as a grey
loading spinner in place - the dial was too thin, the unfilled portion too
prominent, and the needle too small. What worked was a much thicker dial, a
needle held clear of the rim by a visible gap, and the unfilled arc dropped to
20% alpha. A number inside a ring also proved legible at that size, including
three digits at 100%, so it ships as an option rather than being ruled out.

**`QStyleHints::setColorScheme()`** (Qt 6.8+) is enough to implement the
System/Light/Dark setting without a theming layer of our own - it moves the whole
application, QML included.

**No Breeze style for Qt Quick Controls is installed** on a stock Arch Plasma
system (`Basic`, `Fusion`, `Material`, `Universal`, `FluentWinUI3` only). Hence
`Fusion` for the settings form and hand-rolled visuals for the popup, rather than
relying on a native-looking QQC2 style that will not be there.

---

## References

- [jens-duttke/usage-monitor-for-claude][jd] — native Windows/Linux tray monitor
- [Claude-Code-Usage-Monitor#202 — "Anthropic OAuth Usage API"][issue202] — endpoint write-up
- [xikxp1/claude-monitor][xik] — tray monitor with per-model windows
- [StaticB1/claude_ai_usage_widget][sb1] — Linux GTK tray widget + CLI
- [stefanprodan/cctop][cctop] — the status-line capture technique
- [Maciek-roboblog/Claude-Code-Usage-Monitor][ccum] — provenance labelling
- Claude Code `2.1.255` binary — endpoint, headers, internal schemas, retry policy
- Live `GET /api/oauth/usage` — response in §3

[jd]: https://github.com/jens-duttke/usage-monitor-for-claude
[issue202]: https://github.com/Maciek-roboblog/Claude-Code-Usage-Monitor/issues/202
[xik]: https://github.com/xikxp1/claude-monitor
[sb1]: https://github.com/StaticB1/claude_ai_usage_widget
[cctop]: https://github.com/stefanprodan/cctop
[ccum]: https://github.com/Maciek-roboblog/Claude-Code-Usage-Monitor
[c9]: https://github.com/minchenlee/c9watch
[cc]: https://github.com/sverrirsig/claude-control
[csb]: https://github.com/m1ckc3s/claude-status-bar
[cs]: https://github.com/gmr/claude-status
[soa]: https://github.com/sotthang/so-agentbar
[lc]: https://github.com/leonardocouy/claudometer
