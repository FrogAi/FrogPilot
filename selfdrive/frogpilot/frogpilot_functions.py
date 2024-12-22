from pathlib import Path

import datetime
import filecmp
import glob
import json
import shutil
import subprocess
import tarfile
import threading
import time

from openpilot.common.basedir import BASEDIR
from openpilot.common.params_pyx import ParamKeyType
from openpilot.common.time import system_time_valid
from openpilot.system.hardware import HARDWARE

from openpilot.selfdrive.frogpilot.assets.theme_manager import HOLIDAY_THEME_PATH, ThemeManager
from openpilot.selfdrive.frogpilot.frogpilot_utilities import run_cmd
from openpilot.selfdrive.frogpilot.frogpilot_variables import MODELS_PATH, THEME_SAVE_PATH, FrogPilotVariables, get_frogpilot_toggles, params

def backup_directory(backup, destination, success_message, fail_message, minimum_backup_size=0, compressed=False):
  if compressed:
    destination_compressed = destination.with_suffix(".tar.gz")
    if destination_compressed.exists():
      print("Backup already exists. Aborting")
      return

    in_progress_destination = destination.parent / (destination.name + "_in_progress")
    run_cmd(["sudo", "rsync", "-avq", f"{backup}/.", in_progress_destination], "", fail_message)

    in_progress_compressed = destination_compressed.with_suffix(".tar.gz_in_progress")
    with tarfile.open(in_progress_compressed, "w:gz") as tar:
      tar.add(in_progress_destination, arcname=destination.name)

    run_cmd(["sudo", "rm", "-rf", in_progress_destination], success_message, fail_message)
    in_progress_compressed.rename(destination_compressed)

    compressed_backup_size = destination_compressed.stat().st_size
    if minimum_backup_size == 0 or compressed_backup_size < minimum_backup_size:
      params.put_int("MinimumBackupSize", compressed_backup_size)
  else:
    if destination.exists():
      print("Backup already exists. Aborting")
      return

    run_cmd(["sudo", "rsync", "-avq", f"{backup}/.", destination], success_message, fail_message)

def cleanup_backups(directory, limit, success_message, fail_message, compressed=False):
  directory.mkdir(parents=True, exist_ok=True)

  backups = sorted(directory.glob("*_auto*"), key=lambda x: x.stat().st_mtime, reverse=True)
  for backup in backups[:]:
    if backup.name.endswith("_in_progress") or backup.name.endswith("_in_progress.tar.gz"):
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
  cleanup_backups(backup_path, maximum_backups, "Successfully cleaned up old FrogPilot backups", "Failed to cleanup old FrogPilot backups", True)

  _, _, free = shutil.disk_usage(backup_path)
  minimum_backup_size = params.get_int("MinimumBackupSize")
  if free > minimum_backup_size * maximum_backups:
    directory = Path(BASEDIR)
    destination_directory = backup_path / f"{build_metadata.channel}_{build_metadata.openpilot.git_commit_date[12:-16]}_auto"
    backup_directory(directory, destination_directory, f"Successfully backed up FrogPilot to {destination_directory}", f"Failed to backup FrogPilot to {destination_directory}", minimum_backup_size, compressed=True)

def backup_toggles(params_storage):
  for key in params.all_keys():
    if params.get_key_type(key) & ParamKeyType.PERSISTENT:
      value = params.get(key)
      if value is not None:
        params_storage.put(key, value)

  backup_path = Path("/data/toggle_backups")
  maximum_backups = 10
  cleanup_backups(backup_path, maximum_backups, "Successfully cleaned up old toggle backups", "Failed to cleanup old toggle backups")

  directory = Path("/data/params/d")
  destination_directory = backup_path / f"{datetime.datetime.now().strftime('%Y-%m-%d_%I-%M%p').lower()}_auto"
  backup_directory(directory, destination_directory, f"Successfully backed up toggles to {destination_directory}", f"Failed to backup toggles to {destination_directory}")

def convert_params(params_storage):
  print("Starting to convert params")

  def update_values(keys, mappings):
    for key in keys:
      for original, replacement in mappings.items():
        if params.get(key, encoding='utf-8') == original:
          params.put(key, replacement)
        if params_storage.get(key, encoding='utf-8') == original:
          params_storage.put(key, replacement)

  priority_keys = ["SLCPriority1", "SLCPriority2", "SLCPriority3"]
  update_values(priority_keys, {"Offline Maps": "Map Data"})

  bottom_key = ["StartupMessageBottom"]
  update_values(bottom_key, {"so I do what I want 🐸": "Human-tested, frog-approved 🐸"})

  top_key = ["StartupMessageTop"]
  update_values(top_key, {"Hippity hoppity this is my property": "Hop in and buckle up!"})

  models = [
    ("Certified Herbalist", "CertifiedHerbalistDrives", "CertifiedHerbalistScore"),
    ("Dissolved Oxygen", "DissolvedOxygenDrives", "DissolvedOxygenScore"),
    ("Duck Amigo", "DuckAmigoDrives", "DuckAmigoScore"),
    ("Los Angeles", "LosAngelesDrives", "LosAngelesScore"),
    ("North Dakota", "NorthDakotaDrives", "NorthDakotaScore"),
    ("Notre Dame", "NotreDameDrives", "NotreDameScore"),
    ("Radical Turtle", "RadicalTurtleDrives", "RadicalTurtleScore"),
    ("Recertified Herbalist", "RecertifiedHerbalistDrives", "RecertifiedHerbalistScore"),
    ("SecretGoodOpenpilot", "SecretGoodOpenpilotDrives", "SecretGoodOpenpilotScore"),
    ("WD-40", "WD40Drives", "WD40Score")
  ]

  try:
    model_drives_and_scores = json.loads(params.get("ModelDrivesAndScores") or "{}")
  except Exception as error:
    print(f"Error parsing ModelDrivesAndScores JSON: {error}. Initializing empty structure")
    model_drives_and_scores = {}

  for model, drives_param, score_param in models:
    drives = params.get_int(drives_param)
    score = params.get_int(score_param)

    if drives > 0 or score > 0:
      model_drives_and_scores[model] = {
        "Drives": drives,
        "Score": score
      }

    params.remove(drives_param)
    params_storage.remove(drives_param)
    params.remove(score_param)
    params_storage.remove(score_param)

  params.put("ModelDrivesAndScores", json.dumps(model_drives_and_scores))

  print("Param conversion completed")

def frogpilot_boot_functions(build_metadata, params_storage):
  if params.get_bool("HasAcceptedTerms"):
    params_storage.clear_all()

  source = Path(THEME_SAVE_PATH) / "distance_icons"
  destination = Path(THEME_SAVE_PATH) / "theme_packs"
  if source.exists():
    for item in source.iterdir():
      if item.is_dir():
        destination_path = destination / item.name / "distance_icons"
        destination_path.mkdir(parents=True, exist_ok=True)

        for sub_item in item.iterdir():
          destination_file = destination_path / sub_item.name
          if destination_file.exists():
            destination_file.unlink()

          shutil.move(sub_item, destination_path)

        if not any(item.iterdir()):
          item.rmdir()

    if not any(source.iterdir()):
      source.rmdir()

  FrogPilotVariables().update(holiday_theme="stock", started=False)
  ThemeManager().update_active_theme(time_validated=system_time_valid(), frogpilot_toggles=get_frogpilot_toggles())

  def backup_thread():
    while not system_time_valid():
      print("Waiting for system time to become valid...")
      time.sleep(1)

    subprocess.run(["pkill", "-SIGUSR1", "-f", "system.updated.updated"], check=False)

    backup_frogpilot(build_metadata)
    backup_toggles(params_storage)

  threading.Thread(target=backup_thread, daemon=True).start()

def setup_frogpilot(build_metadata):
  run_cmd(["sudo", "mount", "-o", "remount,rw", "/persist"], "Successfully remounted /persist as read-write", "Failed to remount /persist")

  Path(MODELS_PATH).mkdir(parents=True, exist_ok=True)
  Path(THEME_SAVE_PATH).mkdir(parents=True, exist_ok=True)

  for source_suffix, destination_suffix in [
    ("world_frog_day/colors", "theme_packs/frog/colors"),
    ("world_frog_day/distance_icons", "theme_packs/frog-animated/distance_icons"),
    ("world_frog_day/icons", "theme_packs/frog-animated/icons"),
    ("world_frog_day/signals", "theme_packs/frog/signals"),
    ("world_frog_day/sounds", "theme_packs/frog/sounds"),
  ]:
    source = Path(HOLIDAY_THEME_PATH) / source_suffix
    destination = Path(THEME_SAVE_PATH) / destination_suffix
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, destination, dirs_exist_ok=True)

  for source_suffix, destination_suffix in [
    ("world_frog_day/steering_wheel/wheel.png", "steering_wheels/frog.png"),
  ]:
    source = Path(HOLIDAY_THEME_PATH) / source_suffix
    destination = Path(THEME_SAVE_PATH) / destination_suffix
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)

  boot_logo_location = Path("/usr/comma/bg.jpg")
  boot_logo_save_location = Path(BASEDIR) / "selfdrive/frogpilot/assets/other_images/original_bg.jpg"
  frogpilot_boot_logo = Path(BASEDIR) / "selfdrive/frogpilot/assets/other_images/frogpilot_boot_logo.png"

  if not filecmp.cmp(frogpilot_boot_logo, boot_logo_location, shallow=False):
    run_cmd(["sudo", "mount", "-o", "remount,rw", "/usr/comma"], "/usr/comma remounted as read-write", "Failed to remount /usr/comma")
    run_cmd(["sudo", "cp", boot_logo_location, boot_logo_save_location], "Successfully replaced boot logo", "Failed to back up original boot logo")
    run_cmd(["sudo", "cp", frogpilot_boot_logo, boot_logo_location], "Successfully replaced boot logo", "Failed to replace boot logo")

  if build_metadata.channel == "FrogPilot-Development":
    subprocess.run(["sudo", "python3", "/persist/frogsgomoo.py"], check=True)

def uninstall_frogpilot():
  boot_logo_location = Path("/usr/comma/bg.jpg")
  boot_logo_restore_location = Path(BASEDIR) / "selfdrive" / "frogpilot" / "assets" / "other_images" / "original_bg.jpg"

  run_cmd(["sudo", "cp", boot_logo_restore_location, boot_logo_location], "Successfully restored the original boot logo", "Failed to restore the original boot logo")

  HARDWARE.uninstall()
