# The Pond

**The Pond** is the on-device web interface for a FrogPilot/openpilot comma device:
manage settings, browse/stream dashcam + screen recordings, read error logs, control
navigation, lock/unlock Toyota doors, manage Tailscale, and set Toyota security keys —
all from a phone or PC browser on the same network.

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
  Bootstrap Icons, Font Awesome, Open Sans) is **vendored under `assets/vendor/`** and
  served locally, so the static shell loads with **no CDN requests**. The app does make
  several external calls at runtime, each with a timeout / degraded mode:
  - Mapbox map tiles, Search and Directions (`api.mapbox.com`) with the user's key (client-side);
  - the Tailscale package index + archive download (`pkgs.tailscale.com`) during install;
  - GitLab commit + Discord notification (`frogpilot.com/api/...`) on theme submit.

### Layout

```
the_pond.py        Flask app (create_app/main), all routes, origin + onroad
                   gates, mDNS responder + port-80 redirect
helpers.py         pure stdlib seams (testable without openpilot): containment,
                   Origin/Host, allowlists, onroad gates, segments, dates, mDNS
utilities.py       media: bounded ffmpeg (semaphore + nice), Range streaming, caches
templates/index.html   static SPA shell (no Jinja)
assets/components/     Arrow components (router, sidebar, home, nav, recordings, tools…)
assets/js/             shared helpers (api.js fetchJson/downloadBlob, snackbar, utils)
assets/vendor/         vendored offline deps (arrow, router, mapbox-gl, fonts, icons)
tests/                 host pytest + static gate + node security tests; backend pytest (container)
```

---

## Security model

The Pond does not require a login token. Any browser or client that can reach the
device's HTTP port can access the UI and API.

- **Origin checks** — state-changing requests must carry a positively-matching
  `Origin` (or `Referer` fallback); an absent header does not pass. This blocks
  cross-site browser requests from unrelated origins, but it is not authentication.
- **Security headers** — every response carries a strict `Content-Security-Policy`
  (`script-src 'self'`, scoped Mapbox exceptions), `X-Frame-Options: DENY`,
  `X-Content-Type-Options: nosniff`, and `Referrer-Policy: same-origin`.
- **Transport** — the server binds `0.0.0.0` over **plain HTTP** (no TLS), so SecOC
  keys, recordings, logs, and settings are only as private as the network path. Reach
  The Pond over **Tailscale or loopback**, not an untrusted shared Wi-Fi/hotspot.
- **Discovery / port 80** — on-device, The Pond answers mDNS queries for
  `ThePond.local` (broadcasting its current IP on the LAN) and installs an `iptables`
  NAT redirect so port **80** transparently reaches 8082. This is reachability
  convenience, not authentication: it widens the open surface to port 80 and
  advertises the hostname on the local network, while the Origin, CSP, and onroad
  gates above still apply. PC mode does neither.
- **Onroad lockout** — while `IsOnroad` is true, the server returns **423** for the
  mid-drive-dangerous routes: reboot/reset, theme-apply, toggles-restore, Tailscale
  setup/uninstall, and the four bulk **delete-all** routes. Single-item deletes, door
  lock/unlock, and setting a destination stay allowed. SecOC key write/delete are **not**
  yet gated (pending in-vehicle validation — see the remediation report). A polled overlay
  covers the UI (with a "continue anyway" escape for bench/parked use).

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

The suite under `tests/` (see `tests/README.md` for which test runs where):

- **Host tests** — run on any machine (Windows/macOS/Linux), no openpilot stack:
  ```bash
  # from frogpilot/system/the_pond/
  python -m pytest tests/ -q          # helpers, archive-safety, route inventory/contract, static gate
  python tests/check_static.py        # static XSS-sink / header / dead-code gate (also run inside pytest)
  node tests/js/security.mjs          # frontend redirect-validation + XSS-sink regression
  ```
  `test_route_inventory.py` snapshots the live route table (origin/onroad contracts);
  `test_route_contract.py` checks the frontend `fetch()` ↔ backend route contract;
  `check_static.py` enforces the output-encoding and removed-dead-code baselines.
- **Backend tests** — require the Linux openpilot stack (run in the pinned
  `openpilot-base` container, or a local Linux env where `import openpilot` works):
  ```bash
  python -m pytest frogpilot/system/the_pond/tests/test_routes_smoke.py -q   # Flask origin/onroad gate
  ```
  On a host without the stack `test_routes_smoke.py` `pytest.importorskip`s cleanly, so
  the host commands above stay green everywhere.

---

## Adding a page

1. Add a component under `assets/components/`.
2. Register it in `assets/components/router.js` (`createRoute(id, "/path", Component)`).
3. Add a sidebar entry in `assets/components/sidebar.js`.
4. If it calls the backend, add the route to `the_pond.py` and a corresponding entry so
   `test_route_contract.py` stays green. Import Arrow from the vendored module:
   `import { html, reactive } from "/assets/vendor/arrow.mjs"`.
5. **Output encoding:** render any user/server-supplied text with the function form
   `${() => value}` (a safe reactive text node), never the bare `${value}` form — in
   Arrow the bare form is concatenated into `innerHTML` and is an XSS sink. The static
   gate (`check_static.py`) enforces this for the known name fields.
