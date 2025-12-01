#!/usr/bin/env python3
import time

from multiprocessing import Process

from openpilot.common.params import Params
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.hardware import HARDWARE, PC

from openpilot.frogpilot.assets.theme_manager import ThemeManager
from openpilot.frogpilot.common import frogpilot_backups, frogpilot_paths, frogpilot_utilities, frogpilot_variables


def boot_backup(build_metadata):
  while not system_time_valid():
    time.sleep(1)

  frogpilot_backups.backup_frogpilot(build_metadata, Params())


def cleanup_error_logs():
  for error_log in sorted(frogpilot_paths.ERROR_LOGS_PATH.glob("*.log"))[:-frogpilot_variables.MAX_ERROR_LOGS]:
    error_log.unlink()


def frogpilot_boot_functions(build_metadata, params):
  frogpilot_toggles = frogpilot_variables.get_frogpilot_toggles()

  if sync_flag_file(frogpilot_paths.HD_PATH, frogpilot_toggles.use_higher_bitrate):
    HARDWARE.reboot()

  ThemeManager.copy_default_theme()
  ThemeManager(params).update_active_theme(frogpilot_toggles=frogpilot_toggles)

  frogpilot_utilities.delete_path(frogpilot_paths.RESTORE_TEMP_PATH)

  Process(target=boot_backup, args=(build_metadata,), daemon=True).start()


def install_frogpilot():
  paths = [
    frogpilot_paths.ERROR_LOGS_PATH,
    frogpilot_paths.HD_LOGS_PATH
  ]
  for path in paths:
    path.mkdir(parents=True, exist_ok=True)

  cleanup_error_logs()

  update_boot_logo(frogpilot_paths.FROGPILOT_BOOT_LOGO_PATH)


def run_frogsgomoo(build_metadata):
  if frogpilot_utilities.is_FrogsGoMoo_development(build_metadata):
    frogpilot_utilities.run_cmd_on_writable_mount("/persist", ["sudo", "python3", frogpilot_paths.FROGS_GO_MOO_PATH], "Failed to run frogsgomoo.py")


def sync_flag_file(flag_path, enabled):
  if flag_path.is_file() == enabled:
    return False

  if enabled:
    flag_path.touch()
  else:
    flag_path.unlink()

  return True


def sync_params_cache(params, params_cache):
  for key in params.all_keys():
    current_value = params.get(key)
    cached_value = params_cache.get(key)

    if current_value is None:
      if cached_value is not None:
        params.put(key, cached_value)
    elif current_value != cached_value:
      params_cache.put(key, current_value)


def uninstall_frogpilot():
  update_boot_logo(frogpilot_paths.STOCK_BOOT_LOGO_PATH)

  HARDWARE.uninstall()


def update_boot_logo(target_logo):
  if PC:
    return

  if frogpilot_paths.BOOT_LOGO_PATH.read_bytes() != target_logo.read_bytes():
    frogpilot_utilities.run_cmd_on_writable_mount("/", ["sudo", "cp", target_logo, frogpilot_paths.BOOT_LOGO_PATH], "Failed to replace boot logo")


def update_openpilot(thread_manager, params):
  def signal_updater(signal, fail_message):
    last_run = params.get("UpdaterLastRunTime")
    if frogpilot_utilities.run_cmd(["pkill", signal, "-f", frogpilot_variables.UPDATER_PROCESS], fail_message, report=False) is None:
      return False

    while params.get("UpdaterLastRunTime") == last_run:
      time.sleep(1)
    return True

  def wait_until_offroad():
    while params.get_bool("IsOnroad") or thread_manager.is_thread_alive("lock_doors"):
      time.sleep(60)

  def download_update():
    if not signal_updater("-SIGUSR1", "Failed to check for update...") or not params.get_bool("UpdaterFetchAvailable"):
      return False

    wait_until_offroad()

    if frogpilot_paths.BACKUP_PATH.is_file():
      return False

    return signal_updater("-SIGHUP", "Failed to download update...") and params.get_bool("UpdateAvailable")

  if params.get("UpdaterState") != "idle":
    return

  wait_until_offroad()

  if not params.get_bool("UpdateAvailable") and not download_update():
    return

  while download_update():
    continue

  wait_until_offroad()

  if frogpilot_paths.BACKUP_PATH.is_file():
    return

  HARDWARE.reboot()
