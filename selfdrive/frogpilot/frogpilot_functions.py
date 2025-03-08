#!/usr/bin/env python3
from pathlib import Path

import datetime
import filecmp
import glob
import os
import shutil
import subprocess
import tarfile
import threading
import time
import zstandard as zstd

from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params
from openpilot.common.params_pyx import ParamKeyType
from openpilot.common.time import system_time_valid
from openpilot.system.hardware import HARDWARE

from openpilot.selfdrive.frogpilot.assets.model_manager import ModelManager
from openpilot.selfdrive.frogpilot.assets.theme_manager import HOLIDAY_THEME_PATH, ThemeManager
from openpilot.selfdrive.frogpilot.frogpilot_utilities import delete_file, run_cmd
from openpilot.selfdrive.frogpilot.frogpilot_variables import CRASHES_DIR, EXCLUDED_KEYS, MODELS_PATH, THEME_SAVE_PATH, FrogPilotVariables, frogpilot_default_params, get_frogpilot_toggles, params

def backup_directory(backup, destination, success_message, fail_message, minimum_backup_size=0, compressed=False):
  in_progress_destination = destination.parent / (destination.name + "_in_progress")

  if compressed:
    destination_compressed = destination.parent / (destination.name + ".tar.zst")
    if destination_compressed.exists():
      print("Backup already exists. Aborting...")
      return

    run_cmd(["sudo", "rsync", "-avq", f"{backup}/.", in_progress_destination], "", fail_message)

    tar_file = destination.parent / (destination.name + "_in_progress.tar")
    with tarfile.open(tar_file, "w") as tar:
      tar.add(in_progress_destination, arcname=destination.name)

    shutil.rmtree(in_progress_destination, ignore_errors=True)

    compressed_file = destination.parent / (destination.name + "_in_progress.tar.zst")
    with open(compressed_file, "wb") as f:
      cctx = zstd.ZstdCompressor(level=2)
      with open(tar_file, "rb") as tar_f:
        with cctx.stream_writer(f) as compressor:
          while True:
            chunk = tar_f.read(65536)
            if not chunk:
              break
            compressor.write(chunk)

    tar_file.unlink(missing_ok=True)

    final_compressed_file = destination.parent / (destination.name + ".tar.zst")
    compressed_file.rename(final_compressed_file)
    print(f"Backup saved: {final_compressed_file}")

    compressed_backup_size = final_compressed_file.stat().st_size
    if minimum_backup_size == 0 or compressed_backup_size < minimum_backup_size:
      params.put_int("MinimumBackupSize", compressed_backup_size)
  else:
    if destination.exists():
      print("Backup already exists. Aborting...")
      return

    run_cmd(["sudo", "rsync", "-avq", f"{backup}/.", in_progress_destination], success_message, fail_message)
    in_progress_destination.rename(destination)

def cleanup_backups(directory, limit, success_message, fail_message, compressed=False):
  directory.mkdir(parents=True, exist_ok=True)

  backups = sorted(directory.glob("*_auto*"), key=lambda x: x.stat().st_mtime, reverse=True)
  for backup in backups[:]:
    if "_in_progress" in backup.name:
      run_cmd(["sudo", "rm", "-rf", backup], "", fail_message)
      backups.remove(backup)

  for oldest_backup in backups[limit:]:
    if oldest_backup.is_dir():
      run_cmd(["sudo", "rm", "-rf", oldest_backup], success_message, fail_message)
    else:
      run_cmd(["sudo", "rm", oldest_backup], success_message, fail_message)

def backup_frogpilot(build_metadata):
  backup_path = Path("/data/backups")
  maximum_backups = 5
  cleanup_backups(backup_path, maximum_backups, "Successfully cleaned up old FrogPilot backups", "Failed to cleanup old FrogPilot backups", compressed=True)

  _, _, free = shutil.disk_usage(backup_path)
  minimum_backup_size = params.get_int("MinimumBackupSize")
  if free > minimum_backup_size * maximum_backups:
    directory = Path(BASEDIR)
    destination_directory = backup_path / f"{build_metadata.channel}_{build_metadata.openpilot.git_commit_date[12:-16]}_auto"
    backup_directory(directory, destination_directory, f"Successfully backed up FrogPilot to {destination_directory}", f"Failed to backup FrogPilot to {destination_directory}", minimum_backup_size, compressed=True)

def backup_toggles(params_cache):
  params_backup = Params("/data/params_backup")

  changes_found = False
  for key, _, _ in frogpilot_default_params:
    if key in EXCLUDED_KEYS:
      continue
    new_value = params.get(key) or "0"
    current_value = params_backup.get(key) or "0"

    if new_value != current_value:
      params_backup.put(key, new_value)
      params_cache.put(key, new_value)
      changes_found = True

  if not changes_found:
    print("Toggles are identical to the previous backup. Aborting...")
    return

  backup_path = Path("/data/toggle_backups")
  maximum_backups = 10
  cleanup_backups(backup_path, maximum_backups, "Successfully cleaned up old toggle backups", "Failed to cleanup old toggle backups")

  directory = Path("/data/params_backup/d")
  destination_directory = backup_path / f"{datetime.datetime.now().strftime('%Y-%m-%d_%I-%M%p').lower()}_auto"
  backup_directory(directory, destination_directory, f"Successfully backed up toggles to {destination_directory}", f"Failed to backup toggles to {destination_directory}")

def convert_params(params_cache):
  print("Starting to convert params")

  print("Param conversion completed")

def frogpilot_boot_functions(build_metadata, params_cache):
  if params.get_bool("HasAcceptedTerms"):
    params_cache.clear_all()

  FrogPilotVariables().update(holiday_theme="stock", started=False)
  ModelManager().copy_default_model()
  ThemeManager().update_active_theme(time_validated=system_time_valid(), frogpilot_toggles=get_frogpilot_toggles(), boot_run=True)

  def backup_thread():
    while not system_time_valid():
      print("Waiting for system time to become valid...")
      time.sleep(1)

    subprocess.run(["pkill", "-SIGUSR1", "-f", "system.updated.updated"], check=False)

    backup_frogpilot(build_metadata)
    backup_toggles(params_cache)

  threading.Thread(target=backup_thread, daemon=True).start()

def setup_frogpilot(build_metadata):
  run_cmd(["sudo", "mount", "-o", "remount,rw", "/persist"], "Successfully remounted /persist as read-write", "Failed to remount /persist")
  run_cmd(["sudo", "chmod", "0777", "/cache"], "Successfully updated /cache permissions", "Failed to update /cache permissions")

  CRASHES_DIR.mkdir(parents=True, exist_ok=True)
  MODELS_PATH.mkdir(parents=True, exist_ok=True)
  THEME_SAVE_PATH.mkdir(parents=True, exist_ok=True)

  for source_suffix, destination_suffix in [
    ("world_frog_day/colors", "theme_packs/frog/colors"),
    ("world_frog_day/distance_icons", "theme_packs/frog-animated/distance_icons"),
    ("world_frog_day/icons", "theme_packs/frog-animated/icons"),
    ("world_frog_day/signals", "theme_packs/frog/signals"),
    ("world_frog_day/sounds", "theme_packs/frog/sounds"),
  ]:
    source = Path(HOLIDAY_THEME_PATH) / source_suffix
    destination = THEME_SAVE_PATH / destination_suffix
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, destination, dirs_exist_ok=True)

  for source_suffix, destination_suffix in [
    ("world_frog_day/steering_wheel/wheel.png", "steering_wheels/frog.png"),
  ]:
    source = Path(HOLIDAY_THEME_PATH) / source_suffix
    destination = THEME_SAVE_PATH / destination_suffix
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)

  base = Path("/cache")
  d_path = base / "d"
  if d_path.exists():
    for item in base.iterdir():
      if item.name not in {"params", "tracking"}:
        delete_file(item)

  boot_logo_location = Path("/usr/comma/bg.jpg")
  frogpilot_boot_logo = Path(__file__).parent / "assets/other_images/frogpilot_boot_logo.png"
  if not filecmp.cmp(frogpilot_boot_logo, boot_logo_location, shallow=False):
    stock_mount_options = subprocess.run(["findmnt", "-no", "OPTIONS", "/"], capture_output=True, text=True).stdout.strip()

    run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Successfully remounted / as read-write", "Failed to remount / as read-write")
    run_cmd(["sudo", "cp", frogpilot_boot_logo, boot_logo_location], "Successfully replaced boot logo", "Failed to replace boot logo")
    run_cmd(["sudo", "mount", "-o", f"remount,{stock_mount_options}", "/"], "Successfully restored stock mount options", "Failed to restore stock mount options")

  persist_comma_path = Path("/persist/comma")
  backup_comma_path = Path("/data/backup_comma")
  if persist_comma_path.exists():
    shutil.copytree(persist_comma_path, backup_comma_path, dirs_exist_ok=True)
    print("Successfully backed up /persist/comma to /data/backup_comma")

  persist_params_path = Path("/persist/params")
  if persist_params_path.exists() and persist_params_path.is_dir():
    shutil.rmtree(persist_params_path)
    print("Successfully deleted /persist/params")

  persist_tracking_path = Path("/persist/tracking")
  if persist_tracking_path.exists() and persist_tracking_path.is_dir():
    tracking_cache = Params("/cache/tracking")
    tracking_persist = Params("/persist/tracking")

    tracking_cache.put_float("FrogPilotDrives", tracking_persist.get_float("FrogPilotDrives"))
    tracking_cache.put_float("FrogPilotKilometers", tracking_persist.get_float("FrogPilotKilometers"))
    tracking_cache.put_float("FrogPilotMinutes", tracking_persist.get_float("FrogPilotMinutes"))

    shutil.rmtree(persist_tracking_path)
    print("Successfully deleted /persist/tracking")

  if not persist_comma_path.exists():
    shutil.copytree(backup_comma_path, persist_comma_path, dirs_exist_ok=True)
    print("Restored /persist/comma from backup")

  if build_metadata.channel == "FrogPilot-Development" and Path("/persist/frogsgomoo.py").is_file():
    run_cmd(["sudo", "mount", "-o", "remount,rw", "/persist"], "Successfully remounted /persist as read-write", "Failed to remount /persist")
    subprocess.run(["sudo", "python3", "/persist/frogsgomoo.py"], check=True)

def uninstall_frogpilot():
  boot_logo_location = Path("/usr/comma/bg.jpg")
  stock_boot_logo = Path(__file__).parent / "assets/other_images/stock_bg.jpg"

  run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Successfully remounted / as read-write", "Failed to remount / as read-write")
  run_cmd(["sudo", "cp", stock_boot_logo, boot_logo_location], "Successfully restored boot logo", "Failed to restore boot logo")

  HARDWARE.uninstall()
