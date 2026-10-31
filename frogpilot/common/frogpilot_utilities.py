#!/usr/bin/env python3
import datetime
import errno
import json
import math
import numpy as np
import os
import requests
import shutil
import subprocess
import threading
import time
import tomllib
import zipfile

from functools import cache
from pathlib import Path
from zoneinfo import ZoneInfo

import openpilot.system.sentry as sentry
from cereal import car, log
from openpilot.common.params import Params
from openpilot.common.realtime import set_core_affinity
from openpilot.common.swaglog import cloudlog
from openpilot.selfdrive.car.gm.values import CC_ONLY_CAR
from openpilot.selfdrive.car.interfaces import TORQUE_SUBSTITUTE_PATH
from openpilot.system.hardware import PC
from openpilot.system.hardware.hw import Paths
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_variables

NetworkType = log.DeviceState.NetworkType

running_threads = {}

locks = {
  "backup_settings": threading.Lock(),
  "backup_toggles": threading.Lock(),
  "download_models": threading.Lock(),
  "download_themes": threading.Lock(),
  "flash_panda": threading.Lock(),
  "load_conditional_ways": threading.Lock(),
  "lock_doors": threading.Lock(),
  "restore_settings": threading.Lock(),
  "send_door_command": threading.Lock(),
  "send_report": threading.Lock(),
  "send_stats": threading.Lock(),
  "update_account_link": threading.Lock(),
  "update_checks": threading.Lock(),
  "update_maps": threading.Lock(),
  "update_openpilot": threading.Lock(),
  "upload_speed_limits": threading.Lock(),
  "write_speed_limits": threading.Lock()
}

def run_thread_with_lock(name, target, args=(), report=True):
  with locks[name]:
    if not running_threads.get(name, threading.Thread()).is_alive():
      def wrapped_target(*t_args):
        set_core_affinity([0, 1, 2, 3])
        if not PC:
          os.sched_setscheduler(0, os.SCHED_OTHER, os.sched_param(0))

        try:
          target(*t_args)
        except Exception as exception:
          print(f"Error in thread '{name}': {exception}")
          if report:
            sentry.capture_exception(exception, crash_log=False)
      thread = threading.Thread(target=wrapped_target, args=args, daemon=True)
      thread.start()
      running_threads[name] = thread

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

    try:
      threading.Thread(target=self.publish, daemon=True).start()
    except Exception:
      cloudlog.exception("Failed to start the update progress thread")

  def copy_file(self, source, destination):
    with open(source, "rb") as source_file, open(destination, "wb") as destination_file:
      while chunk := source_file.read(shutil.COPY_BUFSIZE):
        destination_file.write(chunk)
        self.copied += len(chunk)

    shutil.copystat(source, destination)

  def copy_tree(self, source, destination):
    try:
      self.copy_total = get_directory_size(source)
    except Exception:
      cloudlog.exception("Failed to measure the update's size for its progress")

    self.copy_started = time.monotonic()

    shutil.copytree(source, destination, symlinks=True, copy_function=self.copy_file)

    self.copy_finished = time.monotonic()

  def get_remaining(self, now):
    if self.copy_finished is not None:
      return max((self.copy_finished - self.copy_started) * frogpilot_variables.UPDATE_CLEANUP_RATIO - (now - self.copy_finished), 0)

    copy_time = self.params.get_float("UpdaterCopySeconds") or frogpilot_variables.UPDATE_COPY_SECONDS

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

    try:
      while self.params.get("UpdaterState", encoding="utf-8") != "idle":
        now = time.monotonic()
        elapsed = now - self.started
        remaining = self.get_remaining(now)

        if remaining is not None:
          target = min(int(100 * elapsed / (elapsed + remaining)), 99)
          percent = max(percent, min(target, percent + 1))

        progress = {"percent": percent}

        if progress != published:
          self.params.put("UpdaterDownloadProgress", json.dumps(progress))
          published = progress

        time.sleep(frogpilot_variables.UPDATE_PROGRESS_INTERVAL)

      self.save_copy_time()

      if self.params.get_bool("UpdateAvailable"):
        while percent < 100:
          percent += 1
          self.params.put("UpdaterDownloadProgress", json.dumps({"percent": percent}))
          time.sleep(frogpilot_variables.UPDATE_PROGRESS_INTERVAL)
    finally:
      self.params.remove("UpdaterDownloadProgress")

  def report_agnos(self, partition, written):
    self.agnos_written[partition["name"]] = written

  def save_copy_time(self):
    if self.copy_finished is None:
      return

    copy_time = self.copy_finished - self.copy_started
    average = self.params.get_float("UpdaterCopySeconds")
    if average > 0:
      copy_time = average + (copy_time - average) * frogpilot_variables.UPDATE_COPY_AVERAGE_WEIGHT
    self.params.put("UpdaterCopySeconds", str(round(copy_time, 1)))

  def start_agnos(self, manifest_path):
    try:
      with open(manifest_path) as manifest:
        self.agnos_size = sum(partition["size"] for partition in json.load(manifest))
      self.agnos_started = time.monotonic()
    except Exception:
      cloudlog.exception("Failed to read the AGNOS size for the update's progress")

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

  sin_delta_lat = math.sin((lat2_rad - lat1_rad) / 2)
  sin_delta_lon = math.sin((lon2_rad - lon1_rad) / 2)

  haversine = sin_delta_lat ** 2 + math.cos(lat1_rad) * math.cos(lat2_rad) * sin_delta_lon ** 2
  haversine = min(1, max(0, haversine))

  angular_distance = 2 * math.atan2(math.sqrt(haversine), math.sqrt(1 - haversine))
  return frogpilot_variables.EARTH_RADIUS * angular_distance

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

def can_disable_openpilot_long(CP, FPCP):
  if CP.experimentalLongitudinalAvailable or CP.safetyConfigs[0].safetyModel == car.CarParams.SafetyModel.hondaNidec:
    return False
  if CP.enableDsu and not FPCP.openpilotLongitudinalControlDisabled:
    return False
  if CP.carName == "gm":
    return CP.carFingerprint in CC_ONLY_CAR
  return True

def clean_model_name(name):
  return (
    name.replace("🗺️", "")
    .replace("📡", "")
    .replace("👀", "")
    .replace("(Default)", "")
    .strip()
  )

def clear_update_progress(params):
  try:
    params.remove("UpdaterDownloadProgress")
  except Exception:
    cloudlog.exception("Failed to clear the update's progress")

def delete_path(path, report=True):
  path = Path(path)
  if path.exists() or path.is_symlink():
    run_cmd(["sudo", "rm", "-rf", str(path)], f"Deleted: {path}", f"Failed to delete: {path}", report=report)

def extract_zip(zip_file, extract_path):
  zip_file = Path(zip_file)
  extract_path = Path(extract_path)
  print(f"Extracting {zip_file} to {extract_path}")

  extract_root = extract_path.resolve()
  with zipfile.ZipFile(zip_file, "r") as zip_ref:
    for member in zip_ref.namelist():
      if not (extract_path / member).resolve().is_relative_to(extract_root):
        raise ValueError(f"Refusing to extract path outside destination: {member}")
    zip_ref.extractall(extract_path)

  zip_file.unlink()
  print(f"Extraction completed: {zip_file} has been removed")

def get_directory_size(path):
  size = 0
  for directory, _, files in os.walk(path):
    for name in files:
      try:
        size += os.lstat(os.path.join(directory, name)).st_size
      except FileNotFoundError:
        continue
  return size

def get_downloaded_models(models, thneed_models):
  return [
    model for model in models
    if all((frogpilot_paths.MODELS_PATH / filename).is_file() for filename, _ in model_files(model, model in thneed_models))
  ]

def get_log_roots():
  return tuple(dict.fromkeys((Paths.log_root(raw=True), Paths.log_root(HD=True, raw=True), Paths.log_root(konik=True, raw=True))))

@cache
def get_model_lat_smooth_seconds(model):
  params = Params()
  models = (params.get("AvailableModels", encoding="utf-8") or "").split(",")
  lat_smooth_seconds = (params.get("ModelLatSmoothSeconds", encoding="utf-8") or "").split(",")
  return float(dict(zip(models, lat_smooth_seconds, strict=False)).get(model) or frogpilot_variables.LAT_SMOOTH_SECONDS)

@cache
def get_model_version(model):
  params = Params()
  models = (params.get("AvailableModels", encoding="utf-8") or "").split(",")
  model_versions = (params.get("ModelVersions", encoding="utf-8") or "").split(",")
  return dict(zip(models, model_versions, strict=False)).get(model) or frogpilot_variables.DEFAULT_MODEL_VERSION

@cache
def get_nnff_model_files():
  return [file.stem for file in frogpilot_paths.NNFF_MODELS_PATH.iterdir() if file.is_file()]

@cache
def get_nnff_substitutes():
  with open(TORQUE_SUBSTITUTE_PATH, "rb") as f:
    return tomllib.load(f)

def get_phoenix_time():
  return datetime.datetime.now().astimezone(ZoneInfo("America/Phoenix")).strftime("%B %d, %Y - %I:%M%p")

def get_target_branches(available_branches, current_branch):
  branches = available_branches.split(",")
  if not is_FrogsGoMoo():
    branches = [branch for branch in branches if not is_hidden_branch(branch)]

  for branch in (current_branch, *frogpilot_variables.TOP_BRANCHES):
    if branch in branches:
      branches.remove(branch)
      branches.insert(0, branch)
  return branches

def has_openpilot_longitudinal(params, car_params):
  if car_params["experimentalLongitudinalAvailable"]:
    has_longitudinal = params.get_bool("ExperimentalLongitudinalEnabled")
  else:
    has_longitudinal = car_params["openpilotLongitudinalControl"]

  honda_nidec = car_params["safetyConfigs"][0]["safetyModel"] == "hondaNidec"
  return has_longitudinal and (honda_nidec or car_params["carName"] == "gm" or not params.get_bool("DisableOpenpilotLongitudinal"))

def has_pending_telemetry(path):
  try:
    return os.getxattr(Path(path, "rlog"), frogpilot_variables.UPLOAD_ATTR_NAME) == frogpilot_variables.UPLOAD_PENDING
  except OSError:
    return False

def has_preserve_attr(path, attribute, value):
  try:
    return os.getxattr(path, attribute) == value
  except OSError as error:
    if error.errno in (errno.ENODATA, errno.ENOENT):
      return False
    raise

def is_classic_model(model):
  return get_model_version(model) in frogpilot_variables.CLASSIC_MODEL_VERSIONS

@cache
def is_FrogsGoMoo():
  return frogpilot_paths.FROGS_GO_MOO_PATH.is_file()

@cache
def is_hidden_branch(branch):
  return branch.lower().startswith(frogpilot_variables.DEVELOPMENT_BRANCH.lower()) or branch in frogpilot_variables.HIDDEN_BRANCHES

def is_ignition_on(sm):
  return any(ps.ignitionLine or ps.ignitionCan for ps in sm["pandaStates"] if ps.pandaType != log.PandaState.PandaType.unknown)

def is_online(device_state):
  return device_state.networkType != NetworkType.none

def is_openpilot_steering(sm):
  return sm["deviceState"].started and (sm["controlsState"].enabled or sm["frogpilotCarState"].alwaysOnLateralEnabled)

def is_route_preserved(path):
  try:
    return os.getxattr(path, frogpilot_variables.ROUTE_PRESERVE_ATTR_NAME) == frogpilot_variables.ROUTE_PRESERVED
  except OSError:
    return False

def is_thread_alive(name):
  with locks[name]:
    return running_threads.get(name, threading.Thread()).is_alive()

def is_tinygrad_model(model):
  return get_model_version(model) not in frogpilot_variables.THNEED_MODEL_VERSIONS

def is_tuning_value_set(params, key):
  if key in frogpilot_variables.ZERO_ALLOWED_TUNING_KEYS:
    return params.get_float(key) != 0 or params.get_float(f"{key}Stock") != 0
  return params.get_float(key) != 0

def is_unset_secret(key, value):
  return key in frogpilot_variables.SECRET_KEYS and value in (b"", b"[]", b"0")

def is_url_pingable(url, session=requests):
  headers = {"Accept": "*/*", "User-Agent": "frogpilot-ping-test/1.0 (https://github.com/FrogAi/FrogPilot)"}
  try:
    return session.head(url, headers=headers, timeout=10, allow_redirects=True).ok
  except Exception:
    return False

def join_thread(name):
  with locks[name]:
    thread = running_threads.get(name)

  if thread is not None:
    thread.join()

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

def model_files(model, thneed):
  if thneed:
    return [(f"{model}.thneed", "model")]
  return [(f"{model}_{filename}", description) for filename, description in frogpilot_variables.TINYGRAD_FILES]

@cache
def nnff_supported(car_fingerprint):
  from openpilot.frogpilot.controls.lib.neural_network_feedforward import get_nn_model_path
  return get_nn_model_path(car_fingerprint, "") is not None

def on_unmetered_network(device_state):
  return not device_state.networkMetered and device_state.networkType in (NetworkType.ethernet, NetworkType.wifi)

def run_cmd(cmd, success_message, fail_message, env=None, report=True):
  try:
    result = subprocess.run(cmd, capture_output=True, check=True, env=env, text=True)
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

def run_git_with_progress(cmd, cwd, update_progress):
  output = []

  with subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding="utf8", errors="replace") as process:
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

def use_classic_longitudinal(model):
  return get_model_version(model) in frogpilot_variables.THNEED_MODEL_VERSIONS or model in frogpilot_variables.PRE_TOMB_RAIDER_MODELS

@cache
def use_current_speed_stop(model):
  return model in frogpilot_variables.CURRENT_SPEED_STOP_MODELS

@cache
def use_desired_curvature_action(model):
  return get_model_version(model) == "v7"

@cache
def use_desired_curvature_head(model):
  return model in (Params().get("DesiredCurvatureHeadModels", encoding="utf-8") or "").split(",")

@cache
def use_konik_server():
  return frogpilot_paths.KONIK_PATH.is_file()

@cache
def use_min_stable_delay(model):
  return model in frogpilot_variables.MIN_STABLE_DELAY_MODELS

@cache
def use_model_lead_trajectory(model):
  return get_model_version(model) == "v9"

@cache
def use_navigation_model(model):
  return get_model_version(model) == "v1"

@cache
def use_speed_error_correction(model):
  return model in frogpilot_variables.PRE_TOMB_RAIDER_MODELS

def valid_map_match(map_match, location_mono_time):
  map_match_age = location_mono_time - map_match.get("location_mono_time", 0)
  return bool(map_match.get("way_id")) and isinstance(map_match.get("is_forward"), bool) and 0 <= map_match_age <= frogpilot_variables.MAP_MATCH_MAX_AGE_NS
