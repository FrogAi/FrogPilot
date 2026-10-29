#!/usr/bin/env python3
import flatbuffers
import json
import math
import numpy as np
import os
import pickle
import requests
import shutil
import subprocess
import threading
import time
import tomllib
import xattr
import zipfile

from functools import cache
from pathlib import Path

from cereal import log, messaging
from opendbc.can.parser import CANParser
from opendbc.car.ford.values import FordSafetyFlags
from opendbc.car.gm.values import CC_ONLY_CAR, GMFlags
from opendbc.car.interfaces import TORQUE_SUBSTITUTE_PATH
from opendbc.car.toyota.carcontroller import DOOR_CONTROL_ADDR, LOCK_CMD, UNLOCK_CMD
from opendbc.car.toyota.values import ToyotaSafetyFlags
from openpilot.common.realtime import DT_DMON, DT_HW, set_core_affinity
from openpilot.selfdrive.pandad import can_capnp_to_list
from openpilot.system import sentry
from openpilot.system.hardware import PC
from openpilot.system.version import get_build_metadata
from panda import Panda

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
            sentry.capture_exception(exception, crash_log=False)

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


class UpdateProgress:
  def __init__(self, params):
    self.params = params

    self.agnos_size = 0
    self.copied = 0
    self.copy_total = 0
    self.download_percent = 0

    self.agnos_written = {}

    self.agnos_started = None
    self.copy_finished = None
    self.copy_started = None
    self.download_finished = None

    self.started = time.monotonic()

    threading.Thread(target=self.publish, daemon=True).start()

  def copy_file(self, source, destination):
    with open(source, "rb") as source_file, open(destination, "wb") as destination_file:
      while chunk := source_file.read(shutil.COPY_BUFSIZE):
        destination_file.write(chunk)
        self.copied += len(chunk)

    shutil.copystat(source, destination)

  def copy_tree(self, source, destination):
    self.copy_total = get_directory_size(source)
    self.copy_started = time.monotonic()

    shutil.copytree(source, destination, symlinks=True, copy_function=self.copy_file)

    self.copy_finished = time.monotonic()

    copy_time = self.copy_finished - self.copy_started
    average = self.params.get("UpdaterCopySeconds")
    if average is not None:
      copy_time = average + (copy_time - average) * frogpilot_variables.UPDATE_COPY_AVERAGE_WEIGHT
    self.params.put("UpdaterCopySeconds", round(copy_time, 1))

  def get_remaining(self, now):
    if self.copy_finished is not None:
      return max((self.copy_finished - self.copy_started) * frogpilot_variables.UPDATE_CLEANUP_RATIO - (now - self.copy_finished), 0)

    copy_time = self.params.get("UpdaterCopySeconds") or frogpilot_variables.UPDATE_COPY_SECONDS

    if self.copy_started is not None:
      return copy_time * (1 - self.copied / self.copy_total + frogpilot_variables.UPDATE_CLEANUP_RATIO)

    install_time = copy_time * (1 + frogpilot_variables.UPDATE_CLEANUP_RATIO)

    if self.agnos_started is not None:
      agnos_written = sum(self.agnos_written.values())
      if agnos_written == 0:
        return None
      return (now - self.agnos_started) * (self.agnos_size - agnos_written) / agnos_written + install_time

    if self.download_finished is not None:
      return install_time

    if self.download_percent == 0:
      return None

    elapsed = now - self.started
    return elapsed * (100 - self.download_percent) / self.download_percent + install_time

  def publish(self):
    percent = 0

    published = None

    while self.params.get("UpdaterState") != "idle":
      now = time.monotonic()
      elapsed = now - self.started
      remaining = self.get_remaining(now)

      if remaining is not None:
        target = min(int(100 * elapsed / (elapsed + remaining)), 99)
        percent = max(percent, min(target, percent + 1))

      progress = {"percent": percent}

      if progress != published:
        self.params.put("UpdaterDownloadProgress", progress)
        published = progress

      time.sleep(frogpilot_variables.UPDATE_PROGRESS_INTERVAL)

    if self.params.get_bool("UpdateAvailable"):
      while percent < 100:
        percent += 1
        self.params.put("UpdaterDownloadProgress", {"percent": percent})
        time.sleep(frogpilot_variables.UPDATE_PROGRESS_INTERVAL)

    self.params.remove("UpdaterDownloadProgress")

  def report_agnos(self, partition, written):
    self.agnos_written[partition["name"]] = written

  def start_agnos(self, manifest_path):
    with open(manifest_path) as manifest:
      self.agnos_size = sum(partition["size"] for partition in json.load(manifest))
    self.agnos_started = time.monotonic()


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


def calculate_lane_widths(model_data):
  lane_lines = np.array([line.y for line in model_data.laneLines])
  road_edges = np.array([edge.y for edge in model_data.roadEdges])
  ego_lines = lane_lines[[1, 2]]

  lane_widths = np.median(np.abs(lane_lines[[0, 3]] - ego_lines), axis=1)
  road_edge_distances = np.median(np.abs(road_edges - ego_lines), axis=1)
  return lane_widths, road_edge_distances


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
    return CP.carFingerprint in CC_ONLY_CAR and not CP.enableGasInterceptorDEPRECATED
  return CP.brand in frogpilot_variables.DISABLE_OPENPILOT_LONG_BRANDS


def contains_event_type(events, frogpilot_events, *event_types):
  return any(events.contains(event_type) or frogpilot_events.contains(event_type) for event_type in event_types)


def delete_path(path, report=True):
  path = Path(path)
  if path.exists() or path.is_symlink():
    run_cmd(["sudo", "rm", "-rf", str(path)], f"Failed to delete: {path}", report=report)


def disable_openpilot_long(CP, FPCP, params, frogpilot_toggles):
  if CP.brand != "mock" and not can_disable_openpilot_long(CP):
    params.put_bool_nonblocking("DisableOpenpilotLongitudinal", False)

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


def get_directory_size(path):
  size = 0
  for directory, _, files in os.walk(path):
    for name in files:
      try:
        size += os.lstat(os.path.join(directory, name)).st_size
      except FileNotFoundError:
        continue
  return size


@cache
def get_model_lat_smooth_seconds(model_id):
  models = {model["id"]: model for model in frogpilot_variables.get_models()}
  return models.get(model_id, frogpilot_variables.DEFAULT_MODEL)["lat_smooth_seconds"]


@cache
def get_nnff_model_files():
  return [file.stem for file in frogpilot_paths.NNFF_MODELS_PATH.iterdir() if file.is_file()]


@cache
def get_nnff_substitutes():
  with open(TORQUE_SUBSTITUTE_PATH, "rb") as f:
    return tomllib.load(f)


@cache
def get_speed_limit_reader_inputs():
  model_file = frogpilot_paths.SPEED_LIMIT_READER_MODEL_PATH / frogpilot_variables.SPEED_LIMIT_READER_MODEL_FILES[0]
  if not model_file.is_file():
    model_file = frogpilot_paths.SPEED_LIMIT_READER_BUNDLED_PATH / frogpilot_variables.SPEED_LIMIT_READER_MODEL_FILES[0]

  model = model_file.read_bytes()
  net = flatbuffers.table.Table(model, flatbuffers.encode.Get(flatbuffers.packer.uoffset, model, 0))
  return json.loads(net.String(net.Pos + net.Offset(4)))


def get_target_branches(available_branches, current_branch):
  branches = available_branches.split(",")
  if not is_FrogsGoMoo():
    branches = [branch for branch in branches if not is_hidden_branch(branch)]

  for branch in (current_branch, *frogpilot_variables.TOP_BRANCHES):
    if branch in branches:
      branches.remove(branch)
      branches.insert(0, branch)
  return branches


def has_openpilot_longitudinal(params, car_params, frogpilot_car_params):
  if car_params["alphaLongitudinalAvailable"]:
    return params.get_bool("AlphaLongitudinalEnabled")

  has_longitudinal = car_params["openpilotLongitudinalControl"] or frogpilot_car_params.get("openpilotLongitudinalControlDisabled", False)
  return has_longitudinal and not params.get_bool("DisableOpenpilotLongitudinal")


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


@cache
def is_pre_tomb_raider_model(model):
  return model in frogpilot_variables.PRE_TOMB_RAIDER_MODELS


def is_route_preserved(path):
  try:
    return xattr.getxattr(path, frogpilot_variables.ROUTE_PRESERVE_ATTR_NAME) == frogpilot_variables.ROUTE_PRESERVED
  except OSError:
    return False


def is_unmetered_network(device_state):
  return not device_state.networkMetered and device_state.networkType in (log.DeviceState.NetworkType.ethernet, log.DeviceState.NetworkType.wifi)


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


def lock_doors(lock_doors_timer, params):
  sm = messaging.SubMaster(["deviceState", "driverMonitoringState", "managerState", "pandaStates"])
  while not (sm.seen["deviceState"] and sm.seen["managerState"]):
    sm.update()

  wait_for_no_driver(params, sm, lock_doors_timer)

  sm.update()
  if is_ignition_on(sm):
    return

  can_parser = CANParser("toyota_nodsu_pt_generated", [("DOOR_LOCKS", 3)], bus=0)
  can_sock = messaging.sub_sock("can", timeout=100)

  pm = messaging.PubMaster(["sendcan"])
  time.sleep(0.2)

  while True:
    sm.update()

    if is_ignition_on(sm):
      break

    sendcan_send = messaging.new_message("sendcan", 1)
    sendcan_send.sendcan[0].address = DOOR_CONTROL_ADDR
    sendcan_send.sendcan[0].dat = LOCK_CMD
    sendcan_send.sendcan[0].src = 0
    pm.send("sendcan", sendcan_send)

    time.sleep(1)

    update_can_parser(can_parser, can_sock)
    if can_parser.ts_nanos["DOOR_LOCKS"]["LOCK_STATUS"] > 0 and can_parser.vl["DOOR_LOCKS"]["LOCK_STATUS"] == 0:
      break


@cache
def nnff_supported(car_fingerprint):
  from openpilot.frogpilot.controls.lib.neural_network_feedforward import get_nn_model_path
  return get_nn_model_path(car_fingerprint, "") is not None


@cache
def predicts_lead_trajectories(model_path):
  for metadata_name in ("supercombo_metadata.pkl", "driving_vision_metadata.pkl", "driving_policy_metadata.pkl"):
    metadata_path = Path(model_path) / metadata_name
    if not metadata_path.is_file():
      continue

    with open(metadata_path, "rb") as metadata_file:
      lead = pickle.load(metadata_file)["output_slices"].get("lead")

    if lead is not None:
      return lead.stop - lead.start == frogpilot_variables.LEAD_TRAJECTORY_OUTPUT_SIZE

  return False


def run_cmd(cmd, fail_message, report=True):
  try:
    result = subprocess.run(cmd, capture_output=True, check=True, text=True)
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


def run_cmd_on_writable_mount(mount_point, cmd, fail_message):
  mount_options = run_cmd(["findmnt", "-n", "-o", "OPTIONS", mount_point], "Failed to retrieve mount options")
  run_cmd(["sudo", "mount", "-o", "remount,rw", mount_point], f"Failed to remount {mount_point}")
  run_cmd(cmd, fail_message)
  run_cmd(["sudo", "mount", "-o", f"remount,{mount_options}", mount_point], f"Failed to restore {mount_point} mount options")


def run_git_with_progress(cmd, cwd, update_progress):
  output = []

  with subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding="utf8") as process:
    for line in process.stdout:
      progress = frogpilot_variables.GIT_PROGRESS_PATTERN.match(line)
      if progress is None:
        if frogpilot_variables.GIT_PROGRESS_SUMMARY_PATTERN.match(line) is None:
          output.append(line)
      elif progress["phase"] == "Receiving objects":
        update_progress.download_percent = int(progress["percent"])
  update_progress.download_finished = time.monotonic()

  if process.returncode != 0:
    raise subprocess.CalledProcessError(process.returncode, cmd, output="".join(output))
  return "".join(output)


def send_door_command(lock):
  sm = messaging.SubMaster(["pandaStates"])

  can_parser = CANParser("toyota_nodsu_pt_generated", [("DOOR_LOCKS", 3)], bus=0)
  can_sock = messaging.sub_sock("can", timeout=100)

  pm = messaging.PubMaster(["sendcan"])
  time.sleep(0.2)

  for _ in range(frogpilot_variables.DOOR_COMMAND_ATTEMPTS):
    sm.update()
    if is_ignition_on(sm):
      return False

    sendcan_send = messaging.new_message("sendcan", 1)
    sendcan_send.sendcan[0].address = DOOR_CONTROL_ADDR
    sendcan_send.sendcan[0].dat = LOCK_CMD if lock else UNLOCK_CMD
    sendcan_send.sendcan[0].src = 0
    pm.send("sendcan", sendcan_send)

    time.sleep(1)

    update_can_parser(can_parser, can_sock)
    if can_parser.ts_nanos["DOOR_LOCKS"]["LOCK_STATUS"] > 0 and (can_parser.vl["DOOR_LOCKS"]["LOCK_STATUS"] == 0) == lock:
      return True
  return False


def update_can_parser(can_parser, can_sock):
  can_parser.update(can_capnp_to_list(messaging.drain_sock_raw(can_sock, wait_for_one=True)))


def update_json_file(path, data):
  temp_path = f"{path}.tmp"
  with open(temp_path, "w") as file:
    file.write(json.dumps(data, separators=(",", ":")))
    file.flush()
    os.fsync(file.fileno())

  os.replace(temp_path, path)


@cache
def use_action_delay(model):
  return model in frogpilot_variables.ACTION_DELAY_MODELS


@cache
def use_current_speed_stop(model):
  return model in frogpilot_variables.CURRENT_SPEED_STOP_MODELS


@cache
def use_konik_server():
  return frogpilot_paths.KONIK_PATH.is_file()


@cache
def use_min_stable_delay(model):
  return model in frogpilot_variables.MIN_STABLE_DELAY_MODELS


def wait_for_no_driver(params, sm, time_threshold):
  can_parser = CANParser("toyota_nodsu_pt_generated", [("BODY_CONTROL_STATE", 3)], bus=0)
  can_sock = messaging.sub_sock("can", timeout=100)

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

    update_can_parser(can_parser, can_sock)

    door_open = any([can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_FL"], can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_FR"],
                     can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_RL"], can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_RR"]])
    if door_open:
      start_time = time.monotonic()

    time.sleep(DT_DMON)

  params.remove("IsDriverViewEnabled")
