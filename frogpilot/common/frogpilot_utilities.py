#!/usr/bin/env python3
import json
import math
import numpy as np
import requests
import subprocess
import tarfile
import threading
import time
import urllib.error
import urllib.request
import uuid
import zipfile

from functools import cache
from pathlib import Path

import openpilot.system.sentry as sentry

from cereal import log, messaging
from opendbc.can.parser import CANParser
from openpilot.common.realtime import DT_DMON, DT_HW
from openpilot.selfdrive.car.toyota.carcontroller import LOCK_CMD
from openpilot.system.hardware import HARDWARE
from panda import Panda

from openpilot.frogpilot.common import frogpilot_api
from openpilot.frogpilot.common.frogpilot_variables import CRUISING_SPEED, DECEL_TIME_MARGIN, EARTH_RADIUS, ERROR_LOGS_PATH, KONIK_PATH, MAPS_PATH, MINIMUM_PLANNED_SPEED
from openpilot.frogpilot.common.frogpilot_variables import params, params_cache, params_memory

running_threads = {}

locks = {
  "backup_toggles": threading.Lock(),
  "capture_report": threading.Lock(),
  "download_all_models": threading.Lock(),
  "download_model": threading.Lock(),
  "download_theme": threading.Lock(),
  "flash_panda": threading.Lock(),
  "lock_doors": threading.Lock(),
  "send_stats": threading.Lock(),
  "update_checks": threading.Lock(),
  "update_maps": threading.Lock(),
  "update_openpilot": threading.Lock()
}

def run_thread_with_lock(name, target, args=(), report=True):
  with locks[name]:
    if not running_threads.get(name, threading.Thread()).is_alive():
      def wrapped_target(*t_args):
        try:
          target(*t_args)
        except urllib.error.HTTPError as error:
          print(f"HTTP error: {error}")
        except subprocess.CalledProcessError as error:
          print(f"CalledProcessError in thread '{name}': {error}")
        except Exception as exception:
          print(f"Error in thread '{name}': {exception}")
          if report:
            sentry.capture_exception(exception)
      thread = threading.Thread(target=wrapped_target, args=args, daemon=True)
      thread.start()
      running_threads[name] = thread

def calculate_distance_to_point(lat1, lon1, lat2, lon2):
  lat1_rad = math.radians(lat1)
  lon1_rad = math.radians(lon1)
  lat2_rad = math.radians(lat2)
  lon2_rad = math.radians(lon2)

  sin_delta_lat = math.sin((lat2_rad - lat1_rad) / 2)
  sin_delta_lon = math.sin((lon2_rad - lon1_rad) / 2)

  haversine = sin_delta_lat ** 2 + math.cos(lat1_rad) * math.cos(lat2_rad) * sin_delta_lon ** 2
  haversine = min(1, max(0, haversine))

  angular_distance = 2 * math.atan2(math.sqrt(haversine), math.sqrt(1 - haversine))
  return EARTH_RADIUS * angular_distance

def calculate_lane_width(lane, current_lane, road_edge=None):
  current_x = np.asarray(current_lane.x)
  current_y = np.asarray(current_lane.y)

  lane_y_interp = np.interp(current_x, np.asarray(lane.x), np.asarray(lane.y))
  distance_to_lane = np.median(np.abs(current_y - lane_y_interp))

  if road_edge is None:
    return float(distance_to_lane)

  road_edge_y_interp = np.interp(current_x, np.asarray(road_edge.x), np.asarray(road_edge.y))
  distance_to_road_edge = np.median(np.abs(current_y - road_edge_y_interp))

  if distance_to_road_edge < distance_to_lane:
    return 0.0

  return float(distance_to_lane)

def capture_report(discord_user, report, frogpilot_toggles):
  error_file_path = ERROR_LOGS_PATH / "error.txt"
  error_content = "No error log found."
  if error_file_path.exists():
    error_content = error_file_path.read_text()[-500:]

  payload = {
    "discord_user": discord_user,
    "error_content": error_content,
    "frogpilot_toggles": frogpilot_toggles,
    "report": report,
    "report_id": str(uuid.uuid4()),
    "report_schema_version": 1,
  }

  response = frogpilot_api.post("/v1/reports", json=payload, headers={"User-Agent": "frogpilot-api/1.0"}, timeout=30)
  if response is not None and 200 <= response.status_code < 300:
    print("Successfully sent error report!")
  else:
    status = "no_response" if response is None else response.status_code
    print(f"Error sending report (status={status})")

def clean_model_name(name):
  return (
    name.replace("🗺️", "")
    .replace("📡", "")
    .replace("👀", "")
    .replace("(Default)", "")
    .strip()
  )

def delete_file(path, print_error=True, report=True):
  path = Path(path)
  if path.is_file() or path.is_symlink():
    run_cmd(["sudo", "rm", "-f", str(path)], f"Deleted file: {path}", f"Failed to delete file: {path}", report=report)
  elif path.is_dir():
    run_cmd(["sudo", "rm", "-rf", str(path)], f"Deleted directory: {path}", f"Failed to delete directory: {path}", report=report)
  elif print_error:
    print(f"File not found: {path}")

def extract_tar(tar_file, extract_path):
  tar_file = Path(tar_file)
  extract_path = Path(extract_path)
  print(f"Extracting {tar_file} to {extract_path}")

  with tarfile.open(tar_file, "r:gz") as tar:
    tar.extractall(path=extract_path, filter="data")

  tar_file.unlink()
  print(f"Extraction completed: {tar_file} has been removed")

def extract_zip(zip_file, extract_path):
  zip_file = Path(zip_file)
  extract_path = Path(extract_path)
  print(f"Extracting {zip_file} to {extract_path}")

  extract_root = extract_path.resolve()
  with zipfile.ZipFile(zip_file, "r") as zip_ref:
    for member in zip_ref.namelist():
      if not (extract_path / member).resolve().is_relative_to(extract_root):
        raise ValueError(f"Refusing to extract path outside destination: {member}")
    zip_ref.extractall(extract_path)

  zip_file.unlink()
  print(f"Extraction completed: {zip_file} has been removed")

def flash_panda():
  for serial in Panda.list():
    try:
      with Panda(serial=serial) as panda:
        print(f"Flashing Panda {serial}")
        panda.flash(force=True)
    except Exception as exception:
      print(f"Failed to flash Panda {serial}: {exception}")
      sentry.capture_exception(exception)

  params_memory.remove("FlashPanda")

def get_lock_status(can_parser, can_sock):
  can_msgs = messaging.drain_sock_raw(can_sock, wait_for_one=True)
  can_parser.update_strings(can_msgs)
  return can_parser.vl["DOOR_LOCKS"]["LOCK_STATUS"]

def is_url_pingable(url):
  if not url:
    return False

  headers = {"User-Agent": "frogpilot-ping-test/1.0 (https://github.com/FrogAi/FrogPilot)"}
  try:
    response = requests.head(url, headers=headers, timeout=10, allow_redirects=True)
    try:
      if response.status_code in (405, 501):
        response.close()
        response = requests.get(url, headers=headers, timeout=10, allow_redirects=True, stream=True)

      return response.ok
    finally:
      response.close()

  except (requests.exceptions.ConnectionError, requests.exceptions.SSLError):
    return False
  except requests.exceptions.RequestException as error:
    print(f"{error.__class__.__name__} while pinging {url}: {error}")
    return False
  except Exception as exception:
    print(f"Unexpected error while pinging {url}: {exception}")
    return False

def load_json_file(path):
  path = Path(path)
  if not path.is_file():
    return {}

  try:
    with open(path) as file:
      data = json.load(file)
  except (OSError, json.JSONDecodeError):
    print(f"Failed to load JSON file: {path}")
    return {}

  if not isinstance(data, dict):
    print(f"Failed to load JSON file: {path}")
    return {}

  return data

def lock_doors(lock_doors_timer, sm):
  wait_for_no_driver(sm, door_checks=True, time_threshold=lock_doors_timer)

  can_parser = CANParser("toyota_nodsu_pt_generated", [("DOOR_LOCKS", 3)], bus=0)
  can_sock = messaging.sub_sock("can", timeout=100)

  pm = messaging.PubMaster(["sendcan"])

  while True:
    sm.update()

    if any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown):
      break

    sendcan_send = messaging.new_message("sendcan", 1)
    sendcan_send.sendcan[0].address = 0x750
    sendcan_send.sendcan[0].dat = LOCK_CMD
    sendcan_send.sendcan[0].src = 0
    pm.send("sendcan", sendcan_send)

    time.sleep(1)

    lock_status = get_lock_status(can_parser, can_sock)
    if lock_status == 0:
      break

def run_cmd(cmd, success_message, fail_message, env=None, report=True):
  try:
    result = subprocess.run(cmd, capture_output=True, check=True, env=env, text=True)
    print(success_message)
    return result.stdout.strip()
  except subprocess.CalledProcessError as exception:
    print(f"Command failed with error: {exception.stderr}")
    print(fail_message)
    if report:
      sentry.capture_exception(exception.stderr)
    return None
  except Exception as exception:
    print(f"Unexpected error occurred: {exception}")
    print(fail_message)
    if report:
      sentry.capture_exception(exception)
    return None

# Credit goes to Pfeiferj!
def select_road_curvature(model_data, v_ego, allowed_lateral_acceleration):
  velocity = np.asarray(model_data.velocity.x)

  road_curvature = np.where(velocity >= MINIMUM_PLANNED_SPEED, np.asarray(model_data.orientationRate.z) / np.maximum(velocity, 1), 0)
  absolute_curvature = np.abs(road_curvature)

  time_to_point = np.maximum(np.asarray(model_data.orientationRate.t), 1)

  curve_speed = np.maximum(np.sqrt(allowed_lateral_acceleration / np.maximum(absolute_curvature, 1e-6)), CRUISING_SPEED)
  required_deceleration = (v_ego - curve_speed) / np.maximum(time_to_point - DECEL_TIME_MARGIN, 1)
  index = np.argmax(required_deceleration if required_deceleration.max() > 0 else absolute_curvature)

  return float(road_curvature[index]), float(time_to_point[index]), float(absolute_curvature.max())

def update_json_file(path, data):
  with open(path, "w") as file:
    json.dump(data, file, indent=2, sort_keys=True)

def update_maps(now):
  try:
    maps_selected = json.loads(params.get("MapsSelected", encoding="utf-8") or "{}")
  except json.JSONDecodeError:
    maps_selected = None

  if not isinstance(maps_selected, dict):
    params.remove("MapsSelected")
    params_cache.remove("MapsSelected")
    return

  if not (maps_selected.get("nations") or maps_selected.get("states")):
    return

  now = now.astimezone()

  day = now.day
  is_first = day == 1
  is_Sunday = now.weekday() == 6
  schedule = params.get_int("PreferredSchedule")

  last_maps_update = params.get("LastMapsUpdate", encoding="utf-8")
  maps_downloaded = MAPS_PATH.exists() and last_maps_update is not None

  if maps_downloaded and (schedule == 0 or (schedule == 1 and not is_Sunday) or (schedule == 2 and not is_first)):
    return

  suffix = "th" if 11 <= day <= 13 else {1: "st", 2: "nd", 3: "rd"}.get(day % 10, "th")
  todays_date = now.strftime(f"%B {day}{suffix}, %Y")

  if maps_downloaded and last_maps_update == todays_date:
    return

  if params.get("OSMDownloadProgress", encoding="utf-8") is not None:
    return

  params_memory.put("OSMDownloadLocations", json.dumps(maps_selected))

  while params_memory.get("OSMDownloadLocations", encoding="utf-8") is not None:
    time.sleep(1)

  download_progress = json.loads(params.get("OSMDownloadProgress", encoding="utf-8") or "{}")

  if download_progress.get("downloaded_files") == download_progress.get("total_files", 0) > 0:
    params.put("LastMapsUpdate", todays_date)

    subprocess.run(["pkill", "mapd"], check=False)

  params.remove("OSMDownloadProgress")

def update_openpilot():
  def update_available():
    run_cmd(["pkill", "-SIGUSR1", "-f", "system.updated.updated"], "Checking for updates...", "Failed to check for update...", report=False)

    while params.get("UpdaterState", encoding="utf-8") != "checking...":
      time.sleep(1)

    while params.get("UpdaterState", encoding="utf-8") == "checking...":
      time.sleep(1)

    if not params.get_bool("UpdaterFetchAvailable"):
      return False

    while params.get_bool("IsOnroad") or running_threads.get("lock_doors", threading.Thread()).is_alive():
      time.sleep(60)

    run_cmd(["pkill", "-SIGHUP", "-f", "system.updated.updated"], "Update available, downloading...", "Failed to download update...", report=False)

    while not params.get_bool("UpdateAvailable"):
      time.sleep(60)

    return True

  if params.get("UpdaterState", encoding="utf-8") != "idle":
    return

  while params.get_bool("IsOnroad") or running_threads.get("lock_doors", threading.Thread()).is_alive():
    time.sleep(60)

  if not update_available():
    return

  while True:
    if not update_available():
      break

  HARDWARE.reboot()

@cache
def use_konik_server():
  return KONIK_PATH.is_file()

def wait_for_no_driver(sm, door_checks=False, time_threshold=60):
  can_parser = CANParser("toyota_nodsu_pt_generated", [("BODY_CONTROL_STATE", 3)], bus=0)
  can_sock = messaging.sub_sock("can", timeout=100)

  while sm["deviceState"].screenBrightnessPercent != 0 or any(proc.name == "dmonitoringd" and proc.running for proc in sm["managerState"].processes):
    sm.update()

    if any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown):
      return

    time.sleep(DT_HW)

  params.put_bool("IsDriverViewEnabled", True)

  while not any(proc.name == "dmonitoringd" and proc.running for proc in sm["managerState"].processes):
    sm.update()

    time.sleep(DT_HW)

  start_time = time.monotonic()
  while True:
    sm.update()

    elapsed_time = time.monotonic() - start_time
    if elapsed_time >= time_threshold:
      break

    if any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown):
      break

    if sm["driverMonitoringState"].faceDetected or not sm.alive["driverMonitoringState"]:
      start_time = time.monotonic()

    if door_checks:
      can_msgs = messaging.drain_sock_raw(can_sock, wait_for_one=True)
      can_parser.update_strings(can_msgs)

      door_open = any([can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_FL"], can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_FR"],
                       can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_RL"], can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_RR"]])
      if door_open:
        start_time = time.monotonic()

    time.sleep(DT_DMON)

  params.remove("IsDriverViewEnabled")
