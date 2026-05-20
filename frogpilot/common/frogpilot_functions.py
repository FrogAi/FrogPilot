#!/usr/bin/env python3
import dataclasses
import requests
import threading
import time

from pathlib import Path

from openpilot.common.api import get_key_pair
from openpilot.common.basedir import BASEDIR
from openpilot.common.constants import CV
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.hardware import HARDWARE

from openpilot.frogpilot.common import frogpilot_utilities


def frogpilot_boot_functions():
  def boot_thread():
    while not system_time_valid():
      print("Waiting for system time to become valid...")
      time.sleep(1)

  threading.Thread(target=boot_thread, daemon=True).start()


def install_frogpilot(build_metadata, params):
  paths = [
  ]
  for path in paths:
    path.mkdir(parents=True, exist_ok=True)

  register_device(build_metadata, params)

  update_boot_logo(Path(BASEDIR) / "frogpilot/assets/other_images/frogpilot_boot_logo.jpg")


def migrate_params_to_si(params):
  def migrate(keys, metric_factor, imperial_factor):
    factor = metric_factor if is_metric else imperial_factor
    for key in keys:
      value = params.get(key)
      if value is None or value == 0:
        continue
      params.put(key, float(value) * factor)

  def migrate_renamed_param(old_key, new_key):
    value = params.get(old_key)
    if value is None:
      return
    if params.get(new_key) is None:
      params.put(new_key, value)
    params.remove(old_key)

  if not params.get_bool("ParamsMigratedToSI"):
    is_metric = params.get_bool("IsMetric")

    migrate((
      "IncreasedStoppedDistance",
      "IncreasedStoppedDistanceLowVisibility",
      "IncreasedStoppedDistanceRain",
      "IncreasedStoppedDistanceRainStorm",
      "IncreasedStoppedDistanceSnow",
      "LaneDetectionWidth",
    ), 1.0, CV.FOOT_TO_METER)

    migrate(("LaneLinesWidth", "RoadEdgesWidth"), 1.0 / 200.0, CV.INCH_TO_CM / 200.0)

    migrate(("PathWidth",), 0.5, CV.FOOT_TO_METER / 2.0)

    migrate((
      "CESignalSpeed",
      "CESpeed",
      "CESpeedLead",
      "MinimumLaneChangeSpeed",
      "Offset1",
      "Offset2",
      "Offset3",
      "Offset4",
      "Offset5",
      "Offset6",
      "Offset7",
      "PauseAOLOnBrake",
      "PauseLateralSpeed",
      "SetSpeedOffset",
    ), CV.KPH_TO_MS, CV.MPH_TO_MS)

    params.put_bool("ParamsMigratedToSI", True)

  migrate_renamed_param("CustomCruise", "CruiseButtonIncrement")
  migrate_renamed_param("CustomCruiseLong", "CruiseButtonIncrementLong")


def register_device(build_metadata, params):
  def register_thread():
    while not frogpilot_utilities.is_url_pingable(frogpilot_variables.FROGPILOT_API):
      time.sleep(60)

    _, _, public_key = get_key_pair()
    payload = {
      "build_metadata": dataclasses.asdict(build_metadata),
      "device": HARDWARE.get_device_type(),
      "device_public_key": public_key,
      "dongle_id": params.get("DongleId"),
      "os_version": HARDWARE.get_os_version(),
    }

    try:
      response = requests.post(
        f"{frogpilot_variables.FROGPILOT_API}/register",
        json=payload,
        headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"},
        timeout=10,
      )
      response.raise_for_status()

      data = response.json()
      params.put("FrogPilotApiToken", data.get("api_token", ""))
      params.put("FrogPilotDongleId", data.get("frogpilot_dongle_id", ""))
    except Exception:
      pass

  threading.Thread(target=register_thread, daemon=True).start()


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
