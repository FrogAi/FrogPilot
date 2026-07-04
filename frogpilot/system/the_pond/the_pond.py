#!/usr/bin/env python3
import base64
import hashlib
import json
import os
import re
import requests
import secrets
import shutil
import socket
import struct
import subprocess
import threading
import time

from datetime import datetime, timedelta, timezone
from flask import Flask, Response, jsonify, render_template, request, send_file, send_from_directory, stream_with_context
from io import BytesIO
from pathlib import Path
from werkzeug.utils import secure_filename

from cereal import car, messaging
from opendbc.can.parser import CANParser
from openpilot.common.swaglog import cloudlog
from openpilot.selfdrive.car.toyota.carcontroller import LOCK_CMD, UNLOCK_CMD
from openpilot.system.hardware import HARDWARE, PC
from openpilot.system.hardware.hw import Paths
from openpilot.system.loggerd.deleter import PRESERVE_ATTR_NAME, PRESERVE_ATTR_VALUE, PRESERVE_COUNT
from openpilot.system.version import get_build_metadata
from panda import Panda

from openpilot.frogpilot.assets.theme_manager import HOLIDAY_THEME_PATH, THEME_COMPONENT_PARAMS
from openpilot.frogpilot.common.frogpilot_utilities import delete_file, get_frogpilot_api_info, get_lock_status, run_cmd, extract_tar
from openpilot.frogpilot.common.frogpilot_variables import ACTIVE_THEME_PATH, ERROR_LOGS_PATH, EXCLUDED_KEYS, FROGPILOT_API, RESOURCES_REPO, SCREEN_RECORDINGS_PATH, THEME_SAVE_PATH,\
                                                           frogpilot_default_params, params, params_memory, update_frogpilot_toggles
from openpilot.frogpilot.system.the_pond import helpers, utilities

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

TMUX_LOGS_PATH = Path("/data/tmux_logs")
TAILSCALE_BASE = "/data/tailscale"
NAVIGATION_TRAINING_PATH = "/data/openpilot/frogpilot/navigation/navigation_training"

_PARAMS_LOCK = threading.RLock()

_CMD_TIMEOUT = 60
_TMUX_STREAM_MAX_SECONDS = 3600

def _run_cmd(cmd, ok, fail, **kw):
  kw.setdefault("timeout", _CMD_TIMEOUT)
  return run_cmd(cmd, ok, fail, **kw)

_CSP = (
  "default-src 'self'; "
  "script-src 'self'; "
  "style-src 'self' 'unsafe-inline'; "
  "img-src 'self' data: blob: https://api.mapbox.com https://*.tiles.mapbox.com; "
  "font-src 'self' data:; "
  "connect-src 'self' https://api.mapbox.com https://events.mapbox.com https://*.tiles.mapbox.com; "
  "worker-src blob:; "
  "child-src blob:; "
  "object-src 'none'; "
  "base-uri 'self'; "
  "form-action 'self'; "
  "frame-ancestors 'none'"
)

def _allowed_hosts():
  hosts = {"localhost", "127.0.0.1"}
  self_host = (request.host or "").split(":")[0].lower()
  if self_host:
    hosts.add(self_host)
  return hosts

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

def _favorite_id(fav):
  raw = f"{fav.get('longitude')},{fav.get('latitude')}|{fav.get('routeId') or ''}|{fav.get('name') or ''}"
  return hashlib.sha1(raw.encode()).hexdigest()

def _frogpilot_api_payload(**extra):
  info = get_frogpilot_api_info()
  return {
    "api_token": info.api_token,
    "build_metadata": info.build_metadata,
    "device": info.device_type,
    "frogpilot_dongle_id": info.dongle_id,
    **extra,
  }

def _gitlab_action(file_path, content):
  return {"action": "create", "file_path": file_path, "content": content, "encoding": "base64"}

_gear_lock = threading.Lock()
_parked_snapshot = {"parked": False, "fresh": False}

def _gear_monitor():
  sm = None
  while True:
    try:
      if sm is None:
        sm = messaging.SubMaster(["frogpilotCarState"])
      sm.update(1000)
      fresh = sm.alive["frogpilotCarState"] and sm.valid["frogpilotCarState"]
      parked = bool(sm["frogpilotCarState"].isParked) if fresh else False
      with _gear_lock:
        _parked_snapshot["parked"] = parked
        _parked_snapshot["fresh"] = fresh
    except Exception:
      sm = None
      with _gear_lock:
        _parked_snapshot["fresh"] = False
      time.sleep(0.5)

def _is_parked():
  with _gear_lock:
    return _parked_snapshot["fresh"] and _parked_snapshot["parked"]

def _drive_locked():
  return params.get_bool("IsOnroad") and not _is_parked()

def setup(app):
  @app.before_request
  def _request_gate():
    if request.method not in ("GET", "HEAD", "OPTIONS"):
      origin = request.headers.get("Origin") or request.headers.get("Referer")
      if not helpers.origin_allowed(origin, _allowed_hosts()):
        return jsonify({"error": "Cross-origin request blocked"}), 403

    if helpers.is_onroad_blocked(request.method, request.path) and _drive_locked():
      return jsonify({"error": "Unavailable while driving. Shift into Park or go offroad to use this."}), 423

  @app.after_request
  def _security_headers(resp):
    resp.headers.setdefault("Content-Security-Policy", _CSP)
    resp.headers["X-Content-Type-Options"] = "nosniff"
    resp.headers["X-Frame-Options"] = "DENY"
    resp.headers["Referrer-Policy"] = "same-origin"
    return resp

  @app.errorhandler(404)
  def not_found(_):
    if request.path.startswith("/api/"):
      return jsonify({"error": "Not found"}), 404
    return render_template("index.html")

  @app.errorhandler(405)
  def method_not_allowed(_):
    if request.path.startswith("/api/"):
      return jsonify({"error": "Method not allowed"}), 405
    return render_template("index.html")

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
    cp = _car_params()
    if cp is None:
      return jsonify({"result": False})
    return jsonify({"result": HARDWARE.get_device_type() != "tici" and cp.carName == "toyota"})

  @app.route("/api/doors/lock", methods=["POST"])
  def lock_doors():
    cp = _car_params()
    if cp is None or cp.carName != "toyota":
      return jsonify({"error": "Door control is only supported on Toyota vehicles"}), 409

    cloudlog.warning("the_pond audit: door lock requested")
    can_parser = CANParser("toyota_nodsu_pt_generated", [("DOOR_LOCKS", 3)], bus=0)
    can_sock = messaging.sub_sock("can", timeout=100)
    try:
      with Panda(disable_checks=True) as panda:
        if not params.get_bool("IsOnroad"):
          panda.set_safety_mode(panda.SAFETY_TOYOTA)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
          panda.can_send(0x750, LOCK_CMD, 0)
          time.sleep(1)
          if get_lock_status(can_parser, can_sock) == 0:
            return {"message": "Doors locked!"}
      return jsonify({"error": "Timed out waiting for doors to lock"}), 504
    finally:
      del can_sock

  @app.route("/api/doors/unlock", methods=["POST"])
  def unlock_doors():
    cp = _car_params()
    if cp is None or cp.carName != "toyota":
      return jsonify({"error": "Door control is only supported on Toyota vehicles"}), 409

    cloudlog.warning("the_pond audit: door unlock requested")
    can_parser = CANParser("toyota_nodsu_pt_generated", [("DOOR_LOCKS", 3)], bus=0)
    can_sock = messaging.sub_sock("can", timeout=100)
    try:
      with Panda(disable_checks=True) as panda:
        if not params.get_bool("IsOnroad"):
          panda.set_safety_mode(panda.SAFETY_TOYOTA)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
          panda.can_send(0x750, UNLOCK_CMD, 0)
          time.sleep(1)
          if get_lock_status(can_parser, can_sock) != 0:
            return {"message": "Doors unlocked!"}
      return jsonify({"error": "Timed out waiting for doors to unlock"}), 504
    finally:
      del can_sock

  @app.route("/api/error_logs", methods=["GET"])
  def get_error_logs():
    if not os.path.exists(ERROR_LOGS_PATH):
      return jsonify([]), 200
    files = utilities.list_file(ERROR_LOGS_PATH)
    filtered = [file for file in files if file.startswith("error")]
    return jsonify(filtered), 200

  @app.route("/api/error_logs/delete_all", methods=["DELETE"])
  def delete_all_error_logs():
    for f in os.listdir(ERROR_LOGS_PATH):
      delete_file(os.path.join(ERROR_LOGS_PATH, f))
    return {"message": "All error logs deleted!"}, 200

  @app.route("/api/error_logs/<filename>", methods=["DELETE"])
  def delete_error_log(filename):
    safe = secure_filename(filename)
    path = os.path.join(ERROR_LOGS_PATH, safe)
    if not safe or not os.path.isfile(path):
      return jsonify({"error": "Not found"}), 404
    delete_file(path)
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
    return {"message": "Destination cleared"}

  @app.route("/api/navigation", methods=["GET"])
  def navigation():
    try:
      last_position = json.loads(params.get("LastGPSPosition", encoding="utf8") or "{}")
    except (ValueError, TypeError):
      last_position = {}
    if not isinstance(last_position, dict):
      last_position = {}
    latitude = last_position.get("latitude", 51.276824158421331)
    longitude = last_position.get("longitude", 30.221928335547232)

    return {
      "amap1KeySet": bool(params.get("AMapKey1", encoding="utf8")),
      "amap2KeySet": bool(params.get("AMapKey2", encoding="utf8")),
      "destination": params.get("NavDestination", encoding="utf8") or "",
      "isMetric": params.get_bool("IsMetric"),
      "lastPosition": {
        "latitude": str(latitude),
        "longitude": str(longitude)
      },
      "mapboxPublic": params.get("MapboxPublicKey", encoding="utf8") or "",
      "mapboxSecretSet": bool(params.get("MapboxSecretKey", encoding="utf8")),
      "previousDestinations": params.get("ApiCache_NavDestinations", encoding="utf8") or "",
    }

  @app.route("/api/navigation", methods=["POST"])
  def set_navigation():
    payload = request.get_json(silent=True) or {}
    if not isinstance(payload, dict):
      return jsonify({"error": "Invalid destination"}), 400
    name = payload.get("name")
    if name is not None and not helpers.is_safe_display_name(name):
      return jsonify({"error": "Invalid destination name"}), 400
    params.put("NavDestination", json.dumps(payload))
    return {"message": "Destination set"}

  @app.route("/api/navigation/favorite", methods=["DELETE"])
  def remove_favorite_destination():
    to_remove = request.get_json(silent=True) or {}

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
    new_fav = request.json or {}

    name = new_fav.get("name")
    if name is not None and not helpers.is_safe_display_name(name):
      return jsonify({"error": "Invalid favorite name"}), 400

    new_fav.setdefault("id", _favorite_id(new_fav))

    with _PARAMS_LOCK:
      existing = json.loads(params.get("FavoriteDestinations", encoding="utf8") or "[]")
      if not any(f.get("id") == new_fav["id"] for f in existing):
        existing.append(new_fav)
      params.put("FavoriteDestinations", json.dumps(existing))
    return {"message": "Destination added to favorites!"}

  @app.route("/api/navigation/favorite/rename", methods=["POST"])
  def rename_favorite_destination():
    data = request.json or {}
    fid = data.get("id")
    route_id_to_rename = data.get("routeId")
    new_name = data.get("name")
    is_home = data.get("is_home")
    is_work = data.get("is_work")

    if not fid and not route_id_to_rename:
      return jsonify({"error": "Missing id or routeId"}), 400

    if new_name is not None and not helpers.is_safe_display_name(new_name):
      return jsonify({"error": "Invalid favorite name"}), 400

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

    saved = []
    for meta in KEYS.values():
      raw = (data.get(meta[0]) or "").strip()
      if not raw:
        continue

      full = raw if raw.startswith(meta[1]) else meta[1] + raw
      if len(full) < meta[4]:
        return jsonify(error=f"{meta[3]} is invalid or too short..."), 400

      params.put(meta[2], full)
      saved.append(meta[3])

    if not saved:
      return jsonify(error="Nothing to update..."), 400

    return jsonify(message=f"{', '.join(saved)} saved successfully!")

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
    deleted = False
    for footage_path in FOOTAGE_PATHS:
      for segment in os.listdir(footage_path):
        if segment == name or segment.startswith(name + "--"):
          delete_file(os.path.join(footage_path, segment))
          deleted = True
    if not deleted:
      return {"error": "Route not found"}, 404
    return {"message": "Route deleted!"}, 200

  @app.route("/api/routes/delete_all", methods=["DELETE"])
  def delete_all_routes():
    for footage_path in FOOTAGE_PATHS:
      if os.path.exists(footage_path):
        for segment in os.listdir(footage_path):
          delete_file(os.path.join(footage_path, segment))

    return {"message": "All routes deleted!"}, 200

  @app.route("/api/routes/<name>/preserve", methods=["POST"])
  def preserve_route(name):
    preserved_routes = 0
    for footage_path in FOOTAGE_PATHS:
      for segment in os.listdir(footage_path):
        if segment.endswith("--0"):
          segment_path = os.path.join(footage_path, segment)
          if PRESERVE_ATTR_NAME in os.listxattr(segment_path) and os.getxattr(segment_path, PRESERVE_ATTR_NAME) == PRESERVE_ATTR_VALUE:
            preserved_routes += 1

    if preserved_routes >= PRESERVE_COUNT:
      return {"error": f"Maximum of {PRESERVE_COUNT} preserved routes reached..."}, 400

    for footage_path in FOOTAGE_PATHS:
      route_path = os.path.join(footage_path, f"{name}--0")
      if os.path.exists(route_path):
        os.setxattr(route_path, PRESERVE_ATTR_NAME, PRESERVE_ATTR_VALUE)
        return {"message": "Route preserved!!"}, 200

    return {"error": "Route not found"}, 404

  @app.route("/api/routes/<name>/preserve", methods=["DELETE"])
  def un_preserve_route(name):
    for footage_path in FOOTAGE_PATHS:
      route_path = os.path.join(footage_path, f"{name}--0")
      if os.path.exists(route_path) and PRESERVE_ATTR_NAME in os.listxattr(route_path):
        os.removexattr(route_path, PRESERVE_ATTR_NAME)
        return {"message": "Route unpreserved!"}, 200
    return {"error": "Route not found"}, 404

  @app.route("/video/<name>/combined", methods=["GET"])
  def get_combined_route_video(name):
    camera = request.args.get("camera", "forward")
    for footage_path in FOOTAGE_PATHS:
      segments = utilities.get_segments_in_route(name, footage_path)
      if segments:
        cam_file = {
          "forward": "fcamera.hevc",
          "wide": "ecamera.hevc",
          "driver": "dcamera.hevc",
        }.get(camera, "fcamera.hevc")

        input_files = [
          os.path.join(footage_path, seg, cam_file)
          for seg in segments
          if os.path.exists(os.path.join(footage_path, seg, cam_file))
        ]

        if not input_files:
          return {"error": "No video files found"}, 404

        mp4_file = utilities.ffmpeg_concat_segments_to_mp4(input_files, cache_key=f"{name}-{camera}")
        return send_file(mp4_file, mimetype="video/mp4", conditional=True)

    return {"error": "Route not found"}, 404

  @app.route("/api/routes/<name>", methods=["GET"])
  def get_route(name):
    for footage_path in FOOTAGE_PATHS:
      base_path = os.path.join(footage_path, f"{name}--0")
      if os.path.exists(base_path):
        segments = utilities.get_segments_in_route(name, footage_path)
        if not segments:
          break

        segment_urls = [f"/video/{segment}" for segment in segments]
        total_duration = sum(utilities.get_video_duration(os.path.join(footage_path, f"{name}--{i}", "fcamera.hevc")) for i in range(len(segment_urls)))
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
    route_name = data.get("name")

    if not route_name:
      return jsonify({"error": "Missing route name"}), 400

    cleared = False
    original_timestamp = None
    for footage_path in FOOTAGE_PATHS:
      if not os.path.exists(footage_path):
        continue

      segments_to_process = [s for s in os.listdir(footage_path) if helpers.route_segment_matches(s, route_name) and os.path.isdir(os.path.join(footage_path, s))]
      if not segments_to_process:
        continue

      for segment in segments_to_process:
        segment_dir = os.path.join(footage_path, segment)
        for item in os.listdir(segment_dir):
          if not item.endswith((".hevc", ".ts", ".png", ".gif")) and item not in utilities.LOG_CANDIDATES:
            try:
              os.remove(os.path.join(segment_dir, item))
              cleared = True
            except OSError:
              pass

        if cleared:
          rlog_path = f"{segment_dir}/rlog"
          route_timestamp_dt = utilities.get_route_start_time(rlog_path)
          original_timestamp = route_timestamp_dt.isoformat() if route_timestamp_dt else None

    if cleared:
      return jsonify({"message": "Route name cleared successfully!", "timestamp": original_timestamp}), 200
    else:
      return jsonify({"error": "Route not found or no custom name to clear"}), 404

  @app.route("/api/routes/rename", methods=["POST"])
  def rename_route():
    data = request.get_json()
    old_name = data.get("old")
    new_name_raw = data.get("new")

    if not old_name or not new_name_raw:
      return jsonify({"error": "Missing old or new name"}), 400

    new_name = secure_filename(new_name_raw)
    if not new_name:
      return jsonify({"error": "Invalid new name"}), 400
    renamed = False
    had_segments = False
    write_error = False

    for footage_path in FOOTAGE_PATHS:
      if not os.path.exists(footage_path):
        continue

      segments_to_process = [s for s in os.listdir(footage_path) if helpers.route_segment_matches(s, old_name) and os.path.isdir(os.path.join(footage_path, s))]
      if not segments_to_process:
        continue
      had_segments = True

      for segment in segments_to_process:
        segment_dir = os.path.join(footage_path, segment)
        for item in os.listdir(segment_dir):
          if not item.endswith((".hevc", ".ts", ".png", ".gif")) and item not in utilities.LOG_CANDIDATES:
            try:
              os.remove(os.path.join(segment_dir, item))
            except OSError:
              pass

      for segment in segments_to_process:
        segment_dir = os.path.join(footage_path, segment)
        new_name_file_path = os.path.join(segment_dir, new_name)

        try:
          with open(new_name_file_path, "a"):
            os.utime(new_name_file_path, None)
          renamed = True
        except OSError as e:
          write_error = True
          cloudlog.exception(f"the_pond rename_route: could not write name marker in {segment_dir}: {e}")

    if renamed:
      return jsonify({"message": "Route renamed successfully!"}), 200
    if had_segments and write_error:
      return jsonify({"error": "Could not rename route"}), 500
    return jsonify({"error": "Route not found"}), 404

  @app.route("/api/screen_recordings/delete/<path:filename>", methods=["DELETE"])
  def delete_screen_recording(filename):
    mp4_path = SCREEN_RECORDINGS_PATH / filename
    if not helpers.is_within(SCREEN_RECORDINGS_PATH, mp4_path):
      return {"error": "Forbidden"}, 403
    if not mp4_path.exists():
      return {"error": "File not found"}, 404

    delete_file(str(mp4_path))

    for ext in (".png", ".gif"):
      thumb = mp4_path.with_suffix(ext)
      if thumb.exists():
        delete_file(str(thumb))

    return {"message": "Deleted"}, 200

  @app.route("/api/screen_recordings/delete_all", methods=["DELETE"])
  def delete_all_screen_recordings():
    files_to_delete = [f for f in os.listdir(SCREEN_RECORDINGS_PATH) if f.endswith(".mp4")]
    for filename in files_to_delete:
      delete_file(os.path.join(SCREEN_RECORDINGS_PATH, filename))
      for ext in (".png", ".gif"):
        thumb = os.path.join(SCREEN_RECORDINGS_PATH, filename.replace(".mp4", ext))
        if os.path.exists(thumb):
          delete_file(thumb)
    return {"message": "All screen recordings deleted!"}, 200

  @app.route("/api/screen_recordings/download/<path:filename>", methods=["GET"])
  def download_screen_recording(filename):
    if not helpers.is_within(SCREEN_RECORDINGS_PATH, SCREEN_RECORDINGS_PATH / filename):
      return {"error": "Forbidden"}, 403
    return send_from_directory(SCREEN_RECORDINGS_PATH, filename, as_attachment=True)

  @app.route("/api/screen_recordings/list", methods=["GET"])
  def list_screen_recordings():
    def generate():
      recordings = sorted(
        [recording for recording in SCREEN_RECORDINGS_PATH.glob("*.mp4") if not Path(f"{recording}.lock").exists()],
        key=lambda p: p.stat().st_mtime,
        reverse=True
      )
      total = len(recordings)

      yield f"data: {json.dumps({'progress': 0, 'total': total})}\n\n"
      for processed, mp4 in enumerate(recordings, start=1):
        try:
          result = utilities.screen_recording_metadata(mp4)
          yield f"data: {json.dumps({'recordings': [result]})}\n\n"
        except Exception:
          cloudlog.exception(f"the_pond list_screen_recordings: failed to process {mp4.name}")
        yield f"data: {json.dumps({'progress': processed, 'total': total})}\n\n"

    return Response(stream_with_context(generate()), mimetype="text/event-stream")

  @app.route("/screen_recordings/<path:filename>", methods=["GET"])
  def serve_screen_recording_asset(filename):
    asset = SCREEN_RECORDINGS_PATH / filename
    if helpers.is_within(SCREEN_RECORDINGS_PATH, asset) and not asset.exists() and asset.suffix in (".png", ".gif"):
      mp4 = asset.with_suffix(".mp4")
      if mp4.exists():
        if asset.suffix == ".png":
          utilities.video_to_png(mp4, asset)
        else:
          utilities.video_to_gif(mp4, asset)
    return send_from_directory(SCREEN_RECORDINGS_PATH, filename)

  @app.route("/api/screen_recordings/rename", methods=["POST"])
  def rename_screen_recording():
    data = request.get_json() or {}
    old = data.get("old")
    new_raw = data.get("new") or ""

    if not old or not new_raw:
      return {"error": "Missing filenames"}, 400

    stem = secure_filename(new_raw[:-4] if new_raw.lower().endswith(".mp4") else new_raw)
    if not stem:
      return {"error": "Invalid new name"}, 400
    new = f"{stem}.mp4"
    old_path = SCREEN_RECORDINGS_PATH / old
    new_path = SCREEN_RECORDINGS_PATH / new

    if not old_path.exists():
      return {"error": "Original file not found"}, 404

    if new_path.exists():
      return {"error": "Target file already exists"}, 400

    old_path.rename(new_path)
    for extension in (".png", ".gif"):
      old_thumb = old_path.with_suffix(extension)
      new_thumb = new_path.with_suffix(extension)

      if old_thumb.exists():
        old_thumb.rename(new_thumb)

    return {"message": "Renamed"}, 200

  @app.route("/api/speed_limits", methods=["POST"])
  def speed_limits():
    try:
      data = json.loads(params.get("SpeedLimitsFiltered") or "[]")
    except (ValueError, TypeError):
      data = []
    if not isinstance(data, list):
      data = []

    current_time = (datetime.now(timezone.utc) - timedelta(days=6, hours=23)).isoformat()
    data = [{**e, "last_vetted": current_time} for e in data if isinstance(e, dict)]

    with _PARAMS_LOCK:
      params.put("SpeedLimitsFiltered", json.dumps(data))

    buffer = BytesIO(json.dumps(data, indent=2).encode())
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
    base = TAILSCALE_BASE
    tailscale_binary = f"{base}/tailscale"
    tailscaled_binary = f"{base}/tailscaled"

    systemd_unit = "/etc/systemd/system/tailscaled.service"

    if os.path.exists(tailscale_binary) and os.path.exists(tailscaled_binary) and os.path.exists(systemd_unit):
      return jsonify({"installed": True})

    try:
      result = subprocess.run(["which", "tailscale"], capture_output=True, text=True, timeout=10)
    except subprocess.TimeoutExpired:
      return jsonify({"installed": False})
    if result.returncode == 0:
      return jsonify({"installed": True})

    return jsonify({"installed": False})

  @app.route("/api/tailscale/setup", methods=["POST"])
  def tailscale_setup():
    cloudlog.warning("the_pond audit: tailscale setup (sudo install systemd unit) requested")
    arch = "arm64"
    base = TAILSCALE_BASE

    version = "1.84.0"
    try:
      idx = requests.get("https://pkgs.tailscale.com/stable/", timeout=15)
      idx.raise_for_status()
      found = re.findall(r"tailscale_(\d+\.\d+\.\d+)_", idx.text)
      if found:
        version = max(found, key=lambda v: tuple(map(int, v.split("."))))
    except requests.RequestException:
      pass

    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
      return jsonify({"error": "Could not determine a valid Tailscale version"}), 502

    bin_dir = f"{base}/tailscale_{version}_{arch}"
    state = f"{base}/state"
    socket = f"{base}/tailscaled.sock"
    tgz_path = f"{base}/tailscale.tgz"

    tgz_url = f"https://pkgs.tailscale.com/stable/tailscale_{version}_{arch}.tgz"

    os.makedirs(state, exist_ok=True)

    _run_cmd(["curl", "-fsSL", "--connect-timeout", "15", "--max-time", "300", tgz_url, "-o", tgz_path],
             "Downloaded Tailscale archive.", "Failed to download Tailscale archive.", timeout=320)
    if not os.path.exists(tgz_path):
      return jsonify({"error": "Failed to download Tailscale archive"}), 502

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

    _run_cmd(["cp", f"{bin_dir}/tailscale", f"{base}/tailscale"], "Copied tailscale binary.", "Failed to copy tailscale binary.")
    _run_cmd(["cp", f"{bin_dir}/tailscaled", f"{base}/tailscaled"], "Copied tailscaled binary.", "Failed to copy tailscaled binary.")
    _run_cmd(["chmod", "+x", f"{base}/tailscale", f"{base}/tailscaled"], "Made binaries executable.", "Failed to chmod binaries.")

    systemd_unit = f"""[Unit]
    Description=Tailscale node agent
    After=network.target

    [Service]
    ExecStart={base}/tailscaled \\
      --tun=userspace-networking \\
      --socks5-server=localhost:1055 \\
      --state={state}/tailscaled.state \\
      --socket={socket} \\
      --statedir={state}
    Restart=on-failure
    RestartSec=5

    [Install]
    WantedBy=multi-user.target
    """
    unit_tmp = f"{base}/tailscaled.service"
    with open(unit_tmp, "w") as f:
      f.write(systemd_unit)

    try:
      _run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Remounted / as read-write.", "Failed to remount / as read-write.")
      _run_cmd(["sudo", "install", "-m", "644", unit_tmp, "/etc/systemd/system/tailscaled.service"], "Installed systemd unit.", "Failed to install systemd unit.")
      _run_cmd(["sudo", "systemctl", "daemon-reload"], "Reloaded systemd daemon.", "Failed to reload systemd daemon.")
      _run_cmd(["sudo", "systemctl", "enable", "/etc/systemd/system/tailscaled.service"], "Enabled tailscaled service.", "Failed to enable tailscaled service.")
      _run_cmd(["sudo", "systemctl", "restart", "tailscaled"], "Started tailscaled service.", "Failed to start tailscaled service.")
    finally:
      _run_cmd(["sudo", "mount", "-o", "remount,ro", "/"], "Remounted / read-only.", "Failed to remount / read-only -- filesystem may be left writable.")

    proc = subprocess.Popen(
      ["sudo", f"{base}/tailscale", "--socket", socket, "up", "--hostname", f"{HARDWARE.get_device_type()}-the-pond"],
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      text=True,
      preexec_fn=os.setsid
    )

    def _terminate():
      _run_cmd(["sudo", "kill", "-TERM", f"-{proc.pid}"], "Sent SIGTERM to Tailscale setup process.", "Failed to send SIGTERM to Tailscale setup process.")

    watchdog = threading.Timer(60, _terminate)
    watchdog.start()
    auth_url = None
    try:
      for line in (proc.stdout or []):
        match = re.search(r"https://login\.tailscale\.com/\S+", line)
        if match:
          auth_url = match.group(0)
          break
    finally:
      watchdog.cancel()
      _terminate()
      try:
        proc.wait(timeout=5)
      except subprocess.TimeoutExpired:
        cloudlog.warning("the_pond tailscale_setup: tailscale up did not exit after SIGTERM")

    if not auth_url:
      return jsonify({"error": "Tailscale did not return an authentication URL. Please try again."}), 504

    return jsonify({
      "message": "Tailscale setup started. Please authenticate in your browser.",
      "auth_url": auth_url
    }), 200

  @app.route("/api/tailscale/uninstall", methods=["POST"])
  def tailscale_uninstall():
    cloudlog.warning("the_pond audit: tailscale uninstall (sudo rm -rf / systemctl) requested")
    base = TAILSCALE_BASE
    state = f"{base}/state"
    unit_path = "/etc/systemd/system/tailscaled.service"
    local_unit = f"{base}/tailscaled.service"

    _run_cmd(["sudo", "systemctl", "stop", "tailscaled"], "Stopped tailscaled.", "Failed to stop tailscaled.")
    _run_cmd(["sudo", "systemctl", "disable", "tailscaled"], "Disabled tailscaled.", "Failed to disable tailscaled.")

    if os.path.exists(unit_path):
      try:
        _run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Remounted / as read-write.", "Failed to remount /.")
        _run_cmd(["sudo", "rm", unit_path], "Removed systemd unit file.", "Failed to remove systemd unit file.")
      finally:
        _run_cmd(["sudo", "mount", "-o", "remount,ro", "/"], "Remounted / read-only.", "Failed to remount / read-only -- filesystem may be left writable.")
      _run_cmd(["sudo", "systemctl", "daemon-reload"], "Reloaded systemd daemon.", "Failed to reload systemd.")

    delete_file(local_unit)

    for filename in ["tailscale", "tailscaled", "tailscale.tgz"]:
      delete_file(os.path.join(base, filename))

    for item in os.listdir(base):
      if item.startswith("tailscale_"):
        item_path = os.path.join(base, item)
        if os.path.isdir(item_path):
          _run_cmd(["sudo", "rm", "-rf", item_path], f"Removed {item_path}.", f"Failed to remove {item_path}.")

    if os.path.exists(state):
      _run_cmd(["sudo", "rm", "-rf", state], "Removed tailscale state dir.", "Failed to remove tailscale state dir.")

    if os.path.exists(base):
      _run_cmd(["sudo", "rm", "-rf", base], "Removed tailscale dir.", "Failed to remove tailscale dir.")

    return jsonify({"message": "Tailscale uninstalled!"}), 200

  @app.route("/api/themes", methods=["POST"])
  def save_theme_route():
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

    slug = display_name.lower().replace("(", "").replace(")", "").replace(" ", "_")
    if "/" in slug or "\\" in slug or ".." in slug:
      return jsonify({"error": "Invalid component name"}), 400

    params_memory.put(mem_key, slug)
    params_memory.put("ThemeDownloadProgress", "Downloading...")

    return jsonify({"message": "Download started", "component": component, "param": mem_key, "slug": slug}), 200

  @app.route("/api/themes/apply", methods=["POST"])
  def apply_theme():
    form_data = request.form.to_dict(flat=True)
    files = request.files

    if not form_data.get("themeName"):
      form_data["themeName"] = f"tmp_{secrets.token_hex(8)}"

    temp_path, error = utilities.create_theme(form_data, files, temporary=True)
    if error:
      return {"error": error}, 400

    persistent_source = THEME_SAVE_PATH / "active_theme_source"
    if persistent_source.exists() or persistent_source.is_symlink():
      delete_file(persistent_source)
    persistent_source.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(temp_path, persistent_source, symlinks=False)
    delete_file(temp_path.parent)
    temp_path = persistent_source

    save_checklist = json.loads(form_data.get("saveChecklist", "{}"))

    if save_checklist.get("colors"):
      asset_location = temp_path / "colors"
      save_location = ACTIVE_THEME_PATH / "colors"
      if save_location.exists() or save_location.is_symlink():
        delete_file(save_location)
      if asset_location.exists():
        save_location.parent.mkdir(parents=True, exist_ok=True)
        save_location.symlink_to(asset_location, target_is_directory=True)

    if save_checklist.get("distance_icons"):
      asset_location = temp_path / "distance_icons"
      save_location = ACTIVE_THEME_PATH / "distance_icons"
      if save_location.exists() or save_location.is_symlink():
        delete_file(save_location)
      if asset_location.exists():
        save_location.parent.mkdir(parents=True, exist_ok=True)
        save_location.symlink_to(asset_location, target_is_directory=True)

    if save_checklist.get("icons"):
      asset_location = temp_path / "icons"
      save_location = ACTIVE_THEME_PATH / "icons"
      if save_location.exists() or save_location.is_symlink():
        delete_file(save_location)
      if asset_location.exists():
        save_location.parent.mkdir(parents=True, exist_ok=True)
        save_location.symlink_to(asset_location, target_is_directory=True)

    if save_checklist.get("sounds"):
      asset_location = temp_path / "sounds"
      save_location = ACTIVE_THEME_PATH / "sounds"
      if save_location.exists() or save_location.is_symlink():
        delete_file(save_location)
      if asset_location.exists():
        save_location.parent.mkdir(parents=True, exist_ok=True)
        save_location.symlink_to(asset_location, target_is_directory=True)

    if save_checklist.get("turn_signals"):
      asset_location = temp_path / "signals"
      save_location = ACTIVE_THEME_PATH / "signals"
      if save_location.exists() or save_location.is_symlink():
        delete_file(save_location)
      if asset_location.exists():
        save_location.parent.mkdir(parents=True, exist_ok=True)
        save_location.symlink_to(asset_location, target_is_directory=True)

    wheel_location = temp_path / "WheelIcon"
    wheel_save_location = ACTIVE_THEME_PATH / "steering_wheel"
    if wheel_location.exists():
      if wheel_save_location.exists():
        delete_file(wheel_save_location)

      wheel_save_location.mkdir(parents=True, exist_ok=True)
      for file in wheel_location.iterdir():
        destination_file = wheel_save_location / file.name
        delete_file(destination_file)
        destination_file.symlink_to(file)

    params.put_bool("PersonalizeOpenpilot", True)
    params_memory.put_bool("UseActiveTheme", True)

    update_frogpilot_toggles()
    return {"message": "Theme applied successfully!"}, 200

  @app.route("/api/themes/asset/<path:theme>/<path:asset_path>")
  def get_theme_asset(theme, asset_path):
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

    if not file_path.exists():
      return "File not found", 404

    return send_file(file_path, as_attachment=False)

  @app.route("/api/themes/delete/<path:theme_path_str>", methods=["DELETE"])
  def delete_theme(theme_path_str):
    theme_type = request.args.get("type", "user")
    component = (request.args.get("component") or "").strip()

    if theme_type == "holiday":
      return jsonify({"message": "Cannot delete holiday themes."}), 403

    if theme_type == "steering_wheel":
      wheels_root = THEME_SAVE_PATH / "steering_wheels"
      wheel_path = wheels_root / theme_path_str
      if not helpers.is_within(wheels_root, wheel_path):
        return jsonify({"message": "Forbidden"}), 403
      if wheel_path.exists():
        delete_file(wheel_path)
        return jsonify({"message": f'Steering wheel "{utilities.normalize_theme_name(wheel_path.stem)}" deleted!'}), 200
      return jsonify({"message": "Steering wheel not found..."}), 404

    packs_root = THEME_SAVE_PATH / "theme_packs"
    theme_path = packs_root / theme_path_str
    if not helpers.is_within(packs_root, theme_path):
      return jsonify({"message": "Forbidden"}), 403
    if not theme_path.is_dir():
      return jsonify({"message": "Theme not found..."}), 404

    if component:
      allowed = {"colors", "distance_icons", "icons", "sounds", "signals"}
      if component not in allowed:
        return jsonify({"message": "Unknown component..."}), 400

      target = theme_path / component
      if not target.exists():
        return jsonify({"message": f'Component "{component}" not found in theme...'}), 404

      delete_file(target)

      return jsonify({"message": f'Removed {component.replace("_", " ")} from "{utilities.normalize_theme_name(theme_path.name)}"!'}), 200

    delete_file(theme_path)
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
        with open(colors_path, "r") as f:
          theme_data["colors"] = json.load(f)
      except (OSError, ValueError):
        cloudlog.exception("the_pond get_default_theme: failed to read active colors.json")

    signals_dir = ACTIVE_THEME_PATH / "signals"
    if signals_dir.exists():
      sequential_files = sorted([f.name for f in signals_dir.glob("turn_signal_*.png") if "blindspot" not in f.name.lower()])
      if sequential_files:
        theme_data["sequentialImages"] = sequential_files
        theme_data["turnSignalType"] = "Sequential"

      theme_data["turnSignalStyle"] = "Traditional"
      theme_data["turnSignalLength"] = 100

      for file in os.listdir(signals_dir):
        if not any(file.endswith(ext) for ext in utilities.IMAGE_EXTS):
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

  @app.route("/api/themes/download", methods=["POST"])
  def download_theme_route():
    theme_path, error = utilities.create_theme(request.form, request.files, temporary=True)
    if error:
      return jsonify({"message": error}), 400

    sane_theme_name = utilities.normalize_theme_name(request.form.get("themeName"), for_path=True)

    archive_path = shutil.make_archive(str(theme_path.parent / sane_theme_name), "zip", theme_path.parent, sane_theme_name)

    memory_file = BytesIO()
    with open(archive_path, "rb") as f:
      memory_file.write(f.read())
    memory_file.seek(0)

    delete_file(theme_path.parent)

    return send_file(memory_file, download_name=f'{sane_theme_name}.zip', as_attachment=True)

  @app.route("/api/themes/list", methods=["GET"])
  def list_themes():
    all_themes = []
    themes_path = THEME_SAVE_PATH / "theme_packs"

    if themes_path.exists():
      for theme_dir in themes_path.iterdir():
        if theme_dir.is_dir():
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
      if (icons_dir / "button_home.gif").exists():
        response_data["images"]["homeButton"] = {
          "filename": "button_home.gif",
          "path": "icons/button_home.gif"
        }
      if (icons_dir / "button_settings.png").exists():
        response_data["images"]["settingsButton"] = {
          "filename": "button_settings.png",
          "path": "icons/button_settings.png"
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
      sequential_files = sorted([f.name for f in signals_dir.glob("turn_signal_*.png") if "blindspot" not in f.name.lower()])
      if sequential_files:
        response_data["sequentialImages"] = sequential_files
        response_data["turnSignalType"] = "Sequential"

      response_data["turnSignalStyle"] = "Traditional"
      response_data["turnSignalLength"] = 100

      for file in os.listdir(signals_dir):
        if not any(file.endswith(ext) for ext in utilities.IMAGE_EXTS):
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
    try:
      theme_name = request.form.get("themeName")
      if not theme_name:
        return jsonify({"error": "Missing theme name"}), 400

      discord_username = request.form.get("discordUsername") or "Unknown"

      theme_path, error = utilities.create_theme(request.form, request.files, temporary=True)
      if error:
        return jsonify({"message": error}), 400

      safe_theme_name = utilities.normalize_theme_name(theme_name, for_path=True)
      combined_name = f"{safe_theme_name}~{discord_username}"

      def gitlab_post(commit_payload):
        resp = requests.post(f"{FROGPILOT_API}/gitlab/commit", json=_frogpilot_api_payload(**commit_payload),
                             headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"}, timeout=60)
        if resp.status_code not in (200, 201):
          raise RuntimeError(f"GitLab commit error {resp.status_code}: {resp.text}")
        return resp.json()

      def encode_file_base64(path):
        with open(path, "rb") as f:
          return base64.b64encode(f.read()).decode("utf-8")

      def send_discord_notification(username, theme_name, asset_types):
        payload = _frogpilot_api_payload(asset_types=asset_types, theme_name=theme_name, username=username)

        try:
          resp = requests.post(f"{FROGPILOT_API}/discord/theme", json=payload, headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"}, timeout=30)
          resp.raise_for_status()
          cloudlog.info("the_pond: sent theme submission notification")
        except requests.exceptions.RequestException:
          cloudlog.exception("the_pond: failed to send theme notification")

      asset_types = []
      submission_urls = {}

      distance_icons_path = theme_path / "distance_icons"
      if distance_icons_path.exists() and any(distance_icons_path.iterdir()):
        zip_path = shutil.make_archive(str(distance_icons_path), "zip", distance_icons_path)
        gitlab_post({
          "branch": "Distance-Icons",
          "commit_message": f"Added Distance Icons: {combined_name}",
          "actions": [_gitlab_action(f"{combined_name}.zip", encode_file_base64(zip_path))],
        })
        asset_types.append("Distance Icons")
        submission_urls["distance_icons"] = f"https://gitlab.com/{RESOURCES_REPO}-Submissions/-/tree/Distance-Icons"

      theme_actions = []
      for folder in ["colors", "icons", "signals", "sounds"]:
        folder_path = theme_path / folder
        if folder_path.exists() and any(folder_path.iterdir()):
          zip_path = shutil.make_archive(str(folder_path), "zip", folder_path)
          theme_actions.append(_gitlab_action(f"{combined_name}/{folder}.zip", encode_file_base64(zip_path)))

      if theme_actions:
        gitlab_post({
          "branch": "Themes",
          "commit_message": f"Added Theme: {combined_name}",
          "actions": theme_actions,
        })
        asset_types.append("Theme")
        submission_urls["theme"] = f"https://gitlab.com/{RESOURCES_REPO}-Submissions/-/tree/Themes"

      wheel_file = request.files.get("steeringWheel")
      if wheel_file and wheel_file.filename:
        suffix = Path(wheel_file.filename).suffix
        wheel_file.seek(0)
        encoded_wheel = base64.b64encode(wheel_file.read()).decode("utf-8")
        gitlab_post({
          "branch": "Steering-Wheels",
          "commit_message": f"Added Steering Wheel: {combined_name}",
          "actions": [_gitlab_action(f"{combined_name}{suffix}", encoded_wheel)],
        })
        asset_types.append("Steering Wheel")
        submission_urls["steering_wheel"] = f"https://gitlab.com/{RESOURCES_REPO}-Submissions/-/tree/Steering-Wheels"

      if not submission_urls:
        return jsonify({"error": "No valid theme data or steering wheel file provided"}), 400

      send_discord_notification(discord_username, theme_name, asset_types)

      return jsonify({
        "message": "Submission successful!",
        "branches": submission_urls
      }), 200

    except Exception:
      cloudlog.exception("the_pond submit_theme failed")
      return jsonify({"error": "Theme submission failed. Please try again later."}), 500

    finally:
      if "theme_path" in locals() and theme_path is not None and theme_path.parent.exists():
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

    _run_cmd(["tmux", "delete-buffer"], "Deleted tmux buffer.", "Failed to delete tmux buffer.")
    return jsonify({"message": "Captured console log successfully!", "log_file": log_filename}), 200

  @app.route("/api/tmux_log/delete/<filename>", methods=["DELETE"])
  def delete_tmux_log(filename):
    safe = secure_filename(filename)
    file_path = TMUX_LOGS_PATH / safe
    if not safe or not helpers.is_within(TMUX_LOGS_PATH, file_path):
      return jsonify({"error": "Forbidden"}), 403
    if file_path.exists():
      delete_file(file_path)
      return jsonify({"message": f"{filename} deleted!"}), 200

    return jsonify({"error": "File not found"}), 404

  @app.route("/api/tmux_log/delete_all", methods=["DELETE"])
  def delete_all_tmux_logs():
    if TMUX_LOGS_PATH.exists():

      delete_file(TMUX_LOGS_PATH)

    TMUX_LOGS_PATH.mkdir(parents=True, exist_ok=True)
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
      try:
        while time.monotonic() < deadline:
          try:
            output = subprocess.check_output(["tmux", "capture-pane", "-p", "-S", "-1000"], text=True, timeout=5)
          except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            yield "data: No active tmux session to stream.\n\n"
            time.sleep(1)
            continue
          except FileNotFoundError:
            yield "data: tmux is not available on this device.\n\n"
            break
          yield "data: " + "\n".join(reversed(output.splitlines())).replace("\n", "\ndata: ") + "\n\n"
          time.sleep(0.1)
      except GeneratorExit:
        return
    return Response(stream_with_context(generate()), mimetype="text/event-stream")

  @app.route("/api/tmux_log/rename/<old>/<new>", methods=["PUT"])
  def rename_tmux_log_path_params(old, new):
    old_safe = secure_filename(old)
    new_safe = secure_filename(new)
    if not old_safe or not new_safe:
      return jsonify({"error": "Invalid name"}), 400
    old_path = TMUX_LOGS_PATH / old_safe
    new_path = TMUX_LOGS_PATH / new_safe
    if not helpers.is_within(TMUX_LOGS_PATH, old_path) or not helpers.is_within(TMUX_LOGS_PATH, new_path):
      return jsonify({"error": "Forbidden"}), 403

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
      keys = json.loads(params.get("SecOCKeys") or "[]")
      keys = [key for key in keys if key.get("name") != name]
      params.put("SecOCKeys", json.dumps(keys))
    return jsonify(keys)

  @app.route("/api/tsk_keys", methods=["GET"])
  def get_secoc_keys():
    return jsonify(json.loads(params.get("SecOCKeys") or "[]"))

  @app.route("/api/tsk_keys", methods=["POST"])
  def save_secoc_keys():
    keys = request.get_json() or []
    if not isinstance(keys, list) or not all(
        isinstance(k, dict)
        and helpers.is_valid_secoc_key(k.get("value"))
        and helpers.is_safe_display_name(k.get("name"))
        for k in keys
    ):
      return jsonify({"error": "Each key needs a safe name and a 32-hexadecimal-character value"}), 400
    params.put("SecOCKeys", json.dumps(keys))

    return jsonify(keys)

  @app.route("/api/tsk_key_set", methods=["POST"])
  def set_secoc_key():
    data = request.get_json(silent=True) or {}
    value = data.get("value")
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
    for key, _, _, _ in frogpilot_default_params:
      if key in EXCLUDED_KEYS:
        continue

      raw_value = params.get(key)
      if isinstance(raw_value, bytes):
        value = raw_value.decode("utf-8", errors="replace")
      else:
        value = raw_value or "0"

      toggle_values[key] = value

    wrapped = json.dumps({"data": toggle_values}, indent=2)

    buffer = BytesIO(wrapped.encode("utf-8"))
    buffer.seek(0)

    return send_file(buffer, as_attachment=True, download_name="toggle_backup.json", mimetype="application/json")

  @app.route("/api/toggles/restore", methods=["POST"])
  def restore_toggle_values():
    request_data = request.get_json()
    if not request_data or "data" not in request_data:
      return jsonify({"success": False, "message": "Missing 'data' in request."}), 400

    allowed_keys = {key for key, _, _, _ in frogpilot_default_params if key not in EXCLUDED_KEYS}

    raw = request_data["data"]
    if isinstance(raw, dict):
      toggle_values = raw
    else:
      try:
        toggle_values = utilities.decode_parameters(raw)
      except Exception:
        return jsonify({"success": False, "message": "Invalid backup data."}), 400

    with _PARAMS_LOCK:
      for key, value in toggle_values.items():
        if key in allowed_keys:
          params.put(key, value)

    update_frogpilot_toggles()
    return jsonify({"success": True, "message": "Toggles restored!"})

  @app.route("/api/toggles/reset_default", methods=["POST"])
  def reset_toggle_values():
    cloudlog.warning("the_pond audit: toggle reset (default) + reboot requested")
    params.put_bool("DoToggleReset", True)
    threading.Timer(0.5, HARDWARE.reboot).start()
    return {"message": "Resetting toggles and rebooting..."}, 200

  @app.route("/api/toggles/reset_stock", methods=["POST"])
  def reset_toggle_values_to_stock():
    cloudlog.warning("the_pond audit: toggle reset (stock) + reboot requested")
    params.put_bool("DoToggleResetStock", True)
    threading.Timer(0.5, HARDWARE.reboot).start()
    return {"message": "Resetting toggles to stock and rebooting..."}, 200

  @app.route("/mapbox-help/<path:filename>", methods=["GET"])
  def serve_mapbox_help(filename):
    return send_from_directory(NAVIGATION_TRAINING_PATH, filename)

  @app.route("/thumbnails/<path:file_path>", methods=["GET"])
  def get_thumbnail(file_path):
    for footage_path in FOOTAGE_PATHS:
      abs_path = os.path.join(footage_path, file_path)
      if not helpers.is_within(footage_path, abs_path):
        continue
      if not os.path.exists(abs_path) and abs_path.endswith(("preview.png", "preview.gif")):
        qcamera = os.path.join(os.path.dirname(abs_path), "qcamera.ts")
        if os.path.exists(qcamera):
          if abs_path.endswith("preview.png"):
            utilities.video_to_png(qcamera, abs_path)
          else:
            utilities.video_to_gif(qcamera, abs_path)
      if os.path.exists(abs_path):
        return send_from_directory(footage_path, file_path, as_attachment=True)
    return {"error": "Thumbnail not found"}, 404

  @app.route("/video/<path>", methods=["GET"])
  def get_video(path):
    camera = request.args.get("camera")
    filename = {"driver": "dcamera.hevc", "wide": "ecamera.hevc"}.get(camera, "fcamera.hevc")
    for footage_path in FOOTAGE_PATHS:
      filepath = os.path.join(footage_path, path, filename)
      if not helpers.is_within(footage_path, filepath):
        continue
      if os.path.exists(filepath):
        mp4_path = utilities.ffmpeg_mp4_wrap_process_builder(filepath)
        return send_file(mp4_path, mimetype="video/mp4", conditional=True)
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
        sock.sendto(helpers.build_mdns_a_response(ip), addr)
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
  app.secret_key = secrets.token_hex(32)
  app.config["MAX_CONTENT_LENGTH"] = 256 * 1024 * 1024
  setup(app)
  return app

def main():
  app = create_app()

  port = 8083 if PC else 8082
  if PC:
    print("\"The Pond\" is not running on a comma device (PC mode, port 8083)")

  if not PC:
    threading.Thread(target=_gear_monitor, daemon=True, name="the_pond_gear").start()
    threading.Thread(target=_mdns_responder, daemon=True, name="the_pond_mdns").start()
    _ensure_port80_redirect(port)
    threading.Thread(target=_self_check, args=(port,), daemon=True, name="the_pond_selfcheck").start()

  app.run(host="0.0.0.0", port=port, threaded=True, debug=False)

if __name__ == "__main__":
  main()
