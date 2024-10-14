import datetime
import errno
import filecmp
import glob
import numpy as np
import os
import shutil
import subprocess
import sys
import tarfile
import threading
import time
import urllib.request

from openpilot.common.basedir import BASEDIR
from openpilot.common.numpy_fast import interp, mean
from openpilot.common.params_pyx import Params, ParamKeyType, UnknownKeyName
from openpilot.common.time import system_time_valid
from openpilot.system.hardware import HARDWARE

ACTIVE_THEME_PATH = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "active_theme")
MODELS_PATH = os.path.join("/data", "models")
RANDOM_EVENTS_PATH = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "random_events")
THEME_SAVE_PATH = os.path.join("/data", "themes")

def update_frogpilot_toggles():
  def update_params():
    params_memory = Params("/dev/shm/params")
    params_memory.put_bool("FrogPilotTogglesUpdated", True)
    time.sleep(1)
    params_memory.put_bool("FrogPilotTogglesUpdated", False)
  threading.Thread(target=update_params).start()

def cleanup_backups(directory, limit, minimum_backup_size=0, compressed=False):
  backups = sorted(glob.glob(os.path.join(directory, "*_auto*")), key=os.path.getmtime, reverse=True)

  for backup in backups:
    if backup.endswith("_in_progress"):
      run_cmd(["sudo", "rm", "-rf", backup], f"Deleted in-progress backup: {os.path.basename(backup)}", f"Failed to delete in-progress backup: {os.path.basename(backup)}")

  if compressed:
    for backup in backups:
      if os.path.getsize(backup) < minimum_backup_size:
        run_cmd(["sudo", "rm", "-rf", backup], f"Deleted incomplete backup: {os.path.basename(backup)}", f"Failed to delete incomplete backup: {os.path.basename(backup)}")

  for old_backup in backups[limit:]:
    run_cmd(["sudo", "rm", "-rf", old_backup], f"Deleted oldest backup: {os.path.basename(old_backup)}", f"Failed to delete backup: {os.path.basename(old_backup)}")

def backup_directory(backup, destination, success_message, fail_message, minimum_backup_size=0, params=None, compressed=False):
  compressed_backup = f"{destination}.tar.gz"
  in_progress_compressed_backup = f"{compressed_backup}_in_progress"
  in_progress_destination = f"{destination}_in_progress"

  os.makedirs(in_progress_destination, exist_ok=False)

  try:
    if not compressed:
      if os.path.exists(destination):
        print("Backup already exists. Aborting.")
        return

      run_cmd(["sudo", "rsync", "-avq", os.path.join(backup, "."), in_progress_destination], success_message, fail_message)
      os.rename(in_progress_destination, destination)
      print(f"Backup successfully created at {destination}.")

    else:
      if os.path.exists(compressed_backup):
        print("Backup already exists. Aborting.")
        return

      run_cmd(["sudo", "rsync", "-avq", os.path.join(backup, "."), in_progress_destination], success_message, fail_message)
      with tarfile.open(in_progress_compressed_backup, "w:gz") as tar:
        tar.add(in_progress_destination, arcname=os.path.basename(destination))

      shutil.rmtree(in_progress_destination)
      os.rename(in_progress_compressed_backup, compressed_backup)
      print(f"Backup successfully compressed to {compressed_backup}.")

      compressed_backup_size = os.path.getsize(compressed_backup)
      if minimum_backup_size == 0 or compressed_backup_size < minimum_backup_size:
        params.put_int_nonblocking("MinimumBackupSize", compressed_backup_size)

    backups = sorted(glob.glob(os.path.join(os.path.dirname(destination), "*_auto*")), key=os.path.getmtime, reverse=True)
    if len(backups) > 1:
      latest_backup = backups[1]
      if compressed:
        if filecmp.cmp(compressed_backup, latest_backup, shallow=False):
          run_cmd(["sudo", "rm", "-rf", compressed_backup], f"Deleted identical backup: {os.path.basename(compressed_backup)}", f"Failed to delete identical backup: {os.path.basename(compressed_backup)}")
      else:
        if filecmp.dircmp(destination, latest_backup).left_only == []:
          run_cmd(["sudo", "rm", "-rf", destination], f"Deleted identical backup: {os.path.basename(destination)}", f"Failed to delete identical backup: {os.path.basename(destination)}")

  except Exception as e:
    print(f"An unexpected error occurred while trying to create the {backup} backup: {e}")

    if os.path.exists(in_progress_destination):
      try:
        shutil.rmtree(in_progress_destination)
      except Exception as e:
        print(f"An unexpected error occurred while trying to delete the incomplete {backup} backup: {e}")

    if os.path.exists(in_progress_compressed_backup):
      try:
        os.remove(in_progress_compressed_backup)
      except Exception as e:
        print(f"An unexpected error occurred while trying to delete the incomplete {backup} backup: {e}")

def backup_frogpilot(build_metadata, params):
  maximum_backups = 5
  minimum_backup_size = params.get_int("MinimumBackupSize")

  backup_path = os.path.join("/data", "backups")
  os.makedirs(backup_path, exist_ok=True)
  cleanup_backups(backup_path, maximum_backups - 1, minimum_backup_size, True)

  total, used, free = shutil.disk_usage(backup_path)
  required_free_space = minimum_backup_size * maximum_backups

  if free > required_free_space:
    branch = build_metadata.channel
    commit = build_metadata.openpilot.git_commit_date[12:-16]
    backup_dir = os.path.join(backup_path, f"{branch}_{commit}_auto")
    backup_directory(BASEDIR, backup_dir, f"Successfully backed up FrogPilot to {backup_dir}.", f"Failed to backup FrogPilot to {backup_dir}.", minimum_backup_size, params, True)

def backup_toggles(params, params_storage):
  for key in params.all_keys():
    if params.get_key_type(key) & ParamKeyType.FROGPILOT_STORAGE:
      value = params.get(key)
      if value is not None:
        params_storage.put(key, value)

  maximum_backups = 10

  backup_path = os.path.join("/data", "toggle_backups")
  os.makedirs(backup_path, exist_ok=True)
  cleanup_backups(backup_path, maximum_backups - 1)

  backup_dir = os.path.join(backup_path, datetime.datetime.now().strftime('%Y-%m-%d_%I-%M%p').lower() + "_auto")
  backup_directory(os.path.join("/data", "params", "d"), backup_dir, f"Successfully backed up toggles to {backup_dir}.", f"Failed to backup toggles to {backup_dir}.")

def calculate_lane_width(lane, current_lane, road_edge):
  current_x = np.array(current_lane.x)
  current_y = np.array(current_lane.y)

  lane_y_interp = interp(current_x, np.array(lane.x), np.array(lane.y))
  road_edge_y_interp = interp(current_x, np.array(road_edge.x), np.array(road_edge.y))

  distance_to_lane = np.mean(np.abs(current_y - lane_y_interp))
  distance_to_road_edge = np.mean(np.abs(current_y - road_edge_y_interp))

  return float(min(distance_to_lane, distance_to_road_edge))

# Credit goes to Pfeiferj!
def calculate_road_curvature(modelData, v_ego):
  orientation_rate = np.abs(modelData.orientationRate.z)
  velocity = modelData.velocity.x
  max_pred_lat_acc = np.amax(orientation_rate * velocity)
  return max_pred_lat_acc / v_ego**2

def convert_params(params, params_storage):
  print("Starting to convert params")

  required_type = str

  def remove_param(key):
    try:
      value = params_storage.get(key)
      value = value.decode('utf-8') if isinstance(value, bytes) else value

      if isinstance(value, str) and value.replace('.', '', 1).isdigit():
        value = float(value) if '.' in value else int(value)

      if (required_type == int and not isinstance(value, int)) or (required_type == str and isinstance(value, int)):
        params.remove(key)
        params_storage.remove(key)
      elif key == "CustomIcons" and value == "frog_(animated)":
        params.remove(key)
        params_storage.remove(key)

    except (UnknownKeyName, ValueError):
      pass
    except Exception as e:
      print(f"An error occurred when converting params: {e}")

  for key in ["CustomColors", "CustomDistanceIcons", "CustomIcons", "CustomSignals", "CustomSounds", "WheelIcon"]:
    remove_param(key)

  def decrease_param(key):
    try:
      value = params_storage.get_float(key)

      if value > 10:
        value /= 10
        params.put_float(key, value)
        params_storage.put_float(key, value)

    except (UnknownKeyName, ValueError):
      pass
    except Exception as e:
      print(f"An error occurred when converting params: {e}")

  for key in ["LaneDetectionWidth", "PathWidth"]:
    decrease_param(key)

  print("Param conversion completed")

def copy_if_exists(source, destination, single_file_name=None):
  if not os.path.exists(source):
    print(f"Source directory {source} does not exist. Skipping copy.")
    return

  if single_file_name:
    os.makedirs(destination, exist_ok=True)
    for item in os.listdir(source):
      shutil.copy2(os.path.join(source, item), os.path.join(destination, single_file_name))
      print(f"Successfully copied {item} to {single_file_name}.")
  else:
    shutil.copytree(source, destination, dirs_exist_ok=True)
    print(f"Successfully copied {source} to {destination}.")

def delete_file(file):
  try:
    if os.path.isfile(file):
      os.remove(file)
      print(f"Deleted file: {file}")
    else:
      print(f"File not found: {file}")
  except Exception as e:
    print(f"An error occurred when deleting {file}: {e}")

def frogpilot_boot_functions(build_metadata, params, params_storage):
  old_screenrecordings = os.path.join("/data", "media", "0", "videos")
  new_screenrecordings = os.path.join("/data", "media", "screen_recordings")

  if os.path.exists(old_screenrecordings):
    shutil.copytree(old_screenrecordings, new_screenrecordings, dirs_exist_ok=True)
    shutil.rmtree(old_screenrecordings)

  while not system_time_valid():
    print("Waiting for system time to become valid...")
    time.sleep(1)

  try:
    backup_frogpilot(build_metadata, params)
    backup_toggles(params, params_storage)
  except Exception as e:
    print(f"An error occurred when creating boot backups: {e}")

def is_url_pingable(url, timeout=5):
  try:
    urllib.request.urlopen(url, timeout=timeout)
    return True
  except Exception as e:
    return False

def run_cmd(cmd, success_message, fail_message, retries=5, delay=1):
  attempt = 0
  while attempt < retries:
    try:
      subprocess.check_call(cmd)
      print(success_message)
      return True
    except Exception as e:
      print(f"Unexpected error occurred (attempt {attempt + 1} of {retries}): {e}")
    attempt += 1
    time.sleep(delay)
  return False

def setup_frogpilot(build_metadata):
  remount_persist = ["sudo", "mount", "-o", "remount,rw", "/persist"]
  if not run_cmd(remount_persist, "Successfully remounted /persist as read-write.", "Failed to remount /persist."):
    HARDWARE.reboot()

  os.makedirs("/persist/params", exist_ok=True)
  os.makedirs(MODELS_PATH, exist_ok=True)
  os.makedirs(THEME_SAVE_PATH, exist_ok=True)

  frog_color_source = os.path.join(ACTIVE_THEME_PATH, "colors")
  frog_color_destination = os.path.join(THEME_SAVE_PATH, "theme_packs/frog/colors")
  copy_if_exists(frog_color_source, frog_color_destination)

  frog_distance_icon_source = os.path.join(ACTIVE_THEME_PATH, "distance_icons")
  frog_distance_icon_destination = os.path.join(THEME_SAVE_PATH, "distance_icons/frog-animated")
  copy_if_exists(frog_distance_icon_source, frog_distance_icon_destination)

  frog_icon_source = os.path.join(ACTIVE_THEME_PATH, "icons")
  frog_icon_destination = os.path.join(THEME_SAVE_PATH, "theme_packs/frog-animated/icons")
  copy_if_exists(frog_icon_source, frog_icon_destination)

  frog_signal_source = os.path.join(ACTIVE_THEME_PATH, "signals")
  frog_signal_destination = os.path.join(THEME_SAVE_PATH, "theme_packs/frog/signals")
  copy_if_exists(frog_signal_source, frog_signal_destination)

  frog_sound_source = os.path.join(ACTIVE_THEME_PATH, "sounds")
  frog_sound_destination = os.path.join(THEME_SAVE_PATH, "theme_packs/frog/sounds")
  copy_if_exists(frog_sound_source, frog_sound_destination)

  steering_wheel_source = os.path.join(ACTIVE_THEME_PATH, "steering_wheel")
  steering_wheel_destination = os.path.join(THEME_SAVE_PATH, "steering_wheels")
  copy_if_exists(steering_wheel_source, steering_wheel_destination, single_file_name="frog.png")

  remount_root = ["sudo", "mount", "-o", "remount,rw", "/"]
  if not run_cmd(remount_root, "File system remounted as read-write.", "Failed to remount file system."):
    HARDWARE.reboot()

  boot_logo_location = "/usr/comma/bg.jpg"
  boot_logo_save_location = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "other_images", "original_bg.jpg")
  frogpilot_boot_logo = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "other_images", "frogpilot_boot_logo.png")

  if not filecmp.cmp(frogpilot_boot_logo, boot_logo_location, shallow=False):
    run_cmd(["sudo", "cp", frogpilot_boot_logo, boot_logo_location], "Successfully replaced bg.jpg with frogpilot_boot_logo.png.", "Failed to replace boot logo.")

  if build_metadata.channel == "FrogPilot-Development":
    subprocess.run(["sudo", "python3", "/persist/frogsgomoo.py"], check=True)

def uninstall_frogpilot():
  boot_logo_location = "/usr/comma/bg.jpg"
  boot_logo_restore_location = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "other_images", "original_bg.jpg")

  copy_cmd = ["sudo", "cp", boot_logo_restore_location, boot_logo_location]
  if not run_cmd(copy_cmd, "Successfully restored the original boot logo.", "Failed to restore the original boot logo."):
    HARDWARE.reboot()

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
