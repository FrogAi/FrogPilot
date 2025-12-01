#!/usr/bin/env python3
import json
import math
import numpy as np
import os
import requests
import subprocess
import threading
import time
import xattr
import zipfile

from functools import cache
from pathlib import Path

from cereal import log, messaging
from opendbc.car.ford.values import FordSafetyFlags
from opendbc.car.gm.values import CC_ONLY_CAR, GMFlags
from opendbc.car.toyota.values import ToyotaSafetyFlags
from openpilot.common.realtime import DT_DMON, DT_HW, set_core_affinity
from openpilot.selfdrive.pandad import can_capnp_to_list
from openpilot.system import sentry
from openpilot.system.hardware import PC
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_variables

class ThreadManager:
  def __init__(self):
    self.thread_lock = threading.Lock()

    self.running_threads = {}

  def run_in_thread(self, target, args=(), report=True):
    name = target.__name__

    with self.thread_lock:
      thread = self.running_threads.get(name)
      if thread is not None and thread.is_alive():
        return

      def wrapped_target():
        set_core_affinity([0, 1, 2, 3])
        if not PC:
          os.sched_setscheduler(0, os.SCHED_OTHER, os.sched_param(0))

        try:
          target(*args)
        except Exception as exception:
          print(f"Error in thread '{name}': {exception}")
          if report:
            sentry.capture_exception(exception)

      thread = threading.Thread(daemon=True, target=wrapped_target)
      thread.start()
      self.running_threads[name] = thread

  def is_thread_alive(self, name):
    with self.thread_lock:
      thread = self.running_threads.get(name)
      return thread is not None and thread.is_alive()

  def join_thread(self, name):
    with self.thread_lock:
      thread = self.running_threads.get(name)

    if thread is not None:
      thread.join()


@cache
def block_user():
  return get_build_metadata().channel == frogpilot_variables.PR_BRANCH and not is_FrogsGoMoo()


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
  required_deceleration = (v_ego - curve_speed) / np.maximum(time_to_point, 1)
  index = np.argmax(required_deceleration) if required_deceleration.max() > 0 else np.argmin(curve_speed)

  return float(road_curvature[index]), float(time_to_point[index]), float(np.abs(road_curvature).max())


def calculate_taco_turn_speeds(orientation_rates, speeds, v_ego):
  curvatures = np.asarray(orientation_rates) / np.clip(speeds, 0.3, 100.0)
  max_lat_accel = np.interp(v_ego, [5, 10, 20], [1.5, 2.0, 3.0])
  return np.sqrt(max_lat_accel / (np.abs(curvatures) + 1e-3)) - 2.0


def can_disable_openpilot_long(CP):
  if CP.brand == "gm":
    return CP.carFingerprint in CC_ONLY_CAR
  return CP.brand in frogpilot_variables.DISABLE_OPENPILOT_LONG_BRANDS


def contains_event_type(events, frogpilot_events, *event_types):
  return any(events.contains(event_type) or frogpilot_events.contains(event_type) for event_type in event_types)


def delete_path(path, report=True):
  path = Path(path)
  if path.exists() or path.is_symlink():
    run_cmd(["sudo", "rm", "-rf", str(path)], f"Failed to delete: {path}", report=report)


def disable_openpilot_long(CP, FPCP, frogpilot_toggles):
  if can_disable_openpilot_long(CP) and not CP.alphaLongitudinalAvailable and frogpilot_toggles.disable_openpilot_long:
    FPCP.openpilotLongitudinalControlDisabled = CP.openpilotLongitudinalControl
    CP.openpilotLongitudinalControl = False

    if CP.brand == "ford":
      CP.safetyConfigs[-1].safetyParam &= ~FordSafetyFlags.LONG_CONTROL.value
    elif CP.brand == "gm":
      CP.flags &= ~GMFlags.CC_LONG.value
    elif CP.brand == "toyota":
      CP.safetyConfigs[0].safetyParam |= ToyotaSafetyFlags.STOCK_LONGITUDINAL.value


def extract_zip(zip_file, extract_path):
  extract_root = Path(extract_path).resolve()
  with zipfile.ZipFile(zip_file, "r") as archive:
    for member in archive.namelist():
      if not (extract_root / member).resolve().is_relative_to(extract_root):
        raise ValueError(f"Refusing to extract path outside destination: {member}")
    archive.extractall(extract_path)

  zip_file.unlink()


def get_target_branches(available_branches, current_branch):
  branches = available_branches.split(",")
  if not is_FrogsGoMoo():
    branches = [branch for branch in branches if not is_hidden_branch(branch)]

  for branch in (current_branch, *frogpilot_variables.TOP_BRANCHES):
    if branch in branches:
      branches.remove(branch)
      branches.insert(0, branch)
  return branches


def has_pending_telemetry(path):
  try:
    return xattr.getxattr(path, frogpilot_variables.UPLOAD_ATTR_NAME) == frogpilot_variables.UPLOAD_PENDING
  except OSError:
    return False


@cache
def is_FrogsGoMoo():
  return frogpilot_paths.FROGS_GO_MOO_PATH.is_file()


def is_FrogsGoMoo_development(build_metadata):
  return build_metadata.channel == frogpilot_variables.DEVELOPMENT_BRANCH and is_FrogsGoMoo()


def is_gps_location_valid(gps_service, sm):
  return sm[gps_service].hasFix and time.monotonic() - sm.recv_time[gps_service] <= 2.0


def is_hidden_branch(branch):
  return branch.lower().startswith(frogpilot_variables.DEVELOPMENT_BRANCH.lower()) or branch in frogpilot_variables.HIDDEN_BRANCHES


def is_ignition_on(sm):
  return any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown)


def is_mapd_data_valid(gps_valid, sm):
  return gps_valid and sm.alive["mapdOut"] and sm["mapdOut"].tileLoaded and sm["mapdOut"].wayId > 0


def is_online(device_state):
  return device_state.networkType != log.DeviceState.NetworkType.none


def is_openpilot_steering(sm):
  return sm["deviceState"].started and (sm["selfdriveState"].enabled or sm["frogpilotCarState"].alwaysOnLateralEnabled)


def is_unmetered_network(device_state):
  return not device_state.networkMetered and device_state.networkType in (log.DeviceState.NetworkType.ethernet, log.DeviceState.NetworkType.wifi)


def is_unset_secret(key, value):
  return key in frogpilot_variables.SECRET_KEYS and value in (b"", b"[]", b"0")


def is_url_pingable(url, session=requests):
  headers = {"Accept": "*/*", "User-Agent": "frogpilot-ping-test/1.0 (https://github.com/FrogAi/FrogPilot)"}
  try:
    return session.head(url, headers=headers, timeout=10, allow_redirects=True).ok
  except Exception:
    return False


def load_json_file(path):
  try:
    with open(path) as file:
      return json.load(file)
  except (OSError, json.JSONDecodeError):
    return {}


def run_cmd(cmd, fail_message, report=True):
  try:
    result = subprocess.run(cmd, capture_output=True, check=True, text=True)
    return result.stdout.strip()
  except subprocess.CalledProcessError as exception:
    print(f"Command failed with error: {exception.stderr}")
    print(fail_message)
    if report:
      sentry.capture_exception(exception, extras={"stderr": exception.stderr})
    return None
  except Exception as exception:
    print(f"Unexpected error occurred: {exception}")
    print(fail_message)
    if report:
      sentry.capture_exception(exception)
    return None


def run_cmd_on_writable_mount(mount_point, cmd, fail_message):
  mount_options = run_cmd(["findmnt", "-n", "-o", "OPTIONS", mount_point], "Failed to retrieve mount options")
  run_cmd(["sudo", "mount", "-o", "remount,rw", mount_point], f"Failed to remount {mount_point}")
  run_cmd(cmd, fail_message)
  run_cmd(["sudo", "mount", "-o", f"remount,{mount_options}", mount_point], f"Failed to restore {mount_point} mount options")


def update_can_parser(can_parser, can_sock):
  can_parser.update(can_capnp_to_list(messaging.drain_sock_raw(can_sock, wait_for_one=True)))


def update_json_file(path, data):
  temp_path = f"{path}.tmp"
  with open(temp_path, "w") as file:
    file.write(json.dumps(data, separators=(",", ":")))
    file.flush()
    os.fsync(file.fileno())

  os.replace(temp_path, path)


def wait_for_no_driver(params, sm, time_threshold):
  while sm["deviceState"].screenBrightnessPercent != 0 or any(proc.name == "dmonitoringd" and proc.running for proc in sm["managerState"].processes):
    sm.update()

    if is_ignition_on(sm):
      return

    time.sleep(DT_HW)

  params.put_bool("IsDriverViewEnabled", True)

  start_time = time.monotonic()
  while True:
    sm.update()

    elapsed_time = time.monotonic() - start_time
    if elapsed_time >= time_threshold:
      break

    if is_ignition_on(sm):
      break

    if sm.updated["deviceState"] and not params.get_bool("IsDriverViewEnabled"):
      params.put_bool("IsDriverViewEnabled", True)

    if sm["driverMonitoringState"].faceDetected or not sm.alive["driverMonitoringState"]:
      start_time = time.monotonic()

    time.sleep(DT_DMON)

  params.remove("IsDriverViewEnabled")
