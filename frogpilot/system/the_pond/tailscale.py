import aiohttp
import asyncio
import hashlib
import json
import re
import shutil
import tarfile
import time

from aiohttp import web

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

DEVICE_NAME_CHECK_INTERVAL = 5

DOWNLOAD_CHUNK_SIZE = 1024 * 1024
DOWNLOAD_TIMEOUT = 600

HOSTNAME_PATTERN = re.compile(r"[A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?")

LOGIN_TIMEOUT = "10m"

STOP_TIMEOUT = 15

TAILSCALE_ARCHITECTURE = "arm64"
TAILSCALE_BINARIES = ("tailscale", "tailscaled")
TAILSCALE_HOSTNAME = "ThePond"
TAILSCALE_INDEX_URL = "https://pkgs.tailscale.com/stable/?mode=json"
TAILSCALE_PACKAGES_URL = "https://pkgs.tailscale.com/stable"

routes = web.RouteTableDef()

device_name = None
device_name_checked = 0.0

async def cancel_login(the_pond):
  job = the_pond.jobs.get("tailscale")
  if job is not None and job[1]["kind"] == "login":
    job[0].cancel()
    await asyncio.wait([job[0]])


def get_command(*args):
  return [str(frogpilot_paths.TAILSCALE_PATH / "tailscale"), f"--socket={frogpilot_paths.TAILSCALE_SOCKET_PATH}", *args]


def is_installed():
  return all((frogpilot_paths.TAILSCALE_PATH / binary).is_file() for binary in TAILSCALE_BINARIES)


async def get_status():
  global device_name, device_name_checked
  device_name_checked = time.monotonic()
  try:
    status = json.loads(await utilities.run_process(get_command("status", "--json")))
  except RuntimeError:
    return None

  device_name = ((status.get("Self") or {}).get("DNSName") or "").partition(".")[0].lower() or None
  return status


async def is_device_name(name):
  if name != device_name and is_installed() and time.monotonic() - device_name_checked >= DEVICE_NAME_CHECK_INTERVAL:
    await get_status()
  return name == device_name


def extract_binaries(archive_path):
  with tarfile.open(archive_path) as archive:
    for member in archive.getmembers():
      name = member.name.rpartition("/")[2]
      if member.isfile() and name in TAILSCALE_BINARIES:
        with archive.extractfile(member) as source, open(frogpilot_paths.TAILSCALE_PATH / f".{name}", "wb") as destination:
          shutil.copyfileobj(source, destination)

  for binary in TAILSCALE_BINARIES:
    path = frogpilot_paths.TAILSCALE_PATH / f".{binary}"
    path.chmod(0o755)
    path.rename(frogpilot_paths.TAILSCALE_PATH / binary)


async def install(the_pond, progress):
  frogpilot_paths.TAILSCALE_PATH.mkdir(parents=True, exist_ok=True)
  archive_path = frogpilot_paths.TAILSCALE_PATH / ".download.tgz"

  try:
    async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=DOWNLOAD_TIMEOUT), raise_for_status=True) as session:
      async with session.get(TAILSCALE_INDEX_URL) as response:
        tarball = (await response.json(content_type=None))["Tarballs"][TAILSCALE_ARCHITECTURE]

      async with session.get(f"{TAILSCALE_PACKAGES_URL}/{tarball}.sha256") as response:
        expected_hash = (await response.text()).split()[0]

      digest = hashlib.sha256()
      async with session.get(f"{TAILSCALE_PACKAGES_URL}/{tarball}") as response:
        progress["total"] = response.content_length or 0
        with open(archive_path, "wb") as file:
          async for chunk in response.content.iter_chunked(DOWNLOAD_CHUNK_SIZE):
            file.write(chunk)
            digest.update(chunk)
            progress["downloaded"] += len(chunk)

    if digest.hexdigest() != expected_hash:
      raise RuntimeError("Tailscale download failed verification")

    await asyncio.to_thread(extract_binaries, archive_path)
  finally:
    archive_path.unlink(missing_ok=True)

  frogpilot_paths.TAILSCALE_STATE_PATH.mkdir(exist_ok=True)
  the_pond.send_request("updateToggles")


async def login(the_pond):
  hostname = json.loads(await utilities.run_process(get_command("debug", "prefs"))).get("Hostname") or TAILSCALE_HOSTNAME
  process = await asyncio.create_subprocess_exec(*get_command("up", "--accept-dns=false", f"--hostname={hostname}", f"--timeout={LOGIN_TIMEOUT}"),
                                                 stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
  try:
    await process.wait()
  finally:
    await utilities.stop_process(process)


async def uninstall(the_pond):
  for binary in TAILSCALE_BINARIES:
    (frogpilot_paths.TAILSCALE_PATH / binary).unlink(missing_ok=True)
  the_pond.send_request("updateToggles")

  for _ in range(STOP_TIMEOUT):
    if not any(process.name == "tailscaled" and process.running for process in the_pond.sm["managerState"].processes):
      break
    await asyncio.sleep(1)

  await asyncio.to_thread(shutil.rmtree, frogpilot_paths.TAILSCALE_PATH, True)


@routes.get("/api/tailscale")
async def get_tailscale(request):
  the_pond = request.app[utilities.THE_POND]

  status = None
  if is_installed():
    tailscale_status = await get_status()
    if tailscale_status is not None:
      device = tailscale_status.get("Self") or {}
      status = {
        "auth_url": tailscale_status.get("AuthURL") or None,
        "backend_state": tailscale_status.get("BackendState"),
        "dns_name": (device.get("DNSName") or "").rstrip(".") or None,
        "hostname": device.get("HostName"),
        "ips": device.get("TailscaleIPs") or [],
        "magic_dns": (tailscale_status.get("CurrentTailnet") or {}).get("MagicDNSEnabled", False),
        "peers_online": sum(bool(peer.get("Online")) for peer in (tailscale_status.get("Peer") or {}).values()),
      }

  return web.json_response({"installed": is_installed(), "job": utilities.get_job(the_pond, "tailscale"), "port": frogpilot_variables.THE_POND_PORT, "status": status})


@routes.post("/api/tailscale")
async def install_tailscale(request):
  the_pond = request.app[utilities.THE_POND]
  if not frogpilot_utilities.is_online(the_pond.sm["deviceState"]):
    raise web.HTTPConflict(text="offline")

  progress = {"downloaded": 0, "kind": "install", "total": 0}
  utilities.start_job(the_pond, "tailscale", install(the_pond, progress), progress)
  return web.Response(status=202)


@routes.post("/api/tailscale/login")
async def login_tailscale(request):
  the_pond = request.app[utilities.THE_POND]
  if not is_installed():
    raise web.HTTPNotFound(text="not_found")

  utilities.start_job(the_pond, "tailscale", login(the_pond), {"kind": "login"})
  return web.Response(status=202)


@routes.post("/api/tailscale/logout")
async def logout_tailscale(request):
  the_pond = request.app[utilities.THE_POND]
  if not is_installed():
    raise web.HTTPNotFound(text="not_found")

  await cancel_login(the_pond)

  await utilities.run_process(get_command("logout"))
  return web.Response(status=204)


@routes.put("/api/tailscale/hostname")
async def set_hostname(request):
  hostname = (await utilities.read_json(request)).get("hostname")
  if not isinstance(hostname, str) or HOSTNAME_PATTERN.fullmatch(hostname) is None:
    raise web.HTTPBadRequest(text="invalid_hostname")
  if not is_installed():
    raise web.HTTPNotFound(text="not_found")

  await utilities.run_process(get_command("set", f"--hostname={hostname}"))
  return web.Response(status=204)


@routes.delete("/api/tailscale")
async def uninstall_tailscale(request):
  the_pond = request.app[utilities.THE_POND]

  await cancel_login(the_pond)

  utilities.start_job(the_pond, "tailscale", uninstall(the_pond), {"kind": "uninstall"})
  return web.Response(status=202)
