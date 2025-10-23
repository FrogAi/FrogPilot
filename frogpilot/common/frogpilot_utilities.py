#!/usr/bin/env python3
import io
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
from openpilot.common.realtime import DT_DMON, DT_HW
from openpilot.selfdrive.car.toyota.carcontroller import LOCK_CMD
from openpilot.system.hardware import HARDWARE
from panda import Panda

from openpilot.frogpilot.common.frogpilot_variables import DISCORD_WEBHOOK_URL_REPORT, EARTH_RADIUS, ERROR_LOGS_PATH, KONIK_PATH, MAPD_PATH, MAPS_PATH, params, params_cache, params_memory

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
  distance_to_lane = np.mean(np.abs(current_y - lane_y_interp))

  if road_edge is None:
    return float(distance_to_lane)

  road_edge_y_interp = np.interp(current_x, np.asarray(road_edge.x), np.asarray(road_edge.y))
  distance_to_road_edge = np.mean(np.abs(current_y - road_edge_y_interp))

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
  if not DISCORD_WEBHOOK_URL_REPORT:
    return

  error_file_path = ERROR_LOGS_PATH / "error.txt"
  error_content = "No error log found."
  if error_file_path.exists():
    error_content = error_file_path.read_text()[:1000]

  toggles_bytes = io.BytesIO(json.dumps(frogpilot_toggles, indent=2).encode("utf-8"))

  message = (
    f"**🚨 New Error Report**\n\n"
    f"**User:** `{discord_user}`\n\n"
    f"**Report:**\n"
    f"```{report}```\n"
    f"**Error Log:**\n"
    f"```{error_content}```\n"
    f"**Toggle Settings:**\n"
  )

  try:
    resp = requests.post(
      DISCORD_WEBHOOK_URL_REPORT,
      data={"content": message},
      files={"file": ("frogpilot_toggles.json", toggles_bytes, "application/json")}
    )
    if resp.status_code not in (200, 204):
      print(f"Discord notification failed: {resp.status_code} {resp.text}")
      return

    mention_resp = requests.post(
      DISCORD_WEBHOOK_URL_REPORT,
      json={"content": "<@&1198482895342411846>"}
    )
    if mention_resp.status_code not in (200, 204):
      print(f"Discord mention failed: {mention_resp.status_code} {mention_resp.text}")
  except Exception as exception:
    print(f"Error sending Discord message: {exception}")

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
      panda = Panda(serial)
      panda.reset(enter_bootstub=True)
      panda.flash()
      panda.close()
    except Exception as exception:
      print(f"Error flashing Panda {serial}: {exception}")
      sentry.capture_exception(exception)

  params_memory.remove("FlashPanda")

def get_lock_status(can_parser, can_sock):
  can_msgs = messaging.drain_sock_raw(can_sock, wait_for_one=True)
  can_parser.update_strings(can_msgs)
  return can_parser.vl["DOOR_LOCKS"]["LOCK_STATUS"]

def is_url_pingable(url):
  headers = {"User-Agent": "frogpilot-ping-test/1.0 (https://github.com/FrogAi/FrogPilot)"}
  try:
    response = requests.head(url, headers=headers, timeout=10, allow_redirects=True)
    if response.status_code in (405, 501):
      response = requests.get(url, headers=headers, timeout=10, allow_redirects=True, stream=True)
    return response.ok
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

  while True:
    sm.update()

    if any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown):
      break

    with Panda(disable_checks=True) as panda:
      panda.set_safety_mode(panda.SAFETY_TOYOTA)
      panda.can_send(0x750, LOCK_CMD, 0)

    time.sleep(1)

    lock_status = get_lock_status(can_parser, can_sock)
    if lock_status == 0:
      break

def run_cmd(cmd, success_message, fail_message, report=True, env=None):
  try:
    result = subprocess.run(cmd, capture_output=True, check=True, env=env, text=True)
    print(success_message)
    return result.stdout.strip()
  except subprocess.CalledProcessError as error:
    print(f"Command failed with return code {error.returncode}")
    if error.stderr:
      print(f"Error Output: {error.stderr.strip()}")
    if report:
      sentry.capture_exception(error)
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
    run_cmd(["pkill", "-SIGUSR1", "-f", "system.updated.updated"], "Updater check signal sent", "Failed to send updater check signal", report=False)

    while params.get("UpdaterState", encoding="utf-8") != "checking...":
      time.sleep(1)

    while params.get("UpdaterState", encoding="utf-8") == "checking...":
      time.sleep(1)

    if not params.get_bool("UpdaterFetchAvailable"):
      return False

    while params.get("UpdaterState", encoding="utf-8") != "idle":
      time.sleep(60)

    run_cmd(["pkill", "-SIGHUP", "-f", "system.updated.updated"], "Updater refresh signal sent", "Failed to send updater refresh signal", report=False)

    while not params.get_bool("UpdateAvailable"):
      time.sleep(60)

    return True

  if params.get("UpdaterState", encoding="utf-8") != "idle":
    return

  if not update_available():
    return

  while params.get_bool("IsOnroad") or params_memory.get_bool("UpdateSpeedLimits") or running_threads.get("lock_doors", threading.Thread()).is_alive():
    time.sleep(60)

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
