import datetime
import filecmp
import glob
import http.client
import numpy as np
import os
import shutil
import socket
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request

from openpilot.common.basedir import BASEDIR
from openpilot.common.numpy_fast import clip, interp, mean
from openpilot.common.params_pyx import Params, ParamKeyType, UnknownKeyName
from openpilot.common.time import system_time_valid
from openpilot.system.hardware import HARDWARE

MODELS_PATH = "/data/models"

def delete_file(file):
  try:
    os.remove(file)
    print(f"Deleted file: {file}")
  except FileNotFoundError:
    print(f"File not found: {file}")
  except Exception as e:
    print(f"An error occurred: {e}")

def is_url_pingable(url, timeout=5):
  try:
    urllib.request.urlopen(url, timeout=timeout)
    return True
  except (http.client.IncompleteRead, http.client.RemoteDisconnected, socket.gaierror, socket.timeout, urllib.error.HTTPError, urllib.error.URLError):
    return False

def update_frogpilot_toggles():
  def update_params():
    params_memory = Params("/dev/shm/params")
    params_memory.put_bool("FrogPilotTogglesUpdated", True)
    time.sleep(1)
    params_memory.put_bool("FrogPilotTogglesUpdated", False)
  threading.Thread(target=update_params).start()

def run_cmd(cmd, success_msg, fail_msg):
  try:
    subprocess.check_call(cmd)
    print(success_msg)
  except subprocess.CalledProcessError as e:
    print(f"{fail_msg}: {e}")
  except Exception as e:
    print(f"Unexpected error occurred: {e}")

def calculate_lane_width(lane, current_lane, road_edge):
  current_x, current_y = np.array(current_lane.x), np.array(current_lane.y)

  lane_y_interp = interp(current_x, np.array(lane.x), np.array(lane.y))
  road_edge_y_interp = interp(current_x, np.array(road_edge.x), np.array(road_edge.y))

  distance_to_lane = np.mean(abs(current_y - lane_y_interp))
  distance_to_road_edge = np.mean(abs(current_y - road_edge_y_interp))

  return float(min(distance_to_lane, distance_to_road_edge))

# Credit goes to Pfeiferj!
def calculate_road_curvature(modelData, v_ego):
  orientation_rate = np.abs(modelData.orientationRate.z)
  velocity = modelData.velocity.x
  max_pred_lat_acc = np.amax(orientation_rate * velocity)
  return abs(float(max(max_pred_lat_acc / v_ego**2, sys.float_info.min)))

def backup_directory(backup, destination, success_msg, fail_msg):
  os.makedirs(destination, exist_ok=True)
  try:
    run_cmd(['sudo', 'cp', '-a', os.path.join(backup, '.'), destination], success_msg, fail_msg)
  except OSError as e:
    if e.errno == 28:
      print("Not enough space to perform the backup.")
    else:
      print(f"Failed to backup due to unexpected error: {e}")

def cleanup_backups(directory, limit):
  backups = sorted(glob.glob(os.path.join(directory, "*_auto")), key=os.path.getmtime, reverse=True)
  for old_backup in backups[limit:]:
    subprocess.run(['sudo', 'rm', '-rf', old_backup], check=True)
    print(f"Deleted oldest backup: {os.path.basename(old_backup)}")

def backup_frogpilot(build_metadata):
  backup_path = "/data/backups"
  cleanup_backups(backup_path, 4)

  branch = build_metadata.channel
  commit = build_metadata.openpilot.git_commit_date[12:-16]

  backup_dir = f"{backup_path}/{branch}_{commit}_auto"
  backup_directory(BASEDIR, backup_dir, f"Successfully backed up FrogPilot to {backup_dir}.", f"Failed to backup FrogPilot to {backup_dir}.")

def backup_toggles(params, params_storage):
  for key in params.all_keys():
    if params.get_key_type(key) & ParamKeyType.FROGPILOT_STORAGE:
      value = params.get(key)
      if value is not None:
        params_storage.put(key, value)

  backup_path = "/data/toggle_backups"
  cleanup_backups(backup_path, 9)

  backup_dir = f"{backup_path}/{datetime.datetime.now().strftime('%Y-%m-%d_%I-%M%p').lower()}_auto"
  backup_directory("/data/params/d", backup_dir, f"Successfully backed up toggles to {backup_dir}.", f"Failed to backup toggles to {backup_dir}.")

def convert_params(params, params_storage):
  def convert_param(key, action_func):
    try:
      if params_storage.check_key(key) and params_storage.get_bool(key):
        action_func()
    except UnknownKeyName:
      pass

  version = 8

  try:
    if params_storage.check_key("ParamConversionVersion") and params_storage.get_int("ParamConversionVersion") == version:
      print("Params already converted, moving on.")
      return
  except UnknownKeyName:
    pass

  print("Converting params...")
  convert_param("ModelSelector", lambda: params.put_nonblocking("ModelManagement", "True"))
  convert_param("DragonPilotTune", lambda: params.put_nonblocking("FrogsGoMooTune", "True"))

  print("Params successfully converted!")
  params_storage.put_int_nonblocking("ParamConversionVersion", version)

def frogpilot_boot_functions(build_metadata, params, params_storage):
  convert_params(params, params_storage)

  while not system_time_valid():
    print("Waiting for system time to become valid...")
    time.sleep(1)

  try:
    backup_frogpilot(build_metadata)
    backup_toggles(params, params_storage)
  except subprocess.CalledProcessError as e:
    print(f"Backup failed: {e}")

def setup_frogpilot(build_metadata):
  remount_persist = ['sudo', 'mount', '-o', 'remount,rw', '/persist']
  run_cmd(remount_persist, "Successfully remounted /persist as read-write.", "Failed to remount /persist.")

  os.makedirs("/persist/params", exist_ok=True)
  os.makedirs(MODELS_PATH, exist_ok=True)

  remount_root = ['sudo', 'mount', '-o', 'remount,rw', '/']
  run_cmd(remount_root, "File system remounted as read-write.", "Failed to remount file system.")

  frogpilot_boot_logo = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/frogpilot_boot_logo.png'
  frogpilot_boot_logo_jpg = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/frogpilot_boot_logo.jpg'

  boot_logo_location = '/usr/comma/bg.jpg'
  boot_logo_save_location = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/original_bg.jpg'

  if not os.path.exists(boot_logo_save_location):
    shutil.copy(boot_logo_location, boot_logo_save_location)
    print("Successfully saved original_bg.jpg.")

  if filecmp.cmp(boot_logo_save_location, frogpilot_boot_logo_jpg, shallow=False):
    os.remove(boot_logo_save_location)

  if not filecmp.cmp(frogpilot_boot_logo, boot_logo_location, shallow=False):
    run_cmd(['sudo', 'cp', frogpilot_boot_logo, boot_logo_location], "Successfully replaced bg.jpg with frogpilot_boot_logo.png.", "Failed to replace boot logo.")

  if build_metadata.channel == "FrogPilot-Development":
    subprocess.run(["sudo", "python3", "/persist/frogsgomoo.py"], check=True)

def uninstall_frogpilot():
  boot_logo_location = '/usr/comma/bg.jpg'
  boot_logo_restore_location = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/original_bg.jpg'

  copy_cmd = ['sudo', 'cp', boot_logo_restore_location, boot_logo_location]
  run_cmd(copy_cmd, "Successfully restored the original boot logo.", "Failed to restore the original boot logo.")

  HARDWARE.uninstall()

class MovingAverageCalculator:
  def __init__(self):
    self.reset_data()

  def add_data(self, value):
    if len(self.data) == 5:
      self.total -= self.data.pop(0)
    self.data.append(value)
    self.total += value

  def get_moving_average(self):
    if len(self.data) == 0:
      return None
    return self.total / len(self.data)

  def reset_data(self):
    self.data = []
    self.total = 0
