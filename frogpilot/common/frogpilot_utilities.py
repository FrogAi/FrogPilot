#!/usr/bin/env python3
import json
import math
import numpy as np
import requests
import shutil
import subprocess
import threading
import time
import urllib.error
import urllib.request
import zipfile

import openpilot.system.sentry as sentry

from functools import cache
from pathlib import Path

from cereal import log
from openpilot.common.realtime import DT_DMON, DT_HW
from openpilot.selfdrive.car.toyota.carcontroller import LOCK_CMD
from openpilot.system.hardware import HARDWARE
from panda import Panda

from openpilot.frogpilot.common.frogpilot_variables import EARTH_RADIUS, KONIK_PATH, MAPD_PATH, MAPS_PATH, params, params_cache, params_memory

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
        except Exception as error:
          print(f"Error in thread '{name}': {error}")
          if report:
            sentry.capture_exception(error)
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

def calculate_distance_to_point(ax, ay, bx, by):
  delta_x = (bx - ax) / 2
  delta_y = (by - ay) / 2

  a = (math.sin(delta_x)**2) + math.cos(ax) * math.cos(bx) * (math.sin(delta_y)**2)
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

  max_pred_lat_acc = max(np.max(orientation_rate * velocity), np.min(orientation_rate * velocity), key=abs)

  return float(max_pred_lat_acc / max(v_ego, 1)**2)

def delete_file(path, report=True):
  path = Path(path)
  if path.is_file() or path.is_symlink():
    run_cmd(["sudo", "rm", "-f", str(path)], success_message=f"Deleted file: {path}", fail_message=f"Failed to delete file: {path}", report=report)
  elif path.is_dir():
    run_cmd(["sudo", "rm", "-rf", str(path)], success_message=f"Deleted directory: {path}", fail_message=f"Failed to delete directory: {path}", report=report)
  else:
    print(f"File not found: {path}")

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
    except Exception as error:
      print(f"Error flashing Panda {serial}: {error}")
      sentry.capture_exception(error)

  params_memory.remove("FlashPanda")

def is_url_pingable(url, timeout=10):
  try:
    headers = {"User-Agent": "Mozilla/5.0"}
    response = requests.get(url, headers=headers, timeout=timeout)
    return response.status_code < 400
  except requests.ConnectionError:
    print(f"ConnectionError while pinging {url}")
  except requests.HTTPError:
    print(f"HTTPError while fetching from {url}")
  except requests.RequestException:
    print(f"RequestException while pinging {url}")
  except requests.Timeout:
    print(f"Timeout while pinging {url}")
  except Exception as error:
    sentry.capture_exception(error)
    print(f"Unexpected error while pinging {url}: {error}")
  return False

def lock_doors(lock_doors_timer, sm):
  wait_for_no_driver(sm, lock_doors_timer)

  if not any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown):
    for _ in range(3):
      with Panda() as panda:
        panda.set_safety_mode(panda.SAFETY_TOYOTA)
        panda.can_send(0x750, LOCK_CMD, 0)
        panda.send_heartbeat()

def run_cmd(cmd, success_message, fail_message, report=True):
  try:
    subprocess.run(cmd, capture_output=True, text=True, check=True)
    print(success_message)
  except subprocess.CalledProcessError as error:
    print(f"Command failed with return code {error.returncode}")
    if error.stderr:
      print(f"Error Output: {error.stderr.strip()}")
    if report:
      sentry.capture_exception(error)
  except Exception as error:
    print(f"Unexpected error occurred: {error}")
    print(fail_message)
    if report:
      sentry.capture_exception(error)

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
  while params_memory.get_bool("UpdateSpeedLimits"):
    time.sleep(60)

  if params.get("UpdaterState", encoding="utf-8") != "idle":
    return

  subprocess.run(["pkill", "-SIGUSR1", "-f", "system.updated.updated"], check=False)

  while params.get("UpdaterState", encoding="utf-8") != "checking...":
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

  while running_threads.get("lock_doors", threading.Thread()).is_alive() or params_memory.get_bool("IsOnroad"):
    time.sleep(60)

  HARDWARE.reboot()

@cache
def use_konik_server():
  return KONIK_PATH.is_file()

def wait_for_no_driver(sm, time_threshold=60):
  while sm["deviceState"].screenBrightnessPercent != 0 or any(proc.name == "dmonitoringd" and proc.running for proc in sm["managerState"].processes):
    if any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown):
      return

    time.sleep(DT_HW)

    sm.update()

  params.put_bool("IsDriverViewEnabled", True)

  while not any(proc.name == "dmonitoringd" and proc.running for proc in sm["managerState"].processes):
    time.sleep(DT_HW)

    sm.update()

  start_time = time.monotonic()
  while True:
    elapsed_time = time.monotonic() - start_time
    if elapsed_time >= time_threshold:
      break

    if any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown):
      params.remove("IsDriverViewEnabled")
      return

    if sm["driverMonitoringState"].faceDetected or not sm.alive["driverMonitoringState"]:
      start_time = time.monotonic()

    time.sleep(DT_DMON)

    sm.update()

  params.remove("IsDriverViewEnabled")
