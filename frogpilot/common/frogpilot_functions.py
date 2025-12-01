#!/usr/bin/env python3
from openpilot.system.hardware import HARDWARE


def frogpilot_boot_functions():


def install_frogpilot():
  paths = [
  ]
  for path in paths:
    path.mkdir(parents=True, exist_ok=True)


def uninstall_frogpilot():
  HARDWARE.uninstall()
