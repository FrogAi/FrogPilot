import threading
import time

from openpilot.system.hardware import HARDWARE

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables

def update_openpilot():
  params = frogpilot_variables.params

  def run_updater(signal, success_message, fail_message):
    failed_count = params.get("UpdateFailedCount")
    last_update_time = params.get("LastUpdateTime")

    if frogpilot_utilities.run_cmd(["pkill", signal, "-f", "system.updated.updated"], success_message, fail_message, report=False) is None:
      return False

    while params.get("LastUpdateTime") == last_update_time:
      if params.get("UpdateFailedCount") not in (failed_count, b"0"):
        return False
      time.sleep(1)

    return True

  def wait_for_offroad():
    while params.get_bool("IsOnroad") or frogpilot_utilities.running_threads.get("lock_doors", threading.Thread()).is_alive():
      time.sleep(60)

  if params.get("UpdaterState", encoding="utf-8") != "idle":
    return

  wait_for_offroad()

  if not run_updater("-SIGUSR1", "Checking for updates...", "Failed to check for update..."):
    return

  if params.get_bool("UpdaterFetchAvailable"):
    wait_for_offroad()

    if frogpilot_paths.BACKUP_PATH.is_file():
      return

    if not run_updater("-SIGHUP", "Update available, downloading...", "Failed to download update...") or params.get_bool("UpdaterFetchAvailable"):
      return
  elif not params.get_bool("UpdateAvailable"):
    return

  wait_for_offroad()

  if frogpilot_paths.BACKUP_PATH.is_file():
    return

  HARDWARE.reboot()
