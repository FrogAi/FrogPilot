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
from opendbc.can.parser import CANParser
from opendbc.car.toyota.carcontroller import LOCK_CMD
from openpilot.common.realtime import DT_DMON, DT_HW
from panda import Panda

from openpilot.frogpilot.common.frogpilot_variables import CRUISING_SPEED, DECEL_TIME_MARGIN, EARTH_RADIUS, FROGS_GO_MOO_PATH, KONIK_PATH, MINIMUM_PLANNED_SPEED

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
  extract_root = Path(extract_path).resolve()
  with zipfile.ZipFile(zip_file, "r") as zip:
    print(f"Extracting {zip_file} to {extract_path}")
    for member in zip.namelist():
      if not (extract_root / member).resolve().is_relative_to(extract_root):
        raise ValueError(f"Refusing to extract path outside destination: {member}")
    zip.extractall(extract_path)

  zip_file.unlink()
  print("Extraction completed!")


def flash_panda(params_memory):
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
  update_can_parser(can_parser, can_sock)
  return can_parser.vl["DOOR_LOCKS"]["LOCK_STATUS"]


@cache
def is_FrogsGoMoo():
  return FROGS_GO_MOO_PATH.is_file()


def is_gps_location_valid(gps_location, gps_service, sm):
  return gps_location.hasFix and time.monotonic() - sm.recv_time[gps_service] <= 2.0


def is_mapd_data_valid(mapd_out, gps_valid, sm):
  return gps_valid and sm.alive["mapdOut"] and mapd_out.tileLoaded and mapd_out.wayId > 0


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


@cache
def use_konik_server():
  return KONIK_PATH.is_file()


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
