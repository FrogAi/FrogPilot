import asyncio
import datetime
import time

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables

GIGABYTE = 1024 ** 3
MEGABYTE = 1024 ** 2

DOWNLOAD_START = {}
MAPS_STORAGE = {}


def download_start(context):
  sm = context.the_pond.sm
  downloaded_files = sm["mapdExtendedOut"].downloadProgress.downloadedFiles
  request_time = sm["frogpilotProcessState"].downloadMapsRequestTime

  if not is_downloading(context):
    DOWNLOAD_START.clear()
  elif DOWNLOAD_START.get("request_time") != request_time or downloaded_files < DOWNLOAD_START["files"]:
    DOWNLOAD_START.update(files=downloaded_files, request_time=request_time, time=time.monotonic())
  return DOWNLOAD_START


def downloading_visible(context):
  return state_reason(is_downloading(context), "Shown while maps are downloading")


def format_duration(seconds):
  hours, remainder = divmod(int(seconds), 60 * 60)
  minutes, seconds = divmod(remainder, 60)

  parts = []
  if hours > 0:
    parts.append(f"{hours} hour" if hours == 1 else f"{hours} hours")
  if minutes > 0:
    parts.append(f"{minutes} minute" if minutes == 1 else f"{minutes} minutes")
  parts.append(f"{seconds} second" if seconds == 1 else f"{seconds} seconds")
  return " ".join(parts)


def idle_visible(context):
  return state_reason(not is_downloading(context), "Shown while no maps are downloading")


def is_downloading(context):
  sm = context.the_pond.sm
  return sm["mapdExtendedOut"].downloadProgress.active or sm["frogpilotProcessState"].downloadingMaps


def map_options(regions, prefix):
  return tuple({"label": label, "value": f"{prefix}{code}"} for code, label in regions.items())


def measure_maps_storage():
  MAPS_STORAGE["size"] = frogpilot_utilities.get_directory_size(frogpilot_paths.MAPS_PATH)


def progress(context):
  download_progress = context.the_pond.sm["mapdExtendedOut"].downloadProgress
  if not download_progress.active:
    return "Calculating..."

  downloaded_files = download_progress.downloadedFiles
  total_files = download_progress.totalFiles
  return f"{downloaded_files} / {total_files} ({downloaded_files * 100 // (total_files or 1)}%)"


def remove_maps_visible(context):
  return idle_visible(context) or state_reason(frogpilot_paths.MAPS_PATH.exists(), "Shown once maps are downloaded")


def state_reason(satisfied, text):
  if satisfied:
    return None
  return {"kind": "state", "text": text}


def storage_used(context):
  if not frogpilot_paths.MAPS_PATH.exists():
    return "0 MB"

  state = (is_downloading(context), context.params.get("LastMapsUpdate"))
  if MAPS_STORAGE.get("state") != state:
    MAPS_STORAGE["state"] = state
    asyncio.get_running_loop().run_in_executor(None, measure_maps_storage)

  size = MAPS_STORAGE.get("size")
  if size is None:
    return "Calculating..."
  if size >= GIGABYTE:
    return f"{size / GIGABYTE:.2f} GB"
  return f"{size / MEGABYTE:.2f} MB"


def time_elapsed(context):
  start = download_start(context)
  if not context.the_pond.sm["mapdExtendedOut"].downloadProgress.active:
    return "Calculating..."
  return format_duration(time.monotonic() - start["time"])


def time_remaining(context):
  start = download_start(context)
  download_progress = context.the_pond.sm["mapdExtendedOut"].downloadProgress
  if not download_progress.active or download_progress.downloadedFiles <= start["files"]:
    return "Calculating..."

  new_files = download_progress.downloadedFiles - start["files"]
  remaining_files = download_progress.totalFiles - download_progress.downloadedFiles
  remaining = (time.monotonic() - start["time"]) * remaining_files / new_files

  finish = datetime.datetime.now() + datetime.timedelta(seconds=remaining)
  return f"{format_duration(remaining)} ({finish.strftime('%I:%M %p').removeprefix('0')})"


ROWS = [
  {
    "description": ("<b>How often openpilot re-downloads the speed limit map data for the places you picked under \"Map Sources\". \"Weekly\" runs " +
                    "every Sunday, \"Monthly\" runs on the 1st, and \"Manually\" waits until you press \"Download regions\" yourself.</b><br><br>There " +
                    "is one exception. Whenever the map data is missing from the device, openpilot starts the download on its own, usually within the " +
                    "hour. Automatic downloads only start on an unmetered connection, such as Wi-Fi, and unlike \"Download regions\" they do not wait " +
                    "until you park."),
    "key": "PreferredSchedule",
    "options": ({"label": "Manually", "value": 0}, {"label": "Weekly", "value": 1}, {"label": "Monthly", "value": 2}),
    "order": 0,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS"),
    "title": "Automatically Update Maps",
    "widget": "select"
  },
  {
    "description": ("<b>Start downloading the speed limit map data for the places you picked under \"Map Sources\".</b><br><br>Your car has to be " +
                    "parked and online. Large areas can take hours and use several gigabytes."),
    "id": "DownloadMaps",
    "key": None,
    "link": "maps",
    "order": 1,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS"),
    "title": "Download Maps",
    "widget": "action"
  },
  {
    "description": None,
    "display": lambda context: context.params.get("LastMapsUpdate") or "Never",
    "id": "LastUpdated",
    "key": None,
    "order": 2,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS"),
    "title": "Last Updated",
    "visible": idle_visible,
    "widget": "text"
  },
  {
    "children": (("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"), ("Maps and Navigation", "SPEED LIMIT MAPS", "STATES")),
    "description": ("<b>Pick the countries or U.S. states you drive in, so openpilot knows their speed limits.</b><br><br>Map data within about 60 " +
                    "miles (100 km) of where you park downloads on its own, so pick only what covers the rest of your driving."),
    "key": "MapsSelected",
    "link": "maps",
    "options": ({"label": "COUNTRIES", "value": 0}, {"label": "STATES", "value": 1}),
    "order": 3,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS"),
    "title": "Map Sources",
    "widget": "buttons"
  },
  {
    "description": None,
    "display": progress,
    "id": "Progress",
    "key": None,
    "order": 4,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS"),
    "title": "Progress",
    "visible": downloading_visible,
    "widget": "text"
  },
  {
    "description": None,
    "display": time_elapsed,
    "id": "TimeElapsed",
    "key": None,
    "order": 5,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS"),
    "title": "Time Elapsed",
    "visible": downloading_visible,
    "widget": "text"
  },
  {
    "description": None,
    "display": time_remaining,
    "id": "TimeRemaining",
    "key": None,
    "order": 6,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS"),
    "title": "Time Remaining",
    "visible": downloading_visible,
    "widget": "text"
  },
  {
    "description": ("<b>Delete your downloaded map data and clear the places you picked under \"Map Sources\", to free up storage.</b><br><br>Only " +
                    "the map data around where you park comes back on its own, the next time the device is on Wi-Fi. Everywhere else, \"Speed Limit " +
                    "Controller\" has no map speed limits until you pick your places again and start a new download."),
    "id": "RemoveMaps",
    "key": None,
    "link": "maps",
    "order": 7,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS"),
    "title": "Remove Maps",
    "visible": remove_maps_visible,
    "widget": "action"
  },
  {
    "description": None,
    "display": storage_used,
    "id": "StorageUsed",
    "key": None,
    "order": 8,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS"),
    "title": "Storage Used",
    "widget": "text"
  },
  {
    "description": None,
    "id": "MapsAfrica",
    "key": "MapsSelected",
    "options": map_options(frogpilot_variables.MAP_COUNTRIES["Africa"], frogpilot_variables.MAP_COUNTRY_PREFIX),
    "order": 0,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "Africa",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsAntarctica",
    "key": "MapsSelected",
    "options": map_options(frogpilot_variables.MAP_COUNTRIES["Antarctica"], frogpilot_variables.MAP_COUNTRY_PREFIX),
    "order": 1,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "Antarctica",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsAsia",
    "key": "MapsSelected",
    "options": map_options(frogpilot_variables.MAP_COUNTRIES["Asia"], frogpilot_variables.MAP_COUNTRY_PREFIX),
    "order": 2,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "Asia",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsEurope",
    "key": "MapsSelected",
    "options": map_options(frogpilot_variables.MAP_COUNTRIES["Europe"], frogpilot_variables.MAP_COUNTRY_PREFIX),
    "order": 3,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "Europe",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsNorthAmerica",
    "key": "MapsSelected",
    "options": map_options(frogpilot_variables.MAP_COUNTRIES["North America"], frogpilot_variables.MAP_COUNTRY_PREFIX),
    "order": 4,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "North America",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsOceania",
    "key": "MapsSelected",
    "options": map_options(frogpilot_variables.MAP_COUNTRIES["Oceania"], frogpilot_variables.MAP_COUNTRY_PREFIX),
    "order": 5,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "Oceania",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsSouthAmerica",
    "key": "MapsSelected",
    "options": map_options(frogpilot_variables.MAP_COUNTRIES["South America"], frogpilot_variables.MAP_COUNTRY_PREFIX),
    "order": 6,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "South America",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsUnitedStatesMidwest",
    "key": "MapsSelected",
    "options": map_options(frogpilot_variables.MAP_STATES["United States - Midwest"], frogpilot_variables.MAP_STATE_PREFIX),
    "order": 0,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "STATES"),
    "title": "United States - Midwest",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsUnitedStatesNortheast",
    "key": "MapsSelected",
    "options": map_options(frogpilot_variables.MAP_STATES["United States - Northeast"], frogpilot_variables.MAP_STATE_PREFIX),
    "order": 1,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "STATES"),
    "title": "United States - Northeast",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsUnitedStatesSouth",
    "key": "MapsSelected",
    "options": map_options(frogpilot_variables.MAP_STATES["United States - South"], frogpilot_variables.MAP_STATE_PREFIX),
    "order": 2,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "STATES"),
    "title": "United States - South",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsUnitedStatesWest",
    "key": "MapsSelected",
    "options": map_options(frogpilot_variables.MAP_STATES["United States - West"], frogpilot_variables.MAP_STATE_PREFIX),
    "order": 3,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "STATES"),
    "title": "United States - West",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsUnitedStatesTerritories",
    "key": "MapsSelected",
    "options": map_options(frogpilot_variables.MAP_STATES["United States - Territories"], frogpilot_variables.MAP_STATE_PREFIX),
    "order": 4,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "STATES"),
    "title": "United States - Territories",
    "widget": "multi_select_grid"
  }
]
