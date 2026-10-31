#!/usr/bin/env python3
import filecmp
import json
import os
import shutil
import subprocess
import time

from multiprocessing import Process

from openpilot.common.conversions import Conversions as CV
from openpilot.common.params import Params
from openpilot.common.time import system_time_valid
from openpilot.system.athena.registration import UNREGISTERED_DONGLE_ID, register
from openpilot.system.hardware import HARDWARE, PC

from openpilot.frogpilot.system.model_manager import ModelManager
from openpilot.frogpilot.assets.theme_manager import ThemeManager
from openpilot.frogpilot.common import frogpilot_backups, frogpilot_paths, frogpilot_utilities, frogpilot_variables

def cleanup_error_logs():
  for error_log in sorted(frogpilot_paths.ERROR_LOGS_PATH.glob("*.log"))[:-frogpilot_variables.MAX_ERROR_LOGS]:
    error_log.unlink()

def cleanup_screen_recordings(limit_bytes):
  recordings = sorted(frogpilot_paths.SCREEN_RECORDINGS_PATH.glob("*.mp4"), key=lambda recording: recording.stat().st_mtime, reverse=True)

  total = 0
  for index, recording in enumerate(recordings):
    total += recording.stat().st_size
    if index == 0 or total <= limit_bytes:
      continue

    for suffix in frogpilot_variables.SCREEN_RECORDING_COMPANION_SUFFIXES:
      frogpilot_utilities.delete_path(recording.with_suffix(suffix), report=False)
    frogpilot_utilities.delete_path(recording, report=False)

def backup_settings(destination):
  for key in frogpilot_variables.SETTINGS_BACKUP_KEYS:
    value = frogpilot_variables.params.get(key)

    if value is not None:
      (destination / key).write_bytes(value)

def boot_backup(build_metadata):
  while not system_time_valid():
    time.sleep(1)

  frogpilot_backups.backup_frogpilot(build_metadata)

def convert_guam_maps_selection():
  maps_selected = frogpilot_variables.params.get("MapsSelected")
  if maps_selected is not None and b'"GU"' in maps_selected:
    frogpilot_variables.params.put("MapsSelected", maps_selected.replace(b'"GU"', b'"GM"'))

def convert_model_stop_time():
  if frogpilot_variables.params.get("CEModelStopTime") is None:
    return

  tuning_level = frogpilot_variables.TUNING_LEVELS["DEVELOPER"]
  if frogpilot_variables.params.get_bool("TuningLevelConfirmed"):
    tuning_level = frogpilot_variables.params.get_int("TuningLevel")

  if tuning_level >= frogpilot_variables.TUNING_LEVELS["DEVELOPER"]:
    frogpilot_variables.params.put_bool("CEStopLights", frogpilot_variables.params.get_int("CEModelStopTime") != 0)

  frogpilot_variables.params.remove("CEModelStopTime")

def convert_params(params_cache):
  print("Starting to convert params")

  for param_store in (frogpilot_variables.params, params_cache):
    value = param_store.get("MaxLateralAcceleration")
    if value is not None:
      try:
        value = json.loads(value)
      except (json.JSONDecodeError, TypeError):
        value = None

      if not isinstance(value, dict):
        param_store.remove("MaxLateralAcceleration")

  for key in (*frogpilot_variables.SECRET_KEYS, "FrogPilotStats", "ShownToggleDescriptions", "ThemesDownloaded"):
    for param_store in (frogpilot_variables.params, params_cache):
      if param_store.get(key) == b"0":
        param_store.remove(key)

  for param_store in (frogpilot_variables.params, params_cache):
    for old_key, new_key in frogpilot_variables.RENAMED_KEYS.items():
      value = param_store.get(old_key)
      if value is not None:
        param_store.put(new_key, value)
        param_store.remove(old_key)

  convert_guam_maps_selection()
  convert_model_stop_time()

  if frogpilot_variables.params.get("SpeedLimitFillerShareData") == b"0":
    frogpilot_variables.params.put_bool("SpeedLimitFiller", False)

  frogpilot_variables.params.remove("SpeedLimitFillerShareData")

  if frogpilot_variables.params.get_bool("IsMetric"):
    metric_conversions = {
      "CESignalSpeed": (CV.MPH_TO_KPH, 0),
      "LaneLinesWidth": (CV.INCH_TO_CM, 0),
      "MinimumLaneChangeSpeed": (CV.MPH_TO_KPH, 0),
      "Offset1": (CV.MPH_TO_KPH, 0),
      "Offset2": (CV.MPH_TO_KPH, 0),
      "Offset3": (CV.MPH_TO_KPH, 0),
      "Offset4": (CV.MPH_TO_KPH, 0),
      "Offset5": (CV.MPH_TO_KPH, 0),
      "Offset6": (CV.MPH_TO_KPH, 0),
      "Offset7": (CV.MPH_TO_KPH, 0),
      "PathWidth": (CV.FOOT_TO_METER, 1),
      "RoadEdgesWidth": (CV.INCH_TO_CM, 0),
    }

    for key, (conversion, decimals) in metric_conversions.items():
      if frogpilot_variables.params.get(key) is None and params_cache.get(key) is None:
        frogpilot_variables.params.put(key, str(round(float(frogpilot_variables.get_default_value(key)) * conversion, decimals)))

  if frogpilot_paths.TRACKING_PATH.exists():
    params_tracking = Params(str(frogpilot_paths.TRACKING_PATH))

    frogpilot_stats = json.loads(frogpilot_variables.params.get("FrogPilotStats") or "{}")
    frogpilot_stats["FrogPilotDrives"] = params_tracking.get_int("FrogPilotDrives")
    frogpilot_stats["FrogPilotMeters"] = params_tracking.get_float("FrogPilotKilometers") * 1000
    frogpilot_stats["FrogPilotSeconds"] = params_tracking.get_float("FrogPilotMinutes") * 60

    frogpilot_variables.params.put("FrogPilotStats", json.dumps(frogpilot_stats))

    frogpilot_utilities.delete_path(frogpilot_paths.TRACKING_PATH)

  print("Param conversion completed")

def frogpilot_boot_functions(build_metadata):
  if frogpilot_variables.params.get_int("TetheringEnabled") == 3:
    frogpilot_variables.params.remove("TetheringEnabled")

  variables = frogpilot_variables.FrogPilotVariables()

  flag_files_changed = sync_flag_file(frogpilot_paths.HD_PATH, variables.frogpilot_toggles.use_higher_bitrate)
  flag_files_changed |= sync_flag_file(frogpilot_paths.KONIK_PATH, variables.frogpilot_toggles.use_konik_server)

  if flag_files_changed:
    HARDWARE.reboot()

  ModelManager(boot_run=True)
  variables.update(holiday_theme="stock", started=False)
  frogpilot_toggles = variables.frogpilot_toggles

  ThemeManager.copy_default_theme()
  ThemeManager(frogpilot_variables.params).update_active_theme(time_validated=system_time_valid(), frogpilot_toggles=frogpilot_toggles, boot_run=True)

  if frogpilot_paths.VIDEO_CACHE_PATH.exists():
    for cached_file in frogpilot_paths.VIDEO_CACHE_PATH.iterdir():
      frogpilot_utilities.delete_path(cached_file)

  if frogpilot_utilities.use_konik_server():
    konik_dongle_id = frogpilot_variables.params.get("KonikDongleId", encoding="utf8")
    if konik_dongle_id in (None, UNREGISTERED_DONGLE_ID):
      Process(target=register_konik, daemon=True).start()
    else:
      frogpilot_variables.params.put("DongleId", konik_dongle_id)
  elif frogpilot_variables.params.get("DongleId", encoding="utf8") == frogpilot_variables.params.get("KonikDongleId", encoding="utf8"):
    frogpilot_variables.params.remove("DongleId")

  shutil.rmtree(frogpilot_paths.RESTORE_TEMP_PATH, ignore_errors=True)

  frogpilot_utilities.delete_path(frogpilot_paths.LEGACY_PARAMS_BACKUP_PATH)

  Process(target=boot_backup, args=(build_metadata,), daemon=True).start()

def register_konik():
  frogpilot_variables.params.put("KonikDongleId", register(register_konik=True))

def remove_tailscale_service():
  legacy_unit = [
    "[Unit]",
    "Description=Tailscale node agent",
    "After=network.target",
    "[Service]",
    f"ExecStart={frogpilot_paths.TAILSCALE_PATH}/tailscaled \\",
    "--tun=userspace-networking \\",
    "--socks5-server=localhost:1055 \\",
    f"--state={frogpilot_paths.TAILSCALE_STATE_PATH}/tailscaled.state \\",
    f"--socket={frogpilot_paths.TAILSCALE_SOCKET_PATH} \\",
    f"--statedir={frogpilot_paths.TAILSCALE_STATE_PATH}",
    "Restart=on-failure",
    "RestartSec=5",
    "[Install]",
    "WantedBy=multi-user.target",
  ]
  unit = [line.strip() for line in frogpilot_paths.UNIT_PATH.read_text().splitlines() if line.strip()]
  if unit != legacy_unit:
    return

  stock_mount_options = subprocess.run(["findmnt", "-no", "OPTIONS", "/"], capture_output=True, text=True, check=True).stdout.strip()

  frogpilot_utilities.run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Successfully remounted / as read-write", "Failed to remount / as read-write")
  frogpilot_utilities.run_cmd(["sudo", "systemctl", "disable", "--now", "tailscaled"],
                              "Successfully stopped the Tailscale service", "Failed to stop the Tailscale service")
  frogpilot_utilities.run_cmd(["sudo", "rm", str(frogpilot_paths.UNIT_PATH)],
                              "Successfully removed the Tailscale service", "Failed to remove the Tailscale service")
  frogpilot_utilities.run_cmd(["sudo", "mount", "-o", f"remount,{stock_mount_options}", "/"],
                              "Successfully restored stock mount options", "Failed to restore stock mount options")

  frogpilot_utilities.run_cmd(["sudo", "systemctl", "daemon-reload"], "Successfully reloaded systemd", "Failed to reload systemd")
  frogpilot_utilities.run_cmd(["sudo", "chown", "-R", f"{os.getuid()}:{os.getgid()}", str(frogpilot_paths.TAILSCALE_PATH)],
                              "Successfully took ownership of Tailscale", "Failed to take ownership of Tailscale")

def restore_settings(source):
  settings = frogpilot_backups.read_settings_backup(source)

  for key, value in settings.items():
    if frogpilot_utilities.is_unset_secret(key, value):
      continue

    if value:
      frogpilot_variables.params.put(key, value)
    else:
      frogpilot_variables.params.remove(key)
      frogpilot_variables.params_cache.remove(key)

  convert_guam_maps_selection()
  convert_model_stop_time()

def setup_frogpilot(build_metadata):
  frogpilot_paths.ERROR_LOGS_PATH.mkdir(parents=True, exist_ok=True)
  frogpilot_paths.HD_LOGS_PATH.mkdir(parents=True, exist_ok=True)
  frogpilot_paths.KONIK_LOGS_PATH.mkdir(parents=True, exist_ok=True)
  frogpilot_paths.MODELS_PATH.mkdir(parents=True, exist_ok=True)
  frogpilot_paths.SCREEN_RECORDINGS_PATH.mkdir(parents=True, exist_ok=True)
  frogpilot_paths.THEME_SAVE_PATH.mkdir(parents=True, exist_ok=True)

  cleanup_error_logs()
  cleanup_screen_recordings(10 * 1024 * 1024 * 1024)

  if frogpilot_paths.UNIT_PATH.is_file():
    remove_tailscale_service()

  update_boot_logo(frogpilot_paths.FROGPILOT_BOOT_LOGO_PATH)

  if build_metadata.channel == frogpilot_variables.DEVELOPMENT_BRANCH and frogpilot_paths.FROGS_GO_MOO_PATH.is_file():
    frogpilot_utilities.run_cmd(["sudo", "mount", "-o", "remount,rw", "/persist"], "Successfully remounted /persist as read-write", "Failed to remount /persist")
    frogpilot_utilities.run_cmd(["sudo", "python3", frogpilot_paths.FROGS_GO_MOO_PATH], "Ran frogsgomoo.py", "Failed to run frogsgomoo.py")

def sync_flag_file(flag_path, enabled):
  if flag_path.is_file() == enabled:
    return False

  if enabled:
    flag_path.touch()
  else:
    flag_path.unlink()

  return True

def uninstall_frogpilot():
  update_boot_logo(frogpilot_paths.STOCK_BOOT_LOGO_PATH)

  HARDWARE.uninstall()

def update_boot_logo(target_logo):
  if PC:
    return

  if not filecmp.cmp(target_logo, frogpilot_paths.BOOT_LOGO_PATH, shallow=False):
    stock_mount_options = subprocess.run(["findmnt", "-no", "OPTIONS", "/"], capture_output=True, text=True, check=True).stdout.strip()

    frogpilot_utilities.run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Successfully remounted / as read-write", "Failed to remount / as read-write")
    frogpilot_utilities.run_cmd(["sudo", "cp", target_logo, frogpilot_paths.BOOT_LOGO_PATH], "Successfully replaced boot logo", "Failed to replace boot logo")
    frogpilot_utilities.run_cmd(["sudo", "mount", "-o", f"remount,{stock_mount_options}", "/"], "Successfully restored stock mount options", "Failed to restore stock mount options")
