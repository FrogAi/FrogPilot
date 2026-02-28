#!/usr/bin/env python3
import dataclasses
import json
import math
import numpy as np
import requests
import shutil
import subprocess
import tarfile
import threading
import time
import urllib.error
import urllib.request
import zipfile

from functools import cache
from pathlib import Path

import openpilot.system.sentry as sentry

from cereal import log, messaging
from opendbc.can.parser import CANParser
from openpilot.common.params import Params, ParamKeyType
from openpilot.common.realtime import DT_DMON, DT_HW
from openpilot.selfdrive.car.toyota.carcontroller import LOCK_CMD
from openpilot.system.hardware import HARDWARE
from openpilot.system.version import get_build_metadata
from panda import Panda

from openpilot.frogpilot.common.frogpilot_variables import EARTH_RADIUS, ERROR_LOGS_PATH, EXCLUDED_KEYS, FROGPILOT_API, KONIK_PATH, MAPD_PATH, MAPS_PATH, GearShifter, frogpilot_default_params, params, params_cache, params_memory, update_frogpilot_toggles

running_threads = {}

locks = {
  "backup_toggles": threading.Lock(),
  "download_all_models": threading.Lock(),
  "download_model": threading.Lock(),
  "download_theme": threading.Lock(),
  "flash_panda": threading.Lock(),
  "lock_doors": threading.Lock(),
  "update_checks": threading.Lock(),
  "update_maps": threading.Lock(),
  "update_openpilot": threading.Lock(),
  "update_tinygrad": threading.Lock()
}

def run_thread_with_lock(name, target, args=(), report=True):
  if not running_threads.get(name, threading.Thread()).is_alive():
    with locks[name]:
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

def calculate_bearing_offset(latitude, longitude, current_bearing, distance):
  bearing = math.radians(current_bearing)
  lat_rad = math.radians(latitude)
  lon_rad = math.radians(longitude)

  delta = distance / EARTH_RADIUS

  new_latitude = math.asin(math.sin(lat_rad) * math.cos(delta) + math.cos(lat_rad) * math.sin(delta) * math.cos(bearing))
  new_longitude = lon_rad + math.atan2(math.sin(bearing) * math.sin(delta) * math.cos(lat_rad),  math.cos(delta) - math.sin(lat_rad) * math.sin(new_latitude))
  return math.degrees(new_latitude), math.degrees(new_longitude)

def calculate_distance_to_point(lat1, lon1, lat2, lon2):
  delta_lat = lat2 - lat1
  delta_lon = lon2 - lon1

  a = (math.sin(delta_lat / 2) ** 2) + math.cos(lat1) * math.cos(lat2) * (math.sin(delta_lon / 2) ** 2)
  c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))

  return EARTH_RADIUS * c

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

# Credit goes to Pfeiferj!
def calculate_road_curvature(modelData, v_ego):
  orientation_rate = np.array(modelData.orientationRate.z)
  velocity = np.array(modelData.velocity.x)
  timebase = np.array(modelData.orientationRate.t)

  lateral_acceleration = orientation_rate * velocity
  index = np.argmax(np.abs(lateral_acceleration))
  predicted_lateral_acc = float(lateral_acceleration[index])
  time_to_curve = float(timebase[index])

  return predicted_lateral_acc / max(v_ego, 1)**2, max(time_to_curve, 1)

def capture_report(discord_user, report, frogpilot_toggles):
  if not is_url_pingable(FROGPILOT_API):
    return

  api_token, build_metadata, device_type, dongle_id = get_frogpilot_api_info()

  error_file_path = ERROR_LOGS_PATH / "error.txt"
  error_content = "No error log found."
  if error_file_path.exists():
    error_content = error_file_path.read_text()[:1000]

  payload = {
    "api_token": api_token,
    "build_metadata": build_metadata,
    "device": device_type,
    "discord_user": discord_user,
    "error_content": error_content,
    "frogpilot_dongle_id": dongle_id,
    "frogpilot_toggles": frogpilot_toggles,
    "report": report,
  }

  try:
    response = requests.post(f"{FROGPILOT_API}/discord/report", json=payload, headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"}, timeout=30)
    response.raise_for_status()
    print("Successfully sent error report!")
  except requests.exceptions.RequestException as exception:
    print(f"Error sending report: {exception}")

def check_remote_toggles(started=False, sm=None, boot_run=False):
  if not boot_run:
    if started and sm["carState"].gearShifter != GearShifter.park:
      return
    if sm["deviceState"].screenBrightnessPercent == 0:
      return

  if not params.get_bool("PondPaired"):
    return

  if not is_url_pingable(FROGPILOT_API):
    return

  try:
    api_token = params.get("FrogPilotApiToken", encoding="utf-8")
    dongle_id = params.get("FrogPilotDongleId", encoding="utf-8")

    if not dongle_id or not api_token:
      return

    response = requests.get(
      f"{FROGPILOT_API}/pond/toggles/pending",
      params={"dongle_id": dongle_id, "api_token": api_token},
      headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"},
      timeout=10,
    )
    response.raise_for_status()

    data = response.json()

    if data.get("paired") is False:
      params.put_bool("PondPaired", False)
      print("Device was unpaired remotely")
      return

    toggles = data.get("toggles")
    if not toggles:
      return

    for key, value in toggles.items():
      if key in EXCLUDED_KEYS:
        continue
      if not params.check_key(key):
        print(f"Skipping unknown param key: {key}")
        continue
      params.put(key, value)

    update_frogpilot_toggles()

    requests.post(
      f"{FROGPILOT_API}/pond/toggles/ack",
      json={
        "api_token": api_token,
        "device": HARDWARE.get_device_type(),
        "frogpilot_dongle_id": dongle_id,
      },
      headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"},
      timeout=10,
    ).raise_for_status()

    print(f"Successfully applied {len(toggles)} remote toggles")

  except Exception as e:
    print(f"Failed to check remote toggles: {e}")

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
    tar.extractall(path=extract_path)

  tar_file.unlink()
  print(f"Extraction completed: {tar_file} has been removed")

def extract_zip(zip_file, extract_path):
  zip_file = Path(zip_file)
  extract_path = Path(extract_path)
  print(f"Extracting {zip_file} to {extract_path}")

  with zipfile.ZipFile(zip_file, "r") as zip_ref:
    zip_ref.extractall(extract_path)

  zip_file.unlink()
  print(f"Extraction completed: {zip_file} has been removed")

def flash_panda():
  for serial in Panda.list():
    try:
      with Panda(serial=serial) as panda:
        print(f"Flashing Panda {serial}")
        panda.flash()
    except Exception as exception:
      print(f"Failed to flash Panda {serial}: {exception}")
      sentry.capture_exception(exception)

  params_memory.remove("FlashPanda")

def get_frogpilot_api_info():
  api_token = params.get("FrogPilotApiToken", encoding="utf-8")
  build_metadata = dataclasses.asdict(get_build_metadata())
  device_type = HARDWARE.get_device_type()
  dongle_id = params.get("FrogPilotDongleId", encoding="utf-8")

  return api_token, build_metadata, device_type, dongle_id


def get_lock_status(can_parser, can_sock):
  can_msgs = messaging.drain_sock_raw(can_sock, wait_for_one=True)
  can_parser.update_strings(can_msgs)
  return can_parser.vl["DOOR_LOCKS"]["LOCK_STATUS"]

def get_sentry_dsn():
  try:
    api_token, build_metadata, device_type, dongle_id = get_frogpilot_api_info()

    payload = {
      "api_token": api_token,
      "build_metadata": build_metadata,
      "device": device_type,
      "frogpilot_dongle_id": dongle_id,
    }

    response = requests.post(f"{FROGPILOT_API}/sentry", json=payload, headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"}, timeout=10)
    response.raise_for_status()
    return response.json().get("dsn", "")
  except Exception:
    return ""


def is_url_pingable(url):
  if not url:
    return False

  if not hasattr(is_url_pingable, "session"):
    is_url_pingable.session = requests.Session()
    is_url_pingable.session.headers.update({"User-Agent": "frogpilot-ping-test/1.0 (https://github.com/FrogAi/FrogPilot)"})

  try:
    response = is_url_pingable.session.head(url, timeout=10, allow_redirects=True)
    if response.status_code in (405, 501):
      response = is_url_pingable.session.get(url, timeout=10, allow_redirects=True, stream=True)

    is_accessible = response.ok
    response.close()
    return is_accessible

  except (requests.exceptions.ConnectionError, requests.exceptions.SSLError):
    return False
  except requests.exceptions.RequestException as error:
    print(f"{error.__class__.__name__} while pinging {url}: {error}")
    return False
  except Exception as exception:
    print(f"Unexpected error while pinging {url}: {exception}")
    return False

def load_json_file(path):
  if path.is_file():
    with open(path) as file:
      return json.load(file)
  return {}

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

def update_json_file(path, data):
  with open(path, "w") as file:
    json.dump(data, file, indent=2, sort_keys=True)

def update_maps(now):
  while not MAPD_PATH.exists():
    time.sleep(60)

  maps_selected = json.loads(params.get("MapsSelected", encoding="utf-8") or "{}")

  if isinstance(maps_selected, int):
    params.remove("MapsSelected")
    params_cache.remove("MapsSelected")
    return

  if not (maps_selected.get("nations") or maps_selected.get("states")):
    return

  day = now.day
  is_first = day == 1
  is_Sunday = now.weekday() == 6
  schedule = params.get_int("PreferredSchedule")

  maps_downloaded = MAPS_PATH.exists()
  if maps_downloaded and (schedule == 0 or (schedule == 1 and not is_Sunday) or (schedule == 2 and not is_first)):
    return

  suffix = "th" if 11 <= day <= 13 else {1: "st", 2: "nd", 3: "rd"}.get(day % 10, "th")
  todays_date = now.strftime(f"%B {day}{suffix}, %Y")

  if maps_downloaded and params.get("LastMapsUpdate", encoding="utf-8") == todays_date:
    return

  if params.get("OSMDownloadProgress", encoding="utf-8") is None:
    params_memory.put("OSMDownloadLocations", json.dumps(maps_selected))

  while params.get("OSMDownloadProgress", encoding="utf-8") is not None:
    time.sleep(60)

  params.put("LastMapsUpdate", todays_date)

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

def upload_toggles():
  if not is_url_pingable(FROGPILOT_API):
    return

  try:
    api_token, build_metadata, device_type, dongle_id = get_frogpilot_api_info()
    if not dongle_id or not api_token:
      return

    toggles = {}
    for key, _, _, _ in frogpilot_default_params:
      if key in EXCLUDED_KEYS or params.get_key_flag(key) & ParamKeyType.DONT_LOG:
        continue
      val = params.get(key)
      if val is not None:
        toggles[key] = val.decode("utf-8", "replace") if isinstance(val, (bytes, bytearray)) else val

    if not toggles:
      return

    payload = {
      "api_token": api_token,
      "build_metadata": build_metadata,
      "device": device_type,
      "dongle_id": dongle_id,
      "frogpilot_dongle_id": dongle_id,
      "toggles": toggles,
    }

    requests.post(
      f"{FROGPILOT_API}/pond/toggles/sync",
      json=payload,
      headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"},
      timeout=10,
    ).raise_for_status()

    print("Successfully uploaded toggles to FrogPilot.com")

  except Exception as e:
    print(f"Failed to upload toggles: {e}")

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
