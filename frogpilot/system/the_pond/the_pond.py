#!/usr/bin/env python3
import hashlib
import json
import math
import os
import re
import requests
import secrets
import shutil
import socket
import stat
import struct
import subprocess
import threading
import time

from contextlib import contextmanager
from datetime import datetime
from flask import Flask, Response, jsonify, render_template, request, send_file, send_from_directory, stream_with_context
from functools import wraps
from io import BytesIO
from pathlib import Path
from werkzeug.utils import secure_filename

from cereal import car, messaging
from opendbc.can.parser import CANParser
from openpilot.common.params import ParamKeyType
from openpilot.common.swaglog import cloudlog
from openpilot.selfdrive.car.toyota.carcontroller import LOCK_CMD, UNLOCK_CMD
from openpilot.system.hardware import HARDWARE, PC
from openpilot.system.hardware.hw import Paths
from openpilot.system.loggerd.deleter import PRESERVE_ATTR_NAME, PRESERVE_ATTR_VALUE, PRESERVE_COUNT
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.assets.theme_manager import HOLIDAY_THEME_PATH, POND_ACTIVE_THEME, THEME_COMPONENT_PARAMS
from openpilot.frogpilot.common.frogpilot_utilities import delete_file, extract_tar
from openpilot.frogpilot.common.frogpilot_variables import (ACTIVE_THEME_PATH, ERROR_LOGS_PATH, EXCLUDED_KEYS, RESOURCES_REPO,
                                                            SCREEN_RECORDINGS_PATH, THEME_SAVE_PATH, frogpilot_default_params, params,
                                                            params_memory, update_frogpilot_toggles)
from openpilot.frogpilot.system.the_pond import helpers, utilities
from openpilot.frogpilot.system.the_pond.theme_submission import ThemeAsset, ThemeSubmissionError, submit_theme_assets

FOOTAGE_PATHS = [
  Paths.log_root(HD=True, raw=True),
  Paths.log_root(konik=True, raw=True),
  Paths.log_root(raw=True),
]

KEYS = {
  "amap1": ("amap1", "", "AMapKey1", "Amap key #1", 39),
  "amap2": ("amap2", "", "AMapKey2", "Amap key #2", 39),
  "public": ("public", "pk.", "MapboxPublicKey", "Public key", 80),
  "secret": ("secret", "sk.", "MapboxSecretKey", "Secret key", 80),
}

_SENTRY_LOG_RE = re.compile(r"\d{4}-\d{2}-\d{2}--\d{2}-\d{2}-\d{2}\.log")
_THEME_ASSET_SUFFIXES = frozenset({".gif", ".jpeg", ".jpg", ".json", ".png", ".wav"})

TMUX_LOGS_PATH = Path("/data/tmux_logs")
TAILSCALE_BASE = "/data/tailscale"
TAILSCALE_UNIT_PATH = Path("/etc/systemd/system/tailscaled.service")
NAVIGATION_TRAINING_PATH = Path(__file__).parents[2] / "navigation/navigation_training"
MAPBOX_HELP_IMAGES = frozenset({"both_keys_set.png", "no_keys_set.png", "public_key_set.png", "setup_completed.png"})

_PARAMS_LOCK = threading.RLock()
_ROUTE_STORAGE_ROOT = Path(FOOTAGE_PATHS[-1]).parent
_ROUTE_MUTATION_LOCK_PATH = _ROUTE_STORAGE_ROOT / ".route_mutation.lock"
_ROUTE_TRASH_PATH = _ROUTE_STORAGE_ROOT / ".route_trash"
_SCREEN_RECORDINGS_LOCK_PATH = Path(SCREEN_RECORDINGS_PATH).with_suffix(".lock")
_SCREEN_RECORDINGS_STAGING_PATH = Path(f"{SCREEN_RECORDINGS_PATH}.in_progress")
_TAILSCALE_LOCK = threading.Lock()

_CMD_TIMEOUT = 60
_DRIVING_ERROR = "Unavailable while driving. Shift into Park or go offroad to use this."
_MAX_FAVORITES = 100
_MAX_SECOC_KEYS = 64
_NAVIGATION_MAX_BYTES = 4096
_TMUX_STREAM_MAX_SECONDS = 3600
_POND_PORT = 8083 if PC else 8082
_POND_PORTS = {_POND_PORT} if PC else {80, _POND_PORT}

def _toggle_backup_keys():
  return {
    key for key, _, _, _ in frogpilot_default_params
    if key not in EXCLUDED_KEYS and not (params.get_key_flag(key) & ParamKeyType.DONT_LOG)
  }

def _run_cmd(cmd, ok, fail, timeout=_CMD_TIMEOUT, env=None):
  try:
    result = subprocess.run(cmd, capture_output=True, check=True, env=env, text=True, timeout=timeout)
  except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
    print(fail)
    raise
  print(ok)
  return result.stdout.strip()

def _tmux_output():
  try:
    output = subprocess.check_output(["tmux", "capture-pane", "-p", "-S", "-1000"], text=True, timeout=5)
    return "\n".join(reversed(output.splitlines()))
  except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
    return "No active tmux session to stream."

def _tailscale_unit():
  return f"""[Unit]
Description=Tailscale node agent
After=network.target

[Service]
ExecStart={TAILSCALE_BASE}/tailscaled \\
  --tun=userspace-networking \\
  --socks5-server=localhost:1055 \\
  --state={TAILSCALE_BASE}/state/tailscaled.state \\
  --socket={TAILSCALE_BASE}/tailscaled.sock \\
  --statedir={TAILSCALE_BASE}/state
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
"""

def _tailscale_marker():
  return Path(TAILSCALE_BASE) / ".the_pond_managed"

def _tailscale_managed():
  expected_unit = _tailscale_unit()
  expected_hash = hashlib.sha256(expected_unit.encode()).hexdigest()
  try:
    marker = _tailscale_marker()
    return (
      (Path(TAILSCALE_BASE) / "tailscale").is_file()
      and (Path(TAILSCALE_BASE) / "tailscaled").is_file()
      and _normalized_unit(TAILSCALE_UNIT_PATH.read_text()) == _normalized_unit(expected_unit)
      and (not marker.exists() or marker.read_text().strip() == expected_hash)
    )
  except OSError:
    return False

def _normalized_unit(unit):
  return "\n".join(line.strip() for line in unit.splitlines() if line.strip())

def _tailscale_operation(function):
  @wraps(function)
  def wrapped(*args, **kwargs):
    if not _TAILSCALE_LOCK.acquire(blocking=False):
      return jsonify({"error": "Another Tailscale operation is already running"}), 409
    try:
      return function(*args, **kwargs)
    finally:
      _TAILSCALE_LOCK.release()
  return wrapped

@contextmanager
def _writable_root():
  was_read_only = bool(os.statvfs("/").f_flag & os.ST_RDONLY)
  try:
    if was_read_only:
      _run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Remounted / as read-write.", "Failed to remount / as read-write.")
    yield
  finally:
    if was_read_only:
      _run_cmd(["sudo", "mount", "-o", "remount,ro", "/"], "Remounted / read-only.", "Failed to restore / as read-only.")

def _delete_and_verify(path):
  delete_file(path)
  return not os.path.lexists(path)

def _quiesce_process(proc):
  if proc.poll() is None:
    try:
      _run_cmd(["sudo", "kill", "-TERM", f"-{proc.pid}"], "Stopped Tailscale setup process.", "Failed to stop Tailscale setup process.")
    except subprocess.CalledProcessError:
      if proc.poll() is None:
        raise
  try:
    proc.wait(timeout=5)
  except subprocess.TimeoutExpired:
    _run_cmd(["sudo", "kill", "-KILL", f"-{proc.pid}"], "Killed unresponsive Tailscale setup process.",
             "Failed to kill unresponsive Tailscale setup process.")
    proc.wait(timeout=5)

_CSP = "; ".join([
  "default-src 'self'",
  "script-src 'self'",
  "style-src 'self' 'unsafe-inline'",
  "img-src 'self' data: blob: https://api.mapbox.com https://*.tiles.mapbox.com",
  "font-src 'self' data:",
  "connect-src 'self' https://api.mapbox.com https://events.mapbox.com https://*.tiles.mapbox.com",
  "worker-src blob:",
  "child-src blob:",
  "object-src 'none'",
  "base-uri 'self'",
  "form-action 'self'",
  "frame-ancestors 'none'",
])

def _trusted_pond_hosts():
  hosts = {"localhost", "127.0.0.1", "::1", helpers.MDNS_HOSTNAME.lower().rstrip(".")}
  if request.remote_addr and (local_ip := _local_ip_for(request.remote_addr)):
    hosts.add(local_ip.lower().rstrip("."))
  return hosts

def _route_is_locked(segment_dir):
  if any(Path(segment_dir).glob("*.lock")):
    return True
  if not _loggerd_may_be_running():
    return False

  segment_name = Path(segment_dir).name
  current_route = params.get("CurrentRoute", encoding="utf8")
  if current_route and helpers.route_segment_matches(segment_name, current_route):
    return True

  try:
    route_count = int(params.get("RouteCount", encoding="utf8"))
  except (TypeError, ValueError):
    return not current_route
  return segment_name.startswith(f"{(route_count - 1) & 0xffffffff:08x}--")

def _valid_route_name(name):
  return isinstance(name, str) and utilities.SEGMENT_RE.fullmatch(f"{name}--0") is not None

def _route_segments(route_name=None):
  return [
    segment
    for footage_path in dict.fromkeys(FOOTAGE_PATHS)
    if os.path.exists(footage_path)
    for segment in Path(footage_path).iterdir()
    if segment.is_dir() and not segment.is_symlink() and utilities.SEGMENT_RE.fullmatch(segment.name)
    and (route_name is None or helpers.route_segment_matches(segment.name, route_name))
  ]

def _quarantine_route_segments(segments):
  try:
    _ROUTE_TRASH_PATH.mkdir(mode=0o775, parents=True, exist_ok=True)
    if _ROUTE_TRASH_PATH.is_symlink() or not _ROUTE_TRASH_PATH.is_dir():
      return [], False

    moved = []
    for segment in segments:
      destination = _ROUTE_TRASH_PATH / f"{secrets.token_hex(16)}-{segment.name}"
      try:
        segment.rename(destination)
        moved.append((segment, destination))
      except OSError:
        cloudlog.exception(f"the_pond: could not quarantine route segment {segment}")
        for original, quarantined in reversed(moved):
          try:
            quarantined.rename(original)
          except OSError:
            cloudlog.exception(f"the_pond: could not restore route segment {original}")
        return [], False
    return [destination for _, destination in moved], True
  except OSError:
    cloudlog.exception("the_pond: could not prepare the route quarantine")
    return [], False

def _delete_quarantined_routes(quarantined):
  deleted_all = True
  for route_path in quarantined:
    delete_file(str(route_path))
    deleted_all &= not route_path.exists()
  return deleted_all

def _screen_recording_path(filename):
  root = Path(SCREEN_RECORDINGS_PATH)
  recording = root / filename
  if not helpers.is_within(root, recording) or recording.suffix.lower() != ".mp4" or recording.is_symlink():
    return None
  return recording

@contextmanager
def _route_mutation_lock(exclusive):
  import fcntl

  try:
    lock_fd = os.open(_ROUTE_MUTATION_LOCK_PATH, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o664)
  except OSError:
    cloudlog.exception("the_pond: could not open the route-mutation coordination lock")
    yield None
    return

  try:
    try:
      lock_type = fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH
      fcntl.flock(lock_fd, lock_type | fcntl.LOCK_NB)
    except BlockingIOError:
      yield False
      return
    yield True
  finally:
    os.close(lock_fd)

@contextmanager
def _screen_recordings_lock(exclusive):
  import fcntl

  try:
    lock_fd = os.open(_SCREEN_RECORDINGS_LOCK_PATH, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o664)
  except OSError:
    cloudlog.exception("the_pond: could not open the screen-recording coordination lock")
    yield None
    return

  try:
    try:
      lock_type = fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH
      fcntl.flock(lock_fd, lock_type | fcntl.LOCK_NB)
    except BlockingIOError:
      yield False
      return
    yield True
  finally:
    os.close(lock_fd)

def _publish_staged_screen_recording(staging_path):
  ready_suffix = ".ready"
  final_name = staging_path.name[:-len(ready_suffix)]
  if not final_name.lower().endswith(".mp4"):
    return False

  destination = Path(SCREEN_RECORDINGS_PATH) / final_name
  suffix = 0
  while True:
    candidate = destination if suffix == 0 else destination.with_name(f"{destination.stem}-{suffix}{destination.suffix}")
    try:
      os.link(staging_path, candidate)
      staging_path.unlink()
      return True
    except FileExistsError:
      try:
        same_file = os.path.samefile(staging_path, candidate)
      except OSError:
        cloudlog.exception(f"the_pond: could not compare completed screen recording {staging_path}")
        return False
      if same_file:
        try:
          staging_path.unlink()
        except OSError:
          cloudlog.exception(f"the_pond: could not finish recovering completed screen recording {staging_path}")
          return False
        return True
      suffix += 1
      continue
    except OSError:
      cloudlog.exception(f"the_pond: could not recover completed screen recording {staging_path}")
      return False

def _reconcile_screen_recordings():
  try:
    if _SCREEN_RECORDINGS_STAGING_PATH.exists():
      if _SCREEN_RECORDINGS_STAGING_PATH.is_symlink() or not _SCREEN_RECORDINGS_STAGING_PATH.is_dir():
        return False
      for staging_path in _SCREEN_RECORDINGS_STAGING_PATH.iterdir():
        if staging_path.is_symlink() or not staging_path.is_file():
          return False
        if staging_path.name.endswith(".partial"):
          staging_path.unlink()
        elif staging_path.name.endswith(".ready"):
          if not _publish_staged_screen_recording(staging_path):
            return False
        else:
          return False
    return True
  except OSError:
    cloudlog.exception("the_pond: could not reconcile stale screen-recording state")
    return False

def _screen_recordings_need_reconciliation():
  try:
    if _SCREEN_RECORDINGS_STAGING_PATH.is_symlink():
      return True
    if not _SCREEN_RECORDINGS_STAGING_PATH.exists():
      return False
    if not _SCREEN_RECORDINGS_STAGING_PATH.is_dir():
      return True
    return next(_SCREEN_RECORDINGS_STAGING_PATH.iterdir(), None) is not None
  except OSError:
    return True

@contextmanager
def _screen_recordings_read_lock():
  reconciled = True
  if _screen_recordings_need_reconciliation():
    with _screen_recordings_lock(True) as storage_coordinated:
      if storage_coordinated is None:
        yield None
        return
      if storage_coordinated:
        reconciled = _reconcile_screen_recordings()

  if not reconciled:
    yield None
    return

  with _screen_recordings_lock(False) as storage_coordinated:
    yield storage_coordinated

def _car_params():
  cp_bytes = params.get("CarParamsPersistent")
  if not cp_bytes:
    return None
  try:
    with car.CarParams.from_bytes(cp_bytes) as cp_reader:
      return cp_reader.as_builder()
  except Exception:
    cloudlog.exception("the_pond: failed to parse CarParamsPersistent")
    return None

_DOOR_COMMAND_LOCK = threading.Lock()

def _doors_supported(cp):
  return cp is not None and cp.carName == "toyota" and HARDWARE.get_device_type() != "tici"

def _door_lock_status(can_parser, can_sock):
  can_parser.update_strings(messaging.drain_sock_raw(can_sock, wait_for_one=True))
  if not can_parser.can_valid:
    return None
  return can_parser.vl["DOOR_LOCKS"]["LOCK_STATUS"]

def _send_door_command(command, expect_locked):
  can_parser = CANParser("toyota_nodsu_pt_generated", [("DOOR_LOCKS", 3)], bus=0)
  can_sock = messaging.sub_sock("can", timeout=100)
  pm = messaging.PubMaster(["sendcan"])
  try:
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
      if params.get_bool("IsOnroad"):
        return None
      message = messaging.new_message("sendcan", 1)
      message.sendcan[0].address = 0x750
      message.sendcan[0].dat = command
      message.sendcan[0].src = 0
      try:
        pm.send("sendcan", message)
      except messaging.MultiplePublishersError:
        return None
      time.sleep(1)
      status = _door_lock_status(can_parser, can_sock)
      if status is not None and (status == 0) == expect_locked:
        return True
    return False
  finally:
    del can_sock

def _send_cached_video(mp4_file):
  try:
    file_stat = os.fstat(mp4_file.fileno())
    response = send_file(mp4_file, mimetype="video/mp4", conditional=False)
    response.content_length = file_stat.st_size
    response.last_modified = file_stat.st_mtime
    response.set_etag(f"{int(file_stat.st_mtime)}-{file_stat.st_size}")
    return response.make_conditional(request.environ, accept_ranges=True, complete_length=file_stat.st_size)
  except Exception:
    mp4_file.close()
    raise

def _stored_secoc_keys():
  try:
    keys = json.loads(params.get("SecOCKeys") or "[]")
  except (TypeError, ValueError):
    return []
  return [key for key in keys if isinstance(key, dict)]

def _redacted_secoc_keys(keys):
  return [{"name": key.get("name"), "value_set": helpers.is_valid_secoc_key(key.get("value"))} for key in keys]

def _favorite_id(fav):
  raw = f"{fav.get('longitude')},{fav.get('latitude')}|{fav.get('routeId') or ''}|{fav.get('name') or ''}"
  return hashlib.sha1(raw.encode()).hexdigest()

def _valid_coordinate(value, minimum, maximum):
  return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) and minimum <= value <= maximum

def _reboot_after_reset(reset_param):
  if params.get_bool("IsOnroad"):
    params.remove(reset_param)
    return
  HARDWARE.reboot()

_gear_lock = threading.Lock()
_parked_snapshot = {
  "parked": False,
  "fresh": False,
  "loggerd_running": False,
  "manager_current": False,
  "onroad": params.get_bool("IsOnroad"),
  "transition_time": time.monotonic(),
}

def _gear_monitor():
  sm = None
  while True:
    try:
      if sm is None:
        sm = messaging.SubMaster(["frogpilotCarState", "managerState"])
      sm.update(1000)
      fresh = sm.alive["frogpilotCarState"] and sm.valid["frogpilotCarState"]
      parked = bool(sm["frogpilotCarState"].isParked) if fresh else False
      is_onroad = params.get_bool("IsOnroad")
      now = time.monotonic()
      with _gear_lock:
        if is_onroad != _parked_snapshot["onroad"]:
          _parked_snapshot["onroad"] = is_onroad
          _parked_snapshot["manager_current"] = False
          _parked_snapshot["transition_time"] = now
        _parked_snapshot["parked"] = parked
        _parked_snapshot["fresh"] = fresh

        if sm.updated["managerState"] and sm.valid["managerState"]:
          loggerd_running = any(
            process.name == "loggerd" and (process.running or process.shouldBeRunning)
            for process in sm["managerState"].processes
          )
          if loggerd_running or now - _parked_snapshot["transition_time"] >= 1.0:
            _parked_snapshot["loggerd_running"] = loggerd_running
            _parked_snapshot["manager_current"] = True
    except Exception:
      sm = None
      with _gear_lock:
        _parked_snapshot["fresh"] = False
      time.sleep(0.5)

def _is_parked():
  with _gear_lock:
    return _parked_snapshot["fresh"] and _parked_snapshot["parked"]

def _loggerd_may_be_running():
  is_onroad = params.get_bool("IsOnroad")
  loggerd_desired = False
  if is_onroad:
    try:
      no_logging = json.loads(params_memory.get("FrogPilotToggles") or b"{}").get("no_logging")
      loggerd_desired = not isinstance(no_logging, bool) or not no_logging
    except (AttributeError, TypeError, ValueError):
      loggerd_desired = True

  with _gear_lock:
    if is_onroad != _parked_snapshot["onroad"]:
      return is_onroad or _parked_snapshot["loggerd_running"]
    return _parked_snapshot["loggerd_running"] or loggerd_desired or not _parked_snapshot["manager_current"]

def _drive_locked():
  return params.get_bool("IsOnroad") and not _is_parked()

def setup(app):
  @app.before_request
  def _request_gate():
    trusted_hosts = _trusted_pond_hosts()
    if not helpers.host_allowed(request.scheme, request.host, trusted_hosts):
      return jsonify({"error": "Unrecognized Host"}), 421

    if request.method not in ("GET", "HEAD", "OPTIONS"):
      origin = request.headers.get("Origin")
      allowed = helpers.origin_allowed(origin, request.scheme, request.host, _POND_PORTS) if origin is not None else \
                helpers.referer_allowed(request.headers.get("Referer"), request.scheme, request.host, _POND_PORTS)
      if not allowed:
        return jsonify({"error": "Cross-origin request blocked"}), 403

    if helpers.is_onroad_blocked(request.method, request.path) and _drive_locked():
      return jsonify({"error": _DRIVING_ERROR}), 423

  @app.after_request
  def _security_headers(resp):
    resp.headers["Content-Security-Policy"] = _CSP
    resp.headers["X-Content-Type-Options"] = "nosniff"
    resp.headers["X-Frame-Options"] = "DENY"
    resp.headers["Referrer-Policy"] = "same-origin"
    resp.headers["Cross-Origin-Resource-Policy"] = "same-origin"
    if request.path.startswith("/api/"):
      resp.headers["Cache-Control"] = "no-store"
    return resp

  @app.errorhandler(404)
  def not_found(_):
    if request.path.startswith(("/api/", "/assets/", "/mapbox-help/", "/screen_recordings/")):
      return jsonify({"error": "Not found"}), 404
    return render_template("index.html")

  @app.errorhandler(405)
  def method_not_allowed(_):
    if request.path.startswith("/api/"):
      return jsonify({"error": "Method not allowed"}), 405
    return render_template("index.html")

  @app.errorhandler(413)
  def payload_too_large(_):
    return jsonify({"error": "That upload is too large. The limit is 32 MB in total."}), 413

  @app.errorhandler(500)
  def internal_error(_):
    if request.path.startswith("/api/"):
      return jsonify({"error": "Internal server error"}), 500
    return render_template("index.html"), 500

  @app.route("/", methods=["GET"])
  def index():
    return render_template("index.html")

  @app.route("/api/onroad", methods=["GET"])
  def onroad_status():
    return jsonify({"onroad": _drive_locked()})

  @app.route("/api/doors_available", methods=["GET"])
  def doors_available():
    return jsonify({"result": _doors_supported(_car_params())})

  @app.route("/api/doors/lock", methods=["POST"])
  def lock_doors():
    if not _doors_supported(_car_params()):
      return jsonify({"error": "Door control is not supported on this device"}), 409
    if params.get_bool("IsOnroad"):
      return jsonify({"error": "Door control is only available when the car is off"}), 409
    if not _DOOR_COMMAND_LOCK.acquire(blocking=False):
      return jsonify({"error": "Another door command is already running"}), 409
    try:
      cloudlog.warning("the_pond audit: door lock requested")
      locked = _send_door_command(LOCK_CMD, True)
      if locked:
        return {"message": "Doors locked!"}
      if locked is None:
        return jsonify({"error": "The car is already locking up on its own. Try again in a moment."}), 409
      return jsonify({"error": "Timed out waiting for doors to lock"}), 504
    finally:
      _DOOR_COMMAND_LOCK.release()

  @app.route("/api/doors/unlock", methods=["POST"])
  def unlock_doors():
    if not _doors_supported(_car_params()):
      return jsonify({"error": "Door control is not supported on this device"}), 409
    if params.get_bool("IsOnroad"):
      return jsonify({"error": "Door control is only available when the car is off"}), 409
    if not _DOOR_COMMAND_LOCK.acquire(blocking=False):
      return jsonify({"error": "Another door command is already running"}), 409
    try:
      cloudlog.warning("the_pond audit: door unlock requested")
      unlocked = _send_door_command(UNLOCK_CMD, False)
      if unlocked:
        return {"message": "Doors unlocked!"}
      if unlocked is None:
        return jsonify({"error": "The car is already locking up on its own. Try again in a moment."}), 409
      return jsonify({"error": "Timed out waiting for doors to unlock"}), 504
    finally:
      _DOOR_COMMAND_LOCK.release()

  @app.route("/api/error_logs", methods=["GET"])
  def get_error_logs():
    if not os.path.exists(ERROR_LOGS_PATH):
      return jsonify([]), 200
    files = utilities.list_file(ERROR_LOGS_PATH)
    filtered = [file for file in files if file.startswith("error") or _SENTRY_LOG_RE.fullmatch(file)]
    return jsonify(filtered), 200

  @app.route("/api/error_logs/delete_all", methods=["DELETE"])
  def delete_all_error_logs():
    try:
      targets = [os.path.join(ERROR_LOGS_PATH, name) for name in os.listdir(ERROR_LOGS_PATH)]
    except FileNotFoundError:
      targets = []
    failed = [path for path in targets if not _delete_and_verify(path)]
    if failed:
      return jsonify({"error": "Some error logs could not be deleted"}), 500
    return {"message": "All error logs deleted!"}, 200

  @app.route("/api/error_logs/<filename>", methods=["DELETE"])
  def delete_error_log(filename):
    safe = secure_filename(filename)
    path = os.path.join(ERROR_LOGS_PATH, safe)
    if not safe or not os.path.isfile(path):
      return jsonify({"error": "Not found"}), 404
    if not _delete_and_verify(path):
      return jsonify({"error": "Error log could not be deleted"}), 500
    return {"message": "Error log deleted!"}

  @app.route("/api/error_logs/<filename>", methods=["GET"])
  def get_error_log(filename):
    safe = secure_filename(filename)
    path = os.path.join(ERROR_LOGS_PATH, safe)
    if not safe or not os.path.isfile(path):
      return jsonify({"error": "Not found"}), 404
    with open(path) as file:
      return file.read(), 200, {"Content-Type": "text/plain; charset=utf-8"}

  @app.route("/api/navigation", methods=["DELETE"])
  def clear_navigation():
    params.remove("NavDestination")
    params.remove("NavDestinationWaypoints")
    return {"message": "Destination cleared"}

  @app.route("/api/navigation", methods=["GET"])
  def navigation():
    try:
      last_position = json.loads(params.get("LastGPSPosition", encoding="utf8") or "{}")
    except (ValueError, TypeError):
      last_position = {}
    latitude = last_position.get("latitude") if isinstance(last_position, dict) else None
    longitude = last_position.get("longitude") if isinstance(last_position, dict) else None
    last_position = {
      "latitude": str(latitude),
      "longitude": str(longitude),
    } if _valid_coordinate(latitude, -90, 90) and _valid_coordinate(longitude, -180, 180) else None

    return {
      "amap1KeySet": bool(params.get("AMapKey1", encoding="utf8")),
      "amap2KeySet": bool(params.get("AMapKey2", encoding="utf8")),
      "destination": params.get("NavDestination", encoding="utf8") or "",
      "isMetric": params.get_bool("IsMetric"),
      "lastPosition": last_position,
      "mapboxPublic": params.get("MapboxPublicKey", encoding="utf8") or "",
      "mapboxSecretSet": bool(params.get("MapboxSecretKey", encoding="utf8")),
      "previousDestinations": params.get("ApiCache_NavDestinations", encoding="utf8") or "",
    }

  @app.route("/api/navigation", methods=["POST"])
  def set_navigation():
    if request.content_length is not None and request.content_length > _NAVIGATION_MAX_BYTES:
      return jsonify({"error": "Destination is too large"}), 413
    payload = request.get_json(silent=True) or {}
    if not isinstance(payload, dict) or not set(payload).issubset({"latitude", "longitude", "name"}):
      return jsonify({"error": "Invalid destination"}), 400
    latitude = payload.get("latitude")
    longitude = payload.get("longitude")
    if not _valid_coordinate(latitude, -90, 90) or not _valid_coordinate(longitude, -180, 180):
      return jsonify({"error": "Invalid destination coordinates"}), 400
    name = payload.get("name")
    if name is not None and not helpers.is_safe_display_name(name):
      return jsonify({"error": "Invalid destination name"}), 400
    params.remove("NavDestinationWaypoints")
    params.put("NavDestination", json.dumps(payload))
    return {"message": "Destination set"}

  @app.route("/api/navigation/favorite", methods=["DELETE"])
  def remove_favorite_destination():
    if request.content_length is not None and request.content_length > _NAVIGATION_MAX_BYTES:
      return jsonify({"error": "Favorite is too large"}), 413
    to_remove = request.get_json(silent=True) or {}
    if not isinstance(to_remove, dict) or not set(to_remove).issubset({"id", "latitude", "longitude", "name", "routeId"}):
      return jsonify({"error": "Invalid favorite"}), 400

    with _PARAMS_LOCK:
      existing = json.loads(params.get("FavoriteDestinations", encoding="utf8") or "[]")
      fid = to_remove.get("id")
      if fid:
        favorites = [f for f in existing if (f.get("id") or _favorite_id(f)) != fid]
      else:
        favorites = [
          f for f in existing
          if not (
            f.get("routeId") == to_remove.get("routeId") and
            f.get("latitude") == to_remove.get("latitude") and
            f.get("longitude") == to_remove.get("longitude") and
            f.get("name") == to_remove.get("name")
          )
        ]
      params.put("FavoriteDestinations", json.dumps(favorites))
    return jsonify(message="Destination removed from favorites!")

  @app.route("/api/navigation/favorite", methods=["GET"])
  def list_favorite_destinations():
    with _PARAMS_LOCK:
      favorites = json.loads(params.get("FavoriteDestinations", encoding="utf8") or "[]")
    for f in favorites:
      f.setdefault("id", _favorite_id(f))
    return jsonify(favorites=favorites)

  @app.route("/api/navigation/favorite", methods=["POST"])
  def add_favorite_destination():
    if request.content_length is not None and request.content_length > _NAVIGATION_MAX_BYTES:
      return jsonify({"error": "Favorite is too large"}), 413
    new_fav = request.get_json(silent=True) or {}
    allowed_fields = {"id", "is_home", "is_work", "latitude", "longitude", "name", "routeId"}
    if not isinstance(new_fav, dict) or not set(new_fav).issubset(allowed_fields):
      return jsonify({"error": "Invalid favorite"}), 400

    name = new_fav.get("name")
    if not helpers.is_safe_display_name(name) or not _valid_coordinate(new_fav.get("latitude"), -90, 90) or \
        not _valid_coordinate(new_fav.get("longitude"), -180, 180):
      return jsonify({"error": "Invalid favorite name"}), 400
    if any(field in new_fav and not isinstance(new_fav[field], bool) for field in ("is_home", "is_work")):
      return jsonify({"error": "Invalid favorite"}), 400
    if any(field in new_fav and (not isinstance(new_fav[field], str) or len(new_fav[field]) > 256) for field in ("id", "routeId")):
      return jsonify({"error": "Invalid favorite"}), 400

    new_fav.setdefault("id", _favorite_id(new_fav))

    with _PARAMS_LOCK:
      existing = json.loads(params.get("FavoriteDestinations", encoding="utf8") or "[]")
      if len(existing) >= _MAX_FAVORITES and not any(f.get("id") == new_fav["id"] for f in existing):
        return jsonify({"error": f"A maximum of {_MAX_FAVORITES} favorites can be saved"}), 400
      if not any(f.get("id") == new_fav["id"] for f in existing):
        existing.append(new_fav)
      params.put("FavoriteDestinations", json.dumps(existing))
    return {"message": "Destination added to favorites!"}

  @app.route("/api/navigation/favorite/rename", methods=["POST"])
  def rename_favorite_destination():
    if request.content_length is not None and request.content_length > _NAVIGATION_MAX_BYTES:
      return jsonify({"error": "Favorite is too large"}), 413
    data = request.get_json(silent=True) or {}
    if not isinstance(data, dict) or not set(data).issubset({"id", "is_home", "is_work", "name", "routeId"}):
      return jsonify({"error": "Invalid favorite"}), 400
    fid = data.get("id")
    route_id_to_rename = data.get("routeId")
    new_name = data.get("name")
    is_home = data.get("is_home")
    is_work = data.get("is_work")

    if not fid and not route_id_to_rename:
      return jsonify({"error": "Missing id or routeId"}), 400

    if new_name is not None and not helpers.is_safe_display_name(new_name):
      return jsonify({"error": "Invalid favorite name"}), 400
    if any(value is not None and not isinstance(value, bool) for value in (is_home, is_work)):
      return jsonify({"error": "Invalid favorite"}), 400

    with _PARAMS_LOCK:
      existing_favorites = json.loads(params.get("FavoriteDestinations", encoding="utf8") or "[]")

      if is_home:
        for favorite in existing_favorites:
          favorite.pop("is_home", None)
      if is_work:
        for favorite in existing_favorites:
          favorite.pop("is_work", None)

      found = False
      for favorite in existing_favorites:
        if (fid and favorite.get("id") == fid) or (not fid and favorite.get("routeId") == route_id_to_rename):
          if new_name:
            favorite["name"] = new_name

          if is_home is not None:
            if is_home:
              favorite["is_home"] = True
              favorite.pop("is_work", None)
            else:
              favorite.pop("is_home", None)

          if is_work is not None:
            if is_work:
              favorite["is_work"] = True
              favorite.pop("is_home", None)
            else:
              favorite.pop("is_work", None)

          found = True
          break

      if not found:
        return jsonify({"error": "Favorite not found"}), 404

      params.put("FavoriteDestinations", json.dumps(existing_favorites))
    return jsonify(message="Favorite updated successfully!")

  @app.route("/api/navigation_key", methods=["DELETE"])
  def delete_navigation_key():
    meta = KEYS.get(request.args.get("type"))
    if not meta:
      return jsonify(error="Unknown key type"), 400
    params.remove(meta[2])
    return jsonify(message=f"{meta[3]} deleted successfully!")

  @app.route("/api/navigation_key", methods=["POST"])
  def set_navigation_keys():
    data = request.get_json() or {}

    updates = []
    for meta in KEYS.values():
      raw = (data.get(meta[0]) or "").strip()
      if not raw:
        continue

      full = raw if raw.startswith(meta[1]) else meta[1] + raw
      if len(full) < meta[4]:
        return jsonify(error=f"{meta[3]} is invalid or too short..."), 400

      updates.append((meta[2], full, meta[3]))

    if not updates:
      return jsonify(error="Nothing to update..."), 400

    for param, value, _ in updates:
      params.put(param, value)

    return jsonify(message=f"{', '.join(label for _, _, label in updates)} saved successfully!")

  @app.route("/api/params", methods=["GET"])
  def get_param():
    key = request.args.get("key")
    if key not in helpers.PARAM_GET_ALLOWLIST:
      return jsonify({"error": "Forbidden"}), 403
    return params.get(key) or "", 200

  @app.route("/api/params_memory", methods=["GET"])
  def get_param_memory():
    key = request.args.get("key")
    if key not in helpers.PARAM_MEMORY_GET_ALLOWLIST:
      return jsonify({"error": "Forbidden"}), 403
    return params_memory.get(key) or "", 200

  @app.route("/api/routes", methods=["GET"])
  def list_routes():
    def generate():
      routes = [(path, name) for path in FOOTAGE_PATHS for name in utilities.get_routes_names(path)]
      total = len(routes)
      yield f"data: {json.dumps({'progress': 0, 'total': total})}\n\n"
      for processed, (path, name) in enumerate(routes, start=1):
        try:
          result = utilities.route_metadata(path, name)
          yield f"data: {json.dumps({'routes': [result]})}\n\n"
        except Exception:
          cloudlog.exception(f"the_pond list_routes: failed to process route {name}")
        yield f"data: {json.dumps({'progress': processed, 'total': total})}\n\n"

    return Response(stream_with_context(generate()), mimetype="text/event-stream")

  @app.route("/api/routes/<name>", methods=["DELETE"])
  def delete_route(name):
    if not _valid_route_name(name):
      return {"error": "Invalid route name"}, 400

    with _route_mutation_lock(True) as mutation_coordinated:
      if mutation_coordinated is None:
        return {"error": "Could not coordinate route storage"}, 500
      if not mutation_coordinated:
        return {"error": "Another route mutation is in progress"}, 423
      segments = _route_segments(name)
      if not segments:
        return {"error": "Route not found"}, 404
      if any(_route_is_locked(segment) for segment in segments):
        return {"error": "Route is still being written"}, 423
      quarantined, moved_all = _quarantine_route_segments(segments)
      if not moved_all:
        return {"error": "Route deletion incomplete"}, 500
      if not _delete_quarantined_routes(quarantined):
        return {"error": "Route deletion incomplete"}, 500
    return {"message": "Route deleted!"}, 200

  @app.route("/api/routes/delete_all", methods=["DELETE"])
  def delete_all_routes():
    with _route_mutation_lock(True) as mutation_coordinated:
      if mutation_coordinated is None:
        return {"error": "Could not coordinate route storage"}, 500
      if not mutation_coordinated:
        return {"error": "Another route mutation is in progress"}, 423
      segments = _route_segments()
      if any(_route_is_locked(segment) for segment in segments):
        return {"error": "A route is still being written"}, 423
      quarantined, moved_all = _quarantine_route_segments(segments)
      if not moved_all:
        return {"error": "Route deletion incomplete"}, 500
      if not _delete_quarantined_routes(quarantined):
        return {"error": "Route deletion incomplete"}, 500
    return {"message": "All routes deleted!"}, 200

  @app.route("/api/routes/<name>/preserve", methods=["POST"])
  def preserve_route(name):
    if not _valid_route_name(name):
      return {"error": "Invalid route name"}, 400

    with _route_mutation_lock(True) as mutation_coordinated:
      if mutation_coordinated is None:
        return {"error": "Could not coordinate route storage"}, 500
      if not mutation_coordinated:
        return {"error": "Another route mutation is in progress"}, 423
      preserved_routes = sum(
        PRESERVE_ATTR_NAME in os.listxattr(segment) and os.getxattr(segment, PRESERVE_ATTR_NAME) == PRESERVE_ATTR_VALUE
        for segment in _route_segments()
        if segment.name.endswith("--0")
      )

      if preserved_routes >= PRESERVE_COUNT:
        return {"error": f"Maximum of {PRESERVE_COUNT} preserved routes reached..."}, 400

      route_path = next((segment for segment in _route_segments(name) if segment.name == f"{name}--0"), None)
      if route_path is not None:
        if _route_is_locked(route_path):
          return {"error": "Route is still being written"}, 423
        os.setxattr(route_path, PRESERVE_ATTR_NAME, PRESERVE_ATTR_VALUE)
        return {"message": "Route preserved!!"}, 200

    return {"error": "Route not found"}, 404

  @app.route("/api/routes/<name>/preserve", methods=["DELETE"])
  def un_preserve_route(name):
    if not _valid_route_name(name):
      return {"error": "Invalid route name"}, 400

    with _route_mutation_lock(True) as mutation_coordinated:
      if mutation_coordinated is None:
        return {"error": "Could not coordinate route storage"}, 500
      if not mutation_coordinated:
        return {"error": "Another route mutation is in progress"}, 423
      route_path = next((segment for segment in _route_segments(name) if segment.name == f"{name}--0"), None)
      if route_path is not None and PRESERVE_ATTR_NAME in os.listxattr(route_path):
        if _route_is_locked(route_path):
          return {"error": "Route is still being written"}, 423
        os.removexattr(route_path, PRESERVE_ATTR_NAME)
        return {"message": "Route unpreserved!"}, 200
    return {"error": "Route not found"}, 404

  @app.route("/video/<name>/combined", methods=["GET"])
  def get_combined_route_video(name):
    camera = request.args.get("camera", "forward")
    camera_files = {"driver": "dcamera.hevc", "forward": "fcamera.hevc", "wide": "ecamera.hevc"}
    if camera not in camera_files:
      return {"error": "Unknown camera"}, 400
    input_files = None
    source_fds = []
    with _route_mutation_lock(False) as mutation_coordinated:
      if mutation_coordinated is None:
        return {"error": "Could not coordinate route storage"}, 500
      if not mutation_coordinated:
        return {"error": "Another route mutation is in progress"}, 423

      for footage_path in FOOTAGE_PATHS:
        segments = utilities.get_segments_in_route(name, footage_path)
        if not segments:
          continue
        segment_dirs = [Path(footage_path) / segment for segment in segments]
        if any(_route_is_locked(segment_dir) for segment_dir in segment_dirs):
          return {"error": "Route is still being written"}, 423

        cam_file = camera_files[camera]
        input_files = [
          str(segment_dir / cam_file)
          for segment_dir in segment_dirs
          if (segment_dir / cam_file).exists()
        ]
        if not input_files:
          return {"error": "No video files found"}, 404

        try:
          for input_file in input_files:
            source_fd = os.open(input_file, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
            try:
              if not stat.S_ISREG(os.fstat(source_fd).st_mode):
                raise OSError("combined-video source is not a regular file")
            except OSError:
              os.close(source_fd)
              raise
            source_fds.append(source_fd)
        except OSError:
          for source_fd in source_fds:
            os.close(source_fd)
          cloudlog.exception(f"the_pond: could not snapshot combined route video for {name}")
          return {"error": "Could not read video source"}, 500
        break

    if input_files is None:
      return {"error": "Route not found"}, 404

    try:
      source_paths = [f"/proc/{os.getpid()}/fd/{source_fd}" for source_fd in source_fds]
      mp4_file = utilities.ffmpeg_concat_segments_to_mp4(
        source_paths,
        cache_key=f"{name}-{camera}",
        cache_identity_files=input_files,
      )
    except (OSError, ValueError):
      cloudlog.exception(f"the_pond: could not process the combined route video for {name}")
      return {"error": "Could not process video"}, 503
    finally:
      for source_fd in source_fds:
        os.close(source_fd)

    return _send_cached_video(mp4_file)

  @app.route("/api/routes/<name>", methods=["GET"])
  def get_route(name):
    for footage_path in FOOTAGE_PATHS:
      base_path = os.path.join(footage_path, f"{name}--0")
      if os.path.exists(base_path):
        segments = utilities.get_segments_in_route(name, footage_path)
        if not segments:
          break

        segment_urls = [f"/video/{segment}" for segment in segments]
        total_duration = sum(utilities.get_video_duration(os.path.join(footage_path, segment, "fcamera.hevc")) for segment in segments)
        return {
          "name": name,
          "segment_urls": segment_urls,
          "total_duration": round(total_duration),
          "date": utilities.get_route_start_time(os.path.join(base_path, "rlog")),
          "available_cameras": utilities.get_available_cameras(base_path),
        }, 200
    return {"error": "Route not found"}, 404

  @app.route("/api/routes/clear_name", methods=["POST"])
  def clear_route_name():
    data = request.get_json()
    if not isinstance(data, dict):
      return jsonify({"error": "Invalid request"}), 400
    route_name = data.get("name")

    if not route_name:
      return jsonify({"error": "Missing route name"}), 400
    if not _valid_route_name(route_name):
      return jsonify({"error": "Invalid route name"}), 400

    with _route_mutation_lock(True) as mutation_coordinated:
      if mutation_coordinated is None:
        return jsonify({"error": "Could not coordinate route storage"}), 500
      if not mutation_coordinated:
        return jsonify({"error": "Another route mutation is in progress"}), 423
      segments = _route_segments(route_name)
      if not segments:
        return jsonify({"error": "Route not found or no custom name to clear"}), 404
      if any(_route_is_locked(segment) for segment in segments):
        return jsonify({"error": "Route is still being written"}), 423

      found_marker = False
      removal_failed = False
      for segment_dir in segments:
        for item in os.listdir(segment_dir):
          if not utilities.is_route_entry_protected(item):
            found_marker = True
            try:
              os.remove(os.path.join(segment_dir, item))
            except OSError:
              removal_failed = True
              cloudlog.exception(f"the_pond clear_route_name: could not remove name marker {item} from {segment_dir}")

      if removal_failed:
        return jsonify({"error": "Could not clear route name completely"}), 500
      if not found_marker:
        return jsonify({"error": "Route not found or no custom name to clear"}), 404

      timestamp_segment = next((segment for segment in segments if segment.name == f"{route_name}--0"), segments[0])
      route_timestamp_dt = utilities.get_route_start_time(timestamp_segment / "rlog")
      original_timestamp = route_timestamp_dt.isoformat() if route_timestamp_dt else None
    return jsonify({"message": "Route name cleared successfully!", "timestamp": original_timestamp}), 200

  @app.route("/api/routes/rename", methods=["POST"])
  def rename_route():
    data = request.get_json()
    if not isinstance(data, dict):
      return jsonify({"error": "Invalid request"}), 400
    old_name = data.get("old")
    new_name_raw = data.get("new")

    if not isinstance(old_name, str) or not isinstance(new_name_raw, str) or not old_name or not new_name_raw:
      return jsonify({"error": "Missing old or new name"}), 400
    if not _valid_route_name(old_name):
      return jsonify({"error": "Invalid route name"}), 400

    new_name = secure_filename(new_name_raw)
    if not new_name or utilities.is_route_entry_protected(new_name):
      return jsonify({"error": "Invalid new name"}), 400

    with _route_mutation_lock(True) as mutation_coordinated:
      if mutation_coordinated is None:
        return jsonify({"error": "Could not coordinate route storage"}), 500
      if not mutation_coordinated:
        return jsonify({"error": "Another route mutation is in progress"}), 423
      segments = _route_segments(old_name)
      if not segments:
        return jsonify({"error": "Route not found"}), 404
      if any(_route_is_locked(segment) for segment in segments):
        return jsonify({"error": "Route is still being written"}), 423

      try:
        old_markers = [
          os.path.join(segment_dir, item)
          for segment_dir in segments
          for item in os.listdir(segment_dir)
          if not utilities.is_route_entry_protected(item) and item != new_name
        ]
      except OSError:
        cloudlog.exception(f"the_pond rename_route: could not inspect route markers for {old_name}")
        return jsonify({"error": "Could not rename route"}), 500

      created_markers = []
      for segment_dir in segments:
        new_name_file_path = os.path.join(segment_dir, new_name)
        target_existed = os.path.lexists(new_name_file_path)
        try:
          with open(new_name_file_path, "a"):
            pass
          if not target_existed:
            created_markers.append(new_name_file_path)
        except OSError as e:
          cloudlog.exception(f"the_pond rename_route: could not write name marker in {segment_dir}: {e}")
          for created_marker in created_markers:
            try:
              if os.path.lexists(created_marker):
                os.remove(created_marker)
            except OSError:
              cloudlog.exception(f"the_pond rename_route: could not roll back new marker {created_marker}")
          return jsonify({"error": "Could not rename route"}), 500

      removed_markers = []
      for old_marker in old_markers:
        try:
          os.remove(old_marker)
          removed_markers.append(old_marker)
        except OSError:
          cloudlog.exception(f"the_pond rename_route: could not remove old marker {old_marker}")
          for removed_marker in removed_markers:
            try:
              with open(removed_marker, "a"):
                pass
            except OSError:
              cloudlog.exception(f"the_pond rename_route: could not restore old marker {removed_marker}")
          for created_marker in created_markers:
            try:
              if os.path.lexists(created_marker):
                os.remove(created_marker)
            except OSError:
              cloudlog.exception(f"the_pond rename_route: could not roll back new marker {created_marker}")
          return jsonify({"error": "Could not rename route"}), 500

    return jsonify({"message": "Route renamed successfully!"}), 200

  @app.route("/api/screen_recordings/delete/<path:filename>", methods=["DELETE"])
  def delete_screen_recording(filename):
    mp4_path = _screen_recording_path(filename)
    if mp4_path is None:
      return {"error": "Forbidden"}, 403

    with _screen_recordings_lock(True) as storage_coordinated:
      if storage_coordinated is None:
        return {"error": "Could not coordinate screen recordings"}, 500
      if not storage_coordinated:
        return {"error": "A recording is still being written"}, 423
      if not _reconcile_screen_recordings():
        return {"error": "Could not reconcile screen recordings"}, 500
      if not mp4_path.is_file():
        return {"error": "File not found"}, 404

      for ext in (".png", ".gif"):
        thumb = mp4_path.with_suffix(ext)
        if thumb.exists() or thumb.is_symlink():
          delete_file(str(thumb))
          if thumb.exists() or thumb.is_symlink():
            return {"error": "Could not delete recording assets"}, 500

      delete_file(str(mp4_path))
      if mp4_path.exists() or mp4_path.is_symlink():
        return {"error": "Could not delete recording"}, 500

    return {"message": "Deleted"}, 200

  @app.route("/api/screen_recordings/delete_all", methods=["DELETE"])
  def delete_all_screen_recordings():
    with _screen_recordings_lock(True) as storage_coordinated:
      if storage_coordinated is None:
        return {"error": "Could not coordinate screen recordings"}, 500
      if not storage_coordinated:
        return {"error": "A recording is still being written"}, 423
      if not _reconcile_screen_recordings():
        return {"error": "Could not reconcile screen recordings"}, 500

      recordings = [recording for recording in Path(SCREEN_RECORDINGS_PATH).glob("*.mp4") if recording.is_file() and not recording.is_symlink()]
      deletion_failed = False
      for recording in recordings:
        companion_failed = False
        for ext in (".png", ".gif"):
          thumb = recording.with_suffix(ext)
          if thumb.exists() or thumb.is_symlink():
            delete_file(str(thumb))
            if thumb.exists() or thumb.is_symlink():
              companion_failed = True
              deletion_failed = True
        if companion_failed:
          continue
        delete_file(str(recording))
        if recording.exists() or recording.is_symlink():
          deletion_failed = True
      if deletion_failed:
        return {"error": "Some screen recordings could not be deleted"}, 500
    return {"message": "All screen recordings deleted!"}, 200

  @app.route("/api/screen_recordings/download/<path:filename>", methods=["GET"])
  def download_screen_recording(filename):
    mp4_path = _screen_recording_path(filename)
    if mp4_path is None:
      return {"error": "Forbidden"}, 403
    with _screen_recordings_read_lock() as storage_coordinated:
      if storage_coordinated is None:
        return {"error": "Could not coordinate screen recordings"}, 500
      if not storage_coordinated:
        return {"error": "Screen recordings are being modified"}, 423
      if not mp4_path.is_file():
        return {"error": "File not found"}, 404
      return send_from_directory(SCREEN_RECORDINGS_PATH, filename, as_attachment=True)

  @app.route("/api/screen_recordings/list", methods=["GET"])
  def list_screen_recordings():
    with _screen_recordings_read_lock() as storage_coordinated:
      if storage_coordinated is None:
        return {"error": "Could not coordinate screen recordings"}, 500
      if not storage_coordinated:
        return {"error": "Screen recordings are being modified"}, 423
      recordings = sorted(
        [
          recording
          for recording in SCREEN_RECORDINGS_PATH.glob("*.mp4")
          if recording.is_file() and not recording.is_symlink()
        ],
        key=lambda p: p.stat().st_mtime,
        reverse=True
      )
      metadata = []
      for mp4 in recordings:
        try:
          metadata.append(utilities.screen_recording_metadata(mp4))
        except Exception:
          metadata.append(None)
          cloudlog.exception(f"the_pond list_screen_recordings: failed to process {mp4.name}")

    def generate():
      total = len(recordings)

      yield f"data: {json.dumps({'progress': 0, 'total': total})}\n\n"
      for processed, result in enumerate(metadata, start=1):
        if result is not None:
          yield f"data: {json.dumps({'recordings': [result]})}\n\n"
        yield f"data: {json.dumps({'progress': processed, 'total': total})}\n\n"

    return Response(stream_with_context(generate()), mimetype="text/event-stream")

  @app.route("/screen_recordings/<path:filename>", methods=["GET"])
  def serve_screen_recording_asset(filename):
    if Path(filename).name != filename or Path(filename).suffix.lower() not in (".gif", ".png"):
      return {"error": "Not found"}, 404
    asset = SCREEN_RECORDINGS_PATH / filename
    if not helpers.is_within(SCREEN_RECORDINGS_PATH, asset) or asset.is_symlink():
      return {"error": "Forbidden"}, 403
    creating_thumbnail = not asset.exists() and asset.suffix in (".png", ".gif")
    source_fd = None
    with _screen_recordings_read_lock() as storage_coordinated:
      if storage_coordinated is None:
        return {"error": "Could not coordinate screen recordings"}, 500
      if not storage_coordinated:
        return {"error": "Screen recordings are busy"}, 423
      mp4 = _screen_recording_path(str(Path(filename).with_suffix(".mp4")))
      if not creating_thumbnail or asset.exists() or mp4 is None or not mp4.is_file():
        return send_from_directory(SCREEN_RECORDINGS_PATH, filename)
      try:
        source_fd = os.open(mp4, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
        if not stat.S_ISREG(os.fstat(source_fd).st_mode):
          raise OSError("screen-recording source is not a regular file")
      except OSError:
        if source_fd is not None:
          os.close(source_fd)
        cloudlog.exception(f"the_pond: could not snapshot screen-recording thumbnail source {mp4}")
        return {"error": "Could not create thumbnail"}, 500

    temporary_asset = asset.with_name(f".{asset.stem}.{secrets.token_hex(8)}{asset.suffix}")
    try:
      source_path = f"/proc/{os.getpid()}/fd/{source_fd}"
      if asset.suffix == ".png":
        utilities.video_to_png(source_path, temporary_asset)
      else:
        utilities.video_to_gif(source_path, temporary_asset)
      if not temporary_asset.is_file() or temporary_asset.stat().st_size == 0:
        raise OSError("thumbnail generation produced no output")
      with _screen_recordings_read_lock() as storage_coordinated:
        if storage_coordinated and mp4.is_file():
          os.replace(temporary_asset, asset)
    except (OSError, subprocess.SubprocessError):
      cloudlog.exception(f"the_pond: could not create screen-recording thumbnail {asset}")
      return {"error": "Could not create thumbnail"}, 500
    finally:
      os.close(source_fd)
      temporary_asset.unlink(missing_ok=True)
    return send_from_directory(SCREEN_RECORDINGS_PATH, filename)

  @app.route("/api/screen_recordings/rename", methods=["POST"])
  def rename_screen_recording():
    data = request.get_json()
    if not isinstance(data, dict):
      return {"error": "Invalid request"}, 400

    old = data.get("old")
    new_raw = data.get("new")

    if not isinstance(old, str) or not isinstance(new_raw, str) or not old or not new_raw:
      return {"error": "Missing filenames"}, 400

    stem = secure_filename(new_raw[:-4] if new_raw.lower().endswith(".mp4") else new_raw)
    if not stem:
      return {"error": "Invalid new name"}, 400
    new = f"{stem}.mp4"
    old_path = _screen_recording_path(old)
    new_path = _screen_recording_path(new)
    if old_path is None or new_path is None:
      return {"error": "Forbidden"}, 403

    with _screen_recordings_lock(True) as storage_coordinated:
      if storage_coordinated is None:
        return {"error": "Could not coordinate screen recordings"}, 500
      if not storage_coordinated:
        return {"error": "A recording is still being written"}, 423
      if not _reconcile_screen_recordings():
        return {"error": "Could not reconcile screen recordings"}, 500
      if not old_path.is_file():
        return {"error": "Original file not found"}, 404
      old_assets = [old_path, old_path.with_suffix(".png"), old_path.with_suffix(".gif")]
      new_assets = [new_path, new_path.with_suffix(".png"), new_path.with_suffix(".gif")]
      if any(path.exists() for path in new_assets):
        return {"error": "Target file already exists"}, 400

      renamed = []
      try:
        for source, target in zip(old_assets, new_assets, strict=True):
          if source.exists():
            source.rename(target)
            renamed.append((source, target))
      except OSError:
        for source, target in reversed(renamed):
          target.rename(source)
        return {"error": "Could not rename recording"}, 500

    return {"message": "Renamed"}, 200

  @app.route("/api/speed_limits", methods=["POST"])
  def speed_limits():
    buffer = BytesIO(params.get("SpeedLimits") or b"[]")
    buffer.seek(0)
    return send_file(buffer, as_attachment=True, download_name="speed_limits.json", mimetype="application/json")

  @app.route("/api/stats", methods=["GET"])
  def get_stats():
    build_metadata = get_build_metadata()

    short_branch = build_metadata.channel
    if short_branch == "FrogPilot-Development":
      env = "Development"
    elif build_metadata.release_channel:
      env = "Release"
    elif short_branch == "FrogPilot-Testing":
      env = "Testing"
    elif short_branch == "FrogPilot-Vetting":
      env = "Vetting"
    elif build_metadata.tested_channel:
      env = "Staging"
    else:
      env = short_branch

    return {
      "diskUsage": utilities.get_disk_usage(),
      "driveStats": utilities.get_drive_stats(),
      "softwareInfo": {
        "branchName": build_metadata.channel,
        "buildEnvironment": env,
        "commitHash": build_metadata.openpilot.git_commit,
        "forkMaintainer": utilities.get_repo_owner(build_metadata.openpilot.git_normalized_origin),
        "updateAvailable": "Yes" if params.get_bool("UpdaterFetchAvailable") else "No",
        "versionDate": utilities.format_git_date(build_metadata.openpilot.git_commit_date),
      },
    }

  @app.route("/api/tailscale/installed", methods=["GET"])
  def tailscale_installed():
    managed = _tailscale_managed()
    external = not managed and (TAILSCALE_UNIT_PATH.exists() or shutil.which("tailscale") is not None)
    return jsonify({"external": external, "installed": managed or external, "managed": managed})

  @app.route("/api/tailscale/setup", methods=["POST"])
  @_tailscale_operation
  def tailscale_setup():
    cloudlog.warning("the_pond audit: tailscale setup (sudo install systemd unit) requested")
    arch = "arm64"
    base = TAILSCALE_BASE

    if (TAILSCALE_UNIT_PATH.exists() or shutil.which("tailscale")) and not _tailscale_managed():
      return jsonify({"error": "An external Tailscale installation already owns the system service"}), 409

    version = "1.84.0"
    try:
      idx = requests.get("https://pkgs.tailscale.com/stable/", timeout=15)
      idx.raise_for_status()
      found = re.findall(r"tailscale_(\d+\.\d+\.\d+)_", idx.text)
      if found:
        version = max(found, key=lambda v: tuple(map(int, v.split("."))))
    except requests.RequestException:
      pass

    bin_dir = f"{base}/tailscale_{version}_{arch}"
    state = f"{base}/state"
    socket = f"{base}/tailscaled.sock"
    tgz_path = f"{base}/tailscale.tgz"

    tgz_url = f"https://pkgs.tailscale.com/stable/tailscale_{version}_{arch}.tgz"

    os.makedirs(state, exist_ok=True)

    _run_cmd(["curl", "-fsSL", "--connect-timeout", "15", "--max-time", "300", tgz_url, "-o", tgz_path],
             "Downloaded Tailscale archive.", "Failed to download Tailscale archive.", timeout=320)

    try:
      sums = requests.get(tgz_url + ".sha256", timeout=15)
      sums.raise_for_status()
      expected = sums.text.strip().split()[0].lower()
    except (requests.RequestException, IndexError):
      delete_file(tgz_path)
      return jsonify({"error": "Could not fetch Tailscale checksum; aborting"}), 502

    digest = hashlib.sha256(Path(tgz_path).read_bytes()).hexdigest().lower()
    if not re.fullmatch(r"[0-9a-f]{64}", expected) or digest != expected:
      delete_file(tgz_path)
      return jsonify({"error": "Tailscale archive checksum mismatch; aborting"}), 502

    extract_tar(tgz_path, base)
    for name in ("tailscale", "tailscaled"):
      shutil.copy2(f"{bin_dir}/{name}", f"{base}/{name}")
      os.chmod(f"{base}/{name}", 0o755)
      print(f"Copied {name} binary.")

    systemd_unit = _tailscale_unit()
    unit_tmp = f"{base}/tailscaled.service"
    with open(unit_tmp, "w") as f:
      f.write(systemd_unit)

    if _drive_locked():
      return jsonify({"error": _DRIVING_ERROR}), 423

    with _writable_root():
      _run_cmd(["sudo", "install", "-m", "644", unit_tmp, str(TAILSCALE_UNIT_PATH)],
               "Installed systemd unit.", "Failed to install systemd unit.")
      _tailscale_marker().write_text(hashlib.sha256(systemd_unit.encode()).hexdigest())
      _run_cmd(["sudo", "systemctl", "daemon-reload"], "Reloaded systemd daemon.", "Failed to reload systemd daemon.")
      _run_cmd(["sudo", "systemctl", "enable", str(TAILSCALE_UNIT_PATH)], "Enabled tailscaled service.", "Failed to enable tailscaled service.")
      _run_cmd(["sudo", "systemctl", "restart", "tailscaled"], "Started tailscaled service.", "Failed to start tailscaled service.")

    proc = subprocess.Popen(
      ["sudo", f"{base}/tailscale", "--socket", socket, "up", "--hostname", f"{HARDWARE.get_device_type()}-the-pond"],
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      text=True,
      preexec_fn=os.setsid
    )

    terminate_lock = threading.Lock()

    def _terminate():
      with terminate_lock:
        _quiesce_process(proc)

    watchdog = threading.Timer(60, _terminate)
    watchdog.start()
    auth_url = None
    try:
      for line in proc.stdout:
        match = re.search(r"https://login\.tailscale\.com/\S+", line)
        if match:
          auth_url = match.group(0)
          break
    finally:
      watchdog.cancel()
      _terminate()

    if not auth_url:
      return jsonify({"error": "Tailscale did not return an authentication URL. Please try again."}), 504

    return jsonify({
      "message": "Tailscale setup started. Please authenticate in your browser.",
      "auth_url": auth_url
    }), 200

  @app.route("/api/tailscale/uninstall", methods=["POST"])
  @_tailscale_operation
  def tailscale_uninstall():
    cloudlog.warning("the_pond audit: tailscale uninstall (sudo rm -rf / systemctl) requested")
    if not _tailscale_managed():
      return jsonify({"error": "The detected Tailscale installation is not managed by The Pond"}), 409

    base = TAILSCALE_BASE
    state = f"{base}/state"
    unit_path = str(TAILSCALE_UNIT_PATH)
    local_unit = f"{base}/tailscaled.service"

    _run_cmd(["sudo", "systemctl", "stop", "tailscaled"], "Stopped tailscaled.", "Failed to stop tailscaled.")
    _run_cmd(["sudo", "systemctl", "disable", "tailscaled"], "Disabled tailscaled.", "Failed to disable tailscaled.")

    if os.path.exists(unit_path):
      if _drive_locked():
        return jsonify({"error": _DRIVING_ERROR}), 423
      with _writable_root():
        _run_cmd(["sudo", "rm", unit_path], "Removed systemd unit file.", "Failed to remove systemd unit file.")
      _run_cmd(["sudo", "systemctl", "daemon-reload"], "Reloaded systemd daemon.", "Failed to reload systemd.")

    if os.path.lexists(local_unit) and not _delete_and_verify(local_unit):
      return jsonify({"error": "Failed to remove the local Tailscale service file"}), 500

    for filename in ["tailscale", "tailscaled", "tailscale.tgz"]:
      path = os.path.join(base, filename)
      if os.path.lexists(path) and not _delete_and_verify(path):
        return jsonify({"error": "Failed to remove a Tailscale installation file"}), 500

    if os.path.exists(state):
      _run_cmd(["sudo", "rm", "-rf", state], "Removed tailscale state dir.", "Failed to remove tailscale state dir.")

    if os.path.exists(base):
      _run_cmd(["sudo", "rm", "-rf", base], "Removed tailscale dir.", "Failed to remove tailscale dir.")

    if any(os.path.lexists(path) for path in (unit_path, local_unit, base)):
      return jsonify({"error": "Tailscale uninstall is incomplete"}), 500

    return jsonify({"message": "Tailscale uninstalled!"}), 200

  @app.route("/api/themes", methods=["POST"])
  def save_theme_route():
    if f'{secure_filename((request.form.get("themeName") or "").replace(" ", "_"))}-user_created' == POND_ACTIVE_THEME:
      return jsonify({"message": "Theme name is invalid."}), 400
    theme_path, error = utilities.create_theme(request.form, request.files)
    if error:
      return jsonify({"message": error}), 400
    return jsonify({"message": f'Theme "{request.form.get("themeName")}" saved!'}), 200

  @app.route("/api/themes/download_asset", methods=["POST"])
  def start_download_asset():
    data = request.get_json() or {}
    raw_component = (data.get("component") or "").strip()
    display_name = (data.get("name") or "").strip()
    if not raw_component or not display_name:
      return jsonify({"error": "Missing component or name"}), 400

    component = "steering_wheels" if raw_component == "steering_wheel" else ("signals" if raw_component == "turn_signals" else raw_component)
    mem_key = THEME_COMPONENT_PARAMS.get(component)
    if not mem_key:
      return jsonify({"error": "Unknown component"}), 400

    slug = helpers.theme_asset_slug(display_name)
    if not helpers.is_safe_slug(slug):
      return jsonify({"error": "Invalid component name"}), 400

    params_memory.put(mem_key, slug)
    params_memory.put("ThemeDownloadProgress", "Downloading...")

    return jsonify({"message": "Download started", "component": component, "param": mem_key, "slug": slug}), 200

  @app.route("/api/themes/apply", methods=["POST"])
  def apply_theme():
    form_data = request.form.to_dict(flat=True)
    form_data["themeName"] = "pond_active"

    _, error = utilities.create_theme(form_data, request.files)
    if error:
      return {"error": error}, 400

    save_checklist = json.loads(form_data.get("saveChecklist", "{}"))
    theme_params = {
      "colors": "CustomColors",
      "distance_icons": "CustomDistanceIcons",
      "icons": "CustomIcons",
      "sounds": "CustomSounds",
      "steering_wheel": "WheelIcon",
      "turn_signals": "CustomSignals",
    }
    selected_params = [param for component, param in theme_params.items() if save_checklist.get(component)]
    previous = {param: params.get(param) for param in selected_params}
    try:
      for param in selected_params:
        params.put(param, POND_ACTIVE_THEME)
      params.put_bool("PersonalizeOpenpilot", True)
    except Exception:
      cloudlog.exception("the_pond: could not apply theme parameters")
      for param, value in previous.items():
        params.remove(param) if value is None else params.put(param, value)
      return {"error": "Theme could not be applied"}, 500

    params_memory.put_bool("UseActiveTheme", True)

    update_frogpilot_toggles()
    return {"message": "Theme applied successfully!"}, 200

  @app.route("/api/themes/asset/<path:theme>/<path:asset_path>")
  def get_theme_asset(theme, asset_path):
    if Path(asset_path).suffix.lower() not in _THEME_ASSET_SUFFIXES:
      return "File not found", 404
    theme_type = request.args.get("type", "")

    if theme_type == "active" or theme == "__active__":
      file_path = ACTIVE_THEME_PATH / asset_path
    elif asset_path.startswith("steering_wheels/"):
      file_path = THEME_SAVE_PATH / asset_path
    elif asset_path.startswith("steering_wheel/") and "holiday" in theme_type:
      file_path = HOLIDAY_THEME_PATH / theme / asset_path
    else:
      base_dir = HOLIDAY_THEME_PATH / theme if "holiday" in theme_type else THEME_SAVE_PATH / "theme_packs" / theme
      file_path = base_dir / asset_path

    file_path = Path(file_path).resolve()
    roots = (Path(ACTIVE_THEME_PATH).resolve(), Path(THEME_SAVE_PATH).resolve(),
             Path(HOLIDAY_THEME_PATH).resolve())
    if not any(helpers.is_within(root, file_path) for root in roots):
      return "Forbidden", 403

    if not file_path.is_file():
      return "File not found", 404

    return send_file(file_path, as_attachment=False)

  @app.route("/api/themes/delete/<path:theme_path_str>", methods=["DELETE"])
  def delete_theme(theme_path_str):
    theme_type = request.args.get("type", "user")
    component = (request.args.get("component") or "").strip()

    if theme_type == "holiday":
      return jsonify({"message": "Cannot delete holiday themes."}), 403
    if params.get_bool("RandomThemes"):
      return jsonify({"message": "Disable random themes before deleting theme assets."}), 409

    selection_params = {
      "colors": "CustomColors", "distance_icons": "CustomDistanceIcons", "icons": "CustomIcons",
      "signals": "CustomSignals", "sounds": "CustomSounds", "steering_wheels": "WheelIcon",
    }
    def selected(theme_name, theme_component):
      selected_name = params.get(selection_params[theme_component], encoding="utf-8") or ""
      return helpers.theme_asset_slug(selected_name) == helpers.theme_asset_slug(theme_name)

    with utilities.theme_mutation_lock():
      if theme_type == "steering_wheel":
        wheels_root = THEME_SAVE_PATH / "steering_wheels"
        wheel_path = wheels_root / theme_path_str
        if wheel_path.resolve().parent != wheels_root.resolve() or wheel_path.stem == POND_ACTIVE_THEME:
          return jsonify({"message": "Forbidden"}), 403
        if selected(wheel_path.stem, "steering_wheels"):
          return jsonify({"message": "Select a different steering wheel before deleting this one."}), 409
        if wheel_path.exists():
          if not _delete_and_verify(wheel_path):
            return jsonify({"message": "Steering wheel could not be deleted."}), 500
          params.remove("ThemesDownloaded")
          return jsonify({"message": f'Steering wheel "{utilities.normalize_theme_name(wheel_path.stem)}" deleted!'}), 200
        return jsonify({"message": "Steering wheel not found..."}), 404

      packs_root = THEME_SAVE_PATH / "theme_packs"
      theme_path = packs_root / theme_path_str
      if theme_path.resolve().parent != packs_root.resolve() or theme_path.name == POND_ACTIVE_THEME:
        return jsonify({"message": "Forbidden"}), 403
      if not theme_path.is_dir():
        return jsonify({"message": "Theme not found..."}), 404

      if component:
        allowed = {"colors", "distance_icons", "icons", "sounds", "signals"}
        if component not in allowed:
          return jsonify({"message": "Unknown component..."}), 400
        if selected(theme_path.name, component):
          return jsonify({"message": "Select a different theme component before deleting this one."}), 409

        target = theme_path / component
        if not target.exists():
          return jsonify({"message": f'Component "{component}" not found in theme...'}), 404

        if not _delete_and_verify(target):
          return jsonify({"message": f'Component "{component}" could not be deleted.'}), 500

        params.remove("ThemesDownloaded")
        return jsonify({"message": f'Removed {component.replace("_", " ")} from "{utilities.normalize_theme_name(theme_path.name)}"!'}), 200

      if any(selected(theme_path.name, item) for item in selection_params if item != "steering_wheels"):
        return jsonify({"message": "Select different theme components before deleting this theme."}), 409
      if not _delete_and_verify(theme_path):
        return jsonify({"message": "Theme could not be deleted."}), 500
      params.remove("ThemesDownloaded")
      return jsonify({"message": f'Theme "{utilities.normalize_theme_name(theme_path.name)}" deleted!'}), 200

  @app.route("/api/themes/default", methods=["GET"])
  def get_default_theme():
    theme_data = {
      "colors": {},
      "images": {},
      "sounds": {},
      "turnSignalLength": 100,
      "turnSignalType": "Single Image",
      "sequentialImages": [],
      "theme_names": {}
    }

    if not params.get_bool("PersonalizeOpenpilot"):
      theme_data["theme_names"] = {
        "colors": "Stock",
        "distanceIcons": "Stock",
        "icons": "Stock",
        "sounds": "Stock",
        "turnSignals": "Stock",
        "steeringWheel": "Stock"
      }
    else:
      theme_param_map = {
        "CustomColors": "colors",
        "CustomDistanceIcons": "distanceIcons",
        "CustomIcons": "icons",
        "CustomSounds": "sounds",
        "CustomSignals": "turnSignals",
        "WheelIcon": "steeringWheel"
      }
      for param, theme_key in theme_param_map.items():
        param_value = params.get(param, encoding="utf-8")
        if param_value:
          theme_data["theme_names"][theme_key] = utilities.normalize_theme_name(param_value)

    colors_path = ACTIVE_THEME_PATH / "colors" / "colors.json"
    if colors_path.exists():
      try:
        with open(colors_path) as f:
          theme_data["colors"] = json.load(f)
      except (OSError, ValueError):
        cloudlog.exception("the_pond get_default_theme: failed to read active colors.json")

    signals_dir = ACTIVE_THEME_PATH / "signals"
    if signals_dir.exists():
      sequential_files = sorted(
        file.name for file in signals_dir.iterdir()
        if file.is_file() and file.suffix.lower() in utilities.IMAGE_EXTS and re.fullmatch(r"turn_signal_\d+", file.stem)
      )
      if sequential_files:
        theme_data["sequentialImages"] = sequential_files
        theme_data["turnSignalType"] = "Sequential"

      theme_data["turnSignalStyle"] = "Traditional"
      theme_data["turnSignalLength"] = 100

      for file in sorted(os.listdir(signals_dir)):
        if Path(file).suffix.lower() not in utilities.IMAGE_EXTS:
          parts = file.split("_")
          if len(parts) == 2:
            theme_data["turnSignalStyle"] = parts[0].capitalize()
            try:
              theme_data["turnSignalLength"] = int(parts[1])
            except ValueError:
              pass
            break

      if turn_signal := utilities.first_image(signals_dir, "turn_signal"):
        theme_data["images"]["turnSignal"] = turn_signal
      if blindspot := utilities.first_image(signals_dir, "turn_signal_blindspot"):
        theme_data["images"]["turnSignalBlindspot"] = blindspot

    icons_path = ACTIVE_THEME_PATH / "icons"
    if icons_path.exists() and icons_path.is_dir():
      for file in os.listdir(icons_path):
        if Path(file).stem == "button_settings":
          theme_data["images"]["settingsButton"] = file
        elif Path(file).stem == "button_home":
          theme_data["images"]["homeButton"] = file

    wheel_path = ACTIVE_THEME_PATH / "steering_wheel"
    if wheel_path.exists() and wheel_path.is_dir():
      wheel_files = list(wheel_path.glob("wheel.*"))
      if wheel_files:
        theme_data["images"]["steeringWheel"] = wheel_files[0].name

    distance_icons_path = ACTIVE_THEME_PATH / "distance_icons"
    if distance_icons_path.exists() and distance_icons_path.is_dir():
      theme_data["images"]["distanceIcons"] = {}
      for file in os.listdir(distance_icons_path):
        key = Path(file).stem
        if key in utilities.DISTANCE_ICON_NAMES:
          theme_data["images"]["distanceIcons"][key] = file

    sounds_path = ACTIVE_THEME_PATH / "sounds"
    if sounds_path.exists() and sounds_path.is_dir():
      for file in os.listdir(sounds_path):
        stem = Path(file).stem
        if stem in utilities.SOUND_NAMES:
          theme_data["sounds"][stem] = file

    return jsonify(theme_data)

  @app.route("/api/themes/list", methods=["GET"])
  def list_themes():
    all_themes = []
    themes_path = THEME_SAVE_PATH / "theme_packs"

    if themes_path.exists():
      for theme_dir in themes_path.iterdir():
        if theme_dir.is_dir() and theme_dir.name != POND_ACTIVE_THEME:
          is_user_created = "-user_created" in theme_dir.name
          components = utilities.check_theme_components(theme_dir)
          all_themes.append({
            "name": utilities.normalize_theme_name(theme_dir.name),
            "path": theme_dir.name,
            "type": "user" if is_user_created else "standard",
            "is_user_created": is_user_created,
            **components
          })

    if HOLIDAY_THEME_PATH.exists():
      for theme_dir in HOLIDAY_THEME_PATH.iterdir():
        if theme_dir.is_dir():
          components = utilities.check_theme_components(theme_dir)
          all_themes.append({
            "name": utilities.normalize_theme_name(theme_dir.name),
            "path": theme_dir.name,
            "type": "holiday",
            "is_user_created": False,
            **components
          })

    wheels_path = THEME_SAVE_PATH / "steering_wheels"
    if wheels_path.exists():
      for wheel_file in wheels_path.iterdir():
        if wheel_file.stem == POND_ACTIVE_THEME:
          continue
        all_themes.append({
          "name": utilities.normalize_theme_name(wheel_file.stem),
          "path": wheel_file.name,
          "type": "steering_wheel",
          "is_user_created": "-user_created" in wheel_file.name,
          "hasSteeringWheel": True,
        })

    return jsonify({"themes": sorted(all_themes, key=lambda x: x["name"])})

  @app.route("/api/themes/load/<path:theme_path>")
  def load_theme(theme_path):
    theme_type = request.args.get("type", "")
    theme_root = HOLIDAY_THEME_PATH if "holiday" in theme_type else THEME_SAVE_PATH / "theme_packs"
    theme_dir = theme_root / theme_path
    if not helpers.is_within(theme_root, theme_dir):
      return jsonify({"error": "Forbidden"}), 403

    response_data = {
      "colors": None,
      "images": {},
      "sounds": {},
      "sequentialImages": [],
      "turnSignalType": "Single Image",
      "turnSignalStyle": "Static",
      "turnSignalLength": 100
    }

    colors_file = theme_dir / "colors" / "colors.json"
    if colors_file.exists():
      try:
        with open(colors_file) as f:
          response_data["colors"] = json.load(f)
      except (OSError, ValueError):
        cloudlog.exception("the_pond load_theme: failed to read colors.json")

    icons_dir = theme_dir / "icons"
    if icons_dir.exists():
      if home_icon := utilities.first_image(icons_dir, "button_home"):
        response_data["images"]["homeButton"] = {
          "filename": home_icon,
          "path": f"icons/{home_icon}"
        }
      if settings_icon := utilities.first_image(icons_dir, "button_settings"):
        response_data["images"]["settingsButton"] = {
          "filename": settings_icon,
          "path": f"icons/{settings_icon}"
        }

    distance_dir = theme_dir / "distance_icons"
    if distance_dir.exists():
      response_data["images"]["distanceIcons"] = {}
      for name in utilities.DISTANCE_ICON_NAMES:
        if fname := utilities.first_image(distance_dir, name):
          response_data["images"]["distanceIcons"][name] = {
            "filename": fname,
            "path": f"distance_icons/{fname}"
          }

    signals_dir = theme_dir / "signals"
    if signals_dir.exists():
      sequential_files = sorted(
        file.name for file in signals_dir.iterdir()
        if file.is_file() and file.suffix.lower() in utilities.IMAGE_EXTS and re.fullmatch(r"turn_signal_\d+", file.stem)
      )
      if sequential_files:
        response_data["sequentialImages"] = sequential_files
        response_data["turnSignalType"] = "Sequential"

      response_data["turnSignalStyle"] = "Traditional"
      response_data["turnSignalLength"] = 100

      for file in sorted(os.listdir(signals_dir)):
        if Path(file).suffix.lower() not in utilities.IMAGE_EXTS:
          parts = file.split("_")
          if len(parts) == 2:
            response_data["turnSignalStyle"] = parts[0].capitalize()
            try:
              response_data["turnSignalLength"] = int(parts[1])
            except ValueError:
              pass
            break

      if turn_signal := utilities.first_image(signals_dir, "turn_signal"):
        response_data["images"]["turnSignal"] = {
          "filename": turn_signal,
          "path": f"signals/{turn_signal}",
        }
      if blindspot := utilities.first_image(signals_dir, "turn_signal_blindspot"):
        response_data["images"]["turnSignalBlindspot"] = {
          "filename": blindspot,
          "path": f"signals/{blindspot}",
        }

    sounds_dir = theme_dir / "sounds"
    if sounds_dir.exists():
      for name in utilities.SOUND_NAMES:
        file_path = sounds_dir / f"{name}.wav"
        if file_path.exists():
          response_data["sounds"][name] = {
            "filename": f"{name}.wav",
            "path": f"sounds/{name}.wav"
          }

    steering_wheel_path = None
    if "holiday" in theme_type:
      steering_dir = theme_dir / "steering_wheel"
      if steering_dir.exists() and steering_dir.is_dir():
        for file in steering_dir.iterdir():
          if file.is_file() and file.suffix.lower() in [".png", ".jpg", ".jpeg", ".gif"]:
            steering_wheel_path = f"steering_wheel/{file.name}"
            break
    else:
      steering_wheels_dir = THEME_SAVE_PATH / "steering_wheels"
      if steering_wheels_dir.exists():
        for file in steering_wheels_dir.iterdir():
          if file.is_file() and file.stem.lower() == theme_path.lower() and file.suffix.lower() in [".png", ".jpg", ".jpeg", ".gif"]:
            steering_wheel_path = f"steering_wheels/{file.name}"
            break

    if steering_wheel_path:
      response_data["images"]["steeringWheel"] = {
        "filename": steering_wheel_path.split("/")[-1],
        "path": steering_wheel_path
      }

    return jsonify(response_data)

  @app.route("/api/themes/submit", methods=["POST"])
  def submit_theme():
    theme_path = None
    try:
      theme_name = request.form.get("themeName")
      if not theme_name:
        return jsonify({"error": "Missing theme name"}), 400

      discord_username = request.form.get("discordUsername") or "Unknown"

      theme_path, error = utilities.create_theme(request.form, request.files, temporary=True)
      if error:
        return jsonify({"message": error}), 400

      assets = []
      submission_urls = {}

      distance_icons_path = theme_path / "distance_icons"
      if distance_icons_path.exists() and any(distance_icons_path.iterdir()):
        zip_path = shutil.make_archive(str(distance_icons_path), "zip", distance_icons_path)
        assets.append(ThemeAsset.from_path("distance_icons", "application/zip", zip_path))
        submission_urls["distance_icons"] = f"https://gitlab.com/{RESOURCES_REPO}-Submissions/-/tree/Distance-Icons"

      for folder in ["colors", "icons", "signals", "sounds"]:
        folder_path = theme_path / folder
        if folder_path.exists() and any(folder_path.iterdir()):
          zip_path = shutil.make_archive(str(folder_path), "zip", folder_path)
          assets.append(ThemeAsset.from_path(folder, "application/zip", zip_path))
          submission_urls["theme"] = f"https://gitlab.com/{RESOURCES_REPO}-Submissions/-/tree/Themes"

      wheel_file = request.files.get("steeringWheel")
      if wheel_file and wheel_file.filename:
        assets.append(ThemeAsset.from_steering_wheel_upload(wheel_file, theme_path))
        submission_urls["steering_wheel"] = f"https://gitlab.com/{RESOURCES_REPO}-Submissions/-/tree/Steering-Wheels"

      if not submission_urls:
        return jsonify({"error": "No valid theme data or steering wheel file provided"}), 400

      submit_theme_assets(theme_name, discord_username, assets)

      return jsonify({
        "message": "Submission successful!",
        "branches": submission_urls
      }), 200

    except ThemeSubmissionError as error:
      cloudlog.exception("the_pond submit_theme rejected")
      return jsonify({"error": str(error)}), 400

    except Exception:
      cloudlog.exception("the_pond submit_theme failed")
      return jsonify({"error": "Theme submission failed. Please try again later."}), 500

    finally:
      if theme_path is not None and theme_path.parent.exists():
        delete_file(theme_path.parent)

  @app.route("/api/tmux_log/capture", methods=["POST"])
  def capture_tmux_log_route():
    TMUX_LOGS_PATH.mkdir(parents=True, exist_ok=True)

    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    log_filename = f"tmux_log_{timestamp}.json"
    log_path = TMUX_LOGS_PATH / log_filename

    _run_cmd(["tmux", "capture-pane", "-J", "-S", "-"], "Captured tmux pane.", "Failed to capture tmux pane.")

    try:
      result = subprocess.run(["tmux", "show-buffer"], capture_output=True, text=True, check=True, timeout=10)
    except (subprocess.CalledProcessError, FileNotFoundError, subprocess.TimeoutExpired):
      return jsonify({"error": "No tmux buffer to capture (is a tmux session running?)"}), 409
    log_path.write_text(result.stdout, encoding="utf-8")

    try:
      _run_cmd(["tmux", "delete-buffer"], "Deleted tmux buffer.", "Failed to delete tmux buffer.")
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
      _delete_and_verify(log_path)
      raise
    return jsonify({"message": "Captured console log successfully!", "log_file": log_filename}), 200

  @app.route("/api/tmux_log/delete/<filename>", methods=["DELETE"])
  def delete_tmux_log(filename):
    safe = secure_filename(filename)
    file_path = TMUX_LOGS_PATH / safe
    if not safe:
      return jsonify({"error": "Forbidden"}), 403
    if file_path.exists():
      if not _delete_and_verify(file_path):
        return jsonify({"error": "Tmux log could not be deleted"}), 500
      return jsonify({"message": f"{filename} deleted!"}), 200

    return jsonify({"error": "File not found"}), 404

  @app.route("/api/tmux_log/delete_all", methods=["DELETE"])
  def delete_all_tmux_logs():
    if TMUX_LOGS_PATH.exists():
      if not _delete_and_verify(TMUX_LOGS_PATH):
        return jsonify({"error": "Tmux logs could not be deleted"}), 500
    try:
      TMUX_LOGS_PATH.mkdir(parents=True, exist_ok=True)
    except OSError:
      return jsonify({"error": "Tmux log directory could not be recreated"}), 500
    if TMUX_LOGS_PATH.is_symlink() or not TMUX_LOGS_PATH.is_dir() or any(TMUX_LOGS_PATH.iterdir()):
      return jsonify({"error": "Tmux log directory could not be reset"}), 500
    return jsonify({"message": "All tmux logs deleted!"}), 200

  @app.route("/api/tmux_log/download/<path:filename>", methods=["GET"])
  def download_tmux_log(filename):
    return send_from_directory(str(TMUX_LOGS_PATH), filename, as_attachment=True)

  @app.route("/api/tmux_log/list", methods=["GET"])
  def list_tmux_logs():
    TMUX_LOGS_PATH.mkdir(parents=True, exist_ok=True)
    files = sorted(TMUX_LOGS_PATH.glob("*.json"), key=lambda file: file.stat().st_mtime, reverse=True)
    return jsonify([{"filename": file.name, "timestamp": file.stat().st_mtime} for file in files])

  @app.route("/api/tmux_log/live", methods=["GET"])
  def stream_tmux_log():
    def generate():
      deadline = time.monotonic() + _TMUX_STREAM_MAX_SECONDS
      previous = None
      while time.monotonic() < deadline:
        try:
          output = _tmux_output()
        except FileNotFoundError:
          yield "data: tmux is not available on this device.\n\n"
          break
        if output != previous:
          previous = output
          yield "data: " + output.replace("\n", "\ndata: ") + "\n\n"
        time.sleep(1)
    return Response(stream_with_context(generate()), mimetype="text/event-stream")

  @app.route("/api/tmux_log/rename/<old>/<new>", methods=["PUT"])
  def rename_tmux_log_path_params(old, new):
    old_safe = secure_filename(old)
    new_safe = secure_filename(new)
    if not old_safe or not new_safe.endswith(".json"):
      return jsonify({"error": "Invalid name"}), 400
    old_path = TMUX_LOGS_PATH / old_safe
    new_path = TMUX_LOGS_PATH / new_safe

    if not old_path.exists():
      return jsonify({"error": "Original file not found"}), 404

    if new_path.exists():
      return jsonify({"error": "Target file already exists"}), 400

    old_path.rename(new_path)

    return jsonify({"message": f"Renamed {old} to {new_safe}!"}), 200

  @app.route("/api/tsk_available", methods=["GET"])
  def tsk_available():
    cp = _car_params()
    if cp is None:
      return jsonify({"result": False})
    return jsonify({"result": cp.secOcRequired})

  @app.route("/api/tsk_keys", methods=["DELETE"])
  def delete_secoc_key():
    name = request.args.get("name")
    if not name:
      return jsonify({"error": "Missing key name"}), 400
    with _PARAMS_LOCK:
      keys = _stored_secoc_keys()
      keys = [key for key in keys if key.get("name") != name]
      params.put("SecOCKeys", json.dumps(keys))
    return jsonify(_redacted_secoc_keys(keys))

  @app.route("/api/tsk_keys", methods=["GET"])
  def get_secoc_keys():
    return jsonify(_redacted_secoc_keys(_stored_secoc_keys()))

  @app.route("/api/tsk_keys", methods=["POST"])
  def save_secoc_keys():
    if request.content_length is not None and request.content_length > 32 * 1024:
      return jsonify({"error": "Key list is too large"}), 413
    submitted = request.get_json(silent=True) or []
    if not isinstance(submitted, list) or len(submitted) > _MAX_SECOC_KEYS or not all(
        isinstance(k, dict) and set(k).issubset({"name", "rename_from", "value"}) and helpers.is_safe_display_name(k.get("name"))
        for k in submitted
    ):
      return jsonify({"error": "Each key needs a safe name and a 32-hexadecimal-character value"}), 400
    if len({entry["name"] for entry in submitted}) != len(submitted):
      return jsonify({"error": "Key names must be unique"}), 400

    with _PARAMS_LOCK:
      stored = {key.get("name"): key.get("value") for key in _stored_secoc_keys()}

      keys = []
      for entry in submitted:
        name = entry.get("name")
        value = entry.get("value") if "value" in entry else stored.get(entry.get("rename_from") or name)
        if not helpers.is_valid_secoc_key(value):
          return jsonify({"error": "Each key needs a safe name and a 32-hexadecimal-character value"}), 400
        keys.append({"name": name, "value": value})

      params.put("SecOCKeys", json.dumps(keys))

    return jsonify(_redacted_secoc_keys(keys))

  @app.route("/api/tsk_key_set", methods=["POST"])
  def set_secoc_key():
    data = request.get_json(silent=True) or {}
    value = data.get("value")
    if value is None:
      value = next((key.get("value") for key in _stored_secoc_keys() if key.get("name") == data.get("name")), None)
    if not helpers.is_valid_secoc_key(value):
      return jsonify({"error": "Key must be 32 hexadecimal characters"}), 400

    cp = _car_params()
    if cp is None or not cp.secOcRequired:
      return jsonify({"error": "SecOC keys are not applicable to this vehicle"}), 409

    cloudlog.warning("the_pond audit: SecOC key applied")
    params.put("SecOCKey", value)
    return "", 204

  @app.route("/api/toggles/backup", methods=["POST"])
  def backup_toggle_values():
    toggle_values = {}
    for key in _toggle_backup_keys():
      raw_value = params.get(key)
      if isinstance(raw_value, bytes):
        value = raw_value.decode("utf-8", errors="replace")
      else:
        value = raw_value or ""

      toggle_values[key] = value

    wrapped = json.dumps({"data": toggle_values}, indent=2)

    buffer = BytesIO(wrapped.encode("utf-8"))
    buffer.seek(0)

    return send_file(buffer, as_attachment=True, download_name="toggle_backup.json", mimetype="application/json")

  @app.route("/api/toggles/restore", methods=["POST"])
  def restore_toggle_values():
    request_data = request.get_json()
    if not isinstance(request_data, dict) or "data" not in request_data:
      return jsonify({"success": False, "message": "Missing 'data' in request."}), 400

    allowed_keys = _toggle_backup_keys()

    raw = request_data["data"]
    if isinstance(raw, dict):
      toggle_values = raw
    else:
      try:
        toggle_values = utilities.decode_parameters(raw)
      except Exception:
        return jsonify({"success": False, "message": "Invalid backup data."}), 400

    if not isinstance(toggle_values, dict) or any(not isinstance(value, str) for value in toggle_values.values()):
      return jsonify({"success": False, "message": "Invalid backup data."}), 400

    with _PARAMS_LOCK:
      previous = {key: params.get(key) for key in toggle_values if key in allowed_keys}
      written = []
      try:
        for key, value in toggle_values.items():
          if key in allowed_keys:
            params.put(key, value)
            written.append(key)
      except Exception:
        cloudlog.exception("the_pond: toggle restore failed; restoring previous values")
        for key in reversed(written):
          try:
            params.remove(key) if previous[key] is None else params.put(key, previous[key])
          except Exception:
            cloudlog.exception(f"the_pond: could not restore toggle {key}")
        return jsonify({"success": False, "message": "Toggles could not be restored."}), 500

    update_frogpilot_toggles()
    return jsonify({"success": True, "message": "Toggles restored!"})

  @app.route("/api/toggles/reset_default", methods=["POST"])
  def reset_toggle_values():
    if params.get_bool("IsOnroad"):
      return {"error": "Toggle reset is only available while offroad"}, 423
    cloudlog.warning("the_pond audit: toggle reset (default) + reboot requested")
    params.put_bool("DoToggleReset", True)
    threading.Timer(0.5, _reboot_after_reset, args=("DoToggleReset",)).start()
    return {"message": "Resetting toggles and rebooting..."}, 200

  @app.route("/api/toggles/reset_stock", methods=["POST"])
  def reset_toggle_values_to_stock():
    if params.get_bool("IsOnroad"):
      return {"error": "Toggle reset is only available while offroad"}, 423
    cloudlog.warning("the_pond audit: toggle reset (stock) + reboot requested")
    params.put_bool("DoToggleResetStock", True)
    threading.Timer(0.5, _reboot_after_reset, args=("DoToggleResetStock",)).start()
    return {"message": "Resetting toggles to stock and rebooting..."}, 200

  @app.route("/mapbox-help/<path:filename>", methods=["GET"])
  def serve_mapbox_help(filename):
    if filename not in MAPBOX_HELP_IMAGES:
      return {"error": "Not found"}, 404
    return send_from_directory(NAVIGATION_TRAINING_PATH, filename)

  @app.route("/thumbnails/<path:file_path>", methods=["GET"])
  def get_thumbnail(file_path):
    parts = Path(file_path).parts
    if len(parts) != 2 or utilities.SEGMENT_RE.fullmatch(parts[0]) is None or parts[1] not in ("preview.png", "preview.gif"):
      return {"error": "Thumbnail not found"}, 404
    for footage_path in FOOTAGE_PATHS:
      preview_path = Path(footage_path) / file_path
      if not helpers.is_within(footage_path, preview_path):
        continue
      if not preview_path.exists():
        segment_dir = preview_path.parent
        qcamera = segment_dir / "qcamera.ts"
        if not qcamera.exists():
          continue

        source_fd = None
        with _route_mutation_lock(False) as mutation_coordinated:
          if mutation_coordinated is None:
            return {"error": "Could not coordinate route storage"}, 500
          if not mutation_coordinated:
            return {"error": "Another route mutation is in progress"}, 423
          if _route_is_locked(segment_dir):
            return {"error": "Route is still being written"}, 423
          try:
            source_fd = os.open(qcamera, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
            if not stat.S_ISREG(os.fstat(source_fd).st_mode):
              raise OSError("qcamera source is not a regular file")
          except OSError:
            if source_fd is not None:
              os.close(source_fd)
            cloudlog.exception(f"the_pond: could not snapshot route thumbnail source {qcamera}")
            return {"error": "Could not read thumbnail source"}, 500

        temporary_preview = preview_path.with_name(f".{preview_path.stem}.{secrets.token_hex(8)}{preview_path.suffix}")
        try:
          source_path = f"/proc/{os.getpid()}/fd/{source_fd}"
          if preview_path.suffix == ".png":
            utilities.video_to_png(source_path, temporary_preview)
          else:
            utilities.video_to_gif(source_path, temporary_preview)
          if not temporary_preview.is_file() or temporary_preview.stat().st_size == 0:
            raise OSError("thumbnail generation produced no output")
          os.replace(temporary_preview, preview_path)
        except (OSError, subprocess.SubprocessError):
          cloudlog.exception(f"the_pond: could not create route thumbnail {preview_path}")
          return {"error": "Could not create thumbnail"}, 500
        finally:
          os.close(source_fd)
          temporary_preview.unlink(missing_ok=True)
      if preview_path.exists():
        return send_from_directory(footage_path, file_path, as_attachment=True)
    return {"error": "Thumbnail not found"}, 404

  @app.route("/video/<path>", methods=["GET"])
  def get_video(path):
    camera = request.args.get("camera", "forward")
    camera_files = {"driver": "dcamera.hevc", "forward": "fcamera.hevc", "wide": "ecamera.hevc"}
    if camera not in camera_files:
      return {"error": "Unknown camera"}, 400
    filename = camera_files[camera]
    for footage_path in FOOTAGE_PATHS:
      filepath = os.path.join(footage_path, path, filename)
      if not helpers.is_within(footage_path, filepath):
        continue
      source_fd = None
      with _route_mutation_lock(False) as mutation_coordinated:
        if mutation_coordinated is None:
          return {"error": "Could not coordinate route storage"}, 500
        if not mutation_coordinated:
          return {"error": "Another route mutation is in progress"}, 423
        if not os.path.exists(filepath):
          continue
        if _route_is_locked(Path(filepath).parent):
          return {"error": "Route is still being written"}, 423
        try:
          source_fd = os.open(filepath, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
          if not stat.S_ISREG(os.fstat(source_fd).st_mode):
            raise OSError("video source is not a regular file")
        except OSError:
          if source_fd is not None:
            os.close(source_fd)
          cloudlog.exception(f"the_pond: could not snapshot route video source {filepath}")
          return {"error": "Could not read video source"}, 500

      try:
        source_path = f"/proc/{os.getpid()}/fd/{source_fd}"
        mp4_file = utilities.ffmpeg_mp4_wrap_process_builder(source_path, cache_key=filepath)
      except (OSError, ValueError):
        cloudlog.exception(f"the_pond: could not process the route video {filepath}")
        return {"error": "Could not process video"}, 503
      finally:
        os.close(source_fd)
      return _send_cached_video(mp4_file)
    return {"error": "Video not found"}, 404

MDNS_GROUP = "224.0.0.251"
MDNS_PORT = 5353

def _local_ip_for(dest_ip):
  sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  try:
    sock.connect((dest_ip, MDNS_PORT))
    return sock.getsockname()[0]
  except OSError:
    return None
  finally:
    sock.close()

def _mdns_responder():
  try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", MDNS_PORT))
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                    struct.pack("=4sl", socket.inet_aton(MDNS_GROUP), socket.INADDR_ANY))
  except OSError:
    cloudlog.exception("the_pond mdns: setup failed")
    return
  while True:
    try:
      data, addr = sock.recvfrom(2048)
      if helpers.is_mdns_query_for(data) and (ip := _local_ip_for(addr[0])):
        destination = addr if addr[1] != MDNS_PORT else (MDNS_GROUP, MDNS_PORT)
        sock.sendto(helpers.build_mdns_a_response(ip), destination)
    except OSError:
      cloudlog.exception("the_pond mdns: loop error")

def _iptables_redirect(op, port):
  return ["sudo", "iptables", "-t", "nat", op, "PREROUTING",
          "-p", "tcp", "--dport", "80", "-j", "REDIRECT", "--to-ports", str(port)]

def _redirect_present(port):
  try:
    return subprocess.run(_iptables_redirect("-C", port), capture_output=True, timeout=_CMD_TIMEOUT).returncode == 0
  except (OSError, subprocess.SubprocessError):
    return False

def _ensure_port80_redirect(port):
  if _redirect_present(port):
    return
  cloudlog.warning(f"the_pond audit: redirecting port 80 -> {port}")
  try:
    subprocess.run(_iptables_redirect("-A", port), capture_output=True, timeout=_CMD_TIMEOUT, check=False)
  except (OSError, subprocess.SubprocessError):
    cloudlog.exception("the_pond: port 80 redirect failed")

def _mdns_reachable():
  query = struct.pack(">HHHHHH", 0, 0, 1, 0, 0, 0) + helpers.encode_dns_name(helpers.MDNS_HOSTNAME) + struct.pack(">HH", 1, 1)
  sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  sock.settimeout(2)
  try:
    sock.sendto(query, (MDNS_GROUP, MDNS_PORT))
    return any(helpers.is_mdns_response_for(sock.recvfrom(2048)[0]) for _ in range(5))
  except OSError:
    return False
  finally:
    sock.close()

def _self_check(port):
  time.sleep(2)
  for label, ok in (("mDNS responder", _mdns_reachable()), (f"port 80 -> {port} redirect", _redirect_present(port))):
    (cloudlog.warning if ok else cloudlog.error)(f"the_pond self-check: {label} {'OK' if ok else 'FAILED'}")

def create_app():
  app = Flask(__name__, static_folder="assets", static_url_path="/assets")
  app.config["MAX_CONTENT_LENGTH"] = 32 * 1024 * 1024
  setup(app)
  return app

def main():
  app = create_app()

  if PC:
    print("\"The Pond\" is not running on a comma device (PC mode, port 8083)")

  if not PC:
    threading.Thread(target=_gear_monitor, daemon=True, name="the_pond_gear").start()
    threading.Thread(target=_mdns_responder, daemon=True, name="the_pond_mdns").start()
    _ensure_port80_redirect(_POND_PORT)
    threading.Thread(target=_self_check, args=(_POND_PORT,), daemon=True, name="the_pond_selfcheck").start()

  app.run(host="0.0.0.0", port=_POND_PORT, threaded=True, debug=False)

if __name__ == "__main__":
  main()
