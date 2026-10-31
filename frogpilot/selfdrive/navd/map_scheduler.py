import json
import time

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_variables

def update_maps(now):
  try:
    maps_selected = json.loads(frogpilot_variables.params.get("MapsSelected", encoding="utf-8") or "{}")
  except json.JSONDecodeError:
    maps_selected = None

  if not isinstance(maps_selected, dict):
    frogpilot_variables.params.remove("MapsSelected")
    frogpilot_variables.params_cache.remove("MapsSelected")
    return

  if not (maps_selected.get("nations") or maps_selected.get("states")):
    return

  now = now.astimezone()

  day = now.day
  is_first = day == 1
  is_sunday = now.weekday() == 6
  schedule = frogpilot_variables.params.get_int("PreferredSchedule")

  last_maps_update = frogpilot_variables.params.get("LastMapsUpdate", encoding="utf-8")
  maps_downloaded = frogpilot_paths.MAPS_PATH.exists() and last_maps_update is not None

  if maps_downloaded and (schedule == 0 or (schedule == 1 and not is_sunday) or (schedule == 2 and not is_first)):
    return

  suffix = "th" if 11 <= day <= 13 else {1: "st", 2: "nd", 3: "rd"}.get(day % 10, "th")
  todays_date = now.strftime(f"%B {day}{suffix}, %Y")

  if maps_downloaded and last_maps_update == todays_date:
    return

  if frogpilot_variables.params.get("OSMDownloadProgress", encoding="utf-8") is not None:
    return

  frogpilot_variables.params_memory.put("OSMDownloadLocations", json.dumps(maps_selected))

  while frogpilot_variables.params_memory.get("OSMDownloadLocations", encoding="utf-8") is not None:
    time.sleep(1)

  download_progress = json.loads(frogpilot_variables.params.get("OSMDownloadProgress", encoding="utf-8") or "{}")

  frogpilot_variables.params.remove("OSMDownloadProgress")

  if download_progress.get("downloaded_files") == download_progress.get("total_files", 0) > 0:
    frogpilot_variables.params.put("LastMapsUpdate", todays_date)

    while frogpilot_variables.params.get_bool("IsOnroad"):
      time.sleep(1)

    frogpilot_variables.params_memory.put_bool("RestartMapd", True)
