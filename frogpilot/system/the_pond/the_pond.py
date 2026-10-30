#!/usr/bin/env python3
import asyncio
import hashlib
import hmac
import ipaddress
import re
import socket
import subprocess
import time

from aiohttp import web

from openpilot.system.hardware import HARDWARE, PC

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import (backups, dashcam, device, doors, home, keys, maps, models, navigation, screen_recordings, search, software,
                                                 tailscale, theme_maker, themes, toggles, tools, utilities, vehicle)

API_VERSION = 1

ASSET_CACHE_CONTROL = "public, max-age=31536000, immutable"
ASSET_LINK_PATTERN = re.compile(r'((?:href|src)="/assets/[^"?]+)"')

COMPRESSED_EXTENSIONS = (".css", ".js", ".svg")

CONTENT_SECURITY_POLICY = "; ".join((
  "default-src 'self'",
  "connect-src 'self' https://api.mapbox.com https://events.mapbox.com",
  "frame-ancestors 'none'",
  "img-src 'self' blob: data: https://api.mapbox.com",
  "object-src 'none'",
  "worker-src 'self' blob:",
))

HOTSPOT_SUBNET = "192.168.43.0/24"

IP_ADDRESS_INTERFACES = ("wlan0", "eth0")

LOCK_CHECK_INTERVAL = 0.5
MAX_UPLOAD_SIZE = 100 * 1024 * 1024
MDNS_CHECK_INTERVAL = 10

OPEN_PATHS = {"/api/pair", "/api/state"}

PAIRING_ATTEMPTS = 10
PAIRING_WINDOW = 10 * 60

SERVICE_TYPE = "_thepond._tcp"

HOTSPOT_RETURN_RULE = ["PREROUTING", "-s", HOTSPOT_SUBNET, "!", "-d", HOTSPOT_SUBNET, "-p", "tcp", "--dport", "80", "-j", "RETURN"]
PORT_REDIRECT_RULE = ["PREROUTING", "-p", "tcp", "--dport", "80", "-j", "REDIRECT", "--to-ports", str(frogpilot_variables.THE_POND_PORT)]

async def is_allowed_host(host):
  if host.startswith("["):
    name = host[1:].partition("]")[0]
  else:
    name = host.partition(":")[0]

  try:
    ipaddress.ip_address(name)
    return True
  except ValueError:
    pass

  name = name.lower().rstrip(".")
  allowed_names = {"localhost", frogpilot_variables.THE_POND_HOSTNAME.lower(), f"{socket.gethostname().lower()}.local"}
  return name in allowed_names or name.endswith(".ts.net") or await tailscale.is_device_name(name)


async def is_hostname_taken():
  resolver = await asyncio.create_subprocess_exec("avahi-resolve", "-4", "-n", frogpilot_variables.THE_POND_HOSTNAME,
                                                  stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL)
  stdout, _ = await resolver.communicate()
  return bool(stdout.strip())


def is_paired(the_pond, request):
  if not the_pond.params.get_bool("ThePondPairing"):
    return True

  token = request.cookies.get(utilities.PAIRING_COOKIE)
  authorization = request.headers.get("Authorization", "")
  if authorization.startswith("Bearer "):
    token = authorization.removeprefix("Bearer ")
  if token is None:
    return False

  token_hash = hashlib.sha256(token.encode()).hexdigest()
  return any(hmac.compare_digest(browser["hash"], token_hash) for browser in the_pond.params.get("ThePondPairedBrowsers"))


@web.middleware
async def guard(request, handler):
  if not await is_allowed_host(request.host):
    raise web.HTTPMisdirectedRequest(text="unknown_host")

  origin = request.headers.get("Origin")
  if request.method not in ("GET", "HEAD") and origin is not None and origin != f"{request.scheme}://{request.host}":
    raise web.HTTPForbidden(text="cross_origin")

  if request.path.startswith("/api/") and request.path not in OPEN_PATHS:
    the_pond = request.app[utilities.THE_POND]
    if the_pond.locked:
      raise utilities.HTTPLocked(text="locked")
    if not is_paired(the_pond, request):
      raise web.HTTPUnauthorized(text="not_paired")

    the_pond.connections.add(request.transport)

  return await handler(request)


@web.middleware
async def compress_responses(request, handler):
  response = await handler(request)
  if response.content_type == "application/json" or request.path.endswith(COMPRESSED_EXTENSIONS):
    response.enable_compression()
  return response


async def add_security_headers(request, response):
  response.headers["Cache-Control"] = ASSET_CACHE_CONTROL if request.path.startswith("/assets/") and "v" in request.query else "no-cache"
  response.headers["Content-Security-Policy"] = CONTENT_SECURITY_POLICY
  response.headers["Referrer-Policy"] = "no-referrer"
  response.headers["X-Content-Type-Options"] = "nosniff"


async def get_state(request):
  the_pond = request.app[utilities.THE_POND]
  paired = is_paired(the_pond, request)
  state = {
    "api_version": API_VERSION,
    "is_metric": the_pond.params.get_bool("IsMetric"),
    "language": the_pond.params.get("LanguageSetting"),
    "locked": the_pond.locked,
    "model": frogpilot_variables.DEVICE_MODELS[HARDWARE.get_device_type()],
    "paired": paired,
    "pairing": the_pond.params.get_bool("ThePondPairing"),
    "tuning_level": frogpilot_variables.get_frogpilot_toggles(the_pond.sm).tuning_level,
  }

  if paired:
    state["name"] = socket.gethostname()
    state["version"] = the_pond.params.get("Version")

  return web.json_response(state)


def get_asset_version():
  digest = hashlib.sha256()
  for path in sorted(frogpilot_paths.THE_POND_ASSETS_PATH.rglob("*")):
    if path.is_file():
      info = path.stat()
      digest.update(f"{path.relative_to(frogpilot_paths.THE_POND_ASSETS_PATH)}:{info.st_size}:{info.st_mtime_ns}".encode())
  return digest.hexdigest()[:12]


async def index(request):
  if request.path.startswith("/api/"):
    raise web.HTTPNotFound(text="not_found")
  html = ASSET_LINK_PATTERN.sub(rf'\1?v={get_asset_version()}"', (frogpilot_paths.THE_POND_ASSETS_PATH / "index.html").read_text())
  return web.Response(text=html, content_type="text/html")


async def pair(request):
  the_pond = request.app[utilities.THE_POND]

  now = time.monotonic()
  the_pond.pairing_failures = [failure for failure in the_pond.pairing_failures if now - failure < PAIRING_WINDOW]
  if len(the_pond.pairing_failures) >= PAIRING_ATTEMPTS:
    raise web.HTTPTooManyRequests(text="too_many_attempts")

  if not the_pond.params.get_bool("ThePondPairing"):
    raise web.HTTPConflict(text="pairing_off")

  the_pond.pairing_failures.append(now)

  body = await utilities.read_json(request)
  code = body.get("code")
  pairing_code = the_pond.params.get("ThePondPairingCode")
  if not isinstance(code, str) or not pairing_code or not hmac.compare_digest(code.encode(), pairing_code.encode()):
    raise web.HTTPForbidden(text="wrong_code")

  the_pond.pairing_failures.remove(now)

  name, token, options = utilities.pair_browser(the_pond.params, request)
  if body.get("token") is True:
    return web.json_response({"token": token})

  response = web.Response(status=204)
  response.set_cookie(name, token, **options)
  return response


async def publish_mdns(app):
  params = app[utilities.THE_POND].params
  service_name = f"The Pond on {socket.gethostname()}"
  service_records = [f"api_version={API_VERSION}", f"model={frogpilot_variables.DEVICE_MODELS[HARDWARE.get_device_type()]}", f"version={params.get('Version')}"]

  process = None
  published_ip_address = None
  service = None

  try:
    while True:
      ip_address = utilities.get_ip_address(IP_ADDRESS_INTERFACES)
      if process is not None and (ip_address != published_ip_address or process.returncode is not None):
        await utilities.stop_process(process)
        process = None

      if process is None and ip_address is not None and not await is_hostname_taken():
        process = await asyncio.create_subprocess_exec("avahi-publish", "-a", "-R", frogpilot_variables.THE_POND_HOSTNAME, ip_address,
                                                       stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
        published_ip_address = ip_address

      if service is None or service.returncode is not None:
        service = await asyncio.create_subprocess_exec("avahi-publish", "-s", service_name, SERVICE_TYPE, str(frogpilot_variables.THE_POND_PORT),
                                                       *service_records, stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)

      await asyncio.sleep(MDNS_CHECK_INTERVAL)
  finally:
    if process is not None:
      await utilities.stop_process(process)
    if service is not None:
      await utilities.stop_process(service)


async def prepare_drives(the_pond, previous_recordings):
  await utilities.run_and_report(dashcam.prepare_routes(the_pond))
  await utilities.run_and_report(screen_recordings.prepare_recordings(the_pond, previous_recordings))


def watch_drive(the_pond):
  drive_active = dashcam.is_drive_active(the_pond)
  if drive_active and not the_pond.drive_active:
    if the_pond.preparation is not None:
      the_pond.preparation.cancel()
    the_pond.recordings_before_drive = screen_recordings.get_recording_paths()
  elif not drive_active and (the_pond.drive_active or the_pond.preparation is None):
    the_pond.preparation = asyncio.create_task(prepare_drives(the_pond, the_pond.recordings_before_drive))
  the_pond.drive_active = drive_active


async def watch_lock(app):
  the_pond = app[utilities.THE_POND]
  while True:
    the_pond.update()
    watch_drive(the_pond)
    await tools.close_idle_shell(the_pond)
    await asyncio.sleep(LOCK_CHECK_INTERVAL)


async def background_tasks(app):
  tasks = [asyncio.create_task(watch_lock(app))]
  if not PC:
    tasks.append(asyncio.create_task(publish_mdns(app)))

  yield

  for task in tasks:
    task.cancel()
  await asyncio.gather(*tasks, return_exceptions=True)


async def port_redirect(app):
  if not PC:
    if subprocess.run(["sudo", "iptables-legacy", "-t", "nat", "-C", *HOTSPOT_RETURN_RULE], capture_output=True).returncode != 0:
      subprocess.run(["sudo", "iptables-legacy", "-t", "nat", "-I", *HOTSPOT_RETURN_RULE], capture_output=True, check=True)
    if subprocess.run(["sudo", "iptables-legacy", "-t", "nat", "-C", *PORT_REDIRECT_RULE], capture_output=True).returncode != 0:
      subprocess.run(["sudo", "iptables-legacy", "-t", "nat", "-A", *PORT_REDIRECT_RULE], capture_output=True, check=True)

  yield

  if not PC:
    subprocess.run(["sudo", "iptables-legacy", "-t", "nat", "-D", *PORT_REDIRECT_RULE], capture_output=True, check=True)
    subprocess.run(["sudo", "iptables-legacy", "-t", "nat", "-D", *HOTSPOT_RETURN_RULE], capture_output=True, check=True)


def create_app():
  app = web.Application(client_max_size=MAX_UPLOAD_SIZE, middlewares=[guard, compress_responses])
  app[utilities.THE_POND] = utilities.ThePond()

  app.cleanup_ctx.append(background_tasks)
  app.cleanup_ctx.append(port_redirect)
  app.on_response_prepare.append(add_security_headers)

  app.router.add_post("/api/pair", pair)
  app.router.add_get("/api/state", get_state)

  app.add_routes(backups.routes)
  app.add_routes(dashcam.routes)
  app.add_routes(device.routes)
  app.add_routes(doors.routes)
  app.add_routes(home.routes)
  app.add_routes(keys.routes)
  app.add_routes(maps.routes)
  app.add_routes(models.routes)
  app.add_routes(navigation.routes)
  app.add_routes(screen_recordings.routes)
  app.add_routes(search.routes)
  app.add_routes(software.routes)
  app.add_routes(tailscale.routes)
  app.add_routes(theme_maker.routes)
  app.add_routes(themes.routes)
  app.add_routes(toggles.routes)
  app.add_routes(tools.routes)
  app.add_routes(vehicle.routes)

  app.router.add_static("/assets", frogpilot_paths.THE_POND_ASSETS_PATH)
  app.router.add_get("/{tail:.*}", index)
  return app


def main():
  frogpilot_utilities.delete_path(frogpilot_paths.THE_POND_VIDEOS_PATH)
  for path in frogpilot_paths.THE_POND_THUMBNAILS_PATH.glob(".*"):
    path.unlink()
  dashcam.remove_stale_files(dashcam.get_routes())
  screen_recordings.remove_stale_files()

  web.run_app(create_app(), access_log=None, handler_cancellation=True, port=frogpilot_variables.THE_POND_PORT, print=None)


if __name__ == "__main__":
  main()
