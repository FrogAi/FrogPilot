#!/usr/bin/env python3
import threading
import time

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


def uninstall_frogpilot():
  HARDWARE.uninstall()
