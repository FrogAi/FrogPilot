# The Pond

**The Pond** is the on-device web interface for a FrogPilot/openpilot comma device:
browse and stream dashcam footage and screen recordings, build and apply themes, read
error logs, control navigation, capture and stream the console log, back up / restore /
reset your toggles, download speed-limit data, lock/unlock Toyota doors, manage Tailscale,
and set Toyota security keys — all from a phone or PC browser on the same network.

It is served by a Flask app (`the_pond.py`) launched by the manager
(`PythonProcess("the_pond", …)`), on **port 8082 on-device / 8083 on PC**.
On-device it is reachable at **`http://ThePond.local`** (the hostname is
advertised over mDNS, and a kernel NAT redirect sends port 80 → 8082) or
directly at `http://<device-ip>:8082`.

---

## Architecture

- **Backend** — a single Flask app built by `create_app()` (`the_pond.py`), run
  threaded. Pure, dependency-free logic (path containment, allowlists, the
  origin/onroad gates, segment grouping, date formatting) lives in `helpers.py` so it is
  unit-testable on any host without the openpilot stack. Media/transcode helpers are in
  `utilities.py`.
- **Frontend** — native ES modules under `assets/`, rendered with
  [Arrow.js](https://www.arrow-js.com) (`@arrow-js/core`) + `@remix-run/router`. **No
  build step, no Node tooling, no bundler.**
- **Vendored, no build step** — every third-party asset (Arrow, the router, Mapbox GL,
  Bootstrap Icons, Open Sans) is **vendored under `assets/vendor/`** and served locally, so
  the static shell loads with **no CDN requests**. Mapbox GL is loaded only when the
  destination page actually needs it. The app does make several external calls at runtime:
  - Mapbox map tiles, Search and Directions (`api.mapbox.com`) with the user's key (client-side).
    Pond-owned Search and Directions requests have a 10-second timeout; requests made
    internally by Mapbox GL follow the vendored library's behavior;
  - Mapbox telemetry (`events.mapbox.com`) — sent by the vendored `mapbox-gl` bundle on every
    map load, carrying an anonymous device id and the user's public token. There is no opt-out;
  - the Tailscale package index, archive, and its `.sha256` checksum (`pkgs.tailscale.com`)
    during install. The archive is verified against that checksum before extraction;
  - authenticated control requests (`api.frogpilot.com`) and server-issued private storage
    uploads on theme submit. Server-side delivery commits the assets to GitLab and sends a
    private email notification.

### Layout

```
the_pond.py        Flask app (create_app/main), all routes, origin + onroad
                   gates, mDNS responder + port-80 redirect
helpers.py         pure stdlib seams (testable without openpilot): containment,
                   Origin/Host, allowlists, onroad gates, segments, dates, mDNS
utilities.py       theme build/publish pipeline, bounded ffmpeg (semaphore + nice),
                   video cache, route/segment listing, drive + disk stats
templates/index.html   static SPA shell (no Jinja)
assets/components/     Arrow components (router, sidebar, home, nav, recordings, tools…)
assets/js/             shared helpers (api.js fetchJson/downloadBlob, snackbar, utils)
assets/vendor/         vendored offline deps (arrow, router, mapbox-gl, fonts, icons)
tests/                 host pytest (no openpilot stack required)
```

---

## Security model

The Pond does not require a login. Any browser or client that can reach the device's HTTP
port can use the UI and API. That is a deliberate choice in favour of it being effortless to
use; the gates below narrow the blast radius without ever asking you for a credential.

- **Host checks** — every request, including GET, must address the device by a name that
  could plausibly be it: an IP address, a bare hostname, or a `.local` / `.lan` / `.home` /
  `.internal` / `.ts.net` name. Only a public registrable domain is refused, which is exactly
  the DNS-rebinding vector — a web page the driver visits re-pointing its own hostname at the
  device so it can read the response as same-origin. Nothing you would ever type is affected.
- **Origin checks** — state-changing requests must additionally carry a positively-matching
  `Origin` (or `Referer` fallback); an absent header does not pass. This blocks
  cross-site browser requests from unrelated origins. It is the main thing standing between
  a web page the driver visits and a destructive request to the device.
- **Security headers** — every response carries a strict `Content-Security-Policy`
  (`script-src 'self'`, scoped Mapbox exceptions), `X-Frame-Options: DENY`,
  `X-Content-Type-Options: nosniff`, `Referrer-Policy: same-origin`, and
  `Cross-Origin-Resource-Policy: same-origin`. `/api/` responses are `Cache-Control: no-store`.
- **Secrets** — SecOC key *values* are never served. The key list returns names and a
  `value_set` flag, and applying a key is done by name, so the value never leaves the device.
  Toggle backups **do** contain your Mapbox and AMap keys and your saved destinations, because
  a restore that does not restore them is not a restore — treat `toggle_backup.json` as
  sensitive and do not post it publicly. The SecOC keys are the one exception and are left
  out: they are vehicle-specific, and `card.py` accepts any 32-hex value without complaint, so
  restoring another car's key would silently arm a wrong one for the next drive.
- **Transport** — the server binds `0.0.0.0` over **plain HTTP** (no TLS), so recordings,
  logs, and settings are only as private as the network path. Reach The Pond over
  **Tailscale or loopback**, not an untrusted shared Wi-Fi/hotspot.
- **Discovery / port 80** — on-device, The Pond answers mDNS queries for
  `ThePond.local` (broadcasting its current IP on the LAN) and installs an `iptables`
  NAT redirect so port **80** transparently reaches 8082. This is reachability
  convenience, not authentication: it widens the open surface to port 80 and
  advertises the hostname on the local network, while the Host, Origin, CSP and onroad
  gates above still apply. PC mode does neither.
- **Tailscale enrolment** — `POST /api/tailscale/setup` returns a `login.tailscale.com`
  URL, and that URL is a bearer capability: whoever opens it first binds this device to
  *their* tailnet. It is gated by the Origin check and by the onroad lockout, and the
  response is never cached. Do not paste that URL anywhere.
- **Tailscale ownership** — The Pond only upgrades or removes the installation whose
  binaries and systemd unit match its own managed layout. If it finds another Tailscale
  installation, the UI reports that it is managed externally and leaves it alone.
- **Onroad lockout** — the gate is **default-deny**. While the car is onroad *and not in
  Park*, the server returns **423** for **every** state-changing request, and for the live
  console stream. A newly added route is blocked automatically unless it is explicitly
  listed in `helpers.ONROAD_ALLOWED` (currently empty), so the block list cannot silently
  drift behind the route table. Reads stay available so the UI can explain itself.
  The gate normally applies when a request starts. Door commands re-check before every CAN
  send, and delayed reset reboots re-check immediately before rebooting, because those two
  actions must stop if the car goes onroad after admission.
- **Onroad overlay** — the browser renders a full-screen overlay and marks the rest of the
  page `inert`, so the lockout cannot be tabbed or screen-reader'd past. It fails safe:
  the overlay stays up until a poll of `/api/onroad` actually succeeds.

---

## Running

On a comma device the manager starts it automatically
(`PythonProcess("the_pond", …)` in `system/manager/process_config.py`). To run/debug
manually on a machine with the openpilot env (Linux):

```bash
python -m openpilot.frogpilot.system.the_pond.the_pond
```

On-device, browse to **`http://ThePond.local`** (or `http://<device-ip>:8082`).
In PC mode the app is available at `http://localhost:8083`.

---

## Tests

The focused suite runs without importing the openpilot stack or touching live Params,
device files, hardware, or the network. It needs pytest plus the Python packages already
used by The Pond (`Flask`, `Pillow`, and `Werkzeug`). See `tests/README.md` for the covered
contracts and browser harness.

```bash
# from the repository root
python -m pytest -c frogpilot/system/the_pond/tests/pytest.ini \
  --confcutdir=frogpilot/system/the_pond/tests \
  frogpilot/system/the_pond/tests/ -q
```

The `-c` is required: the repository's own `pyproject.toml` configures `pytest-cpp` and turns
warnings into errors, and its root `conftest.py` imports `openpilot`.

`test_the_pond.py` exercises the actual helper and handler function bodies with temporary
files and fake device interfaces. It covers request-origin validation, upload format checks,
route-delete rollback, thumbnail semantics, partial theme application, sparse route timing,
recording rename cleanup, navigation validation, door/reset onroad transitions, toggle
rollback, tmux capture sharing, video-cache eviction, SecOC validation, and Tailscale
ownership detection.

Anything that needs a real `flock`, panda, ffmpeg codec run, root remount, systemd mutation,
or network namespace remains a device or disposable-image test and is deliberately not
performed by the host suite.

---

## Adding a page

1. Add a component under `assets/components/`.
2. Register it in `assets/components/router.js` (`createRoute(id, "/path", Component)`).
3. Add a sidebar entry in `assets/components/sidebar.js`.
4. If it calls the backend, add the route to `the_pond.py`. A new state-changing route is
   blocked while driving automatically — see the onroad lockout above. Import Arrow from
   the vendored module: `import { html, reactive } from "/assets/vendor/arrow.mjs"`.
5. **Output encoding:** render any user/server-supplied text with the function form
   `${() => value}` (a safe reactive text node), never the bare `${value}` form — in Arrow
   the bare form is concatenated straight into `innerHTML` (`arrow.mjs:412` feeding `:602`)
   and is an XSS sink. Three traps the function form does **not** save you from:
   - a nested ``html`…` `` *inside* the function re-enters that same `innerHTML` sink, so the
     values interpolated in the inner template need the function form too;
   - **partial** attribute interpolation is not a binding at all. Arrow only binds an
     attribute whose *entire* value is the expression, so `class="prefix-${...}"` is both
     concatenated into the HTML **and** leaves its expression unconsumed — which shifts every
     later expression in the template by one slot and silently breaks unrelated parts of the
     component. Always wrap the whole value: ``class="${() => `prefix-${x}`}"``;
   - `setAttribute` is not a URL sanitizer — never build `href`/`src` from untrusted text.
