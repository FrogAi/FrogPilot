#!/usr/bin/env python3
import json
import math
import numpy as np
import os
import requests
import subprocess
import threading
import time
import zipfile

from functools import cache
from pathlib import Path

import openpilot.system.sentry as sentry

from cereal import log, messaging
from openpilot.common.realtime import DT_DMON, DT_HW
from openpilot.selfdrive.pandad import can_capnp_to_list
from openpilot.system.loggerd.xattr_cache import getxattr
from panda import Panda

from openpilot.frogpilot.common import frogpilot_variables

class ThreadManager:
  def __init__(self):
    self.thread_lock = threading.Lock()

    self.running_threads = {}

  def run_with_lock(self, target, args=(), report=True):
    name = target.__name__

    with self.thread_lock:
      thread = self.running_threads.get(name)
      if thread is not None and thread.is_alive():
        return

      def wrapped_target(*t_args):
        try:
          target(*t_args)
        except Exception as exception:
          print(f"Error in thread '{name}': {exception}")
          if report:
            sentry.capture_exception(exception, crash_log=False)

      thread = threading.Thread(args=args, daemon=True, target=wrapped_target)
      thread.start()
      self.running_threads[name] = thread

  def is_thread_alive(self, name):
    with self.thread_lock:
      thread = self.running_threads.get(name)
      return thread is not None and thread.is_alive()


def calculate_curve_speed(road_curvature, lateral_acceleration, roll_compensation):
  geometric_lateral_acceleration = np.maximum(lateral_acceleration + np.sign(road_curvature) * roll_compensation, 0)
  return np.maximum(np.sqrt(geometric_lateral_acceleration / np.maximum(np.abs(road_curvature), 1e-6)), frogpilot_variables.CRUISING_SPEED)


def calculate_distance_to_point(lat1, lon1, lat2, lon2):
  lat1_rad = math.radians(lat1)
  lon1_rad = math.radians(lon1)
  lat2_rad = math.radians(lat2)
  lon2_rad = math.radians(lon2)

  delta_lat = lat2_rad - lat1_rad
  delta_lon = lon2_rad - lon1_rad

  a = (math.sin(delta_lat / 2) ** 2) + math.cos(lat1_rad) * math.cos(lat2_rad) * (math.sin(delta_lon / 2) ** 2)
  a = min(1, max(0, a))
  c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))

  return frogpilot_variables.EARTH_RADIUS * c


def calculate_road_curvature(model_data, v_ego, lateral_acceleration, roll_compensation):
  velocity = np.asarray(model_data.velocity.x)
  road_curvature = np.where(velocity >= frogpilot_variables.MINIMUM_PLANNED_SPEED, np.asarray(model_data.orientationRate.z) / np.maximum(velocity, 1), 0)

  distance_to_point = np.concatenate(([0], np.cumsum(np.hypot(np.diff(model_data.position.x), np.diff(model_data.position.y)))))
  time_to_point = distance_to_point / max(v_ego, frogpilot_variables.CRUISING_SPEED)

  curve_speed = calculate_curve_speed(road_curvature, lateral_acceleration, roll_compensation)
  required_deceleration = (v_ego - curve_speed) / np.maximum(time_to_point - frogpilot_variables.DECEL_TIME_MARGIN, 1)
  index = np.argmax(required_deceleration) if required_deceleration.max() > 0 else np.argmin(curve_speed)

  return float(road_curvature[index]), float(time_to_point[index]), float(np.abs(road_curvature).max())


def contains_event_type(events, frogpilot_events, *event_types):
  return any(events.contains(event_type) or frogpilot_events.contains(event_type) for event_type in event_types)


def delete_file(path, report=True):
  path = Path(path)
  if path.is_file() or path.is_symlink():
    run_cmd(["sudo", "rm", "-f", str(path)], None, f"Failed to delete file: {path}", report=report)
  elif path.is_dir():
    run_cmd(["sudo", "rm", "-rf", str(path)], None, f"Failed to delete directory: {path}", report=report)


def extract_zip(zip_file, extract_path):
  extract_root = Path(extract_path).resolve()
  with zipfile.ZipFile(zip_file, "r") as archive:
    for member in archive.namelist():
      if not (extract_root / member).resolve().is_relative_to(extract_root):
        raise ValueError(f"Refusing to extract path outside destination: {member}")
    archive.extractall(extract_path)

  zip_file.unlink()


def flash_panda():
  serials = Panda.list()
  flashed = len(serials) > 0

  for serial in serials:
    try:
      with Panda(serial=serial) as panda:
        panda.flash(force=True)
    except Exception as exception:
      print(f"Failed to flash Panda {serial}: {exception}")
      sentry.capture_exception(exception, crash_log=False)
      flashed = False

  return flashed


def has_pending_telemetry(path):
  return getxattr(path, frogpilot_variables.UPLOAD_ATTR_NAME) == frogpilot_variables.UPLOAD_PENDING


@cache
def is_FrogsGoMoo():
  return frogpilot_variables.FROGS_GO_MOO_PATH.is_file()


def is_gps_location_valid(gps_location, gps_service, sm):
  return gps_location.hasFix and time.monotonic() - sm.recv_time[gps_service] <= 2.0


def is_mapd_data_valid(mapd_out, gps_valid, sm):
  return gps_valid and sm.alive["mapdOut"] and mapd_out.tileLoaded and mapd_out.wayId > 0


def is_unmetered_network(device_state):
  return not device_state.networkMetered and device_state.networkType in (log.DeviceState.NetworkType.ethernet, log.DeviceState.NetworkType.wifi)


def is_url_pingable(url, session=requests):
  if not url:
    return False

  headers = {"Accept": "*/*", "User-Agent": "frogpilot-ping-test/1.0 (https://github.com/FrogAi/FrogPilot)"}
  try:
    response = session.head(url, headers=headers, timeout=10, allow_redirects=True)
    try:
      if response.status_code in (405, 501):
        response.close()
        response = session.get(url, headers=headers, timeout=10, allow_redirects=True, stream=True)

      return response.ok
    finally:
      response.close()

  except Exception:
    return False


def load_json_file(path):
  path = Path(path)
  if not path.is_file():
    return {}

  try:
    with open(path) as file:
      data = json.load(file)
  except (OSError, json.JSONDecodeError):
    return {}

  return data


def run_cmd(cmd, success_message, fail_message, env=None, report=True):
  try:
    result = subprocess.run(cmd, capture_output=True, check=True, env=env, text=True)
    if success_message:
      print(success_message)
    return result.stdout.strip()
  except subprocess.CalledProcessError as exception:
    print(f"Command failed with error: {exception.stderr}")
    print(fail_message)
    if report:
      sentry.capture_exception(exception, crash_log=False, extras={"stderr": exception.stderr})
    return None
  except Exception as exception:
    print(f"Unexpected error occurred: {exception}")
    print(fail_message)
    if report:
      sentry.capture_exception(exception, crash_log=False)
    return None


def update_can_parser(can_parser, can_sock):
  can_parser.update(can_capnp_to_list(messaging.drain_sock_raw(can_sock, wait_for_one=True)))


def update_json_file(path, data):
  temp_path = f"{path}.tmp"
  with open(temp_path, "w") as file:
    json.dump(data, file, indent=2, sort_keys=True)
    file.flush()
    os.fsync(file.fileno())

  os.replace(temp_path, path)


def wait_for_no_driver(params, sm, time_threshold):
  while sm["deviceState"].screenBrightnessPercent != 0 or any(proc.name == "dmonitoringd" and proc.running for proc in sm["managerState"].processes):
    sm.update()

    if any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown):
      return

    time.sleep(DT_HW)

  params.put_bool("IsDriverViewEnabled", True)

  while not any(proc.name == "dmonitoringd" and proc.running for proc in sm["managerState"].processes):
    sm.update()

    if not params.get_bool("IsDriverViewEnabled"):
      params.put_bool("IsDriverViewEnabled", True)

    time.sleep(DT_HW)

  start_time = time.monotonic()
  while True:
    sm.update()

    elapsed_time = time.monotonic() - start_time
    if elapsed_time >= time_threshold:
      break

    if any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown):
      break

    if not params.get_bool("IsDriverViewEnabled"):
      params.put_bool("IsDriverViewEnabled", True)

    if sm["driverMonitoringState"].faceDetected or not sm.alive["driverMonitoringState"]:
      start_time = time.monotonic()

    time.sleep(DT_DMON)

  params.remove("IsDriverViewEnabled")
