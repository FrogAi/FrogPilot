#!/usr/bin/env python3
import time

from multiprocessing import Process

from openpilot.common.params import Params
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.hardware import HARDWARE, PC

from openpilot.frogpilot.common import frogpilot_backups, frogpilot_paths, frogpilot_utilities, frogpilot_variables


def boot_backup(build_metadata):
  while not system_time_valid():
    time.sleep(1)

  frogpilot_backups.backup_frogpilot(build_metadata, Params())


def frogpilot_boot_functions(build_metadata):
  frogpilot_toggles = frogpilot_variables.get_frogpilot_toggles()

  frogpilot_utilities.delete_path(frogpilot_paths.RESTORE_TEMP_PATH)

  Process(target=boot_backup, args=(build_metadata,), daemon=True).start()


def install_frogpilot():
  paths = [
  ]
  for path in paths:
    path.mkdir(parents=True, exist_ok=True)

  update_boot_logo(frogpilot_paths.FROGPILOT_BOOT_LOGO_PATH)


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
