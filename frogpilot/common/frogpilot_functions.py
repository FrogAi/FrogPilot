#!/usr/bin/env python3
import dataclasses
import hashlib
import json
import math
import re
import secrets
import shutil
import threading
import time

from pathlib import Path

from cereal import messaging
from openpilot.common.basedir import BASEDIR
from openpilot.common.gps import get_gps_location_service
from openpilot.common.params import Params
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.athena.registration import register
from openpilot.system.hardware import HARDWARE

from openpilot.frogpilot.assets.theme_manager import ThemeManager
from openpilot.frogpilot.common import frogpilot_api
from openpilot.frogpilot.common.frogpilot_backups import backup_frogpilot
from openpilot.frogpilot.common.frogpilot_utilities import (
  delete_file, is_FrogsGoMoo, is_gps_location_valid, run_cmd, update_json_file, use_konik_server
)
from openpilot.frogpilot.common.frogpilot_variables import (
  EARTH_RADIUS, ERROR_LOGS_PATH, FROGS_GO_MOO_PATH, HD_LOGS_PATH, KONIK_LOGS_PATH, MAPD_ARCHIVE_SIZE_DEGREES, MAPD_MAX_MAP_AGE_DAYS, MAPS_PATH,
  SCREEN_RECORDINGS_PATH, THEME_SAVE_PATH, FrogPilotVariables, get_frogpilot_toggles
)


def capture_report(discord_user, report, params, frogpilot_toggles):
  api_info = frogpilot_api.get_info()

  error_file_path = ERROR_LOGS_PATH / "error.txt"
  error_content = "No error log found."
  if error_file_path.exists():
    error_content = error_file_path.read_text()[:1000]

  payload = {
    "build_metadata": api_info["build_metadata"],
    "device": api_info["device_type"],
    "discord_user": discord_user,
    "error_content": error_content,
    "frogpilot_dongle_id": api_info["dongle_id"],
    "frogpilot_toggles": frogpilot_toggles,
    "report": report,
  }

  response = frogpilot_api.post("/discord/report", json=payload, headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"}, timeout=30)
  if response is not None and 200 <= response.status_code < 300:
    print("Successfully sent error report!")
  else:
    status = response.status_code if response is not None else "unavailable"
    print(f"Error sending report: {status}")


def cleanup_screen_recordings(limit_bytes):
  recordings = sorted(SCREEN_RECORDINGS_PATH.glob("*.mp4"), key=lambda recording: recording.stat().st_mtime, reverse=True)

  total = 0
  for recording in recordings:
    total += recording.stat().st_size
    if total > limit_bytes:
      delete_file(recording, report=False)


def download_maps(locations, params_memory):
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

    if params_memory.get_bool("CancelDownloadMaps"):
      msg = messaging.new_message("mapdIn")
      msg.mapdIn.type = 27
      pm.send("mapdIn", msg)

      params_memory.remove("CancelDownloadMaps")

      return False

    if sm.updated["mapdExtendedOut"]:
      progress = sm["mapdExtendedOut"].downloadProgress

      if progress.active:
        started = True

      if not progress.active and started:
        return not progress.cancelled and progress.downloadedFiles == progress.totalFiles > 0

    if not started and time.monotonic() - download_requested_at >= 10:
      return False


def download_nearby_maps(latitude, longitude, params_memory):
  if not (-90 <= latitude <= 90 and -180 <= longitude <= 180):
    return False

  angular_radius = 100_000 / EARTH_RADIUS
  latitude_offset = math.degrees(angular_radius)

  min_latitude = max(-90, math.floor((latitude - latitude_offset) / MAPD_ARCHIVE_SIZE_DEGREES) * MAPD_ARCHIVE_SIZE_DEGREES)
  max_latitude = min(90, math.ceil((latitude + latitude_offset) / MAPD_ARCHIVE_SIZE_DEGREES) * MAPD_ARCHIVE_SIZE_DEGREES)

  if abs(latitude) + latitude_offset >= 90:
    min_longitude = -180
    max_longitude = 180
  else:
    longitude_offset = math.degrees(math.asin(math.sin(angular_radius) / math.cos(math.radians(latitude))))
    min_longitude = math.floor((longitude - longitude_offset) / MAPD_ARCHIVE_SIZE_DEGREES) * MAPD_ARCHIVE_SIZE_DEGREES
    max_longitude = math.ceil((longitude + longitude_offset) / MAPD_ARCHIVE_SIZE_DEGREES) * MAPD_ARCHIVE_SIZE_DEGREES

  archive_directories = []
  current_time = time.time()
  download_menu = {"nearby": {}}

  for archive_latitude in range(min_latitude, max_latitude, MAPD_ARCHIVE_SIZE_DEGREES):
    for archive_longitude in range(min_longitude, max_longitude, MAPD_ARCHIVE_SIZE_DEGREES):
      archive_longitude = (archive_longitude + 180) % 360 - 180
      archive_directory = MAPS_PATH / str(archive_latitude) / str(archive_longitude)

      if archive_directory.exists() and current_time - archive_directory.stat().st_mtime <= MAPD_MAX_MAP_AGE_DAYS * 24 * 60 * 60:
        continue

      archive_directories.append(archive_directory)
      download_menu["nearby"][f"{archive_latitude}_{archive_longitude}"] = {
        "full_name": "Nearby Maps",
        "bounding_box": {
          "min_lon": archive_longitude,
          "min_lat": archive_latitude,
          "max_lon": archive_longitude + MAPD_ARCHIVE_SIZE_DEGREES,
          "max_lat": archive_latitude + MAPD_ARCHIVE_SIZE_DEGREES,
        },
      }

  if not download_menu["nearby"]:
    return True

  menu_path = Path(BASEDIR) / "mapd_download_menu.json"
  previous_menu = menu_path.read_bytes() if menu_path.exists() else None

  update_json_file(menu_path, download_menu)

  try:
    downloaded = download_maps(",".join(f"nearby.{archive_name}" for archive_name in download_menu["nearby"]), params_memory)

    if downloaded:
      for archive_directory in archive_directories:
        if archive_directory.is_dir():
          archive_directory.touch()

    return downloaded
  finally:
    if previous_menu is None:
      delete_file(menu_path, report=False)
    else:
      menu_path.write_bytes(previous_menu)


def frogpilot_boot_functions(build_metadata, params):
  params_memory = Params(memory=True)

  migrate_mapd_settings(params)

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
    except (json.JSONDecodeError, TypeError, ValueError):
      pass

  FrogPilotVariables()
  ThemeManager(params, params_memory, boot_run=True).update_active_theme(time_validated=system_time_valid(), frogpilot_toggles=get_frogpilot_toggles(), boot_run=True)

  if use_konik_server():
    if params.get("KonikDongleId") is not None:
      params.put("DongleId", params.get("KonikDongleId"))
    else:
      params.put("KonikDongleId", register(show_spinner=True, register_konik=True))
      params.put("DongleId", params.get("KonikDongleId"))
  elif params.get("DongleId") == params.get("KonikDongleId"):
    params.put("DongleId", params.get("StockDongleId"))

  shutil.rmtree("/data/restore_temp", ignore_errors=True)

  def boot_thread():
    while not system_time_valid():
      print("Waiting for system time to become valid...")
      time.sleep(1)

    backup_frogpilot(build_metadata, params)

  threading.Thread(target=boot_thread, daemon=True).start()


def install_frogpilot(build_metadata, params):
  paths = [
    ERROR_LOGS_PATH,
    HD_LOGS_PATH,
    KONIK_LOGS_PATH,
    SCREEN_RECORDINGS_PATH,
    THEME_SAVE_PATH
  ]
  for path in paths:
    path.mkdir(parents=True, exist_ok=True)

  cleanup_screen_recordings(10 * 1024 * 1024 * 1024)

  register_device(build_metadata, params)

  update_boot_logo(frogpilot=True)


def migrate_mapd_settings(params):
  if params.get("MapdSettings") == {}:
    params.put("MapdSettings", params.get_default_value("MapdSettings"))


def migrate_params(params, params_cache):
  param_types = {
    "MaxLateralAcceleration": dict,
  }

  for key, expected_type in param_types.items():
    for param_store in (params, params_cache):
      value = param_store.get(key)

      if value is not None and not isinstance(value, expected_type):
        param_store.remove(key)


def register_device(build_metadata, params):
  def register_thread():
    while not system_time_valid():
      time.sleep(1)

    api_token = params.get("FrogPilotApiToken") or secrets.token_urlsafe(32)
    payload = {
      "api_token_hash": hashlib.sha256(api_token.encode()).hexdigest(),
      "build_metadata": dataclasses.asdict(build_metadata),
      "device_type": HARDWARE.get_device_type(),
      "os_version": HARDWARE.get_os_version(),
    }

    while True:
      response = frogpilot_api.signed_post("/v1/register", payload)
      if response is not None and response.status_code == 200:
        try:
          frogpilot_dongle_id = response.json().get("frogpilot_dongle_id")
        except (AttributeError, ValueError):
          frogpilot_dongle_id = None

        if isinstance(frogpilot_dongle_id, str) and re.fullmatch(r"[a-z0-9]{16}", frogpilot_dongle_id):
          params.put("FrogPilotApiToken", api_token)
          params.put("FrogPilotDongleId", frogpilot_dongle_id)
          print("Successfully registered device!")
          return

        print("Malformed registration response")
        time.sleep(frogpilot_api.get_retry_delay(response))
        continue

      if response is not None and response.status_code != 429 and response.status_code < 500:
        break

      time.sleep(frogpilot_api.get_retry_delay(response))

    print("Failed to register device")

  threading.Thread(target=register_thread, daemon=True).start()


def run_frogsgomoo(build_metadata):
  if build_metadata.channel == "FrogPilot-Development" and is_FrogsGoMoo():
    mount_options = run_cmd(["findmnt", "-n", "-o", "OPTIONS", "/persist"], "Successfully retrieved mount options", "Failed to retrieve mount options")
    run_cmd(["sudo", "mount", "-o", "remount,rw", "/persist"], "Successfully remounted /persist as read-write", "Failed to remount /persist")
    run_cmd(["sudo", "python3", FROGS_GO_MOO_PATH], "Successfully ran frogsgomoo.py", "Failed to run frogsgomoo.py")
    run_cmd(["sudo", "mount", "-o", f"remount,{mount_options}", "/persist"], "Successfully restored /persist mount options", "Failed to restore /persist mount options")


def uninstall_frogpilot():
  update_boot_logo(stock=True)

  HARDWARE.uninstall()


def update_boot_logo(frogpilot=False, stock=False):
  boot_logo_location = Path("/usr/comma/bg.jpg")

  if frogpilot:
    target_logo = Path(BASEDIR) / "frogpilot/assets/other_images/frogpilot_boot_logo.jpg"
  elif stock:
    target_logo = Path(BASEDIR) / "frogpilot/assets/other_images/stock_bg.jpg"
  else:
    print('Error: Must specify either "frogpilot=True" or "stock=True"')
    return

  if not target_logo.is_file():
    print(f"Error: Target logo file not found at {target_logo}")
    return

  if boot_logo_location.read_bytes() != target_logo.read_bytes():
    mount_options = run_cmd(["findmnt", "-n", "-o", "OPTIONS", "/"], "Successfully retrieved mount options", "Failed to retrieve mount options")
    run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Successfully remounted / as read-write", "Failed to remount /")
    run_cmd(["sudo", "cp", target_logo, boot_logo_location], "Successfully replaced boot logo", "Failed to replace boot logo")
    run_cmd(["sudo", "mount", "-o", f"remount,{mount_options}", "/"], "Successfully restored / mount options", "Failed to restore / mount options")


def update_maps(now, params, params_memory, sm, manual_update=False):
  if not sm["deviceState"].networkMetered:
    gps_location_service = get_gps_location_service(params)
    gps_location = sm[gps_location_service]

    if is_gps_location_valid(gps_location, gps_location_service, sm):
      download_nearby_maps(gps_location.latitude, gps_location.longitude, params_memory)

  if sm["deviceState"].networkMetered and not manual_update:
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
  maps_downloaded = MAPS_PATH.exists() and bool(last_maps_update)

  if maps_downloaded and (schedule == 0 or (schedule == 1 and not is_sunday) or (schedule == 2 and not is_first)) and not manual_update:
    return

  suffix = "th" if 11 <= day <= 13 else {1: "st", 2: "nd", 3: "rd"}.get(day % 10, "th")
  todays_date = now.strftime(f"%B {day}{suffix}, %Y")

  if maps_downloaded and last_maps_update == todays_date and not manual_update:
    return

  if download_maps(maps_selected, params_memory):
    params.put("LastMapsUpdate", todays_date)

  params_memory.remove("DownloadMaps")


def update_openpilot(thread_manager, params):
  def update_available():
    run_cmd(["pkill", "-SIGUSR1", "-f", "system.updated.updated"], "Checking for updates...", "Failed to check for update...", report=False)

    while params.get("UpdaterState") != "checking...":
      time.sleep(1)

    while params.get("UpdaterState") == "checking...":
      time.sleep(1)

    if not params.get_bool("UpdaterFetchAvailable"):
      return False

    while params.get_bool("IsOnroad") or thread_manager.is_thread_alive("lock_doors"):
      time.sleep(60)

    run_cmd(["pkill", "-SIGHUP", "-f", "system.updated.updated"], "Update available, downloading...", "Failed to download update...", report=False)

    while not params.get_bool("UpdateAvailable"):
      time.sleep(60)

    return True

  if params.get("UpdaterState") != "idle":
    return

  while params.get_bool("IsOnroad") or thread_manager.is_thread_alive("lock_doors"):
    time.sleep(60)

  if not update_available():
    return

  while True:
    if not update_available():
      break

  HARDWARE.reboot()
