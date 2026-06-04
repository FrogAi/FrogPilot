#!/usr/bin/env python3
import dataclasses
import json
import math
import numpy as np
import os
import requests
import shutil
import subprocess
import threading
import time
import zipfile

from pathlib import Path

import openpilot.system.sentry as sentry

from cereal import log, messaging
from opendbc.can import CANParser
from openpilot.common.params import Params
from openpilot.common.realtime import DT_DMON, DT_HW
from openpilot.common.utils import atomic_write, run_cmd as base_run_cmd
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import get_safe_obstacle_distance, get_stopped_equivalence_factor, get_T_FOLLOW
from openpilot.system.hardware import HARDWARE
from openpilot.system.version import get_build_metadata

@dataclasses.dataclass(frozen=True)
class FrogPilotApiInfo:
  api_token: str | None
  build_metadata: dict
  device_type: str
  dongle_id: str | None
  os_version: str | None


class ThreadManager:
  def __init__(self):
    self.thread_lock = threading.Lock()

    self.running_threads = {}

  def is_thread_alive(self, name):
    with self.thread_lock:
      thread = self.running_threads.get(name)
      return thread is not None and thread.is_alive()

  def run_with_lock(self, target, args=(), report=True, *, thread_name=None):
    name = thread_name or target.__name__

    if not isinstance(args, (tuple, list)):
      args = (args,)

    with self.thread_lock:
      dead_threads = [key for key, thread in self.running_threads.items() if not thread.is_alive()]
      for key in dead_threads:
        del self.running_threads[key]

      if name in self.running_threads and self.running_threads[name].is_alive():
        return

      def wrapped_target(*t_args):
        try:
          target(*t_args)
        except Exception as exception:
          print(f"Error in thread '{name}': {exception}")
          if report:
            sentry.capture_exception(exception)

      thread = threading.Thread(args=args, daemon=True, name=name, target=wrapped_target)
      thread.start()
      self.running_threads[name] = thread


def calculate_bearing_offset(latitude, longitude, current_bearing, distance):
  bearing_rad = math.radians(current_bearing)
  latitude_rad = math.radians(latitude)
  longitude_rad = math.radians(longitude)

  angular_distance = distance / frogpilot_variables.EARTH_RADIUS

  new_latitude_rad = math.asin(
    math.sin(latitude_rad) * math.cos(angular_distance) +
    math.cos(latitude_rad) * math.sin(angular_distance) * math.cos(bearing_rad)
  )
  new_longitude_rad = longitude_rad + math.atan2(
    math.sin(bearing_rad) * math.sin(angular_distance) * math.cos(latitude_rad),
    math.cos(angular_distance) - math.sin(latitude_rad) * math.sin(new_latitude_rad)
  )

  new_latitude = math.degrees(new_latitude_rad)
  new_longitude = ((math.degrees(new_longitude_rad) + 540) % 360) - 180
  return new_latitude, new_longitude


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
  return frogpilot_variables.EARTH_RADIUS * angular_distance


def calculate_road_curvature(modelData):
  orientation_rate = np.array(modelData.orientationRate.z)
  timebase = np.array(modelData.orientationRate.t)
  velocity = np.array(modelData.velocity.x)

  lateral_acceleration = orientation_rate * velocity
  index = np.argmax(np.abs(lateral_acceleration))
  predicted_lateral_acc = float(lateral_acceleration[index])
  time_to_curve = float(timebase[index])

  return float(predicted_lateral_acc / max(velocity[index], 1)**2), max(time_to_curve, 1)


def clean_model_name(model_name):
  return model_name.strip().removesuffix(" (Default)").rstrip()


def contains_event_type(events, frogpilot_events, *event_types):
  return any(events.contains(event_type) or frogpilot_events.contains(event_type) for event_type in event_types)


def delete_file(path, print_error=True, report=True):
  path = Path(path)
  if path.is_file() or path.is_symlink():
    path_type = "file"
    command = ["sudo", "rm", "-f", "--", str(path)]
  elif path.is_dir():
    path_type = "directory"
    command = ["sudo", "rm", "-rf", "--", str(path)]
  else:
    if print_error:
      print(f"Path not found: {path}")
    return

  run_cmd(command, f"Deleted {path_type}: {path}", f"Failed to delete {path_type}: {path}", report=report)


def desired_follow_distance(v_ego, v_lead, t_follow=None):
  if t_follow is None:
    t_follow = get_T_FOLLOW()
  return get_safe_obstacle_distance(v_ego, t_follow) - get_stopped_equivalence_factor(v_lead)


def extract_zip(zip_file, extract_path):
  zip_file = Path(zip_file)
  extract_root = Path(extract_path)
  extract_root.mkdir(parents=True, exist_ok=True)
  extract_root_resolved = extract_root.resolve()

  with zipfile.ZipFile(zip_file, "r") as archive:
    print(f"Extracting {zip_file} to {extract_root}")
    members = [
      (member, (extract_root / member.filename.replace("\\", "/")).resolve())
      for member in archive.infolist()
    ]

    for member, destination in members:
      if os.path.commonpath((extract_root_resolved, destination)) != str(extract_root_resolved):
        raise ValueError(f"Refusing to extract path outside destination: {member.filename}")

    for member, destination in members:
      if member.is_dir():
        destination.mkdir(parents=True, exist_ok=True)
        continue

      destination.parent.mkdir(parents=True, exist_ok=True)
      with archive.open(member, "r") as source, destination.open("wb") as target:
        shutil.copyfileobj(source, target)

  zip_file.unlink()
  print("Extraction completed!")


def get_frogpilot_api_info():
  params = Params()
  return FrogPilotApiInfo(
    api_token=params.get("FrogPilotApiToken"),
    build_metadata=dataclasses.asdict(get_build_metadata()),
    device_type=HARDWARE.get_device_type(),
    dongle_id=params.get("FrogPilotDongleId"),
    os_version=HARDWARE.get_os_version(),
  )


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


def run_cmd(cmd, success_message, fail_message, env=None, raise_on_failure=True, report=False):
  failure = None
  try:
    result = base_run_cmd(cmd, env=env)
    print(success_message)
    return result
  except subprocess.CalledProcessError as exception:
    failure = exception
    error_output = exception.stderr or exception.output
    if error_output:
      print(f"Command failed with error: {error_output}")
  except Exception as exception:
    failure = exception
    print(f"Unexpected error occurred: {exception}")

  print(fail_message)
  if report:
    sentry.capture_exception(failure)
  if raise_on_failure:
    raise failure
  return None


def update_can_parser(can_parser, can_sock):
  can_packets = []
  for msg in messaging.drain_sock(can_sock, wait_for_one=True):
    if msg.which() != "can":
      continue

    can_packets.append((msg.logMonoTime, [(frame.address, frame.dat, frame.src) for frame in msg.can]))

  can_parser.update(can_packets)


def update_json_file(path, data):
  path = Path(path)
  path.parent.mkdir(parents=True, exist_ok=True)

  with atomic_write(str(path), "w", overwrite=True) as file:
    file.write(json.dumps(data, indent=2, sort_keys=True))
    file.write("\n")


def wait_for_no_driver(params, sm, door_checks=False, time_threshold=60):
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
      update_can_parser(can_parser, can_sock)

      door_open = any([can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_FL"], can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_FR"],
                       can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_RL"], can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_RR"]])
      if door_open:
        start_time = time.monotonic()

    time.sleep(DT_DMON)

  params.remove("IsDriverViewEnabled")
