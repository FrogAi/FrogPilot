#!/usr/bin/env python3
from openpilot.system.hardware import HARDWARE, PC


def frogpilot_boot_functions():


def install_frogpilot():
  paths = [
  ]
  for path in paths:
    path.mkdir(parents=True, exist_ok=True)

  update_boot_logo(frogpilot_paths.FROGPILOT_BOOT_LOGO_PATH)


def uninstall_frogpilot():
  update_boot_logo(frogpilot_paths.STOCK_BOOT_LOGO_PATH)

  HARDWARE.uninstall()


def update_boot_logo(target_logo):
  if PC:
    return

  if frogpilot_paths.BOOT_LOGO_PATH.read_bytes() != target_logo.read_bytes():
    frogpilot_utilities.run_cmd_on_writable_mount("/", ["sudo", "cp", target_logo, frogpilot_paths.BOOT_LOGO_PATH], "Failed to replace boot logo")
