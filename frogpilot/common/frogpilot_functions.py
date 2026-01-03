#!/usr/bin/env python3
import time

from multiprocessing import Process
from pathlib import Path

from openpilot.common.basedir import BASEDIR
from openpilot.common.constants import CV
from openpilot.common.params import Params
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.hardware import HARDWARE, PC

from openpilot.frogpilot.common import frogpilot_backups, frogpilot_utilities, frogpilot_variables


def boot_backup(build_metadata):
  while not system_time_valid():
    time.sleep(1)

  frogpilot_backups.backup_frogpilot(build_metadata, Params())


def frogpilot_boot_functions(build_metadata):
  frogpilot_toggles = frogpilot_variables.get_frogpilot_toggles()

  frogpilot_utilities.delete_file("/data/restore_temp")

  Process(target=boot_backup, args=(build_metadata,), daemon=True).start()


def install_frogpilot():
  paths = [
  ]
  for path in paths:
    path.mkdir(parents=True, exist_ok=True)

  update_boot_logo(Path(BASEDIR) / "frogpilot/assets/other_images/frogpilot_boot_logo.jpg")


def migrate_params(params, params_cache):
  param_types = {
    "MaxLateralAcceleration": dict,
  }

  for key, expected_type in param_types.items():
    for param_store in (params, params_cache):
      value = param_store.get(key)

      if value is not None and not isinstance(value, expected_type):
        param_store.remove(key)

  if params.get_bool("IsMetric"):
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
      if params.get(key) is None and not Path(params_cache.get_param_path(key)).is_file():
        params.put(key, round(params.get_default_value(key) * conversion, decimals))


def uninstall_frogpilot():
  update_boot_logo(Path(BASEDIR) / "frogpilot/assets/other_images/stock_bg.jpg")

  HARDWARE.uninstall()


def update_boot_logo(target_logo):
  if PC:
    return

  boot_logo_location = Path("/usr/comma/bg.jpg")

  if boot_logo_location.read_bytes() != target_logo.read_bytes():
    mount_options = frogpilot_utilities.run_cmd(["findmnt", "-n", "-o", "OPTIONS", "/"], None, "Failed to retrieve mount options")
    frogpilot_utilities.run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], None, "Failed to remount /")
    frogpilot_utilities.run_cmd(["sudo", "cp", target_logo, boot_logo_location], None, "Failed to replace boot logo")
    frogpilot_utilities.run_cmd(["sudo", "mount", "-o", f"remount,{mount_options}", "/"], None, "Failed to restore / mount options")


def update_openpilot(thread_manager, params):
  def signal_updater(signal, fail_message):
    last_run = params.get("UpdaterLastRunTime")
    if frogpilot_utilities.run_cmd(["pkill", signal, "-f", "system.updated.updated"], None, fail_message, report=False) is None:
      return False

    while params.get("UpdaterLastRunTime") == last_run:
      time.sleep(1)
    return True

  def wait_until_offroad():
    while params.get_bool("IsOnroad") or thread_manager.is_thread_alive("lock_doors"):
      time.sleep(60)

  def update_available():
    if not signal_updater("-SIGUSR1", "Failed to check for update...") or not params.get_bool("UpdaterFetchAvailable"):
      return False

    wait_until_offroad()

    return signal_updater("-SIGHUP", "Failed to download update...") and params.get_bool("UpdateAvailable")

  if params.get("UpdaterState") != "idle":
    return

  wait_until_offroad()

  if not params.get_bool("UpdateAvailable") and not update_available():
    return

  while update_available():
    pass

  wait_until_offroad()

  HARDWARE.reboot()
