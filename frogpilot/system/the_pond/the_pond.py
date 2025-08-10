#!/usr/bin/env python3
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timedelta, timezone
from flask import Flask, Response, jsonify, render_template, request, send_file, send_from_directory
from io import BytesIO
from pathlib import Path
from werkzeug.utils import secure_filename

import errno
import json
import os
import re
import requests
import secrets
import signal
import subprocess
import time
import traceback

from cereal import car, messaging
from opendbc.can.parser import CANParser
from openpilot.common.realtime import DT_HW
from openpilot.selfdrive.car.toyota.carcontroller import LOCK_CMD, UNLOCK_CMD
from openpilot.system.hardware import HARDWARE, PC
from openpilot.system.hardware.hw import Paths
from openpilot.system.loggerd.deleter import PRESERVE_ATTR_NAME, PRESERVE_ATTR_VALUE, PRESERVE_COUNT
from openpilot.system.version import get_build_metadata
from panda import Panda

from openpilot.frogpilot.common.frogpilot_utilities import delete_file, get_lock_status, run_cmd
from openpilot.frogpilot.common.frogpilot_variables import ERROR_LOGS_PATH, EXCLUDED_KEYS, SCREEN_RECORDINGS_PATH,\
                                                           frogpilot_default_params, params, update_frogpilot_toggles
from openpilot.frogpilot.system.the_pond import utilities

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

def setup(app):
  @app.errorhandler(404)
  def not_found(_):
    return render_template("index.html")

  @app.route("/", methods=["GET"])
  def index():
    return render_template("index.html")

  @app.route("/api/doors_available", methods=["GET"])
  def doors_available():
    with car.CarParams.from_bytes(params.get("CarParamsPersistent")) as cp_reader:
      CP = cp_reader.as_builder()

    return jsonify({"result": HARDWARE.get_device_type() != "tici" and CP.carName == "toyota"})

  @app.route("/api/doors/lock", methods=["POST"])
  def lock_doors():
    can_parser = CANParser("toyota_nodsu_pt_generated", [("DOOR_LOCKS", 3)], bus=0)
    can_sock = messaging.sub_sock("can", timeout=100)

    while True:
      with Panda(disable_checks=True) as panda:
        panda.set_safety_mode(panda.SAFETY_TOYOTA)
        panda.can_send(0x750, LOCK_CMD, 0)

      time.sleep(1)

      lock_status = get_lock_status(can_parser, can_sock)
      if lock_status == 0:
        break

    return {"message": "Doors locked!"}

  @app.route("/api/doors/unlock", methods=["POST"])
  def unlock_doors():
    can_parser = CANParser("toyota_nodsu_pt_generated", [("DOOR_LOCKS", 3)], bus=0)
    can_sock = messaging.sub_sock("can", timeout=100)

    while True:
      with Panda(disable_checks=True) as panda:
        panda.set_safety_mode(panda.SAFETY_TOYOTA)
        panda.can_send(0x750, UNLOCK_CMD, 0)

      time.sleep(1)

      lock_status = get_lock_status(can_parser, can_sock)
      if lock_status != 0:
        break

    return {"message": "Doors unlocked!"}

  @app.route("/api/error_logs", methods=["GET"])
  def get_error_logs():
    if request.accept_mimetypes["text/html"]:
      return render_template("v2/error-logs.jinja", active="error_logs")

    if request.accept_mimetypes["application/json"]:
      files = utilities.list_file(ERROR_LOGS_PATH)
      filtered = [file for file in files if not file.startswith("error")]
      return filtered, 200

  @app.route("/api/error_logs/delete_all", methods=["DELETE"])
  def delete_all_error_logs():
    for f in os.listdir(ERROR_LOGS_PATH):
      delete_file(os.path.join(ERROR_LOGS_PATH, f))
    return {"message": "All error logs deleted!"}, 200

  @app.route("/api/error_logs/<filename>", methods=["DELETE"])
  def delete_error_log(filename):
    delete_file(os.path.join(ERROR_LOGS_PATH, filename))
    return {"message": "Error log deleted!"}

  @app.route("/api/error_logs/<filename>", methods=["GET"])
  def get_error_log(filename):
    with open(os.path.join(ERROR_LOGS_PATH, filename)) as file:
      return file.read(), 200, {"Content-Type": "text/plain; charset=utf-8"}

  @app.route("/api/navigation", methods=["DELETE"])
  def clear_navigation():
    params.remove("NavDestination")
    return {"message": "Destination cleared"}

  @app.route("/api/navigation", methods=["GET"])
  def navigation():
    last_position = json.loads(
      params.get("LastGPSPosition", encoding="utf8") or
      "{\"latitude\": 51.276824158421331, \"longitude\": 30.221928335547232, \"altitude\": 111.0}"
    )

    return {
      "amap1Key": params.get("AMapKey1", encoding="utf8") or "",
      "amap2Key": params.get("AMapKey2", encoding="utf8") or "",
      "destination": params.get("NavDestination", encoding="utf8") or "",
      "isMetric": params.get_bool("IsMetric"),
      "lastPosition": {
        "latitude": str(last_position["latitude"]),
        "longitude": str(last_position["longitude"])
      },
      "mapboxPublic": params.get("MapboxPublicKey", encoding="utf8") or "",
      "mapboxSecret": params.get("MapboxSecretKey", encoding="utf8") or "",
      "previousDestinations": params.get("ApiCache_NavDestinations", encoding="utf8") or "",
    }

  @app.route("/api/navigation", methods=["POST"])
  def set_navigation():
    params.remove("NavDestination")

    time.sleep(1)

    params.put("NavDestination", json.dumps(request.json))
    return {"message": "Destination set"}

  @app.route("/api/navigation/favorite", methods=["DELETE"])
  def remove_favorite_destination():
    to_remove = request.json

    existing = json.loads(params.get("FavoriteDestinations", encoding="utf8") or "[]")
    favorites = [favorite for favorite in existing if favorite.get("routeId") != to_remove.get("routeId")]

    params.put("FavoriteDestinations", json.dumps(favorites))
    return jsonify(message="Destination removed from favorites!")

  @app.route("/api/navigation/favorite", methods=["GET"])
  def list_favorite_destinations():
    favorites = json.loads(params.get("FavoriteDestinations", encoding="utf8") or "[]")
    return jsonify(favorites=favorites)

  @app.route("/api/navigation/favorite", methods=["POST"])
  def add_favorite_destination():
    new_favorites = request.json

    existing = json.loads(params.get("FavoriteDestinations", encoding="utf8") or "[]")
    if new_favorites not in existing:
      existing.append(new_favorites)

    params.put("FavoriteDestinations", json.dumps(existing))
    return {"message": "Destination added to favorites!"}

  @app.route("/api/navigation/favorite/rename", methods=["POST"])
  def rename_favorite_destination():
    data = request.json
    route_id_to_rename = data.get("routeId")
    new_name = data.get("name")
    is_home = data.get("is_home")
    is_work = data.get("is_work")

    if not route_id_to_rename:
      return jsonify({"error": "Missing routeId"}), 400

    existing_favorites = json.loads(params.get("FavoriteDestinations", encoding="utf8") or "[]")

    if is_home:
      for favorite in existing_favorites:
        favorite.pop("is_home", None)
    if is_work:
      for favorite in existing_favorites:
        favorite.pop("is_work", None)

    found = False
    for favorite in existing_favorites:
      if favorite.get("routeId") == route_id_to_rename:
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
    return params.get(request.args.get("key")) or "", 200

  @app.route("/api/routes", methods=["GET"])
  def list_routes():
    def generate():
      routes = [(path, name) for path in FOOTAGE_PATHS for name in utilities.get_routes_names(path)]
      total = len(routes)
      yield f"data: {json.dumps({'progress': 0, 'total': total})}\n\n"

      with ThreadPoolExecutor(max_workers=10) as executor:
        futures = {executor.submit(utilities.process_route, path, name): (path, name) for path, name in routes}
        for processed, future in enumerate(as_completed(futures), start=1):
          try:
            result = future.result()
            yield f"data: {json.dumps({'routes': [result]})}\n\n"
          except Exception as exception:
            print(f"Error processing route: {exception}")
          yield f"data: {json.dumps({'progress': processed, 'total': total})}\n\n"

        for path, name in routes:
          utilities.process_route_gif(path, name)

    return Response(generate(), mimetype="text/event-stream")

  @app.route("/api/routes/<name>", methods=["DELETE"])
  def delete_route(name):
    for footage_path in FOOTAGE_PATHS:
      for segment in os.listdir(footage_path):
        if segment.startswith(name):
          delete_file(os.path.join(footage_path, segment))
    return {"message": "Route deleted!"}, 200

  @app.route("/api/routes/delete_all", methods=["DELETE"])
  def delete_all_routes():
    route_names = set()
    for footage_path in FOOTAGE_PATHS:
      if os.path.exists(footage_path):
        for segment in os.listdir(footage_path):
          route_names.add(segment.split("--")[0])

    for route_name in sorted(list(route_names)):
      for footage_path in FOOTAGE_PATHS:
        if os.path.exists(footage_path):
          for segment in os.listdir(footage_path):
            if segment.startswith(route_name):
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
      if PRESERVE_ATTR_NAME in os.listxattr(route_path):
        os.removexattr(route_path, PRESERVE_ATTR_NAME)
        return {"message": "Route unpreserved!"}, 200
    return {"error": "Route not found"}, 404

  @app.route("/api/routes/<name>", methods=["GET"])
  def get_route(name):
    for footage_path in FOOTAGE_PATHS:
      base_path = f"{footage_path}{name}--0"
      if os.path.exists(base_path):
        segments = utilities.get_segments_in_route(name, footage_path)
        if not segments:
          break

        segment_urls = [f"/video/{segment}" for segment in segments]
        total_duration = sum(utilities.get_video_duration(f"{footage_path}{name}--{i}/fcamera.hevc") for i in range(len(segment_urls)))
        return {
          "name": name,
          "segment_urls": segment_urls,
          "total_duration": round(total_duration),
          "date": utilities.get_route_start_time(footage_path),
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

      segments_to_process = [s for s in os.listdir(footage_path) if s.startswith(route_name) and os.path.isdir(os.path.join(footage_path, s))]
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
    renamed = False

    for footage_path in FOOTAGE_PATHS:
      if not os.path.exists(footage_path):
        continue

      segments_to_process = [s for s in os.listdir(footage_path) if s.startswith(old_name) and os.path.isdir(os.path.join(footage_path, s))]
      if not segments_to_process:
        continue

      for segment in segments_to_process:
        segment_dir = os.path.join(footage_path, segment)
        for item in os.listdir(segment_dir):
          if not item.endswith((".hevc", ".ts", ".png", ".gif", "rlog")):
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
          return jsonify({"error": f"Error creating new name file: {e}"}), 500

    if renamed:
      return jsonify({"message": "Route renamed successfully!"}), 200
    else:
      return jsonify({"error": "Route not found"}), 404

  @app.route("/api/screen_recordings/delete/<path:filename>", methods=["DELETE"])
  def delete_screen_recording(filename):
    mp4_path = SCREEN_RECORDINGS_PATH / filename
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

      with ThreadPoolExecutor(max_workers=10) as executor:
        futures = {executor.submit(utilities.process_screen_recording, mp4): mp4 for mp4 in recordings}
        for processed, future in enumerate(as_completed(futures), start=1):
          try:
            result = future.result()
            yield f"data: {json.dumps({'recordings': [result]})}\n\n"
          except Exception as exception:
            print(f"Error processing recording: {exception}")

          yield f"data: {json.dumps({'progress': processed, 'total': total})}\n\n"

        for recording in recordings:
          utilities.process_screen_recording_gif(recording)

    return Response(generate(), mimetype="text/event-stream")

  @app.route("/screen_recordings/<path:filename>", methods=["GET"])
  def serve_screen_recording_asset(filename):
    return send_from_directory(SCREEN_RECORDINGS_PATH, filename)

  @app.route("/api/screen_recordings/rename", methods=["POST"])
  def rename_screen_recording():
    data = request.get_json() or {}
    old = data.get("old")
    new_raw = data.get("new")

    if not old or not new_raw:
      return {"error": "Missing filenames"}, 400

    new = secure_filename(new_raw)
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

  @app.route("/api/speed_limits", methods=["GET"])
  def speed_limits():
    data = json.loads(params.get("SpeedLimitsFiltered") or "[]")
    current_time = (datetime.now(timezone.utc) - timedelta(days=6, hours=23)).isoformat()
    data = [{**e, "last_vetted": current_time} for e in data]

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
    elif build_metadata.tested_channel:
      env = "Staging"
    else:
      env = short_branch

    try:
      response = requests.get(f"https://api.comma.ai/v1/devices/{params.get('DongleId', encoding='utf8')}/firehose_stats", timeout=10)
      response.raise_for_status()
      firehose_stats = response.json().get("firehose", 0)
    except (requests.RequestException, ValueError) as e:
      firehose_stats = 0

    return {
      "diskUsage": utilities.get_disk_usage(),
      "driveStats": utilities.get_drive_stats(),
      "firehoseStats": {"segments": firehose_stats},
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
    base = "/data/tailscale"
    tailscale_binary = f"{base}/tailscale"
    tailscaled_binary = f"{base}/tailscaled"

    systemd_unit = "/etc/systemd/system/tailscaled.service"

    if os.path.exists(tailscale_binary) and os.path.exists(tailscaled_binary) and os.path.exists(systemd_unit):
      return jsonify({"installed": True})

    result = subprocess.run(["which", "tailscale"], capture_output=True, text=True)
    if result.returncode == 0:
      return jsonify({"installed": True})

    return jsonify({"installed": False})

  @app.route("/api/tailscale/setup", methods=["POST"])
  def tailscale_setup():
    arch = "arm64"
    base = "/data/tailscale"

    result = subprocess.run(
      "curl -s https://pkgs.tailscale.com/stable/ | grep -oP 'tailscale_\\K[0-9]+\\.[0-9]+\\.[0-9]+' | sort -V | tail -1",
      shell=True, capture_output=True, text=True
    )

    version = result.stdout.strip() or "1.84.0"

    bin_dir = f"{base}/tailscale_{version}_{arch}"
    state = f"{base}/state"
    socket = f"{base}/tailscaled.sock"
    tgz_path = f"{base}/tailscale.tgz"

    tgz_url = f"https://pkgs.tailscale.com/stable/tailscale_{version}_{arch}.tgz"

    os.makedirs(state, exist_ok=True)

    run_cmd(["curl", "-fsSL", tgz_url, "-o", tgz_path], "Downloaded Tailscale archive.", "Failed to download Tailscale archive.")
    run_cmd(["tar", "xzf", tgz_path, "-C", base], "Extracted Tailscale archive.", "Failed to extract Tailscale archive.")
    run_cmd(["cp", f"{bin_dir}/tailscale", f"{base}/tailscale"], "Copied tailscale binary.", "Failed to copy tailscale binary.")
    run_cmd(["cp", f"{bin_dir}/tailscaled", f"{base}/tailscaled"], "Copied tailscaled binary.", "Failed to copy tailscaled binary.")
    run_cmd(["chmod", "+x", f"{base}/tailscale", f"{base}/tailscaled"], "Made binaries executable.", "Failed to chmod binaries.")

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

    run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Remounted / as read-write.", "Failed to remount / as read-write.")
    run_cmd(["sudo", "install", "-m", "644", unit_tmp, "/etc/systemd/system/tailscaled.service"], "Installed systemd unit.", "Failed to install systemd unit.")
    run_cmd(["sudo", "systemctl", "daemon-reload"], "Reloaded systemd daemon.", "Failed to reload systemd daemon.")
    run_cmd(["sudo", "systemctl", "enable", "/etc/systemd/system/tailscaled.service"], "Enabled tailscaled service.", "Failed to enable tailscaled service.")
    run_cmd(["sudo", "systemctl", "restart", "tailscaled"], "Started tailscaled service.", "Failed to start tailscaled service.")

    proc = subprocess.Popen(
      ["sudo", f"{base}/tailscale", "--socket", socket, "up", "--hostname", f"{HARDWARE.get_device_type()}-the-pond"],
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      text=True,
      preexec_fn=os.setsid
    )

    auth_url = None
    for line in proc.stdout:
      match = re.search(r"https://login\.tailscale\.com/\S+", line)
      if match and not auth_url:
        auth_url = match.group(0)
        run_cmd(["sudo", "kill", "-TERM", f"-{proc.pid}"], "Sent SIGTERM to Tailscale setup process.", "Failed to send SIGTERM to Tailscale setup process.")
        proc.wait(timeout=5)
        break

    return jsonify({
      "message": "Tailscale setup started. Please authenticate in your browser.",
      "auth_url": auth_url
    }), 200

  @app.route("/api/tailscale/uninstall", methods=["POST"])
  def tailscale_uninstall():
    base = "/data/tailscale"
    state = f"{base}/state"
    unit_path = "/etc/systemd/system/tailscaled.service"
    local_unit = f"{base}/tailscaled.service"

    run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Remounted / as read-write.", "Failed to remount /.")
    run_cmd(["sudo", "systemctl", "stop", "tailscaled"], "Stopped tailscaled.", "Failed to stop tailscaled.")
    run_cmd(["sudo", "systemctl", "disable", "tailscaled"], "Disabled tailscaled.", "Failed to disable tailscaled.")

    if os.path.exists(unit_path):
      run_cmd(["sudo", "rm", unit_path], "Removed systemd unit file.", "Failed to remove systemd unit file.")
      run_cmd(["sudo", "systemctl", "daemon-reload"], "Reloaded systemd daemon.", "Failed to reload systemd.")

    delete_file(local_unit)

    for filename in ["tailscale", "tailscaled", "tailscale.tgz"]:
      delete_file(os.path.join(base, filename))

    for item in os.listdir(base):
      if item.startswith("tailscale_"):
        item_path = os.path.join(base, item)
        if os.path.isdir(item_path):
          run_cmd(["sudo", "rm", "-rf", item_path], f"Removed {item_path}.", f"Failed to remove {item_path}.")

    if os.path.exists(state):
      run_cmd(["sudo", "rm", "-rf", state], "Removed tailscale state dir.", "Failed to remove tailscale state dir.")

    if os.path.exists(base):
      run_cmd(["sudo", "rm", "-rf", base], "Removed tailscale dir.", "Failed to remove tailscale dir.")

    return jsonify({"message": "Tailscale uninstalled!"}), 200

  @app.route("/api/tmux_log/capture", methods=["POST"])
  def capture_tmux_log_route():
    TMUX_LOGS_PATH.mkdir(parents=True, exist_ok=True)

    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    log_filename = f"tmux_log_{timestamp}.json"
    log_path = TMUX_LOGS_PATH / log_filename

    run_cmd(["tmux", "capture-pane", "-J", "-S", "-"], "Captured tmux pane.", "Failed to capture tmux pane.")

    result = subprocess.run(["tmux", "show-buffer"], capture_output=True, text=True, check=True)
    log_path.write_text(result.stdout, encoding="utf-8")

    run_cmd(["tmux", "delete-buffer"], "Deleted tmux buffer.", "Failed to delete tmux buffer.")
    return jsonify({"message": "Captured console log successfully!", "log_file": log_filename}), 200

  @app.route("/api/tmux_log/delete/<filename>", methods=["DELETE"])
  def delete_tmux_log(filename):
    file_path = TMUX_LOGS_PATH / filename
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
    if subprocess.run(["tmux", "has-session", "-t", "comma"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0:
      run_cmd(["tmux", "new-session", "-d", "-s", "comma", "-x", "240", "-y", "70", "bash"], "Started tmux session", "Failed to start tmux session")
    else:
      run_cmd(["tmux", "resize-window", "-t", "comma:0", "-x", "240", "-y", "70"], "Resized tmux window", "Failed to resize tmux window")

    def generate():
      while True:
        output = subprocess.check_output(["tmux", "capture-pane", "-t", "comma:0", "-p", "-S", "-1000"], text=True)

        yield "data: " + "\n".join(reversed(output.splitlines())).replace("\n", "\ndata: ") + "\n\n"

        time.sleep(0.1)
    return Response(generate(), mimetype="text/event-stream")

  @app.route("/api/tmux_log/rename/<old>/<new>", methods=["PUT"])
  def rename_tmux_log_path_params(old, new):
    old_path = TMUX_LOGS_PATH / old
    new_safe = secure_filename(new)
    new_path = TMUX_LOGS_PATH / new_safe

    if not old_path.exists():
      return jsonify({"error": "Original file not found"}), 404

    if new_path.exists():
      return jsonify({"error": "Target file already exists"}), 400

    old_path.rename(new_path)

    return jsonify({"message": f"Renamed {old} to {new_safe}!"}), 200

  @app.route("/api/tsk_available", methods=["GET"])
  def tsk_available():
    with car.CarParams.from_bytes(params.get("CarParamsPersistent")) as cp_reader:
      CP = cp_reader.as_builder()

    return jsonify({"result": CP.secOcRequired})

  @app.route("/api/tsk_keys", methods=["DELETE"])
  def delete_secoc_key():
    name = request.args.get("name")
    keys = json.loads(params.get("SecOCKeys") or "[]")
    keys = [key for key in keys if key.get("name") != name]
    params.put("SecOCKeys", json.dumps(keys))
    return jsonify(keys)

  @app.route("/api/tsk_keys", methods=["GET"])
  def get_secoc_keys():
    return jsonify(json.loads(params.get("SecOCKeys", "[]")))

  @app.route("/api/tsk_keys", methods=["POST"])
  def save_secoc_keys():
    keys = request.get_json() or []
    params.put("SecOCKeys", json.dumps(keys))

    return jsonify(keys)

  @app.route("/api/tsk_key_set", methods=["POST"])
  def set_secoc_key():
    data = request.get_json()
    if not data or "value" not in data:
      return jsonify({"error": "Missing key value"}), 400

    value = data["value"]
    if not isinstance(value, str):
      return jsonify({"error": "Key value must be a string"}), 400

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

    encoded = utilities.encode_parameters(toggle_values)
    wrapped = json.dumps({"data": encoded}, indent=2)

    buffer = BytesIO(wrapped.encode("utf-8"))
    buffer.seek(0)

    return send_file(buffer, as_attachment=True, download_name="toggle_backup.json", mimetype="application/json")

  @app.route("/api/toggles/restore", methods=["POST"])
  def restore_toggle_values():
    request_data = request.get_json()
    if not request_data or "data" not in request_data:
      return jsonify({"success": False, "message": "Missing 'data' in request."}), 400

    allowed_keys = {key for key, _, _, _ in frogpilot_default_params if key not in EXCLUDED_KEYS}

    toggle_values = utilities.decode_parameters(request_data["data"])
    for key, value in toggle_values.items():
      if key in allowed_keys:
        params.put(key, value)

    update_frogpilot_toggles()
    return jsonify({"success": True, "message": "Toggles restored!"})

  @app.route("/api/toggles/reset_default", methods=["POST"])
  def reset_toggle_values():
    params.put_bool("DoToggleReset", True)
    HARDWARE.reboot()

  @app.route("/api/toggles/reset_stock", methods=["POST"])
  def reset_toggle_values_to_stock():
    params.put_bool("DoToggleResetStock", True)
    HARDWARE.reboot()

  @app.route("/mapbox-help/<path:filename>", methods=["GET"])
  def serve_mapbox_help(filename):
    return send_from_directory("/data/openpilot/frogpilot/navigation/navigation_training", filename)

  @app.route("/playground", methods=["GET"])
  def playground():
    return render_template("playground.html")

  @app.route("/thumbnails/<path:file_path>", methods=["GET"])
  def get_thumbnail(file_path):
    for footage_path in FOOTAGE_PATHS:
      if os.path.exists(os.path.join(footage_path, file_path)):
        return send_from_directory(footage_path, file_path, as_attachment=True)
    return {"error": "Thumbnail not found"}, 404

  @app.route("/video/<path>", methods=["GET"])
  def get_video(path):
    camera = request.args.get("camera")
    filename = {"driver": "dcamera.hevc", "wide": "ecamera.hevc"}.get(camera, "fcamera.hevc")

    for footage_path in FOOTAGE_PATHS:
      filepath = f"{footage_path}{path}/{filename}"
      if os.path.exists(filepath):
        process = utilities.ffmpeg_mp4_wrap_process_builder(filepath)
        return Response(process.stdout.read(), status=200, mimetype="video/mp4")

    return {"error": "Video not found"}, 404


def main():
  app = Flask(__name__, static_folder="assets", static_url_path="/assets")
  setup(app)

  debug = PC or __package__ == "the_pond"
  port = 8083 if debug else 8082

  if debug:
    print("\"The Pond\" is not running on a comma device, enabling debug mode")

  app.secret_key = secrets.token_hex(32)
  app.run(host="0.0.0.0", port=port, debug=debug)

if __name__ == "__main__":
  main()
