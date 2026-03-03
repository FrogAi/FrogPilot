#!/usr/bin/env python3
import dataclasses
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
from opendbc.can.parser import CANParser
from opendbc.car.toyota.carcontroller import LOCK_CMD
from openpilot.common.params import Params, ParamKeyFlag, ParamKeyType
from openpilot.common.realtime import DT_DMON, DT_HW
from openpilot.system.hardware import HARDWARE
from openpilot.system.version import get_build_metadata
from panda import Panda

from openpilot.frogpilot.common.frogpilot_variables import EARTH_RADIUS, EXCLUDED_KEYS, FROGPILOT_API, FROGS_GO_MOO_PATH, GearShifter, KONIK_PATH, update_frogpilot_toggles

class ThreadManager:
  def __init__(self):
    self.thread_lock = threading.Lock()

    self.running_threads = {}

  def run_with_lock(self, target, args=(), report=True):
    name = target.__name__

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

      thread = threading.Thread(args=args, daemon=True, target=wrapped_target)
      thread.start()
      self.running_threads[name] = thread

  def is_thread_alive(self, name):
    with self.thread_lock:
      thread = self.running_threads.get(name)
      return thread is not None and thread.is_alive()


def calculate_bearing_offset(latitude, longitude, current_bearing, distance):
  bearing = math.radians(current_bearing)
  lat_rad = math.radians(latitude)
  lon_rad = math.radians(longitude)

  delta = distance / EARTH_RADIUS

  new_lat = math.asin(math.sin(lat_rad) * math.cos(delta) + math.cos(lat_rad) * math.sin(delta) * math.cos(bearing))
  new_lon = lon_rad + math.atan2(math.sin(bearing) * math.sin(delta) * math.cos(lat_rad),  math.cos(delta) - math.sin(lat_rad) * math.sin(new_lat))
  return math.degrees(new_lat), math.degrees(new_lon)


def calculate_distance_to_point(lat1, lon1, lat2, lon2):
  lat1_rad = math.radians(lat1)
  lon1_rad = math.radians(lon1)
  lat2_rad = math.radians(lat2)
  lon2_rad = math.radians(lon2)

  delta_lat = lat2_rad - lat1_rad
  delta_lon = lon2_rad - lon1_rad

  a = (math.sin(delta_lat / 2) ** 2) + math.cos(lat1_rad) * math.cos(lat2_rad) * (math.sin(delta_lon / 2) ** 2)
  c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))

  return EARTH_RADIUS * c


def calculate_lane_width(lane_line1, lane_line2, road_edge=None):
  lane_line1_x = np.asarray(lane_line1.x)
  lane_line1_y = np.asarray(lane_line1.y)

  lane_line2_x = np.asarray(lane_line2.x)
  lane_line2_y = np.asarray(lane_line2.y)

  lane_y_interp = np.interp(lane_line2_x, lane_line1_x, lane_line1_y)
  distance_to_lane = np.median(np.abs(lane_line2_y - lane_y_interp))

  if road_edge is None:
    return float(distance_to_lane)

  edge_line_x = np.asarray(road_edge.x)
  edge_line_y = np.asarray(road_edge.y)

  edge_y_interp = np.interp(lane_line2_x, edge_line_x, edge_line_y)
  distance_to_road_edge = np.median(np.abs(lane_line2_y - edge_y_interp))

  if distance_to_road_edge < distance_to_lane:
    return 0.0

  return float(distance_to_lane)


# Credit goes to Pfeiferj!
def calculate_road_curvature(modelData):
  orientation_rate = np.array(modelData.orientationRate.z)
  timebase = np.array(modelData.orientationRate.t)
  velocity = np.array(modelData.velocity.x)

  lateral_acceleration = orientation_rate * velocity
  index = np.argmax(np.abs(lateral_acceleration))
  predicted_lateral_acc = float(lateral_acceleration[index])
  time_to_curve = float(timebase[index])

  return float(predicted_lateral_acc / max(velocity[index], 1)**2), max(time_to_curve, 1)


def check_remote_toggles(started, params, sm=None, boot_run=False):
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
    api_token = params.get("FrogPilotApiToken")
    dongle_id = params.get("FrogPilotDongleId")

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
      try:
        params.check_key(key)
      except Exception:
        print(f"Skipping unknown param key: {key}")
        continue

      if value is None:
        continue

      try:
        casted_value = params.cpp2python(key, value.encode("utf-8") if isinstance(value, str) else value)
        if casted_value is not None:
          params.put(key, casted_value)

      except Exception as exception:
        print(f"Skipping remote toggle {key}: {exception}")
        continue

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
  return name.replace("(Default)", "").strip()


def contains_event_type(events, frogpilot_events, *event_types):
  return any(events.contains(event_type) or frogpilot_events.contains(event_type) for event_type in event_types)


def delete_file(path, print_error=True, report=True):
  path = Path(path)
  if path.is_file() or path.is_symlink():
    run_cmd(["sudo", "rm", "-f", str(path)], f"Deleted file: {path}", f"Failed to delete file: {path}", report=report)
  elif path.is_dir():
    run_cmd(["sudo", "rm", "-rf", str(path)], f"Deleted directory: {path}", f"Failed to delete directory: {path}", report=report)
  elif print_error:
    print(f"File not found: {path}")

def extract_zip(zip_file, extract_path):
  with zipfile.ZipFile(zip_file, "r") as zip:
    print(f"Extracting {zip_file} to {extract_path}")
    zip.extractall(extract_path)

  zip_file.unlink()
  print(f"Extraction completed!")


def flash_panda(params_memory):
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
  params = Params()

  api_token = params.get("FrogPilotApiToken")
  build_metadata = dataclasses.asdict(get_build_metadata())
  device_type = HARDWARE.get_device_type()
  dongle_id = params.get("FrogPilotDongleId")

  return api_token, build_metadata, device_type, dongle_id


def get_lock_status(can_parser, can_sock):
  update_can_parser(can_parser, can_sock)
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


@cache
def is_FrogsGoMoo():
  return FROGS_GO_MOO_PATH.is_file()


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
    try:
      with open(path) as file:
        return json.load(file)
    except json.JSONDecodeError:
      print(f"Failed to load JSON file: {path}")
      return {}
  return {}


def lock_doors(lock_doors_timer, sm, params):
  wait_for_no_driver(params, sm, door_checks=True, time_threshold=lock_doors_timer)

  sm.update()
  if any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown):
    return

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


def update_can_parser(can_parser, can_sock):
  can_msgs = messaging.drain_sock(can_sock, wait_for_one=True)
  can_parser.update([(msg.logMonoTime, [[frame.address, frame.dat, frame.src] for frame in msg.can]) for msg in can_msgs if msg.which() == "can"])


def update_json_file(path, data):
  temp_path = f"{path}.tmp"
  with open(temp_path, "w") as file:
    json.dump(data, file, indent=2, sort_keys=True)
    file.flush()
    os.fsync(file.fileno())

  os.replace(temp_path, path)


def upload_toggles(params):
  if not is_url_pingable(FROGPILOT_API):
    return

  try:
    api_token, build_metadata, device_type, dongle_id = get_frogpilot_api_info()
    if not dongle_id or not api_token:
      return

    toggles = {}
    for key in params.all_keys():
      key_str = key.decode("utf-8") if isinstance(key, bytes) else str(key)
      if key_str in EXCLUDED_KEYS:
        continue
      if params.get_key_flag(key) & ParamKeyFlag.DONT_LOG:
        continue

      value = params.get(key)
      if value is None:
        continue

      key_type = params.get_type(key)
      if key_type == ParamKeyType.BYTES:
        value = value.decode("utf-8", "replace")
      elif key_type == ParamKeyType.TIME:
        value = value.isoformat()
      toggles[key_str] = value

    payload = {
      "api_token": api_token,
      "build_metadata": build_metadata,
      "device": device_type,
      "dongle_id": dongle_id,
      "frogpilot_dongle_id": dongle_id,
      "toggles": toggles,
    }

    response = requests.post(
      f"{FROGPILOT_API}/pond/toggles/sync",
      json=payload,
      headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"},
      timeout=10,
    )
    response.raise_for_status()

    print("Successfully uploaded toggles to FrogPilot.com")

  except Exception as e:
    print(f"Failed to upload toggles: {e}")


@cache
def use_konik_server():
  return KONIK_PATH.is_file() or params.get_bool("UseKonikServer")


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
