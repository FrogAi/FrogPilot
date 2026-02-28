#!/usr/bin/env python3
import dataclasses
import datetime
import filecmp
import glob
import json
import requests
import shutil
import subprocess
import tarfile
import threading
import time
import zstandard as zstd

from pathlib import Path

from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params
from openpilot.common.time import system_time_valid
from openpilot.system.athena.registration import register
from openpilot.system.hardware import HARDWARE

from openpilot.frogpilot.assets.model_manager import ModelManager
from openpilot.frogpilot.assets.theme_manager import ThemeManager
from openpilot.frogpilot.common.frogpilot_utilities import delete_file, is_url_pingable, run_cmd, run_thread_with_lock, use_konik_server
from openpilot.frogpilot.common.frogpilot_variables import (
  ERROR_LOGS_PATH, EXCLUDED_KEYS, FROGPILOT_API, HD_LOGS_PATH, KONIK_LOGS_PATH, MODELS_PATH, SCREEN_RECORDINGS_PATH,
  THEME_SAVE_PATH, VIDEO_CACHE_PATH, FrogPilotVariables, frogpilot_default_params, get_frogpilot_toggles, params
)
from openpilot.frogpilot.system.frogpilot_stats import send_stats

def backup_directory(backup, destination, success_message, fail_message, minimum_backup_size=0, compressed=False):
  in_progress_destination = destination.parent / (destination.name + "_in_progress")
  in_progress_destination.mkdir(parents=True, exist_ok=True)

  if compressed:
    destination_compressed = destination.parent / (destination.name + ".tar.zst")
    if destination_compressed.exists():
      delete_file(in_progress_destination, report=False)
      print("Backup already exists. Aborting...")
      return

    run_cmd(["sudo", "rsync", "-avq", f"{backup}/.", in_progress_destination], "", fail_message, report=False)

    tar_file = destination.parent / (destination.name + "_in_progress.tar")
    with tarfile.open(tar_file, "w") as tar:
      tar.add(in_progress_destination, arcname=destination.name)

    delete_file(in_progress_destination, report=False)

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

    compressed_file.rename(destination_compressed)
    print(f"Backup saved: {destination_compressed}")

    compressed_backup_size = destination_compressed.stat().st_size
    if minimum_backup_size == 0 or compressed_backup_size < minimum_backup_size:
      params.put_int("MinimumBackupSize", compressed_backup_size)
  else:
    if destination.exists():
      delete_file(in_progress_destination, report=False)
      print("Backup already exists. Aborting...")
      return

    run_cmd(["sudo", "rsync", "-avq", f"{backup}/.", in_progress_destination], success_message, fail_message, report=False)
    in_progress_destination.rename(destination)

def cleanup_backups(directory, limit, compressed=False):
  directory.mkdir(parents=True, exist_ok=True)

  for in_progress in directory.glob("*_in_progress*"):
    delete_file(in_progress, report=False)

  backups = sorted(directory.glob("*_auto*"), key=lambda x: x.stat().st_mtime, reverse=True)
  for oldest_backup in backups[limit:]:
    delete_file(oldest_backup, report=False)

def backup_frogpilot(build_metadata):
  backup_path = Path("/data/backups")
  maximum_backups = 3
  cleanup_backups(backup_path, maximum_backups, compressed=True)

  _, _, free = shutil.disk_usage(backup_path)
  minimum_backup_size = params.get_int("MinimumBackupSize")
  if free > minimum_backup_size * maximum_backups:
    directory = Path(BASEDIR)
    destination_directory = backup_path / f"{build_metadata.channel}_{build_metadata.openpilot.git_commit_date[12:-16]}_auto"
    backup_directory(directory, destination_directory, f"Successfully backed up FrogPilot to {destination_directory}", f"Failed to backup FrogPilot to {destination_directory}", minimum_backup_size, compressed=True)

def backup_toggles(params_cache):
  params_backup = Params("/data/params_backup")

  changes_found = False
  for key, _, _, _ in frogpilot_default_params:
    new_value = params.get(key)
    current_value = params_backup.get(key)

    if new_value != current_value:
      if new_value is not None:
        params_backup.put(key, new_value)
        params_cache.put(key, new_value)

      if key not in EXCLUDED_KEYS:
        changes_found = True

  if changes_found:
    params.put_bool("PondUploadPending", True)

  backup_path = Path("/data/toggle_backups")
  maximum_backups = 5

  cleanup_backups(backup_path, maximum_backups)

  existing_backups = list(backup_path.glob("*"))
  if not changes_found and existing_backups:
    print("Toggles are identical to the previous backup. Aborting...")
    return

  directory = Path("/data/params_backup/d")
  destination_directory = backup_path / f"{datetime.datetime.now().strftime('%Y-%m-%d_%I-%M%p').lower()}_auto"
  backup_directory(directory, destination_directory, f"Successfully backed up toggles to {destination_directory}", f"Failed to backup toggles to {destination_directory}")

def convert_params(params_cache):
  print("Starting to convert params")

  if Path("/cache/tracking").exists():
    params_tracking = Params("/cache/tracking")

    frogpilot_stats = json.loads(params.get("FrogPilotStats") or "{}")
    frogpilot_stats["FrogPilotDrives"] = params_tracking.get_int("FrogPilotDrives")
    frogpilot_stats["FrogPilotMeters"] = params_tracking.get_float("FrogPilotKilometers") * 1000
    frogpilot_stats["FrogPilotSeconds"] = params_tracking.get_float("FrogPilotMinutes") * 60

    params.put("FrogPilotStats", json.dumps(frogpilot_stats))

    delete_file("/cache/tracking")

  print("Param conversion completed")

def frogpilot_boot_functions(build_metadata, params_cache):
  if params.get_bool("HasAcceptedTerms"):
    params_cache.clear_all()

  FrogPilotVariables().update(holiday_theme="stock", started=False)
  ModelManager(boot_run=True)
  ThemeManager(boot_run=True).update_active_theme(time_validated=system_time_valid(), frogpilot_toggles=get_frogpilot_toggles(), boot_run=True)

  if VIDEO_CACHE_PATH.exists():
    for video in VIDEO_CACHE_PATH.glob("*.mp4"):
      delete_file(video)

  if use_konik_server():
    if params.get("KonikDongleId", encoding="utf8") != None:
      params.put("DongleId", params.get("KonikDongleId", encoding="utf8"))
    else:
      params.put("KonikDongleId", register(show_spinner=True, register_konik=True))
      params.put("DongleId", params.get("KonikDongleId", encoding="utf8"))
  elif params.get("DongleId", encoding="utf8") == params.get("KonikDongleId", encoding="utf8"):
    params.remove("DongleId")

  params.put("BuildMetadata", json.dumps(dataclasses.asdict(build_metadata)))

  def boot_thread():
    while not system_time_valid():
      print("Waiting for system time to become valid...")
      time.sleep(1)

    backup_frogpilot(build_metadata)
    backup_toggles(params_cache)

    send_stats()

  threading.Thread(target=boot_thread, daemon=True).start()

def setup_frogpilot(build_metadata):
  ERROR_LOGS_PATH.mkdir(parents=True, exist_ok=True)
  HD_LOGS_PATH.mkdir(parents=True, exist_ok=True)
  KONIK_LOGS_PATH.mkdir(parents=True, exist_ok=True)
  MODELS_PATH.mkdir(parents=True, exist_ok=True)
  SCREEN_RECORDINGS_PATH.mkdir(parents=True, exist_ok=True)
  THEME_SAVE_PATH.mkdir(parents=True, exist_ok=True)

  boot_logo_location = Path("/usr/comma/bg.jpg")
  frogpilot_boot_logo = Path(__file__).parents[1] / "assets/other_images/frogpilot_boot_logo.png"
  if not filecmp.cmp(frogpilot_boot_logo, boot_logo_location, shallow=False):
    stock_mount_options = subprocess.run(["findmnt", "-no", "OPTIONS", "/"], capture_output=True, text=True, check=True).stdout.strip()

    run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Successfully remounted / as read-write", "Failed to remount / as read-write")
    run_cmd(["sudo", "cp", frogpilot_boot_logo, boot_logo_location], "Successfully replaced boot logo", "Failed to replace boot logo")
    run_cmd(["sudo", "mount", "-o", f"remount,{stock_mount_options}", "/"], "Successfully restored stock mount options", "Failed to restore stock mount options")

  if build_metadata.channel == "FrogPilot-Development" and Path("/persist/frogsgomoo.py").is_file():
    run_cmd(["sudo", "mount", "-o", "remount,rw", "/persist"], "Successfully remounted /persist as read-write", "Failed to remount /persist")
    run_cmd(["sudo", "python3", "/persist/frogsgomoo.py"], "Ran frogsgomoo.py", "Failed to run frogsgomoo.py")

  register_device(build_metadata)

def register_device(build_metadata):
  def register_thread():
    while not is_url_pingable(FROGPILOT_API):
      time.sleep(60)

    payload = {
      "build_metadata": dataclasses.asdict(build_metadata),
      "device": HARDWARE.get_device_type(),
      "dongle_id": params.get("DongleId", encoding="utf8"),
      "frogpilot_dongle_id": params.get("FrogPilotDongleId", encoding="utf8"),
    }

    try:
      response = requests.post(f"{FROGPILOT_API}/register", json=payload, headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"}, timeout=10)
      response.raise_for_status()

      data = response.json()
      print(f"Device registration successful: dongle_id={data.get('frogpilot_dongle_id', '')[:8]}..., token={'set' if data.get('api_token') else 'empty'}")
      params.put("FrogPilotApiToken", data.get("api_token", ""))
      params.put("FrogPilotDongleId", data.get("frogpilot_dongle_id"))
    except Exception as e:
      print(f"Device registration failed: {e}")
      if hasattr(e, 'response') and e.response is not None:
        print(f"  Status: {e.response.status_code}, Body: {e.response.text[:200]}")

  threading.Thread(target=register_thread, daemon=True).start()


def uninstall_frogpilot():
  boot_logo_location = Path("/usr/comma/bg.jpg")
  stock_boot_logo = Path(__file__).parents[1] / "assets/other_images/stock_bg.jpg"

  run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Successfully remounted / as read-write", "Failed to remount / as read-write")
  run_cmd(["sudo", "cp", stock_boot_logo, boot_logo_location], "Successfully restored boot logo", "Failed to restore boot logo")

  HARDWARE.uninstall()
