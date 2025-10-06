#!/usr/bin/env python3
import filecmp
import subprocess

from pathlib import Path

from openpilot.system.hardware import HARDWARE

from openpilot.frogpilot.common.frogpilot_utilities import run_cmd

def install_frogpilot(build_metadata, params):
  paths = [
  ]
  for path in paths:
    path.mkdir(parents=True, exist_ok=True)

  boot_logo_location = Path("/usr/comma/bg.jpg")
  frogpilot_boot_logo = Path(__file__).resolve().parents[1] / "assets/other_images/frogpilot_boot_logo.jpg"

  if not filecmp.cmp(frogpilot_boot_logo, boot_logo_location, shallow=False):
    mount_options = subprocess.run(['findmnt', '-n', '-o', 'OPTIONS', '/'], capture_output=True, text=True).stdout.strip()
    run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Successfully remounted / as read-write", "Failed to remount /")
    run_cmd(["sudo", "cp", frogpilot_boot_logo, boot_logo_location], "Successfully replaced boot logo", "Failed to replace boot logo")
    run_cmd(["sudo", "mount", "-o", f"remount,{mount_options}", "/"], "Successfully restored / mount options", "Failed to restore / mount options")

  if build_metadata.channel == "FrogPilot-Development" and Path("/persist/frogsgomoo.py").is_file():
    mount_options = subprocess.run(['findmnt', '-n', '-o', 'OPTIONS', '/persist'], capture_output=True, text=True).stdout.strip()
    run_cmd(["sudo", "mount", "-o", "remount,rw", "/persist"], "Successfully remounted /persist as read-write", "Failed to remount /persist")
    run_cmd(["sudo", "python3", "/persist/frogsgomoo.py"], "Ran frogsgomoo.py", "Failed to run frogsgomoo.py")
    run_cmd(["sudo", "mount", "-o", f"remount,{mount_options}", "/persist"], "Successfully restored /persist mount options", "Failed to restore /persist mount options")


def uninstall_frogpilot():
  boot_logo_location = Path("/usr/comma/bg.jpg")
  stock_boot_logo = Path(__file__).resolve().parents[1] / "assets/other_images/stock_bg.jpg"

  mount_options = subprocess.run(['findmnt', '-n', '-o', 'OPTIONS', '/'], capture_output=True, text=True).stdout.strip()
  run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Successfully remounted / as read-write", "Failed to remount /")
  run_cmd(["sudo", "cp", stock_boot_logo, boot_logo_location], "Successfully restored boot logo", "Failed to restored boot logo")
  run_cmd(["sudo", "mount", "-o", f"remount,{mount_options}", "/"], "Successfully restored / mount options", "Failed to restore / mount options")

  HARDWARE.uninstall()
