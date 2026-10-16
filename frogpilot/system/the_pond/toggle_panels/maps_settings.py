import asyncio
import datetime
import time

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables

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


def measure_maps_storage():
  MAPS_STORAGE["size"] = frogpilot_utilities.get_directory_size(frogpilot_variables.MAPS_PATH)


def progress(context):
  download_progress = context.the_pond.sm["mapdExtendedOut"].downloadProgress
  if not download_progress.active:
    return "Calculating..."

  downloaded_files = download_progress.downloadedFiles
  total_files = download_progress.totalFiles
  return f"{downloaded_files} / {total_files} ({downloaded_files * 100 // (total_files or 1)}%)"


def remove_maps_visible(context):
  return idle_visible(context) or state_reason(frogpilot_variables.MAPS_PATH.exists(), "Shown once maps are downloaded")


def state_reason(satisfied, text):
  if satisfied:
    return None
  return {"kind": "state", "text": text}


def storage_used(context):
  if not frogpilot_variables.MAPS_PATH.exists():
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
                    "every Sunday, \"Monthly\" runs on the 1st, and \"Manually\" waits until you press \"DOWNLOAD\" yourself.</b><br><br>There is " +
                    "one exception. Whenever the map data is missing from the device, openpilot starts the download on its own, usually within the " +
                    "hour. Automatic downloads only start on an unmetered connection, such as Wi-Fi, and unlike \"DOWNLOAD\" they do not wait until " +
                    "you park."),
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
    "options": (
      {"label": "Algeria", "value": "nation.DZ"},
      {"label": "Angola", "value": "nation.AO"},
      {"label": "Benin", "value": "nation.BJ"},
      {"label": "Botswana", "value": "nation.BW"},
      {"label": "Burkina Faso", "value": "nation.BF"},
      {"label": "Burundi", "value": "nation.BI"},
      {"label": "Cameroon", "value": "nation.CM"},
      {"label": "Central African Republic", "value": "nation.CF"},
      {"label": "Chad", "value": "nation.TD"},
      {"label": "Congo (Brazzaville)", "value": "nation.CG"},
      {"label": "Congo (Kinshasa)", "value": "nation.CD"},
      {"label": "Djibouti", "value": "nation.DJ"},
      {"label": "Egypt", "value": "nation.EG"},
      {"label": "Equatorial Guinea", "value": "nation.GQ"},
      {"label": "Eritrea", "value": "nation.ER"},
      {"label": "Eswatini", "value": "nation.SZ"},
      {"label": "Ethiopia", "value": "nation.ET"},
      {"label": "Gabon", "value": "nation.GA"},
      {"label": "Gambia", "value": "nation.GM"},
      {"label": "Ghana", "value": "nation.GH"},
      {"label": "Guinea", "value": "nation.GN"},
      {"label": "Guinea-Bissau", "value": "nation.GW"},
      {"label": "Ivory Coast", "value": "nation.CI"},
      {"label": "Kenya", "value": "nation.KE"},
      {"label": "Lesotho", "value": "nation.LS"},
      {"label": "Liberia", "value": "nation.LR"},
      {"label": "Libya", "value": "nation.LY"},
      {"label": "Madagascar", "value": "nation.MG"},
      {"label": "Malawi", "value": "nation.MW"},
      {"label": "Mali", "value": "nation.ML"},
      {"label": "Mauritania", "value": "nation.MR"},
      {"label": "Morocco", "value": "nation.MA"},
      {"label": "Mozambique", "value": "nation.MZ"},
      {"label": "Namibia", "value": "nation.NA"},
      {"label": "Niger", "value": "nation.NE"},
      {"label": "Nigeria", "value": "nation.NG"},
      {"label": "Rwanda", "value": "nation.RW"},
      {"label": "Senegal", "value": "nation.SN"},
      {"label": "Sierra Leone", "value": "nation.SL"},
      {"label": "Somalia", "value": "nation.SO"},
      {"label": "South Africa", "value": "nation.ZA"},
      {"label": "South Sudan", "value": "nation.SS"},
      {"label": "Sudan", "value": "nation.SD"},
      {"label": "Tanzania", "value": "nation.TZ"},
      {"label": "Togo", "value": "nation.TG"},
      {"label": "Tunisia", "value": "nation.TN"},
      {"label": "Uganda", "value": "nation.UG"},
      {"label": "Zambia", "value": "nation.ZM"},
      {"label": "Zimbabwe", "value": "nation.ZW"}
    ),
    "order": 0,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "Africa",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsAntarctica",
    "key": "MapsSelected",
    "options": ({"label": "Antarctica", "value": "nation.AQ"},),
    "order": 1,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "Antarctica",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsAsia",
    "key": "MapsSelected",
    "options": (
      {"label": "Afghanistan", "value": "nation.AF"},
      {"label": "Armenia", "value": "nation.AM"},
      {"label": "Azerbaijan", "value": "nation.AZ"},
      {"label": "Bangladesh", "value": "nation.BD"},
      {"label": "Bhutan", "value": "nation.BT"},
      {"label": "Brunei", "value": "nation.BN"},
      {"label": "Cambodia", "value": "nation.KH"},
      {"label": "China", "value": "nation.CN"},
      {"label": "Cyprus", "value": "nation.CY"},
      {"label": "East Timor", "value": "nation.TL"},
      {"label": "India", "value": "nation.IN"},
      {"label": "Indonesia", "value": "nation.ID"},
      {"label": "Iran", "value": "nation.IR"},
      {"label": "Iraq", "value": "nation.IQ"},
      {"label": "Israel", "value": "nation.IL"},
      {"label": "Japan", "value": "nation.JP"},
      {"label": "Jordan", "value": "nation.JO"},
      {"label": "Kazakhstan", "value": "nation.KZ"},
      {"label": "Kuwait", "value": "nation.KW"},
      {"label": "Kyrgyzstan", "value": "nation.KG"},
      {"label": "Laos", "value": "nation.LA"},
      {"label": "Lebanon", "value": "nation.LB"},
      {"label": "Malaysia", "value": "nation.MY"},
      {"label": "Mongolia", "value": "nation.MN"},
      {"label": "Myanmar", "value": "nation.MM"},
      {"label": "Nepal", "value": "nation.NP"},
      {"label": "North Korea", "value": "nation.KP"},
      {"label": "Oman", "value": "nation.OM"},
      {"label": "Pakistan", "value": "nation.PK"},
      {"label": "Palestine", "value": "nation.PS"},
      {"label": "Philippines", "value": "nation.PH"},
      {"label": "Qatar", "value": "nation.QA"},
      {"label": "Russia", "value": "nation.RU"},
      {"label": "Saudi Arabia", "value": "nation.SA"},
      {"label": "South Korea", "value": "nation.KR"},
      {"label": "Sri Lanka", "value": "nation.LK"},
      {"label": "Syria", "value": "nation.SY"},
      {"label": "Taiwan", "value": "nation.TW"},
      {"label": "Tajikistan", "value": "nation.TJ"},
      {"label": "Thailand", "value": "nation.TH"},
      {"label": "Turkey", "value": "nation.TR"},
      {"label": "Turkmenistan", "value": "nation.TM"},
      {"label": "United Arab Emirates", "value": "nation.AE"},
      {"label": "Uzbekistan", "value": "nation.UZ"},
      {"label": "Vietnam", "value": "nation.VN"},
      {"label": "Yemen", "value": "nation.YE"}
    ),
    "order": 2,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "Asia",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsEurope",
    "key": "MapsSelected",
    "options": (
      {"label": "Albania", "value": "nation.AL"},
      {"label": "Austria", "value": "nation.AT"},
      {"label": "Belarus", "value": "nation.BY"},
      {"label": "Belgium", "value": "nation.BE"},
      {"label": "Bosnia and Herzegovina", "value": "nation.BA"},
      {"label": "Bulgaria", "value": "nation.BG"},
      {"label": "Croatia", "value": "nation.HR"},
      {"label": "Czech Republic", "value": "nation.CZ"},
      {"label": "Denmark", "value": "nation.DK"},
      {"label": "Estonia", "value": "nation.EE"},
      {"label": "Finland", "value": "nation.FI"},
      {"label": "France", "value": "nation.FR"},
      {"label": "Georgia", "value": "nation.GE"},
      {"label": "Germany", "value": "nation.DE"},
      {"label": "Greece", "value": "nation.GR"},
      {"label": "Hungary", "value": "nation.HU"},
      {"label": "Iceland", "value": "nation.IS"},
      {"label": "Ireland", "value": "nation.IE"},
      {"label": "Italy", "value": "nation.IT"},
      {"label": "Latvia", "value": "nation.LV"},
      {"label": "Lithuania", "value": "nation.LT"},
      {"label": "Luxembourg", "value": "nation.LU"},
      {"label": "Moldova", "value": "nation.MD"},
      {"label": "Montenegro", "value": "nation.ME"},
      {"label": "Netherlands", "value": "nation.NL"},
      {"label": "North Macedonia", "value": "nation.MK"},
      {"label": "Norway", "value": "nation.NO"},
      {"label": "Poland", "value": "nation.PL"},
      {"label": "Portugal", "value": "nation.PT"},
      {"label": "Romania", "value": "nation.RO"},
      {"label": "Serbia", "value": "nation.RS"},
      {"label": "Slovakia", "value": "nation.SK"},
      {"label": "Slovenia", "value": "nation.SI"},
      {"label": "Spain", "value": "nation.ES"},
      {"label": "Sweden", "value": "nation.SE"},
      {"label": "Switzerland", "value": "nation.CH"},
      {"label": "Ukraine", "value": "nation.UA"},
      {"label": "United Kingdom", "value": "nation.GB"}
    ),
    "order": 3,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "Europe",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsNorthAmerica",
    "key": "MapsSelected",
    "options": (
      {"label": "Bahamas", "value": "nation.BS"},
      {"label": "Belize", "value": "nation.BZ"},
      {"label": "Canada", "value": "nation.CA"},
      {"label": "Costa Rica", "value": "nation.CR"},
      {"label": "Cuba", "value": "nation.CU"},
      {"label": "Dominican Republic", "value": "nation.DO"},
      {"label": "El Salvador", "value": "nation.SV"},
      {"label": "Greenland", "value": "nation.GL"},
      {"label": "Guatemala", "value": "nation.GT"},
      {"label": "Haiti", "value": "nation.HT"},
      {"label": "Honduras", "value": "nation.HN"},
      {"label": "Jamaica", "value": "nation.JM"},
      {"label": "Mexico", "value": "nation.MX"},
      {"label": "Nicaragua", "value": "nation.NI"},
      {"label": "Panama", "value": "nation.PA"},
      {"label": "Trinidad and Tobago", "value": "nation.TT"},
      {"label": "United States", "value": "nation.US"}
    ),
    "order": 4,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "North America",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsOceania",
    "key": "MapsSelected",
    "options": (
      {"label": "Australia", "value": "nation.AU"},
      {"label": "Fiji", "value": "nation.FJ"},
      {"label": "French Southern Territories", "value": "nation.TF"},
      {"label": "New Caledonia", "value": "nation.NC"},
      {"label": "New Zealand", "value": "nation.NZ"},
      {"label": "Papua New Guinea", "value": "nation.PG"},
      {"label": "Solomon Islands", "value": "nation.SB"},
      {"label": "Vanuatu", "value": "nation.VU"}
    ),
    "order": 5,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "Oceania",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsSouthAmerica",
    "key": "MapsSelected",
    "options": (
      {"label": "Argentina", "value": "nation.AR"},
      {"label": "Bolivia", "value": "nation.BO"},
      {"label": "Brazil", "value": "nation.BR"},
      {"label": "Chile", "value": "nation.CL"},
      {"label": "Colombia", "value": "nation.CO"},
      {"label": "Ecuador", "value": "nation.EC"},
      {"label": "Falkland Islands", "value": "nation.FK"},
      {"label": "Guyana", "value": "nation.GY"},
      {"label": "Paraguay", "value": "nation.PY"},
      {"label": "Peru", "value": "nation.PE"},
      {"label": "Suriname", "value": "nation.SR"},
      {"label": "Uruguay", "value": "nation.UY"},
      {"label": "Venezuela", "value": "nation.VE"}
    ),
    "order": 6,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "COUNTRIES"),
    "title": "South America",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsUnitedStatesMidwest",
    "key": "MapsSelected",
    "options": (
      {"label": "Illinois", "value": "us_state.IL"},
      {"label": "Indiana", "value": "us_state.IN"},
      {"label": "Iowa", "value": "us_state.IA"},
      {"label": "Kansas", "value": "us_state.KS"},
      {"label": "Michigan", "value": "us_state.MI"},
      {"label": "Minnesota", "value": "us_state.MN"},
      {"label": "Missouri", "value": "us_state.MO"},
      {"label": "Nebraska", "value": "us_state.NE"},
      {"label": "North Dakota", "value": "us_state.ND"},
      {"label": "Ohio", "value": "us_state.OH"},
      {"label": "South Dakota", "value": "us_state.SD"},
      {"label": "Wisconsin", "value": "us_state.WI"}
    ),
    "order": 0,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "STATES"),
    "title": "United States - Midwest",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsUnitedStatesNortheast",
    "key": "MapsSelected",
    "options": (
      {"label": "Connecticut", "value": "us_state.CT"},
      {"label": "Maine", "value": "us_state.ME"},
      {"label": "Massachusetts", "value": "us_state.MA"},
      {"label": "New Hampshire", "value": "us_state.NH"},
      {"label": "New Jersey", "value": "us_state.NJ"},
      {"label": "New York", "value": "us_state.NY"},
      {"label": "Pennsylvania", "value": "us_state.PA"},
      {"label": "Rhode Island", "value": "us_state.RI"},
      {"label": "Vermont", "value": "us_state.VT"}
    ),
    "order": 1,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "STATES"),
    "title": "United States - Northeast",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsUnitedStatesSouth",
    "key": "MapsSelected",
    "options": (
      {"label": "Alabama", "value": "us_state.AL"},
      {"label": "Arkansas", "value": "us_state.AR"},
      {"label": "Delaware", "value": "us_state.DE"},
      {"label": "District of Columbia", "value": "us_state.DC"},
      {"label": "Florida", "value": "us_state.FL"},
      {"label": "Georgia", "value": "us_state.GA"},
      {"label": "Kentucky", "value": "us_state.KY"},
      {"label": "Louisiana", "value": "us_state.LA"},
      {"label": "Maryland", "value": "us_state.MD"},
      {"label": "Mississippi", "value": "us_state.MS"},
      {"label": "North Carolina", "value": "us_state.NC"},
      {"label": "Oklahoma", "value": "us_state.OK"},
      {"label": "South Carolina", "value": "us_state.SC"},
      {"label": "Tennessee", "value": "us_state.TN"},
      {"label": "Texas", "value": "us_state.TX"},
      {"label": "Virginia", "value": "us_state.VA"},
      {"label": "West Virginia", "value": "us_state.WV"}
    ),
    "order": 2,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "STATES"),
    "title": "United States - South",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsUnitedStatesWest",
    "key": "MapsSelected",
    "options": (
      {"label": "Alaska", "value": "us_state.AK"},
      {"label": "Arizona", "value": "us_state.AZ"},
      {"label": "California", "value": "us_state.CA"},
      {"label": "Colorado", "value": "us_state.CO"},
      {"label": "Hawaii", "value": "us_state.HI"},
      {"label": "Idaho", "value": "us_state.ID"},
      {"label": "Montana", "value": "us_state.MT"},
      {"label": "Nevada", "value": "us_state.NV"},
      {"label": "New Mexico", "value": "us_state.NM"},
      {"label": "Oregon", "value": "us_state.OR"},
      {"label": "Utah", "value": "us_state.UT"},
      {"label": "Washington", "value": "us_state.WA"},
      {"label": "Wyoming", "value": "us_state.WY"}
    ),
    "order": 3,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "STATES"),
    "title": "United States - West",
    "widget": "multi_select_grid"
  },
  {
    "description": None,
    "id": "MapsUnitedStatesTerritories",
    "key": "MapsSelected",
    "options": (
      {"label": "American Samoa", "value": "us_state.AS"},
      {"label": "Guam", "value": "us_state.GM"},
      {"label": "Northern Mariana Islands", "value": "us_state.MP"},
      {"label": "Puerto Rico", "value": "us_state.PR"},
      {"label": "Virgin Islands", "value": "us_state.VI"}
    ),
    "order": 4,
    "path": ("Maps and Navigation", "SPEED LIMIT MAPS", "STATES"),
    "title": "United States - Territories",
    "widget": "multi_select_grid"
  }
]
