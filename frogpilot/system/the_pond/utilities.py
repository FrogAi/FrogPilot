import asyncio
import datetime
import fcntl
import hashlib
import math
import mimetypes
import secrets
import socket
import struct
import weakref

from aiohttp import web

from cereal import messaging
from openpilot.common.params import Params
from openpilot.system import sentry

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables

PAIRING_COOKIE = "the_pond"
PAIRING_COOKIE_AGE = 10 * 365 * 24 * 60 * 60

SIOCGIFADDR = 0x8915

STREAM_CHUNK_SIZE = 256 * 1024

class HTTPLocked(web.HTTPClientError):
  status_code = 423


class ThePond:
  def __init__(self):
    self.params = Params(return_defaults=True)

    self.pm = messaging.PubMaster(["thePondRequest"])
    self.sm = messaging.SubMaster(["deviceState", "frogpilotCarState", "frogpilotDeviceState", "frogpilotPlan", "frogpilotProcessState",
                                   "managerState", "mapdExtendedOut", "pandaStates", "selfdriveState"])

    self.connections = weakref.WeakSet()
    self.file_locks = {}
    self.heavy_work = asyncio.Semaphore(1)
    self.jobs = {}

    self.drive_active = False
    self.preparation = None
    self.recordings_before_drive = set()

    self.locked = True

    self.pairing_failures = []

    self.reboot_required = set()

    self.storage_usage = None
    self.storage_usage_expiry = 0.0

    self.tmux_captures = {}

  def is_locked(self):
    if not self.sm.seen["deviceState"]:
      return True
    if not self.sm["deviceState"].started or frogpilot_utilities.is_FrogsGoMoo():
      return False
    return not self.sm["frogpilotCarState"].parked

  def send_request(self, kind, value=None):
    msg = messaging.new_message("thePondRequest")
    setattr(msg.thePondRequest.request, kind, value)
    self.pm.send("thePondRequest", msg)
    return msg.logMonoTime

  def update(self):
    self.sm.update(0)

    locked = self.is_locked()
    if locked and not self.locked:
      for transport in list(self.connections):
        transport.close()
      for task, *_ in self.jobs.values():
        task.cancel()

    self.locked = locked

    if self.sm.seen["deviceState"] and not self.sm["deviceState"].started:
      self.reboot_required.clear()


THE_POND = web.AppKey("the_pond", ThePond)

def convert_metric(key, value, metric):
  conversion, decimals = frogpilot_variables.METRIC_CONVERSIONS[key]

  if metric:
    converted = value * conversion
  else:
    converted = value * (1 / conversion)

  if decimals is not None:
    converted = round_half_away(converted * 10 ** decimals) / 10 ** decimals

  if isinstance(value, int):
    return int(converted)
  return converted


async def create_file(the_pond, path, cmd):
  path.parent.mkdir(parents=True, exist_ok=True)

  temporary_path = path.with_name(f".{path.name}")
  try:
    await run_heavy_process(the_pond, [*cmd, "-y", str(temporary_path)])
    temporary_path.rename(path)
  finally:
    temporary_path.unlink(missing_ok=True)


def format_theme_name(key, use_files):
  user_created = key.endswith(frogpilot_variables.USER_CREATED_THEME_SUFFIX)
  if user_created:
    key = key.removesuffix(frogpilot_variables.USER_CREATED_THEME_SUFFIX)

  key, _, creator = key.partition("~")

  separator = "-" if "-" in key else "_"
  parts = [part[0].upper() + part[1:] for part in key.split(separator) if part]

  if not user_created and not use_files and "-" in key and len(parts) > 1:
    display_name = f"{parts[0]} ({' '.join(parts[1:])})"
  else:
    display_name = " ".join(parts)
  if user_created:
    display_name += " 🌟"
  if creator:
    display_name += f" - by: {creator}"
  return display_name


def get_file(directory, name):
  path = directory / name
  if name in ("", ".", "..") or path.parent != directory or not path.is_file():
    raise web.HTTPNotFound(text="not_found")
  return path


def get_ip_address(interfaces):
  for interface in interfaces:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
      try:
        return socket.inet_ntoa(fcntl.ioctl(sock.fileno(), SIOCGIFADDR, struct.pack("256s", interface.encode()))[20:24])
      except OSError:
        continue
  return None


def get_job(the_pond, name):
  job = the_pond.jobs.get(name)
  if job is None:
    return None

  task, info = job
  if not task.done():
    state = "running"
  elif task.cancelled() or task.exception() is not None:
    state = "failed"
  else:
    state = "done"
  return {**info, "state": state}


def is_issue_report_available(params):
  return frogpilot_variables.REPORT_GIT_REMOTE in (params.get("GitRemote") or "").lower()


def is_valid_name(name):
  return frogpilot_variables.VALID_NAME_PATTERN.fullmatch(name) is not None and ".." not in name and not name.startswith("-")


def list_files(directory, pattern="*"):
  files = []
  for path in directory.glob(pattern):
    if path.is_file():
      stat = path.stat()
      files.append({"modified": int(stat.st_mtime), "name": path.name, "size": stat.st_size})
  return sorted(files, key=lambda file: file["modified"], reverse=True)


def new_pairing_code():
  return f"{secrets.randbelow(1_000_000):06d}"


def pair_browser(params, request):
  token = secrets.token_urlsafe(32)

  browsers = params.get("ThePondPairedBrowsers")
  browsers.append({"hash": hashlib.sha256(token.encode()).hexdigest(), "id": secrets.token_hex(4),
                   "name": request.headers.get("User-Agent", "")[:200], "paired": int(datetime.datetime.now().timestamp())})
  params.put("ThePondPairedBrowsers", browsers)

  return PAIRING_COOKIE, token, {"httponly": True, "max_age": PAIRING_COOKIE_AGE, "samesite": "Strict"}


async def read_json(request):
  try:
    body = await request.json()
  except ValueError:
    raise web.HTTPBadRequest(text="invalid_json") from None

  if not isinstance(body, dict):
    raise web.HTTPBadRequest(text="invalid_json")
  return body


def reset_curve_data(params):
  params.put("CalibratedLateralAcceleration", params.get_default_value("CalibratedLateralAcceleration"))
  for key in frogpilot_variables.CURVE_DATA_KEYS:
    params.remove(key)


def round_half_away(value):
  return math.copysign(math.floor(abs(value) + 0.5), value)


async def run_and_report(coroutine):
  try:
    await coroutine
  except Exception as exception:
    sentry.capture_exception(exception, crash_log=False)


async def run_heavy_process(the_pond, cmd):
  async with the_pond.heavy_work:
    return await run_process(cmd)


async def run_job(coroutine):
  try:
    await coroutine
  except Exception as exception:
    sentry.capture_exception(exception, crash_log=False)
    raise


async def run_process(cmd):
  process = await asyncio.create_subprocess_exec("nice", "-n", "19", "ionice", "-c", "3", *cmd,
                                                 stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
  try:
    stdout, stderr = await process.communicate()
  finally:
    await stop_process(process)

  if process.returncode != 0:
    raise RuntimeError(f"{cmd[0]} failed ({process.returncode}): {stderr.decode(errors='replace')[-1000:]}")
  return stdout


def start_job(the_pond, name, coroutine, info):
  job = the_pond.jobs.get(name)
  if job is not None and not job[0].done():
    coroutine.close()
    raise web.HTTPConflict(text="busy")
  the_pond.jobs[name] = (asyncio.create_task(run_job(coroutine)), info)


async def stream_process(request, cmd, filename, stdin=None):
  process = await asyncio.create_subprocess_exec("nice", "-n", "19", "ionice", "-c", "3", *cmd,
                                                 stdin=asyncio.subprocess.PIPE if stdin is not None else asyncio.subprocess.DEVNULL,
                                                 stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
  try:
    if stdin is not None:
      process.stdin.write(stdin)
      await process.stdin.drain()
      process.stdin.close()

    response = web.StreamResponse(headers={"Content-Disposition": f'attachment; filename="{filename}"'})
    response.content_type = mimetypes.guess_type(filename)[0] or "application/octet-stream"
    await response.prepare(request)

    while chunk := await process.stdout.read(STREAM_CHUNK_SIZE):
      await response.write(chunk)

    stderr = await process.stderr.read()
    await process.wait()
  finally:
    await stop_process(process)

  if process.returncode != 0:
    raise RuntimeError(f"{cmd[0]} failed ({process.returncode}): {stderr.decode(errors='replace')[-1000:]}")

  await response.write_eof()
  return response


async def stop_process(process):
  if process.returncode is None:
    process.kill()
    await process.wait()
