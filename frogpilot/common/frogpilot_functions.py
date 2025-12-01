#!/usr/bin/env python3
from pathlib import Path

from openpilot.common.basedir import BASEDIR
from openpilot.system.hardware import HARDWARE, PC

from openpilot.frogpilot.common import frogpilot_utilities


def frogpilot_boot_functions():


def install_frogpilot():
  paths = [
  ]
  for path in paths:
    path.mkdir(parents=True, exist_ok=True)

  update_boot_logo(Path(BASEDIR) / "frogpilot/assets/other_images/frogpilot_boot_logo.jpg")


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
