#!/usr/bin/env python3
import threading
import time

from pathlib import Path

from openpilot.common.basedir import BASEDIR
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.hardware import HARDWARE


def frogpilot_boot_functions():
  def boot_thread():
    while not system_time_valid():
      print("Waiting for system time to become valid...")
      time.sleep(1)

  threading.Thread(target=boot_thread, daemon=True).start()


def install_frogpilot():
  paths = [
  ]
  for path in paths:
    path.mkdir(parents=True, exist_ok=True)

  update_boot_logo(Path(BASEDIR) / "frogpilot/assets/other_images/frogpilot_boot_logo.jpg")


def uninstall_frogpilot():
  update_boot_logo(Path(BASEDIR) / "frogpilot/assets/other_images/stock_bg.jpg")

  HARDWARE.uninstall()


def update_boot_logo(new_boot_logo):
  boot_logo_location = Path("/usr/comma/bg.jpg")

  if not boot_logo_location.is_file() or not new_boot_logo.is_file():
    print(f"Error: missing boot logo path: {boot_logo_location if not boot_logo_location.is_file() else new_boot_logo}")
    return

  if boot_logo_location.read_bytes() == new_boot_logo.read_bytes():
    return

  tmp = boot_logo_location.with_suffix(".jpg.tmp")
  try:
    mount_options = frogpilot_utilities.run_cmd(["findmnt", "-n", "-o", "OPTIONS", "/"], "Successfully retrieved mount options", "Failed to retrieve mount options")
    if not mount_options:
      return

    frogpilot_utilities.run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Successfully remounted / as read-write", "Failed to remount /")
    try:
      frogpilot_utilities.run_cmd(["sudo", "cp", str(new_boot_logo), str(tmp)], "Successfully copied boot logo to temp", "Failed to copy boot logo to temp")
      frogpilot_utilities.run_cmd(["sudo", "mv", str(tmp), str(boot_logo_location)], "Successfully replaced boot logo", "Failed to replace boot logo")
      frogpilot_utilities.run_cmd(["sync"], "Successfully synced filesystem", "Failed to sync filesystem")
    finally:
      frogpilot_utilities.run_cmd(["sudo", "rm", "-f", str(tmp)], "Successfully cleaned up temp boot logo", "Failed to clean up temp boot logo", raise_on_failure=False)
      frogpilot_utilities.run_cmd(["sudo", "mount", "-o", f"remount,{mount_options}", "/"], "Successfully restored / mount options", "Failed to restore / mount options")
  except Exception:
    return
