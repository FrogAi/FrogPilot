#!/usr/bin/env python3
import json
import math
import numpy as np
import shutil
import subprocess
import threading
import time
import urllib.request
import zipfile

import openpilot.system.sentry as sentry

from pathlib import Path
from urllib.error import HTTPError

from cereal import log
from openpilot.common.realtime import DT_DMON, DT_HW
from openpilot.selfdrive.car.toyota.carcontroller import LOCK_CMD
from openpilot.system.hardware import HARDWARE
from panda import Panda

from openpilot.selfdrive.frogpilot.frogpilot_variables import EARTH_RADIUS, MAPD_PATH, MAPS_PATH, params, params_memory

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
  "update_models": threading.Lock(),
  "update_openpilot": threading.Lock(),
  "update_themes": threading.Lock()
}

def run_thread_with_lock(name, target, args=()):
  if not running_threads.get(name, threading.Thread()).is_alive():
    with locks[name]:
      def wrapped_target(*t_args):
        try:
          target(*t_args)
        except HTTPError as error:
          print(f"HTTP error while accessing {api_url}: {error}")
        except subprocess.CalledProcessError as error:
          print(f"CalledProcessError in thread '{name}': {error}")
        except Exception as error:
          print(f"Error in thread '{name}': {error}")
          sentry.capture_exception(error)
      thread = threading.Thread(target=wrapped_target, args=args, daemon=True)
      thread.start()
      running_threads[name] = thread

def calculate_distance_to_point(ax, ay, bx, by):
  a = math.sin((bx - ax) / 2) * math.sin((bx - ax) / 2) + math.cos(ax) * math.cos(bx) * math.sin((by - ay) / 2) * math.sin((by - ay) / 2)
  c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
  return EARTH_RADIUS * c

def calculate_lane_width(lane, current_lane, road_edge):
  current_x = np.array(current_lane.x)
  current_y = np.array(current_lane.y)

  lane_y_interp = np.interp(current_x, np.array(lane.x), np.array(lane.y))
  road_edge_y_interp = np.interp(current_x, np.array(road_edge.x), np.array(road_edge.y))

  distance_to_lane = np.mean(np.abs(current_y - lane_y_interp))
  distance_to_road_edge = np.mean(np.abs(current_y - road_edge_y_interp))

  return float(min(distance_to_lane, distance_to_road_edge))

# Credit goes to Pfeiferj!
def calculate_road_curvature(modelData, v_ego):
  orientation_rate = np.abs(modelData.orientationRate.z)
  velocity = modelData.velocity.x
  max_pred_lat_acc = np.amax(orientation_rate * velocity)
  return max_pred_lat_acc / max(v_ego, 1)**2

def delete_file(path):
  path = Path(path)
  try:
    if path.is_file() or path.is_symlink():
      path.unlink()
      print(f"Deleted file: {path}")
    elif path.is_dir():
      shutil.rmtree(path)
      print(f"Deleted directory: {path}")
    else:
      print(f"File not found: {path}")
  except Exception as error:
    print(f"An error occurred when deleting {path}: {error}")
    sentry.capture_exception(error)

def extract_zip(zip_file, extract_path):
  zip_file = Path(zip_file)
  extract_path = Path(extract_path)
  print(f"Extracting {zip_file} to {extract_path}")

  try:
    with zipfile.ZipFile(zip_file, "r") as zip_ref:
      zip_ref.extractall(extract_path)
    zip_file.unlink()
    print(f"Extraction completed: {zip_file} has been removed")
  except Exception as error:
    print(f"An error occurred while extracting {zip_file}: {error}")
    sentry.capture_exception(error)

def flash_panda():
  HARDWARE.reset_internal_panda()
  Panda().wait_for_panda(None, 30)
  params_memory.put_bool("FlashPanda", False)

def is_url_pingable(url, timeout=10):
  try:
    request = urllib.request.Request(
      url,
      headers={
        "User-Agent": "Mozilla/5.0 (compatible; Python urllib)",
        "Accept": "*/*",
        "Connection": "keep-alive"
      }
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
      return response.status == 200
  except Exception as error:
    print(f"Unexpected error when pinging {url}: {error}")
  return False

def lock_doors(lock_doors_timer, sm):
  while any(proc.name == "dmonitoringd" and proc.running for proc in sm["managerState"].processes):
    time.sleep(DT_HW)
    sm.update()

  params.put_bool("IsDriverViewEnabled", True)

  while not any(proc.name == "dmonitoringd" and proc.running for proc in sm["managerState"].processes):
    time.sleep(DT_HW)
    sm.update()

  start_time = time.monotonic()
  while True:
    elapsed_time = time.monotonic() - start_time
    if elapsed_time >= lock_doors_timer:
      break

    if any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown):
      params.remove("IsDriverViewEnabled")
      return

    if sm["driverMonitoringState"].faceDetected or not sm.alive["driverMonitoringState"]:
      start_time = time.monotonic()

    time.sleep(DT_DMON)
    sm.update()

  panda = Panda()
  panda.set_safety_mode(panda.SAFETY_ALLOUTPUT)
  panda.can_send(0x750, LOCK_CMD, 0)
  panda.set_safety_mode(panda.SAFETY_TOYOTA)
  panda.send_heartbeat()

  params.remove("IsDriverViewEnabled")

def run_cmd(cmd, success_message, fail_message):
  try:
    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    print(success_message)
  except subprocess.CalledProcessError as error:
    print(f"Command failed with return code {error.returncode}")
    print(f"Error Output: {error.stderr.strip()}")
    sentry.capture_exception(error)
  except Exception as error:
    print(f"Unexpected error occurred: {error}")
    print(fail_message)
    sentry.capture_exception(error)

def update_maps(now):
  while not MAPD_PATH.exists():
    time.sleep(60)

  maps_selected = json.loads(params.get("MapsSelected", encoding="utf-8") or "{}")

  if isinstance(maps_selected, int):
    params.remove("MapsSelected")
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

  suffix = "th" if 4 <= day <= 20 or 24 <= day <= 30 else ["st", "nd", "rd"][day % 10 - 1]
  todays_date = now.strftime(f"%B {day}{suffix}, %Y")

  if maps_downloaded and params.get("LastMapsUpdate", encoding="utf-8") == todays_date:
    return

  if params.get("OSMDownloadProgress", encoding="utf-8") is None:
    params_memory.put("OSMDownloadLocations", json.dumps(maps_selected))

  while params.get("OSMDownloadProgress", encoding="utf-8") is not None:
    time.sleep(60)

  params.put("LastMapsUpdate", todays_date)

def update_openpilot(manually_updated, frogpilot_toggles):
  if not frogpilot_toggles.automatic_updates or manually_updated:
    return

  subprocess.run(["pkill", "-SIGUSR1", "-f", "system.updated.updated"], check=False)
  while not params.get("UpdaterState", encoding="utf-8") == "checking...":
    time.sleep(DT_HW)
  while params.get("UpdaterState", encoding="utf-8") == "checking...":
    time.sleep(DT_HW)

  if not params.get_bool("UpdaterFetchAvailable"):
    return

  while params.get("UpdaterState", encoding="utf-8") != "idle":
    time.sleep(DT_HW)

  subprocess.run(["pkill", "-SIGHUP", "-f", "system.updated.updated"], check=False)
  while not params.get_bool("UpdateAvailable"):
    time.sleep(DT_HW)

  while params.get_bool("IsOnroad") or running_threads.get("lock_doors", threading.Thread()).is_alive():
    time.sleep(60)

  HARDWARE.reboot()
