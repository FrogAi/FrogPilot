#!/usr/bin/env python3
import json
import math
import subprocess
import time

from multiprocessing import Process
from pathlib import Path

from cereal import messaging
from openpilot.common.basedir import BASEDIR
from openpilot.common.constants import CV
from openpilot.common.params import Params
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.athena.registration import register
from openpilot.system.hardware import HARDWARE, PC

from openpilot.frogpilot.assets.theme_manager import ThemeManager
from openpilot.frogpilot.common import frogpilot_backups, frogpilot_utilities, frogpilot_variables

MAPD_DOWNLOAD_MENU_PATH = Path(BASEDIR) / "mapd_download_menu.json"


def boot_backup(build_metadata):
  while not system_time_valid():
    time.sleep(1)

  frogpilot_backups.backup_frogpilot(build_metadata, Params())


def cleanup_screen_recordings(limit_bytes):
  recordings = sorted(frogpilot_variables.SCREEN_RECORDINGS_PATH.glob("*.mp4"), key=lambda recording: recording.stat().st_mtime, reverse=True)

  total = 0
  for recording in recordings:
    total += recording.stat().st_size
    if total <= limit_bytes:
      continue

    for companion in (recording.with_suffix(".png"), recording.with_suffix(".gif")):
      frogpilot_utilities.delete_file(companion, report=False)
    frogpilot_utilities.delete_file(recording, report=False)


def download_maps(locations, cancel_download):
  pm = messaging.PubMaster(["mapdIn"])
  sm = messaging.SubMaster(["mapdExtendedOut"])

  time.sleep(1)

  msg = messaging.new_message("mapdIn")
  msg.mapdIn.type = 0
  msg.mapdIn.str = locations
  pm.send("mapdIn", msg)

  download_requested_at = time.monotonic()

  started = False

  while True:
    sm.update(1000)

    if cancel_download.is_set():
      msg = messaging.new_message("mapdIn")
      msg.mapdIn.type = 27
      pm.send("mapdIn", msg)

      return False

    if sm.updated["mapdExtendedOut"]:
      progress = sm["mapdExtendedOut"].downloadProgress

      if progress.active:
        started = True

      if not progress.active and started:
        return not progress.cancelled and progress.downloadedFiles == progress.totalFiles > 0

    if not started and time.monotonic() - download_requested_at >= 10:
      return False


def download_nearby_maps(latitude, longitude, cancel_download):
  angular_radius = 100_000 / frogpilot_variables.EARTH_RADIUS
  latitude_offset = math.degrees(angular_radius)
  longitude_offset = math.degrees(math.asin(math.sin(angular_radius) / math.cos(math.radians(latitude))))

  min_latitude = math.floor((latitude - latitude_offset) / frogpilot_variables.MAPD_ARCHIVE_SIZE_DEGREES) * frogpilot_variables.MAPD_ARCHIVE_SIZE_DEGREES
  max_latitude = math.ceil((latitude + latitude_offset) / frogpilot_variables.MAPD_ARCHIVE_SIZE_DEGREES) * frogpilot_variables.MAPD_ARCHIVE_SIZE_DEGREES
  min_longitude = math.floor((longitude - longitude_offset) / frogpilot_variables.MAPD_ARCHIVE_SIZE_DEGREES) * frogpilot_variables.MAPD_ARCHIVE_SIZE_DEGREES
  max_longitude = math.ceil((longitude + longitude_offset) / frogpilot_variables.MAPD_ARCHIVE_SIZE_DEGREES) * frogpilot_variables.MAPD_ARCHIVE_SIZE_DEGREES

  current_time = time.time()
  download_menu = {"nearby": {}}

  for archive_latitude in range(min_latitude, max_latitude, frogpilot_variables.MAPD_ARCHIVE_SIZE_DEGREES):
    for archive_longitude in range(min_longitude, max_longitude, frogpilot_variables.MAPD_ARCHIVE_SIZE_DEGREES):
      archive_longitude = (archive_longitude + 180) % 360 - 180
      archive_directory = frogpilot_variables.MAPS_PATH / str(archive_latitude) / str(archive_longitude)

      if any(current_time - tile.stat().st_mtime <= frogpilot_variables.MAPD_MAX_MAP_AGE_DAYS * 24 * 60 * 60 for tile in archive_directory.glob("*")):
        continue

      download_menu["nearby"][f"{archive_latitude}_{archive_longitude}"] = {
        "full_name": "Nearby Maps",
        "bounding_box": {
          "min_lon": archive_longitude,
          "min_lat": archive_latitude,
          "max_lon": archive_longitude + frogpilot_variables.MAPD_ARCHIVE_SIZE_DEGREES,
          "max_lat": archive_latitude + frogpilot_variables.MAPD_ARCHIVE_SIZE_DEGREES,
        },
      }

  if download_menu["nearby"]:
    frogpilot_utilities.update_json_file(MAPD_DOWNLOAD_MENU_PATH, download_menu)
    download_maps(",".join(f"nearby.{archive_name}" for archive_name in download_menu["nearby"]), cancel_download)

  frogpilot_utilities.delete_file(MAPD_DOWNLOAD_MENU_PATH, report=False)


def frogpilot_boot_functions(build_metadata, params):
  if params.get("MapdSettings") == {}:
    params.put("MapdSettings", params.get_default_value("MapdSettings"))

  maps_selected = params.get("MapsSelected")
  if maps_selected:
    try:
      data = json.loads(maps_selected)
      if isinstance(data, dict):
        new_items = []
        for nation in data.get("nations", []):
          new_items.append(f"nation.{nation}")
        for state in data.get("states", []):
          new_items.append(f"us_state.{state}")
        new_items.sort()
        params.put("MapsSelected", ",".join(new_items))
    except json.JSONDecodeError:
      pass

  if params.get("TetheringEnabled") == 3:
    params.remove("TetheringEnabled")

  frogpilot_toggles = frogpilot_variables.get_frogpilot_toggles()

  if not frogpilot_variables.HD_PATH.is_file() and frogpilot_toggles.use_higher_bitrate:
    frogpilot_variables.HD_PATH.touch()
    HARDWARE.reboot()
  elif frogpilot_variables.HD_PATH.is_file() and not frogpilot_toggles.use_higher_bitrate:
    frogpilot_variables.HD_PATH.unlink()
    HARDWARE.reboot()

  if not frogpilot_variables.KONIK_PATH.is_file() and frogpilot_toggles.use_konik_server:
    frogpilot_variables.KONIK_PATH.touch()
    HARDWARE.reboot()
  elif frogpilot_variables.KONIK_PATH.is_file() and not frogpilot_toggles.use_konik_server:
    frogpilot_variables.KONIK_PATH.unlink()
    HARDWARE.reboot()

  ThemeManager(params, boot_run=True).update_active_theme(time_validated=system_time_valid(), frogpilot_toggles=frogpilot_toggles, boot_run=True)

  if frogpilot_utilities.use_konik_server():
    if params.get("KonikDongleId") is not None:
      params.put("DongleId", params.get("KonikDongleId"))
    else:
      Process(target=register_konik, daemon=True).start()
  elif params.get("DongleId") == params.get("KonikDongleId"):
    params.put("DongleId", params.get("StockDongleId"))

  frogpilot_utilities.delete_file("/data/restore_temp")

  Process(target=boot_backup, args=(build_metadata,), daemon=True).start()


def install_frogpilot():
  paths = [
    frogpilot_variables.ERROR_LOGS_PATH,
    frogpilot_variables.HD_LOGS_PATH,
    frogpilot_variables.KONIK_LOGS_PATH,
    frogpilot_variables.SCREEN_RECORDINGS_PATH
  ]
  for path in paths:
    path.mkdir(parents=True, exist_ok=True)

  cleanup_screen_recordings(10 * 1024 * 1024 * 1024)

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


def register_konik():
  Params().put("KonikDongleId", register(register_konik=True))


def run_frogsgomoo(build_metadata):
  if build_metadata.channel == "FrogPilot-Development" and frogpilot_utilities.is_FrogsGoMoo():
    mount_options = frogpilot_utilities.run_cmd(["findmnt", "-n", "-o", "OPTIONS", "/persist"], None, "Failed to retrieve mount options")
    frogpilot_utilities.run_cmd(["sudo", "mount", "-o", "remount,rw", "/persist"], None, "Failed to remount /persist")
    frogpilot_utilities.run_cmd(["sudo", "python3", frogpilot_variables.FROGS_GO_MOO_PATH], None, "Failed to run frogsgomoo.py")
    frogpilot_utilities.run_cmd(["sudo", "mount", "-o", f"remount,{mount_options}", "/persist"], None, "Failed to restore /persist mount options")


def soft_reboot():
  Path("/tmp/booted").touch()

  subprocess.check_call(["sudo", "systemctl", "restart", "--no-block", "comma"])


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


def update_maps(now, params, cancel_download, sm, manual_update=False):
  cancel_download.clear()

  last_gps_position = params.get("LastGPSPosition")
  if last_gps_position and not sm["deviceState"].networkMetered:
    position = json.loads(last_gps_position)
    download_nearby_maps(position["latitude"], position["longitude"], cancel_download)

  if cancel_download.is_set() or (sm["deviceState"].networkMetered and not manual_update):
    return

  maps_selected = params.get("MapsSelected")
  if not maps_selected:
    return

  now = now.astimezone()

  day = now.day
  is_first = day == 1
  is_sunday = now.weekday() == 6
  schedule = params.get("PreferredSchedule")

  last_maps_update = params.get("LastMapsUpdate")
  maps_downloaded = frogpilot_variables.MAPS_PATH.exists() and bool(last_maps_update)

  if maps_downloaded and (schedule == 0 or (schedule == 1 and not is_sunday) or (schedule == 2 and not is_first)) and not manual_update:
    return

  suffix = "th" if 11 <= day <= 13 else {1: "st", 2: "nd", 3: "rd"}.get(day % 10, "th")
  todays_date = now.strftime(f"%B {day}{suffix}, %Y")

  if maps_downloaded and last_maps_update == todays_date and not manual_update:
    return

  frogpilot_utilities.delete_file(MAPD_DOWNLOAD_MENU_PATH, report=False)

  if download_maps(maps_selected, cancel_download):
    params.put("LastMapsUpdate", todays_date)


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
