#!/usr/bin/env python3
import json
import math
import re

from functools import cache, lru_cache
from types import SimpleNamespace

import cereal.messaging as messaging

from cereal import car, custom, log
from opendbc.car import gen_empty_fingerprint
from opendbc.car.car_helpers import interfaces
from opendbc.car.gm.values import CC_ONLY_CAR
from opendbc.car.honda.values import HONDA_BOSCH
from opendbc.car.hyundai.values import HyundaiFlags
from opendbc.car.interfaces import GearShifter
from opendbc.car.mock.values import CAR as MOCK
from opendbc.car.toyota.values import ToyotaFlags, ToyotaFrogPilotFlags
from openpilot.common.constants import CV
from openpilot.common.params import Params
from openpilot.selfdrive.controls.lib.drive_helpers import MAX_LATERAL_ACCEL_NO_ROLL
from openpilot.selfdrive.controls.lib.latcontrol_torque import KP
from openpilot.selfdrive.locationd import torqued
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.system.hardware.power_monitoring import VBATT_PAUSE_CHARGING
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.common import frogpilot_paths

CITY_SPEED_LIMIT = 25                     # m/s, ~56 mph; at or above this counts as highway driving
CRUISING_SPEED = 5                        # m/s, roughly the speed cars go when not touching the gas while in drive
CURVE_AHEAD_ENTER = 0.6                   # m/s^2, peak lateral acceleration on the planned path that counts as a curve ahead
CURVE_AHEAD_EXIT = 0.4                    # m/s^2, peak lateral acceleration on the planned path below which the curve ahead has ended
DEFAULT_LATERAL_ACCELERATION = 2.0        # m/s^2, typical lateral acceleration when taking curves
DOOR_COMMAND_ATTEMPTS = 10                # Lock or unlock frames sent, one per second, before a door command gives up
EARTH_RADIUS = 6378137                    # Radius of the Earth in meters
LATERAL_DELAY_OFFSET = 0.2                # Seconds added to the steering actuator delay to get the total lateral delay
LEAD_DEPARTURE_SPEED = 1.0                # m/s, leads moving slower than this are treated as stopped
LEAD_STOP_MARGIN = 4                      # Meters past the planned stopping point where a lead still counts as relevant
MAPBOX_MONTHLY_REQUEST_LIMIT = 100_000    # Mapbox's free monthly request allowance
MAPD_ARCHIVE_SIZE_DEGREES = 2             # Size of mapd's downloadable archive cells in degrees
MAPD_MAX_MAP_AGE_DAYS = 28                # Maximum age of downloaded archive cells before refreshing
MAX_ACCELERATION = 4.0                    # ISO 15622:2018
MAX_T_FOLLOW = 3.0                        # Maximum allowed following duration. Larger values risk losing track of the lead but may be increased as models improve
MINIMUM_LATERAL_ACCELERATION = 1.3        # m/s^2, typical minimum lateral acceleration when taking curves
MINIMUM_PLANNED_SPEED = 2.0               # m/s, model path points predicted slower than this are stop approaches, not curves
PLANNER_TIME = ModelConstants.T_IDXS[-1]  # Length of time the model projects out for
READER_CLIP_SHARE = 0.1                   # Share of the speed limit sign reader's new limits recorded as clips, so wrong limits can be found
SIGN_IMAGE_MARGIN = 0.5                   # Share of a sign's width and height added on each side when its image is saved for Speed Limit Filler
STOPPING_SPEED = 1.0                      # m/s, planned speeds below this count as the model coming to a stop
TACO_LAG_TIME = 4                         # Seconds of driving taken off each turn's distance so TACO brakes early enough to reach the turn speed
THRESHOLD = 1 - 1 / math.e                # Requires the condition to be true for ~1 second
UNSURE_SIGN_AGREEMENT = 0.5               # Probability at or above which a sign the speed limit sign reader leaves unposted is recorded as a clip
UPDATE_CLEANUP_RATIO = 0.93               # How long git's cleanup after an update's copy takes relative to the copy, measured on a C3
UPDATE_COPY_AVERAGE_WEIGHT = 0.3          # Share of the way each update's copy time moves the remembered average copy time toward it
UPDATE_COPY_SECONDS = 22                  # Seconds an update's copy takes until a device's first update measures its own, measured on a C3
UPDATE_PROGRESS_INTERVAL = 0.1            # Seconds between update progress checks; the shown percent climbs at most 1% per check so it counts up smoothly

DISABLE_OPENPILOT_LONG_BRANDS = {"ford", "toyota"}
LKAS_BUTTON_BRANDS = {"chrysler", "ford", "gm", "honda", "mazda", "nissan"}
MAP_GEARS_BRANDS = {"gm", "toyota"}

NON_DRIVING_GEARS = [GearShifter.neutral, GearShifter.park, GearShifter.reverse, GearShifter.unknown]

RANDOM_EVENT_END = custom.FrogPilotOnroadEvent.EventName.youveGotMail
RANDOM_EVENT_START = custom.FrogPilotOnroadEvent.EventName.accel30

RESET_REMOVED_KEYS = {
  "CarParamsPersistent",
  "CarParamsPrevRoute",
  "FrogPilotTelemetry",
  "FrogPilotTelemetryConfirmed",
  "GithubSshKeys",
  "GithubUsername",
  "LastGPSPosition",
  "SshEnabled"
}

SECOC_KEY_PATTERN = re.compile(r"[0-9A-Fa-f]{32}")

SECRET_KEYS = {"DiscordUsername", "FavoriteDestinations", "MapboxPublicKey", "MapboxSecretKey", "NavDestination", "SecOCKey", "SecOCKeys", "WeatherToken"}

DEVELOPMENT_BRANCH = "FrogPilot-Development"
PR_BRANCH = "MAKE-PRS-HERE"
RELEASE_BRANCH = "FrogPilot"
STAGING_BRANCH = "FrogPilot-Staging"
TESTING_BRANCH = "FrogPilot-Testing"
VETTING_BRANCH = "FrogPilot-Vetting"

HIDDEN_BRANCHES = {PR_BRANCH, VETTING_BRANCH}
TOP_BRANCHES = ("devel-staging", "devel", "nightly", "nightly-dev", "master")

GIT_PROGRESS_PATTERN = re.compile(r"(?:remote: )?(?P<phase>\w+ \w+): +(?P<percent>\d+)% \(\d+/\d+\)")
GIT_PROGRESS_SUMMARY_PATTERN = re.compile(r"remote: (?:\w+ \w+: \d+, done\.|Total \d+ )")
UPDATER_PROCESS = "system.updated.updated"

ACTION_DELAY_MODELS = {"cd210-model", "pop-model"}
CURRENT_SPEED_STOP_MODELS = {"cd210-model", "pop-model"}
DEFAULT_MODEL = {"id": "dark-souls", "lat_smooth_seconds": 0.1, "name": "Dark Souls"}
LEAD_TRAJECTORY_OUTPUT_SIZE = 144
MIN_STABLE_DELAY_MODELS = {"cd210-model"}
PRE_TOMB_RAIDER_MODELS = {"alabama", "certified-herbalist", "dragon-rider", "duck-amigo", "filet-o-fish", "kerrygold", "last-horizon", "liquid-crystal",
                          "los-angeles", "national-public-radio", "north-america", "north-dakota", "notre-dame", "null-pointer", "postal-service",
                          "recertified-herbalist", "vegan-filet-o-fish", "vegetarian-filet-o-fish", "wd-40"}
SPLIT_MODEL_FILES = ("driving_policy_metadata.pkl", "driving_policy_tinygrad.pkl", "driving_vision_metadata.pkl", "driving_vision_tinygrad.pkl")

SPEED_LIMIT_READER_COMPATIBILITY = 1
SPEED_LIMIT_READER_MODEL_FILES = ("speed_limit_reader.mnn",)

RESOURCES_REPO = "FrogAi/FrogPilot-Resources"

GITHUB_URL = f"https://raw.githubusercontent.com/{RESOURCES_REPO}"
GITLAB_URL = f"https://gitlab.com/{RESOURCES_REPO}/-/raw"
RESOURCE_REPOSITORY_URLS = (GITHUB_URL, GITLAB_URL)

MAP_COUNTRY_PREFIX = "nation."
MAP_STATE_PREFIX = "us_state."

MAPBOX_KEY_MIN_LENGTH = 80
MAPBOX_PUBLIC_KEY_PREFIX = "pk."
MAPBOX_PUBLIC_KEY_TEST_URL = "https://api.mapbox.com/geocoding/v5/mapbox.places/mapbox.json?access_token={}"
MAPBOX_SECRET_KEY_PREFIX = "sk."
MAPBOX_SECRET_KEY_TEST_URL = "https://api.mapbox.com/directions/v5/mapbox/driving/-73.989,40.733;-74,40.733?access_token={}"

REPORT_CRASH_ISSUE = 'I saw an alert that said "openpilot crashed"'
REPORT_OTHER_ISSUE = "Something else (please describe)"
REPORT_UNSURE_ISSUE = "I'm not sure if this is normal or a bug:"
REPORT_DESCRIBED_ISSUES = (REPORT_CRASH_ISSUE, REPORT_OTHER_ISSUE, REPORT_UNSURE_ISSUE)

REPORT_DESCRIPTION_MAX_LENGTH = 300
REPORT_DESCRIPTION_MIN_LENGTH = 10
REPORT_DISCORD_USER_MAX_LENGTH = 64
REPORT_REPOSITORIES = ("frogai/frogpilot", "frogai/frogpilot-development", "frogai/openpilot")

THE_POND_HOSTNAME = "ThePond.local"
THE_POND_PORT = 8082

WEATHER_API_URL = "https://api.openweathermap.org/data/4.0/onecall/current"
WEATHER_KEY_MAX_LENGTH = 32
WEATHER_KEY_TEST_URL = f"{WEATHER_API_URL}?lat=42.4293&lon=-83.9850&appid={{}}"

USER_CREATED_THEME_SUFFIX = "-user_created"

ROUTE_PRESERVE_ATTR_NAME = "user.frogpilot_preserve"
ROUTE_PRESERVED = b"1"

UPLOAD_ATTR_NAME = "user.frogpilot_telemetry"
UPLOAD_DONE = b"done"
UPLOAD_PENDING = b"pending"

VALID_NAME_PATTERN = re.compile(r"[A-Za-z0-9._-]+")

AUTOMATIC_BACKUP_SUFFIX = "_auto"
IN_PROGRESS_BACKUP_SUFFIX = "_in_progress"
SCREEN_RECORDING_COMPANION_SUFFIXES = (".gif", ".png")

ACCELERATION_PROFILES = {
  "STANDARD": 0,
  "ECO": 1,
  "SPORT": 2,
  "SPORT_PLUS": 3
}

BUILTIN_THEMES = {
  "april_fools": "April Fools",
  "christmas": "Christmas",
  "cinco_de_mayo": "Cinco de Mayo",
  "easter": "Easter",
  "fourth_of_july": "Fourth of July",
  "halloween": "Halloween",
  "may_the_fourth": "May the Fourth",
  "new_years": "New Year's",
  "none": "None",
  "st_patricks_day": "St. Patrick's Day",
  "stitch_day": "Stitch Day",
  "stock": "Stock",
  "thanksgiving": "Thanksgiving",
  "valentines_day": "Valentine's Day",
  "world_frog_day": "World Frog Day"
}

BUTTON_FUNCTIONS = {
  "NOTHING": 0,
  "PERSONALITY_PROFILE": 1,
  "FORCE_COAST": 2,
  "PAUSE_LATERAL": 3,
  "PAUSE_LONGITUDINAL": 4,
  "EXPERIMENTAL_MODE": 5,
  "TRAFFIC_MODE": 6
}

BUTTON_FUNCTION_LABELS = {
  0: "No Action",
  1: "Change \"Personality Profile\"",
  2: "Force openpilot to Coast",
  3: "Pause Steering",
  4: "Pause Acceleration/Braking",
  5: "Toggle \"Experimental Mode\" On/Off",
  6: "Toggle \"Traffic Mode\" On/Off"
}

CALIBRATION_KEYS = {"CalibrationParams", "LiveDelay", "LiveParameters", "LiveParametersV2", "LiveTorqueParameters"}

CAR_MAKES = ("Acura", "Audi", "Buick", "Cadillac", "Chevrolet", "Chrysler", "CUPRA", "Dodge", "Ford", "Genesis", "GMC", "Holden", "Honda", "Hyundai", "Jeep",
             "Kia", "Lexus", "Lincoln", "MAN", "Mazda", "Nissan", "Peugeot", "Porsche", "Ram", "Rivian", "SEAT", "Škoda", "Subaru", "Tesla", "Toyota",
             "Volkswagen")

CAR_MAKE_FOLDERS = {
  "acura": "honda",
  "audi": "volkswagen",
  "buick": "gm",
  "cadillac": "gm",
  "chevrolet": "gm",
  "chrysler": "chrysler",
  "cupra": "volkswagen",
  "dodge": "chrysler",
  "ford": "ford",
  "genesis": "hyundai",
  "gmc": "gm",
  "holden": "gm",
  "honda": "honda",
  "hyundai": "hyundai",
  "jeep": "chrysler",
  "kia": "hyundai",
  "lexus": "toyota",
  "lincoln": "ford",
  "man": "volkswagen",
  "mazda": "mazda",
  "nissan": "nissan",
  "peugeot": "psa",
  "porsche": "volkswagen",
  "ram": "chrysler",
  "rivian": "rivian",
  "seat": "volkswagen",
  "škoda": "volkswagen",
  "subaru": "subaru",
  "tesla": "tesla",
  "toyota": "toyota",
  "volkswagen": "volkswagen"
}

CE_STATUS = {
  "OFF": 0,              # Off
  "USER_DISABLED": 1,    # "Experimental Mode" disabled by user
  "USER_OVERRIDDEN": 2,  # "Experimental Mode" enabled by user
  "CURVATURE": 3,        # Road curvature condition
  "LEAD": 4,             # Slower lead vehicle condition
  "SIGNAL": 5,           # Turn signal condition
  "SPEED": 6,            # Speed condition
  "SPEED_LIMIT": 7,      # Speed limit controller condition
  "STOP_LIGHT": 8,       # Stop light or sign condition
}

CURVE_DATA_KEYS = {"CalibrationProgress", "CurvatureData", "MaxLateralAcceleration"}

CURVE_SPEED_PROFILES = {
  "GENTLE": 0,
  "STANDARD": 1,
  "SPORT": 2,
  "AUTO": 3
}

DECELERATION_PROFILES = {
  "STANDARD": 0,
  "ECO": 1,
  "ECO_PLUS": 2
}

DEVELOPER_SIDEBAR_METRICS = {
  "NONE": 0,
  "ACCELERATION_CURRENT": 1,
  "ACCELERATION_MAX": 2,
  "AUTOTUNE_ACTUATOR_DELAY": 3,
  "AUTOTUNE_FRICTION": 4,
  "AUTOTUNE_LATERAL_ACCELERATION": 5,
  "AUTOTUNE_STEER_RATIO": 6,
  "AUTOTUNE_STIFFNESS_FACTOR": 7,
  "ENGAGEMENT_LATERAL": 8,
  "ENGAGEMENT_LONGITUDINAL": 9,
  "LATERAL_STEERING_ANGLE": 10,
  "LATERAL_TORQUE_USED": 11,
  "LONGITUDINAL_ACTUATOR_ACCELERATION": 12,
  "LONGITUDINAL_MPC_JERK_ACCELERATION": 13,
  "LONGITUDINAL_MPC_JERK_DANGER_ZONE": 14,
  "LONGITUDINAL_MPC_JERK_SPEED_CONTROL": 15
}

DEVELOPER_SIDEBAR_METRIC_LABELS = {
  0: "None",
  1: "Acceleration: Current",
  2: "Acceleration: Max",
  3: "Auto Tune: Actuator Delay",
  4: "Auto Tune: Friction",
  5: "Auto Tune: Lateral Acceleration",
  6: "Auto Tune: Steer Ratio",
  7: "Auto Tune: Stiffness Factor",
  8: "Engagement %: Lateral",
  9: "Engagement %: Longitudinal",
  10: "Lateral Control: Steering Angle",
  11: "Lateral Control: Torque % Used",
  12: "Longitudinal Control: Actuator Acceleration Output",
  13: "Longitudinal MPC Jerk: Acceleration",
  14: "Longitudinal MPC Jerk: Danger Zone",
  15: "Longitudinal MPC Jerk: Speed Control"
}

DEVICE_MODELS = {
  "mici": "comma four",
  "pc": "PC",
  "tici": "comma three",
  "tizi": "comma 3X"
}

DEVICE_SHUTDOWN_TIMES = {
  0: 300,      # 5 mins
  1: 900,      # 15 mins
  2: 1800,     # 30 mins
  3: 2700,     # 45 mins
  4: 3600,     # 1 hour
  5: 7200,     # 2 hours
  6: 10800,    # 3 hours
  7: 14400,    # 4 hours
  8: 18000,    # 5 hours
  9: 21600,    # 6 hours
  10: 25200,   # 7 hours
  11: 28800,   # 8 hours
  12: 32400,   # 9 hours
  13: 36000,   # 10 hours
  14: 39600,   # 11 hours
  15: 43200,   # 12 hours
  16: 46800,   # 13 hours
  17: 50400,   # 14 hours
  18: 54000,   # 15 hours
  19: 57600,   # 16 hours
  20: 61200,   # 17 hours
  21: 64800,   # 18 hours
  22: 68400,   # 19 hours
  23: 72000,   # 20 hours
  24: 75600,   # 21 hours
  25: 79200,   # 22 hours
  26: 82800,   # 23 hours
  27: 86400,   # 24 hours
  28: 90000,   # 25 hours
  29: 93600,   # 26 hours
  30: 97200,   # 27 hours
  31: 100800,  # 28 hours
  32: 104400,  # 29 hours
  33: 108000,  # 30 hours
}

DOWNLOADABLE_PARAMS = {
  "colors": "DownloadableColors",
  "distance_icons": "DownloadableDistanceIcons",
  "icons": "DownloadableIcons",
  "signals": "DownloadableSignals",
  "sounds": "DownloadableSounds",
  "steering_wheels": "DownloadableWheels",
}

EXCLUDED_KEYS = {
  "ApiCache_DriveStats",
  "ApiCache_DriveStatsKonik",
  "CalibratedLateralAcceleration",
  "CalibrationProgress",
  "CurvatureData",
  "DoSoftReboot",
  "DownloadableColors",
  "DownloadableDistanceIcons",
  "DownloadableIcons",
  "DownloadableSignals",
  "DownloadableSounds",
  "DownloadableWheels",
  "ForceOffroad",
  "ForceOnroad",
  "FrogPilotApiToken",
  "FrogPilotCarParams",
  "FrogPilotCarParamsPersistent",
  "FrogPilotDongleId",
  "FrogPilotRegistration",
  "FrogPilotStats",
  "FrogPilotTelemetry",
  "FrogPilotTelemetryConfirmed",
  "KonikDongleId",
  "KonikMinutes",
  "LastMapsUpdate",
  "LastUpdateRouteCount",
  "LastUpdateUptimeOnroad",
  "MapBoxRequests",
  "MaxLateralAcceleration",
  "MinimumBackupSize",
  "ModelDrivesAndScores",
  "NNFFModelName",
  "openpilotMinutes",
  "PreviousSpeedLimit",
  "RandomizedModel",
  "RouteCount",
  "StockDongleId",
  "ThemesDownloaded",
  "ThePondPairedBrowsers",
  "ThePondPairing",
  "ThePondPairingCode",
  "Updated",
  "UptimeOffroad",
  "UptimeOnroad"
}

FROGPILOT_STATS_TYPES = {
  "AEBEvents": "count",
  "AOLTime": "timePercent",
  "CruiseSpeedTimes": "speed",
  "CurrentMonthsMeters": "distance",
  "DayTime": "timePercent",
  "Disengages": "count",
  "Engages": "count",
  "ExperimentalModeTime": "timePercent",
  "FrogChirps": "count",
  "FrogHops": "count",
  "FrogPilotDrives": "count",
  "FrogPilotMeters": "distance",
  "FrogPilotSeconds": "time",
  "FrogSqueaks": "count",
  "GoatScreams": "count",
  "LateralTime": "timePercent",
  "LongestDistanceWithoutOverride": "distance",
  "LongitudinalTime": "timePercent",
  "MaxAcceleration": "accel",
  "ModelTimes": "parent",
  "NightTime": "timePercent",
  "Overrides": "count",
  "OverrideTime": "timePercent",
  "PersonalityTimes": "parent",
  "RandomEvents": "parent",
  "StandstillTime": "timePercent",
  "StopLightTime": "timePercent",
  "TrackedTime": "time",
  "WeatherTimes": "parent"
}

HOLIDAY_GREETINGS = {
  "april_fools": "Happy April Fool's Day! 🤡",
  "christmas_week": "Merry Christmas! 🎄",
  "cinco_de_mayo": "¡Feliz Cinco de Mayo! 🌮",
  "easter_week": "Happy Easter! 🐰",
  "fourth_of_july": "Happy Fourth of July! 🎆",
  "halloween_week": "Happy Halloween! 🎃",
  "may_the_fourth": "May the 4th be with you! 🚀",
  "new_years": "Happy New Year! 🎉",
  "st_patricks_day": "Happy St. Patrick's Day! 🍀",
  "stitch_day": "Happy Stitch Day! 💙",
  "thanksgiving_week": "Happy Thanksgiving! 🦃",
  "valentines_day": "Happy Valentine's Day! ❤️",
  "world_frog_day": "Happy World Frog Day! 🐸"
}

MAP_COUNTRIES = {
  "Africa": {
    "DZ": "Algeria",
    "AO": "Angola",
    "BJ": "Benin",
    "BW": "Botswana",
    "BF": "Burkina Faso",
    "BI": "Burundi",
    "CM": "Cameroon",
    "CF": "Central African Republic",
    "TD": "Chad",
    "CG": "Congo (Brazzaville)",
    "CD": "Congo (Kinshasa)",
    "DJ": "Djibouti",
    "EG": "Egypt",
    "GQ": "Equatorial Guinea",
    "ER": "Eritrea",
    "SZ": "Eswatini",
    "ET": "Ethiopia",
    "GA": "Gabon",
    "GM": "Gambia",
    "GH": "Ghana",
    "GN": "Guinea",
    "GW": "Guinea-Bissau",
    "CI": "Ivory Coast",
    "KE": "Kenya",
    "LS": "Lesotho",
    "LR": "Liberia",
    "LY": "Libya",
    "MG": "Madagascar",
    "MW": "Malawi",
    "ML": "Mali",
    "MR": "Mauritania",
    "MA": "Morocco",
    "MZ": "Mozambique",
    "NA": "Namibia",
    "NE": "Niger",
    "NG": "Nigeria",
    "RW": "Rwanda",
    "SN": "Senegal",
    "SL": "Sierra Leone",
    "SO": "Somalia",
    "ZA": "South Africa",
    "SS": "South Sudan",
    "SD": "Sudan",
    "TZ": "Tanzania",
    "TG": "Togo",
    "TN": "Tunisia",
    "UG": "Uganda",
    "ZM": "Zambia",
    "ZW": "Zimbabwe"
  },
  "Antarctica": {
    "AQ": "Antarctica"
  },
  "Asia": {
    "AF": "Afghanistan",
    "AM": "Armenia",
    "AZ": "Azerbaijan",
    "BD": "Bangladesh",
    "BT": "Bhutan",
    "BN": "Brunei",
    "KH": "Cambodia",
    "CN": "China",
    "CY": "Cyprus",
    "TL": "East Timor",
    "IN": "India",
    "ID": "Indonesia",
    "IR": "Iran",
    "IQ": "Iraq",
    "IL": "Israel",
    "JP": "Japan",
    "JO": "Jordan",
    "KZ": "Kazakhstan",
    "KW": "Kuwait",
    "KG": "Kyrgyzstan",
    "LA": "Laos",
    "LB": "Lebanon",
    "MY": "Malaysia",
    "MN": "Mongolia",
    "MM": "Myanmar",
    "NP": "Nepal",
    "KP": "North Korea",
    "OM": "Oman",
    "PK": "Pakistan",
    "PS": "Palestine",
    "PH": "Philippines",
    "QA": "Qatar",
    "RU": "Russia",
    "SA": "Saudi Arabia",
    "KR": "South Korea",
    "LK": "Sri Lanka",
    "SY": "Syria",
    "TW": "Taiwan",
    "TJ": "Tajikistan",
    "TH": "Thailand",
    "TR": "Turkey",
    "TM": "Turkmenistan",
    "AE": "United Arab Emirates",
    "UZ": "Uzbekistan",
    "VN": "Vietnam",
    "YE": "Yemen"
  },
  "Europe": {
    "AL": "Albania",
    "AT": "Austria",
    "BY": "Belarus",
    "BE": "Belgium",
    "BA": "Bosnia and Herzegovina",
    "BG": "Bulgaria",
    "HR": "Croatia",
    "CZ": "Czech Republic",
    "DK": "Denmark",
    "EE": "Estonia",
    "FI": "Finland",
    "FR": "France",
    "GE": "Georgia",
    "DE": "Germany",
    "GR": "Greece",
    "HU": "Hungary",
    "IS": "Iceland",
    "IE": "Ireland",
    "IT": "Italy",
    "LV": "Latvia",
    "LT": "Lithuania",
    "LU": "Luxembourg",
    "MD": "Moldova",
    "ME": "Montenegro",
    "NL": "Netherlands",
    "MK": "North Macedonia",
    "NO": "Norway",
    "PL": "Poland",
    "PT": "Portugal",
    "RO": "Romania",
    "RS": "Serbia",
    "SK": "Slovakia",
    "SI": "Slovenia",
    "ES": "Spain",
    "SE": "Sweden",
    "CH": "Switzerland",
    "UA": "Ukraine",
    "GB": "United Kingdom"
  },
  "North America": {
    "BS": "Bahamas",
    "BZ": "Belize",
    "CA": "Canada",
    "CR": "Costa Rica",
    "CU": "Cuba",
    "DO": "Dominican Republic",
    "SV": "El Salvador",
    "GL": "Greenland",
    "GT": "Guatemala",
    "HT": "Haiti",
    "HN": "Honduras",
    "JM": "Jamaica",
    "MX": "Mexico",
    "NI": "Nicaragua",
    "PA": "Panama",
    "TT": "Trinidad and Tobago",
    "US": "United States"
  },
  "Oceania": {
    "AU": "Australia",
    "FJ": "Fiji",
    "TF": "French Southern Territories",
    "NC": "New Caledonia",
    "NZ": "New Zealand",
    "PG": "Papua New Guinea",
    "SB": "Solomon Islands",
    "VU": "Vanuatu"
  },
  "South America": {
    "AR": "Argentina",
    "BO": "Bolivia",
    "BR": "Brazil",
    "CL": "Chile",
    "CO": "Colombia",
    "EC": "Ecuador",
    "FK": "Falkland Islands",
    "GY": "Guyana",
    "PY": "Paraguay",
    "PE": "Peru",
    "SR": "Suriname",
    "UY": "Uruguay",
    "VE": "Venezuela"
  }
}

MAP_STATES = {
  "United States - Midwest": {
    "IL": "Illinois",
    "IN": "Indiana",
    "IA": "Iowa",
    "KS": "Kansas",
    "MI": "Michigan",
    "MN": "Minnesota",
    "MO": "Missouri",
    "NE": "Nebraska",
    "ND": "North Dakota",
    "OH": "Ohio",
    "SD": "South Dakota",
    "WI": "Wisconsin"
  },
  "United States - Northeast": {
    "CT": "Connecticut",
    "ME": "Maine",
    "MA": "Massachusetts",
    "NH": "New Hampshire",
    "NJ": "New Jersey",
    "NY": "New York",
    "PA": "Pennsylvania",
    "RI": "Rhode Island",
    "VT": "Vermont"
  },
  "United States - South": {
    "AL": "Alabama",
    "AR": "Arkansas",
    "DE": "Delaware",
    "DC": "District of Columbia",
    "FL": "Florida",
    "GA": "Georgia",
    "KY": "Kentucky",
    "LA": "Louisiana",
    "MD": "Maryland",
    "MS": "Mississippi",
    "NC": "North Carolina",
    "OK": "Oklahoma",
    "SC": "South Carolina",
    "TN": "Tennessee",
    "TX": "Texas",
    "VA": "Virginia",
    "WV": "West Virginia"
  },
  "United States - West": {
    "AK": "Alaska",
    "AZ": "Arizona",
    "CA": "California",
    "CO": "Colorado",
    "HI": "Hawaii",
    "ID": "Idaho",
    "MT": "Montana",
    "NV": "Nevada",
    "NM": "New Mexico",
    "OR": "Oregon",
    "UT": "Utah",
    "WA": "Washington",
    "WY": "Wyoming"
  },
  "United States - Territories": {
    "AS": "American Samoa",
    "GM": "Guam",
    "MP": "Northern Mariana Islands",
    "PR": "Puerto Rico",
    "VI": "Virgin Islands"
  }
}

METRIC_CONVERSIONS = {
  "CESignalSpeed": (CV.MPH_TO_KPH, 0),
  "CESpeed": (CV.MPH_TO_KPH, 0),
  "CESpeedLead": (CV.MPH_TO_KPH, 0),
  "IncreasedStoppedDistance": (CV.FOOT_TO_METER, None),
  "IncreasedStoppedDistanceLowVisibility": (CV.FOOT_TO_METER, None),
  "IncreasedStoppedDistanceRain": (CV.FOOT_TO_METER, None),
  "IncreasedStoppedDistanceRainStorm": (CV.FOOT_TO_METER, None),
  "IncreasedStoppedDistanceSnow": (CV.FOOT_TO_METER, None),
  "LaneDetectionWidth": (CV.FOOT_TO_METER, None),
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
  "PauseAOLOnBrake": (CV.MPH_TO_KPH, 0),
  "PauseLateralSpeed": (CV.MPH_TO_KPH, 0),
  "RoadEdgesWidth": (CV.INCH_TO_CM, 0),
  "SetSpeedOffset": (CV.MPH_TO_KPH, 0)
}

REPORT_ISSUES = (
  "Acceleration feels harsh or jerky",
  "An alert was unclear and I didn't know what it meant",
  "Braking is too sudden or uncomfortable",
  REPORT_UNSURE_ISSUE,
  "My screen froze or is stuck loading something",
  "My steering wheel buttons aren't working",
  "openpilot disengages when I don't expect it",
  "openpilot doesn't react to stopped vehicles ahead",
  "openpilot doesn't resume from a stop",
  "openpilot feels sluggish or slow to respond",
  "Steering feels twitchy or unnatural",
  "The car doesn't follow curves well",
  "The car isn't staying centered in its lane",
  REPORT_OTHER_ISSUE
)

SETTING_LABELS = {
  "AccelerationPath": {"name": "Acceleration Path"},
  "AccelerationProfile": {"name": "Acceleration Profile", "values": {0: "Standard", 1: "Eco", 2: "Sport", 3: "Sport+"}},
  "AdbEnabled": {"name": "Enable ADB"},
  "AdjacentLeadsUI": {"name": "Adjacent Leads Tracking"},
  "AdjacentPath": {"name": "Adjacent Lanes"},
  "AdjacentPathMetrics": {"name": "Adjacent Lane Metrics"},
  "AdvancedCustomUI": {"name": "Advanced UI Controls"},
  "AdvancedLateralTune": {"name": "Advanced Lateral Tuning"},
  "AdvancedLongitudinalTune": {"name": "Advanced Longitudinal Tuning"},
  "AggressiveFollow": {"decimals": 2, "name": "Aggressive Personality: Following Distance", "unit": " seconds", "values": {1: "1.00 second"}},
  "AggressiveJerkAcceleration": {"name": "Aggressive Personality: Acceleration Smoothness", "unit": "%"},
  "AggressiveJerkDanger": {"name": "Aggressive Personality: Safety Gap Bias", "unit": "%"},
  "AggressiveJerkDeceleration": {"name": "Aggressive Personality: Braking Smoothness", "unit": "%"},
  "AggressiveJerkSpeed": {"name": "Aggressive Personality: Speed-Up Response", "unit": "%"},
  "AggressiveJerkSpeedDecrease": {"name": "Aggressive Personality: Slowdown Response", "unit": "%"},
  "AggressivePersonalityProfile": {"name": "Aggressive Personality"},
  "AlertVolumeControl": {"name": "Alert Volumes"},
  "AlphaLongitudinalEnabled": {"name": "openpilot Longitudinal Control (Alpha)"},
  "AlwaysOnDM": {"name": "Always-On Driver Monitoring"},
  "AlwaysOnLateral": {"name": "Always On Lateral"},
  "AlwaysOnLateralLKAS": {"name": "Always On Lateral: Enable With LKAS"},
  "AutomaticallyDownloadModels": {"name": "Automatically Download New Models"},
  "AutomaticUpdates": {"name": "Automatically Update FrogPilot"},
  "BlacklistedModels": {"name": "Model Blacklist"},
  "BlindSpotMetrics": {"name": "Border Metrics (Blind Spot)"},
  "BlindSpotPath": {"name": "Blind Spot Path"},
  "BorderMetrics": {"name": "Border Metrics"},
  "CameraView": {"name": "Camera View", "values": {0: "Auto", 1: "Driver", 2: "Standard", 3: "Wide"}},
  "CarMake": {"name": "Car Make"},
  "CarModel": {"name": "Car Model"},
  "CarModelName": {"name": "Car Model Name"},
  "CECurves": {"name": "Conditional Experimental Mode: Curve Detected Ahead"},
  "CECurvesLead": {"name": "Conditional Experimental Mode: Curve Detected Ahead (With Lead)"},
  "CELead": {"name": "Conditional Experimental Mode: Lead Detected Ahead"},
  "CESignalLaneDetection": {"name": "Conditional Experimental Mode: Turn Signal Below (Not For Detected Lanes)"},
  "CESignalSpeed": {"name": "Conditional Experimental Mode: Turn Signal Below", "unit": (" mph", " km/h"), "values": {0: "Off"}},
  "CESlowerLead": {"name": "Conditional Experimental Mode: Lead Detected Ahead (Slower Lead)"},
  "CESpeed": {"name": "Conditional Experimental Mode: Below", "unit": (" mph", " km/h"), "values": {0: "Off"}},
  "CESpeedLead": {"name": "Conditional Experimental Mode: Below (With Lead)", "unit": (" mph", " km/h"), "values": {0: "Off"}},
  "CEStopLights": {"name": "Conditional Experimental Mode: \"Detected\" Stop Lights/Signs"},
  "CEStoppedLead": {"name": "Conditional Experimental Mode: Lead Detected Ahead (Stopped Lead)"},
  "ClusterOffset": {"decimals": 3, "name": "Toyota/Lexus Settings: Dashboard Speed Offset", "unit": "x"},
  "ColorScheme": {"name": "Color Scheme"},
  "Compass": {"name": "Compass"},
  "ConditionalExperimental": {"name": "Conditional Experimental Mode"},
  "CurveSpeedController": {"name": "Curve Speed Controller"},
  "CurveSpeedProfile": {"name": "Curve Speed Profile", "values": {0: "Gentle", 1: "Standard", 2: "Sport", 3: "Adaptive"}},
  "CustomAlerts": {"name": "Extra Alerts"},
  "CustomCruise": {"name": "Cruise Interval", "unit": (" mph", " km/h"), "values": {0: "Off"}},
  "CustomCruiseLong": {"name": "Cruise Interval (Hold)", "unit": (" mph", " km/h"), "values": {0: "Off"}},
  "CustomPersonalities": {"name": "Driving Personalities"},
  "CustomThemes": {"name": "Custom Themes"},
  "CustomUI": {"name": "Driving Screen Widgets"},
  "DebugMode": {"name": "Debug Mode"},
  "DecelerationProfile": {"name": "Deceleration Profile", "values": {0: "Standard", 1: "Eco", 2: "Eco+"}},
  "DeveloperMetrics": {"name": "Developer Metrics"},
  "DeveloperSidebar": {"name": "Developer Sidebar"},
  "DeveloperSidebarMetric1": {"name": "Developer Sidebar: Metric #1", "values": DEVELOPER_SIDEBAR_METRIC_LABELS},
  "DeveloperSidebarMetric2": {"name": "Developer Sidebar: Metric #2", "values": DEVELOPER_SIDEBAR_METRIC_LABELS},
  "DeveloperSidebarMetric3": {"name": "Developer Sidebar: Metric #3", "values": DEVELOPER_SIDEBAR_METRIC_LABELS},
  "DeveloperSidebarMetric4": {"name": "Developer Sidebar: Metric #4", "values": DEVELOPER_SIDEBAR_METRIC_LABELS},
  "DeveloperSidebarMetric5": {"name": "Developer Sidebar: Metric #5", "values": DEVELOPER_SIDEBAR_METRIC_LABELS},
  "DeveloperSidebarMetric6": {"name": "Developer Sidebar: Metric #6", "values": DEVELOPER_SIDEBAR_METRIC_LABELS},
  "DeveloperSidebarMetric7": {"name": "Developer Sidebar: Metric #7", "values": DEVELOPER_SIDEBAR_METRIC_LABELS},
  "DeveloperUI": {"name": "Developer UI"},
  "DeveloperWidgets": {"name": "Developer Widgets"},
  "DeviceManagement": {"name": "Device Settings"},
  "DeviceShutdown": {
    "name": "Device Shutdown Timer",
    "values": {
      0: "5 mins",
      1: "15 mins",
      2: "30 mins",
      3: "45 mins",
      4: "1 hour",
      5: "2 hours",
      6: "3 hours",
      7: "4 hours",
      8: "5 hours",
      9: "6 hours",
      10: "7 hours",
      11: "8 hours",
      12: "9 hours",
      13: "10 hours",
      14: "11 hours",
      15: "12 hours",
      16: "13 hours",
      17: "14 hours",
      18: "15 hours",
      19: "16 hours",
      20: "17 hours",
      21: "18 hours",
      22: "19 hours",
      23: "20 hours",
      24: "21 hours",
      25: "22 hours",
      26: "23 hours",
      27: "24 hours",
      28: "25 hours",
      29: "26 hours",
      30: "27 hours",
      31: "28 hours",
      32: "29 hours",
      33: "30 hours"
    }
  },
  "DisableOnroadUploads": {"name": "Disable Uploads (Disable Onroad Only)"},
  "DisableOpenpilotLongitudinal": {"name": "Disable openpilot Longitudinal Control"},
  "DiscordUsername": {"name": "Discord Username"},
  "DisengageOnAccelerator": {"name": "Disengage on Accelerator Pedal"},
  "DisengageVolume": {"name": "Disengage Volume", "unit": "%", "values": {0: "Muted", 101: "Auto"}},
  "DistanceButtonControl": {"name": "Distance Button", "values": BUTTON_FUNCTION_LABELS},
  "DistanceIconPack": {"name": "Custom Themes: Personality Button"},
  "DriverCamera": {"name": "Show Driver Camera When In Reverse"},
  "DrivingModel": {"name": "Driving Model"},
  "DynamicPathWidth": {"name": "Dynamic Path Width"},
  "DynamicPedalsOnUI": {"name": "Gas / Brake Pedal Indicators (Dynamic)"},
  "EngageVolume": {"name": "Engage Volume", "unit": "%", "values": {0: "Muted", 101: "Auto"}},
  "ExperimentalMode": {"name": "Experimental Mode"},
  "Fahrenheit": {"name": "Numerical Temperature Gauge (Fahrenheit)"},
  "ForceAutoTune": {"name": "Force Auto-Tune On"},
  "ForceAutoTuneOff": {"name": "Force Auto-Tune Off"},
  "ForceFingerprint": {"name": "Disable Automatic Fingerprint Detection"},
  "ForceStops": {"name": "Force Stop at \"Detected\" Stop Lights/Signs"},
  "ForceTorqueController": {"name": "Force Torque Controller"},
  "FPSCounter": {"name": "FPS Display"},
  "FrogsGoMoosTweak": {"name": "Toyota/Lexus Settings: FrogsGoMoo's Personal Tweaks"},
  "GoatScream": {"name": "Goat Scream"},
  "GreenLightAlert": {"name": "Green Light Alert"},
  "GsmApn": {"name": "APN Setting"},
  "GsmMetered": {"name": "Cellular Metered"},
  "GsmRoaming": {"name": "Enable Roaming"},
  "HideAlerts": {"name": "Hide Non-Critical Alerts"},
  "HideLeadMarker": {"name": "Hide Lead Marker"},
  "HideMaxSpeed": {"name": "Hide Max Speed"},
  "HideSpeed": {"name": "Hide Current Speed"},
  "HideSpeedLimit": {"name": "Hide Speed Limits"},
  "HigherBitrate": {"name": "High-Quality Recording"},
  "HolidayThemes": {"name": "Holiday Themes"},
  "HondaAltTune": {"name": "Acura/Honda Settings: Gentle Following"},
  "HondaLowSpeedPedal": {"name": "Acura/Honda Settings: Responsive Pedal at Low Speeds"},
  "HondaMaxBrake": {"name": "Acura/Honda Settings: Increased Braking Force"},
  "HumanAcceleration": {"name": "Human-Like Acceleration"},
  "HumanFollowing": {"name": "Human-Like Following"},
  "HumanLaneChanges": {"name": "Human-Like Lane Changes"},
  "IconPack": {"name": "Icon Pack"},
  "IncreasedStoppedDistance": {
    "decimals": 1,
    "name": "Increase Stopped Distance by",
    "unit": (" feet", " meters"),
    "values": ({0: "Off", 1: "1.0 foot"}, {0: "Off", 1: "1.0 meter"})
  },
  "IncreasedStoppedDistanceLowVisibility": {
    "decimals": 1,
    "name": "Low Visibility: Increase Stopped Distance by",
    "unit": (" feet", " meters"),
    "values": ({0: "Off", 1: "1.0 foot"}, {0: "Off", 1: "1.0 meter"})
  },
  "IncreasedStoppedDistanceRain": {
    "decimals": 1,
    "name": "Rain: Increase Stopped Distance by",
    "unit": (" feet", " meters"),
    "values": ({0: "Off", 1: "1.0 foot"}, {0: "Off", 1: "1.0 meter"})
  },
  "IncreasedStoppedDistanceRainStorm": {
    "decimals": 1,
    "name": "Rainstorms: Increase Stopped Distance by",
    "unit": (" feet", " meters"),
    "values": ({0: "Off", 1: "1.0 foot"}, {0: "Off", 1: "1.0 meter"})
  },
  "IncreasedStoppedDistanceSnow": {
    "decimals": 1,
    "name": "Snow: Increase Stopped Distance by",
    "unit": (" feet", " meters"),
    "values": ({0: "Off", 1: "1.0 foot"}, {0: "Off", 1: "1.0 meter"})
  },
  "IncreaseFollowingLowVisibility": {"decimals": 2, "name": "Low Visibility: Increase Following Distance by", "unit": " seconds", "values": {1: "1.00 second"}},
  "IncreaseFollowingRain": {"decimals": 2, "name": "Rain: Increase Following Distance by", "unit": " seconds", "values": {1: "1.00 second"}},
  "IncreaseFollowingRainStorm": {"decimals": 2, "name": "Rainstorms: Increase Following Distance by", "unit": " seconds", "values": {1: "1.00 second"}},
  "IncreaseFollowingSnow": {"decimals": 2, "name": "Snow: Increase Following Distance by", "unit": " seconds", "values": {1: "1.00 second"}},
  "IncreaseThermalLimits": {"name": "Raise Temperature Limits"},
  "InstantReplay": {
    "name": "Capture Recent Footage",
    "values": {
      0: "Off",
      30: "30 seconds",
      60: "1 minute",
      90: "1.5 minutes",
      120: "2 minutes",
      150: "2.5 minutes",
      180: "3 minutes",
      210: "3.5 minutes",
      240: "4 minutes",
      270: "4.5 minutes",
      300: "5 minutes"
    }
  },
  "IsLdwEnabled": {"name": "Enable Lane Departure Warnings"},
  "IsMetric": {"name": "Use Metric System"},
  "JoystickDebugMode": {"name": "Joystick Debug Mode"},
  "LaneChanges": {"name": "Lane Changes"},
  "LaneChangeTime": {"decimals": 1, "name": "Lane Change Delay", "unit": " seconds", "values": {0: "Instant", 1: "1.0 second"}},
  "LaneDetectionWidth": {
    "decimals": 1,
    "name": "Minimum Lane Width",
    "unit": (" feet", " meters"),
    "values": ({0: "Off", 1: "1.0 foot"}, {0: "Off", 1: "1.0 meter"})
  },
  "LaneLinesWidth": {"name": "Lane Lines Width", "unit": (" inches", " cm"), "values": ({0: "Off", 1: "1 inch"}, {0: "Off"})},
  "LanguageSetting": {"name": "Change Language"},
  "LateralTune": {"name": "Lateral Tuning"},
  "LeadDepartingAlert": {"name": "Lead Departing Alert"},
  "LeadDetectionThreshold": {"name": "Lead Detection Sensitivity", "unit": "%"},
  "LeadInfo": {"name": "Lead Info"},
  "LKASButtonControl": {"name": "LKAS Button", "values": BUTTON_FUNCTION_LABELS},
  "LockDoors": {"name": "Toyota/Lexus Settings: Automatically Lock/Unlock Doors (Lock)"},
  "LockDoorsTimer": {"name": "Toyota/Lexus Settings: Lock Doors On Ignition Off After", "unit": " seconds", "values": {0: "Never"}},
  "LongDistanceButtonControl": {"name": "Distance Button (Long Press)", "values": BUTTON_FUNCTION_LABELS},
  "LongitudinalActuatorDelay": {"decimals": 2, "name": "Advanced Longitudinal Tuning: Actuator Delay", "unit": " seconds", "values": {1: "1.00 second"}},
  "LongitudinalManeuverMode": {"name": "Longitudinal Maneuver Mode"},
  "LongitudinalPersonality": {"name": "Driving Personality", "values": {0: "Aggressive", 1: "Standard", 2: "Relaxed"}},
  "LongitudinalTune": {"name": "Acceleration and Braking"},
  "LongPitch": {"name": "General Motors Settings: Smooth Pedal Response on Hills"},
  "LoudBlindspotAlert": {"name": "Louder Blind Spot Alert"},
  "LowVoltageShutdown": {"decimals": 1, "name": "Low-Voltage Cutoff", "unit": " volts"},
  "MapAcceleration": {"name": "Map Accel/Decel to Gears (Acceleration)"},
  "MapboxPublicKey": {"name": "Public Mapbox Key"},
  "MapboxSecretKey": {"name": "Secret Mapbox Key"},
  "MapDeceleration": {"name": "Map Accel/Decel to Gears (Deceleration)"},
  "MapGears": {"name": "Map Accel/Decel to Gears"},
  "MapsSelected": {"name": "Map Sources"},
  "MaxDesiredAcceleration": {"decimals": 1, "name": "Advanced Longitudinal Tuning: Maximum Acceleration", "unit": " m/s²"},
  "MinimumLaneChangeSpeed": {"name": "Minimum Lane Change Speed", "unit": (" mph", " km/h"), "values": {0: "Any speed"}},
  "ModelRandomizer": {"name": "Model Randomizer"},
  "ModelUI": {"name": "Model UI"},
  "NavigationUI": {"name": "Navigation Widgets"},
  "NNFF": {"name": "Neural Network Feedforward (NNFF)"},
  "NNFFLite": {"name": "Neural Network Feedforward (NNFF) Lite"},
  "NoLogging": {"name": "Disable Logging"},
  "NoUploads": {"name": "Disable Uploads"},
  "NudgelessLaneChange": {"name": "Automatic Lane Changes"},
  "NumericalTemp": {"name": "Numerical Temperature Gauge"},
  "Offset1": {
    "name": ("Speed Limit Offsets: Speed Offset (0–24 mph)", "Speed Limit Offsets: Speed Offset (0–29 km/h)"),
    "unit": (" mph", " km/h"),
    "values": {0: "Off"}
  },
  "Offset2": {
    "name": ("Speed Limit Offsets: Speed Offset (25–34 mph)", "Speed Limit Offsets: Speed Offset (30–49 km/h)"),
    "unit": (" mph", " km/h"),
    "values": {0: "Off"}
  },
  "Offset3": {
    "name": ("Speed Limit Offsets: Speed Offset (35–44 mph)", "Speed Limit Offsets: Speed Offset (50–59 km/h)"),
    "unit": (" mph", " km/h"),
    "values": {0: "Off"}
  },
  "Offset4": {
    "name": ("Speed Limit Offsets: Speed Offset (45–54 mph)", "Speed Limit Offsets: Speed Offset (60–79 km/h)"),
    "unit": (" mph", " km/h"),
    "values": {0: "Off"}
  },
  "Offset5": {
    "name": ("Speed Limit Offsets: Speed Offset (55–64 mph)", "Speed Limit Offsets: Speed Offset (80–99 km/h)"),
    "unit": (" mph", " km/h"),
    "values": {0: "Off"}
  },
  "Offset6": {
    "name": ("Speed Limit Offsets: Speed Offset (65–74 mph)", "Speed Limit Offsets: Speed Offset (100–119 km/h)"),
    "unit": (" mph", " km/h"),
    "values": {0: "Off"}
  },
  "Offset7": {
    "name": ("Speed Limit Offsets: Speed Offset (75–99 mph)", "Speed Limit Offsets: Speed Offset (120–140 km/h)"),
    "unit": (" mph", " km/h"),
    "values": {0: "Off"}
  },
  "OneLaneChange": {"name": "One Lane Change Per Signal"},
  "OnroadDistanceButton": {"name": "Driving Personality Button"},
  "OpenpilotEnabledToggle": {"name": "Enable openpilot"},
  "PathEdgeWidth": {"name": "Path Edges Width", "unit": "%", "values": {0: "Off"}},
  "PathWidth": {"decimals": 1, "name": "Path Width", "unit": (" feet", " meters"), "values": ({0: "Off", 1: "1.0 foot"}, {0: "Off", 1: "1.0 meter"})},
  "PauseAOLOnBrake": {"name": "Always On Lateral: Pause on Brake Press Below", "unit": (" mph", " km/h"), "values": {0: "Off"}},
  "PauseLateralOnSignal": {"name": "Pause Steering Below (Turn Signal Only)"},
  "PauseLateralSpeed": {"name": "Pause Steering Below", "unit": (" mph", " km/h"), "values": {0: "Off"}},
  "PedalsOnUI": {"name": "Gas / Brake Pedal Indicators"},
  "PreferredSchedule": {"name": "Automatically Update Maps", "values": {0: "Manually", 1: "Weekly", 2: "Monthly"}},
  "PromptDistractedVolume": {"name": "Distracted Driver Volume", "unit": "%", "values": {0: "Muted", 101: "Auto"}},
  "PromptVolume": {"name": "Prompt Volume", "unit": "%", "values": {0: "Muted", 101: "Auto"}},
  "QOLLateral": {"name": "Steering: Quality of Life"},
  "QOLLongitudinal": {"name": "Gas / Brake: Quality of Life"},
  "QOLVisuals": {"name": "Driving View: Quality of Life"},
  "RadarTracksUI": {"name": "Radar Tracks"},
  "RainbowPath": {"name": "Rainbow Path"},
  "RandomEvents": {"name": "Random Events"},
  "RandomThemes": {"name": "Random Themes"},
  "RandomThemesHolidays": {"name": "Random Themes (Include Holiday Themes)"},
  "RecordAudio": {"name": "Record and Upload Microphone Audio"},
  "RecordFront": {"name": "Record and Upload Driver Camera"},
  "ReduceAccelerationLowVisibility": {"name": "Low Visibility: Reduce Acceleration by", "unit": "%"},
  "ReduceAccelerationRain": {"name": "Rain: Reduce Acceleration by", "unit": "%"},
  "ReduceAccelerationRainStorm": {"name": "Rainstorms: Reduce Acceleration by", "unit": "%"},
  "ReduceAccelerationSnow": {"name": "Snow: Reduce Acceleration by", "unit": "%"},
  "ReduceLateralAccelerationLowVisibility": {"name": "Low Visibility: Reduce Cornering Force by", "unit": "%"},
  "ReduceLateralAccelerationRain": {"name": "Rain: Reduce Cornering Force by", "unit": "%"},
  "ReduceLateralAccelerationRainStorm": {"name": "Rainstorms: Reduce Cornering Force by", "unit": "%"},
  "ReduceLateralAccelerationSnow": {"name": "Snow: Reduce Cornering Force by", "unit": "%"},
  "RefuseVolume": {"name": "Won't Engage Volume", "unit": "%", "values": {0: "Muted", 101: "Auto"}},
  "RelaxedFollow": {"decimals": 2, "name": "Relaxed Personality: Following Distance", "unit": " seconds", "values": {1: "1.00 second"}},
  "RelaxedJerkAcceleration": {"name": "Relaxed Personality: Acceleration Smoothness", "unit": "%"},
  "RelaxedJerkDanger": {"name": "Relaxed Personality: Safety Gap Bias", "unit": "%"},
  "RelaxedJerkDeceleration": {"name": "Relaxed Personality: Braking Smoothness", "unit": "%"},
  "RelaxedJerkSpeed": {"name": "Relaxed Personality: Speed-Up Response", "unit": "%"},
  "RelaxedJerkSpeedDecrease": {"name": "Relaxed Personality: Slowdown Response", "unit": "%"},
  "RelaxedPersonalityProfile": {"name": "Relaxed Personality"},
  "ResetCurveData": {"name": "Reset Curve Data"},
  "ReverseCruise": {"name": "Reverse Cruise Increase"},
  "RoadEdgesWidth": {"name": "Road Edges Width", "unit": (" inches", " cm"), "values": ({0: "Off", 1: "1 inch"}, {0: "Off"})},
  "RoadNameUI": {"name": "Road Name"},
  "RotatingWheel": {"name": "Rotating Steering Wheel"},
  "ScreenBrightness": {"name": "Screen Brightness (Offroad)", "unit": "%", "values": {0: "Screen Off", 101: "Auto"}},
  "ScreenBrightnessOnroad": {"name": "Screen Brightness (Onroad)", "unit": "%", "values": {0: "Screen Off", 101: "Auto"}},
  "ScreenManagement": {"name": "Screen Settings"},
  "ScreenRecorder": {"name": "Screen Recorder"},
  "ScreenTimeout": {"name": "Screen Timeout (Offroad)", "unit": " seconds"},
  "ScreenTimeoutOnroad": {"name": "Screen Timeout (Onroad)", "unit": " seconds"},
  "SetSpeedLimit": {"name": "Match Speed Limit on Engage"},
  "SetSpeedOffset": {"name": "Offset Set Speed by", "unit": (" mph", " km/h"), "values": {0: "Off"}},
  "ShowCEMStatus": {"name": "Conditional Experimental Mode: Status Widget"},
  "ShowCPU": {"name": "Sidebar Metrics (CPU)"},
  "ShowCSCStatus": {"name": "Curve Speed Controller: Status Widget"},
  "ShowGPU": {"name": "Sidebar Metrics (GPU)"},
  "ShowIP": {"name": "Sidebar Metrics (IP)"},
  "ShowMemoryUsage": {"name": "Sidebar Metrics (RAM)"},
  "ShowSLCOffset": {"name": "Show Speed Limit Offset"},
  "ShowSpeedLimits": {"name": "Show Speed Limits"},
  "ShowSteering": {"name": "Border Metrics (Steering Torque)"},
  "ShowStoppingPoint": {"name": "Model Stopping Point"},
  "ShowStoppingPointMetrics": {"name": "Model Stopping Point (Show Distance)"},
  "ShowStorageLeft": {"name": "Sidebar Metrics (SSD Left)"},
  "ShowStorageUsed": {"name": "Sidebar Metrics (SSD Used)"},
  "SidebarMetrics": {"name": "Sidebar Metrics"},
  "SignalAnimation": {"name": "Custom Themes: Turn Signal"},
  "SignalMetrics": {"name": "Border Metrics (Turn Signal)"},
  "SLCConfirmation": {"name": "Confirm New Speed Limits"},
  "SLCConfirmationHigher": {"name": "Confirm New Speed Limits (Higher Limits)"},
  "SLCConfirmationLower": {"name": "Confirm New Speed Limits (Lower Limits)"},
  "SLCFallback": {"name": "Speed Limit Controller: Fallback Speed", "values": {0: "Set Speed", 1: "Experimental Mode", 2: "Previous Limit"}},
  "SLCLookaheadHigher": {"name": "Higher Limit Lookahead Time", "unit": " seconds", "values": {1: "1 second"}},
  "SLCLookaheadLower": {"name": "Lower Limit Lookahead Time", "unit": " seconds", "values": {1: "1 second"}},
  "SLCMapboxFiller": {"name": "Speed Limit Controller: Use Mapbox as Fallback"},
  "SLCOverride": {"name": "Speed Limit Controller: Override Speed", "values": {0: "None", 1: "Set With Gas Pedal", 2: "Max Set Speed"}},
  "SLCPriority": {"name": "Speed Limit Source Priority"},
  "SLCPriority1": {
    "name": "Speed Limit Source Priority (Primary)",
    "values": {"Camera": "Camera", "Dashboard": "Dashboard", "Highest": "Highest", "Lowest": "Lowest", "Map Data": "Map Data", "None": "None"}
  },
  "SLCPriority2": {
    "name": "Speed Limit Source Priority (Secondary)",
    "values": {"Camera": "Camera", "Dashboard": "Dashboard", "Highest": "Highest", "Lowest": "Lowest", "Map Data": "Map Data", "None": "None"}
  },
  "SLCPriority3": {
    "name": "Speed Limit Source Priority (Tertiary)",
    "values": {"Camera": "Camera", "Dashboard": "Dashboard", "Highest": "Highest", "Lowest": "Lowest", "Map Data": "Map Data", "None": "None"}
  },
  "SNGHack": {"name": "Toyota/Lexus Settings: Stop-and-Go Hack"},
  "SoundPack": {"name": "Sound Pack"},
  "SpeedLimitChangedAlert": {"name": "Speed Limit Changed Alert"},
  "SpeedLimitController": {"name": "Speed Limit Controller"},
  "SpeedLimitFiller": {"name": "Speed Limit Filler"},
  "SpeedLimitSources": {"name": "Show Speed Limit Sources"},
  "StandardFollow": {"decimals": 2, "name": "Standard Personality: Following Distance", "unit": " seconds", "values": {1: "1.00 second"}},
  "StandardJerkAcceleration": {"name": "Standard Personality: Acceleration Smoothness", "unit": "%"},
  "StandardJerkDanger": {"name": "Standard Personality: Safety Gap Bias", "unit": "%"},
  "StandardJerkDeceleration": {"name": "Standard Personality: Braking Smoothness", "unit": "%"},
  "StandardJerkSpeed": {"name": "Standard Personality: Speed-Up Response", "unit": "%"},
  "StandardJerkSpeedDecrease": {"name": "Standard Personality: Slowdown Response", "unit": "%"},
  "StandardPersonalityProfile": {"name": "Standard Personality"},
  "StandbyMode": {"name": "Standby Mode"},
  "StartAccel": {"decimals": 2, "name": "Advanced Longitudinal Tuning: Start Acceleration", "unit": " m/s²"},
  "StartupMessageBottom": {"name": "Startup Alert (Bottom Half)"},
  "StartupMessageTop": {"name": "Startup Alert (Top Half)"},
  "StaticPedalsOnUI": {"name": "Gas / Brake Pedal Indicators (Static)"},
  "SteerDelay": {"decimals": 2, "name": "Advanced Lateral Tuning: Actuator Delay"},
  "SteerFriction": {"decimals": 2, "name": "Advanced Lateral Tuning: Friction"},
  "SteerKP": {"decimals": 2, "name": "Advanced Lateral Tuning: Kp Factor"},
  "SteerLatAccel": {"decimals": 2, "name": "Advanced Lateral Tuning: Lateral Acceleration"},
  "SteerRatio": {"decimals": 2, "name": "Advanced Lateral Tuning: Steer Ratio"},
  "StopAccel": {"decimals": 2, "name": "Advanced Longitudinal Tuning: Stop Acceleration", "unit": " m/s²"},
  "StoppedTimer": {"name": "Stopped Timer"},
  "StoppingDecelRate": {"decimals": 3, "name": "Advanced Longitudinal Tuning: Stopping Rate", "unit": " m/s²/s"},
  "SubaruSNG": {"name": "Subaru Settings: Stop and Go"},
  "TacoTune": {"name": "\"Taco Bell Run\" Turn Speed Hack"},
  "TacoTuneHacks": {"name": "Hyundai/Kia/Genesis Settings: \"Taco Bell Run\" Torque Hack"},
  "TetheringEnabled": {"name": "Enable Tethering", "values": {0: "Off", 1: "Always", 2: "Only Onroad", 3: "Until Reboot"}},
  "ToyotaDoors": {"name": "Toyota/Lexus Settings: Automatically Lock/Unlock Doors"},
  "ToyotaDSUBypass": {"name": "Toyota/Lexus Settings: DSU Re-Route Harness"},
  "TuningLevel": {"name": "Tuning Level", "values": {0: "Minimal", 1: "Standard", 2: "Advanced", 3: "Developer"}},
  "TurnDesires": {"name": "Steer Into Turns Below Lane Change Speed"},
  "UnlockDoors": {"name": "Toyota/Lexus Settings: Automatically Lock/Unlock Doors (Unlock)"},
  "UseKonikServer": {"name": "Use Konik Server"},
  "UseSI": {"name": "Use International System of Units"},
  "UseVienna": {"name": "Use Vienna-Style Speed Signs"},
  "VEgoStarting": {"decimals": 2, "name": "Advanced Longitudinal Tuning: Start Speed", "unit": " m/s"},
  "VEgoStopping": {"decimals": 2, "name": "Advanced Longitudinal Tuning: Stop Speed", "unit": " m/s"},
  "VeryLongDistanceButtonControl": {"name": "Distance Button (Very Long Press)", "values": BUTTON_FUNCTION_LABELS},
  "VoltSNG": {"name": "General Motors Settings: Stop-and-Go Hack"},
  "WarningImmediateVolume": {"name": "Urgent Warning Volume", "unit": "%", "values": {0: "Muted", 101: "Auto"}},
  "WarningSoftVolume": {"name": "Soft Warning Volume", "unit": "%", "values": {0: "Muted", 101: "Auto"}},
  "WeatherPresets": {"name": "Weather Condition Offsets"},
  "WeatherToken": {"name": "Weather Condition Offsets: Your Own Key"},
  "WheelIcon": {"name": "Custom Themes: Steering Wheel"},
  "WheelSpeed": {"name": "Use Wheel Speed"}
}

SETTINGS_BACKUP_KEYS = {"FavoriteDestinations", "SecOCKey", "SecOCKeys", *SETTING_LABELS}

SPEED_LIMIT_PRIORITIES = {
  "HIGHEST": "Highest",
  "LOWEST": "Lowest"
}

SPEED_LIMIT_SOURCES = {
  "CAMERA": "Camera",
  "DASHBOARD": "Dashboard",
  "MAP_DATA": "Map Data",
  "MAPBOX": "Mapbox",
  "NONE": "None"
}

STARTUP_ALERT_MAX_LENGTHS = {"StartupMessageBottom": 45, "StartupMessageTop": 35}

THEME_COLOR_KEYS = ("LaneLines", "LeadMarker", "Path", "PathEdge", "Sidebar1", "Sidebar2", "Sidebar3")

THEME_EXCLUDED_BUILTINS = {
  "colors": {"none"},
  "distance_icons": {"april_fools", "easter", "none"},
  "icons": {"none"},
  "signals": {"stock"},
  "sounds": {"none"},
  "steering_wheels": set()
}

THEME_PARAMS = {
  "colors": "ColorScheme",
  "distance_icons": "DistanceIconPack",
  "icons": "IconPack",
  "signals": "SignalAnimation",
  "sounds": "SoundPack",
  "steering_wheels": "WheelIcon"
}

THEME_SIGNAL_STYLES = ("static", "traditional")

THEME_SLOTS = {
  "distance_icons": ("aggressive", "relaxed", "standard", "traffic"),
  "icons": ("button_flag", "button_home", "button_settings"),
  "sounds": ("disengage", "engage", "prompt", "prompt_distracted", "refuse", "startup", "warning_immediate", "warning_soft")
}

TUNING_LEVELS = {
  "MINIMAL": 0,
  "STANDARD": 1,
  "ADVANCED": 2,
  "DEVELOPER": 3
}

@cache
def get_mock_car_params():
  return interfaces[MOCK.MOCK].get_params(MOCK.MOCK, gen_empty_fingerprint(), [], False, False, False).as_reader()

def get_models():
  return json.loads(frogpilot_paths.MODELS_LIST_PATH.read_text()) if frogpilot_paths.MODELS_LIST_PATH.is_file() else []

def is_model_complete(model_path):
  if (model_path / "supercombo_metadata.pkl").is_file():
    return (model_path / "supercombo.thneed").is_file() or (model_path / "supercombo_tinygrad.pkl").is_file()

  return all((model_path / file_name).is_file() for file_name in SPLIT_MODEL_FILES)

def get_frogpilot_toggles(sm=None):
  if sm is None:
    process_frogpilot_toggles.cache_clear()
    return process_frogpilot_toggles("")
  return process_frogpilot_toggles(sm["frogpilotProcessState"].frogpilotToggles)

@lru_cache(maxsize=1)
def process_frogpilot_toggles(toggles):
  if toggles:
    return SimpleNamespace(**json.loads(toggles))
  return FrogPilotVariables().frogpilot_toggles

class FrogPilotVariables:
  def __init__(self):
    self.params = Params(return_defaults=True)

    self.frogpilot_toggles = SimpleNamespace()

    self.default_values = {key.decode(): self.params.get_default_value(key) for key in self.params.all_keys()}
    self.stock_values = {key.decode(): self.params.get_stock_value(key) for key in self.params.all_keys()}
    self.tuning_levels = {key.decode(): self.params.get_tuning_level(key) for key in self.params.all_keys()}

    build_metadata = get_build_metadata()

    branch = build_metadata.channel
    self.development_branch = branch == DEVELOPMENT_BRANCH
    self.release_branch = branch == RELEASE_BRANCH
    self.vetting_branch = branch == VETTING_BRANCH

    self.frogs_go_moo = frogpilot_paths.FROGS_GO_MOO_PATH.is_file()

    remote = build_metadata.openpilot.git_origin.lower().removesuffix(".git")
    repository = "/".join(remote.replace(":", "/").split("/")[-2:])
    self.issue_report_available = repository in REPORT_REPOSITORIES and (not build_metadata.openpilot.is_dirty or self.frogs_go_moo)

    self.stock_colors = json.loads(frogpilot_paths.STOCK_THEME_COLORS_PATH.read_text())

    self.update()

  def get_button_function(self, key):
    function = self.get_value(key, cast=int)
    if function == BUTTON_FUNCTIONS["PAUSE_LATERAL"] or self.frogpilot_toggles.openpilot_longitudinal:
      return function
    return BUTTON_FUNCTIONS["NOTHING"]

  def get_color(self, key, theme_colors):
    for source in (theme_colors, self.stock_colors):
      color = source.get(key)
      if isinstance(color, dict):
        return f"#{color.get('alpha', 255):02X}{color.get('red', 255):02X}{color.get('green', 255):02X}{color.get('blue', 255):02X}"
    return "#FFFFFFFF"

  def get_value(self, key, cast=bool, condition=True, conversion=None, default=None, minimum=None, maximum=None):
    if not condition or self.frogpilot_toggles.tuning_level < self.tuning_levels[key]:
      if default is not None:
        return default

      if cast is bool:
        return self.default_values[key] if condition else False

      if condition:
        value = self.default_values[key]
      else:
        value = self.stock_values[key]
    elif cast is bool:
      return self.params.get_bool(key)
    else:
      value = self.params.get(key)

    if cast is not None:
      value = cast(value)

    if conversion is not None:
      value *= conversion

    if minimum is not None and value < minimum:
      value = minimum
    elif maximum is not None and value > maximum:
      value = maximum

    return value

  def torque_cache_matches(self, CP, version):
    msg_bytes = self.params.get("CarParamsPrevRoute")
    if not msg_bytes:
      return False

    prev_CP = messaging.log_from_bytes(msg_bytes, car.CarParams)
    return torqued.TorqueEstimator.get_restore_key(prev_CP, version) == torqued.TorqueEstimator.get_restore_key(CP, torqued.VERSION)

  def update(self, holiday_theme="stock", started=False):
    from openpilot.frogpilot.common import frogpilot_utilities

    toggle = self.frogpilot_toggles
    toggle.tuning_level = self.params.get("TuningLevel") if self.params.get_bool("TuningLevelConfirmed") else TUNING_LEVELS["ADVANCED"]

    msg_bytes = self.params.get("CarParams" if started else "CarParamsPersistent")
    if msg_bytes:
      CP = messaging.log_from_bytes(msg_bytes, car.CarParams)
    else:
      CP = get_mock_car_params()

    is_torque_car = CP.lateralTuning.which() == "torque"
    if is_torque_car:
      friction = CP.lateralTuning.torque.friction
      latAccelFactor = CP.lateralTuning.torque.latAccelFactor
    else:
      friction = 0.0
      latAccelFactor = 0.0

    fpmsg_bytes = self.params.get("FrogPilotCarParams" if started else "FrogPilotCarParamsPersistent")
    if fpmsg_bytes:
      FPCP = messaging.log_from_bytes(fpmsg_bytes, custom.FrogPilotCarParams)
    else:
      FPCP = interfaces[MOCK.MOCK].get_frogpilot_params(MOCK.MOCK, gen_empty_fingerprint(), [], CP, toggle)

    selected_car_model = self.params.get("CarModel")
    toggle.force_fingerprint = self.get_value("ForceFingerprint", condition=selected_car_model != self.default_values["CarModel"])

    alpha_longitudinal = CP.alphaLongitudinalAvailable
    toggle.car_make = CP.brand
    toggle.car_model = selected_car_model if toggle.force_fingerprint else CP.carFingerprint
    toggle.can_disable_openpilot_long = frogpilot_utilities.can_disable_openpilot_long(CP) and not alpha_longitudinal
    toggle.disable_openpilot_long = self.get_value("DisableOpenpilotLongitudinal", condition=toggle.can_disable_openpilot_long)
    has_bsm = CP.enableBsm
    toggle.has_dashboard_speed_limit = FPCP.hasDashboardSpeedLimit
    toggle.has_nnff = frogpilot_utilities.nnff_supported(toggle.car_model)
    has_radar = not CP.radarUnavailable
    toggle.has_sdsu = toggle.car_make == "toyota" and bool(FPCP.flags & ToyotaFrogPilotFlags.SMART_DSU.value)
    has_sng = CP.autoResumeSng
    toggle.has_zss = toggle.car_make == "toyota" and bool(FPCP.flags & ToyotaFrogPilotFlags.ZSS.value)
    hyundai_canfd = toggle.car_make == "hyundai" and CP.safetyConfigs[-1].safetyModel == car.CarParams.SafetyModel.hyundaiCanfd
    is_angle_car = CP.steerControlType == car.CarParams.SteerControlType.angle
    toggle.is_gm_cc_only = CP.carFingerprint in CC_ONLY_CAR
    toggle.is_honda_nidec = toggle.car_make == "honda" and CP.carFingerprint not in HONDA_BOSCH
    learned_lateral_profile = self.params.get("MaxLateralAcceleration")
    learned_lateral_acceleration = learned_lateral_profile.get("value", 0.0) if learned_lateral_profile.get("car_fingerprint") == toggle.car_model else 0.0
    toggle.lkas_allowed_for_aol = toggle.car_make == "hyundai" and bool(CP.flags & HyundaiFlags.CANFD or CP.flags & HyundaiFlags.HAS_LDA_BUTTON)
    toggle.has_lkas_button = toggle.car_make in LKAS_BUTTON_BRANDS or toggle.lkas_allowed_for_aol or (toggle.car_make == "toyota" and bool(CP.flags & ToyotaFlags.TSS2))
    longitudinalActuatorDelay = CP.longitudinalActuatorDelay
    toggle.openpilot_longitudinal = CP.openpilotLongitudinalControl and (started or not toggle.disable_openpilot_long)
    pcm_cruise = CP.pcmCruise
    prohibited_main_aol = not toggle.openpilot_longitudinal and toggle.lkas_allowed_for_aol
    startAccel = CP.startAccel
    stopAccel = CP.stopAccel
    steerActuatorDelay = CP.steerActuatorDelay
    steerKp = KP
    steerRatio = CP.steerRatio
    stoppingDecelRate = CP.stoppingDecelRate
    vEgoStarting = CP.vEgoStarting
    vEgoStopping = CP.vEgoStopping

    toggle.has_auto_tune = toggle.car_make in torqued.ALLOWED_CARS and is_torque_car

    msg_bytes = self.params.get("LiveTorqueParameters")
    if not started:
      self.drive_start_torque_cache = None
    elif self.drive_start_torque_cache is None:
      self.drive_start_torque_cache = msg_bytes or b""

    if msg_bytes:
      LTP = messaging.log_from_bytes(msg_bytes, log.Event).liveTorqueParameters
      toggle.liveValid = LTP.liveValid

      if msg_bytes == self.drive_start_torque_cache:
        toggle.liveValid &= self.torque_cache_matches(CP, LTP.version)
    else:
      toggle.liveValid = False

    max_lateral_accel = CP.maxLateralAccel
    if is_angle_car:
      max_lateral_accel = max(max_lateral_accel, learned_lateral_acceleration)
    elif is_torque_car and toggle.liveValid:
      max_lateral_accel *= LTP.latAccelFactorFiltered / latAccelFactor
    toggle.maxLateralAccel = min(max_lateral_accel, MAX_LATERAL_ACCEL_NO_ROLL)

    toggle.debug_mode = self.params.get_bool("DebugMode")
    toggle.force_offroad = self.params.get_bool("ForceOffroad")
    toggle.force_onroad = self.params.get_bool("ForceOnroad")
    toggle.issue_report_available = self.issue_report_available

    toggle.is_metric = self.params.get_bool("IsMetric")
    distance_conversion = 1 if toggle.is_metric else CV.FOOT_TO_METER
    small_distance_conversion = 1 if toggle.is_metric else CV.INCH_TO_CM
    speed_conversion = CV.KPH_TO_MS if toggle.is_metric else CV.MPH_TO_MS

    advanced_custom_ui = self.get_value("AdvancedCustomUI")
    toggle.hide_alerts = self.get_value("HideAlerts", condition=advanced_custom_ui and not toggle.debug_mode)
    toggle.hide_lead_marker = self.get_value("HideLeadMarker", condition=toggle.openpilot_longitudinal and advanced_custom_ui and not toggle.debug_mode)
    toggle.hide_max_speed = self.get_value("HideMaxSpeed", condition=advanced_custom_ui and not toggle.debug_mode)
    toggle.hide_speed = self.get_value("HideSpeed", condition=advanced_custom_ui and not toggle.debug_mode)
    toggle.hide_speed_limit = self.get_value("HideSpeedLimit", condition=advanced_custom_ui and not toggle.debug_mode)
    toggle.use_wheel_speed = self.get_value("WheelSpeed", condition=advanced_custom_ui)

    advanced_lateral_tuning = self.get_value("AdvancedLateralTune")
    toggle.force_auto_tune = self.get_value("ForceAutoTune", condition=is_torque_car and not is_angle_car and not toggle.has_auto_tune and advanced_lateral_tuning)
    toggle.force_auto_tune_off = self.get_value("ForceAutoTuneOff", condition=is_torque_car and not is_angle_car and toggle.has_auto_tune and advanced_lateral_tuning)
    toggle.force_torque_controller = self.get_value("ForceTorqueController", condition=advanced_lateral_tuning)
    toggle.steerActuatorDelay = self.get_value("SteerDelay", cast=float, condition=advanced_lateral_tuning and self.params.get("SteerDelay") != 0, default=steerActuatorDelay, minimum=0.01, maximum=1.0)
    toggle.use_custom_steerActuatorDelay = round(toggle.steerActuatorDelay, 2) != round(steerActuatorDelay, 2)
    toggle.friction = self.get_value("SteerFriction", cast=float, condition=advanced_lateral_tuning and self.params.get("SteerFriction") != 0, default=friction, minimum=0.01, maximum=1)
    toggle.use_custom_friction = (round(toggle.friction, 2) != round(friction, 2) and is_torque_car and not toggle.has_auto_tune and not toggle.force_auto_tune) or toggle.force_auto_tune_off
    toggle.steerKp = self.get_value("SteerKP", cast=float, condition=is_torque_car and not is_angle_car and advanced_lateral_tuning and self.params.get("SteerKP") != 0, default=steerKp, minimum=steerKp * 0.5, maximum=steerKp * 1.5)
    toggle.latAccelFactor = self.get_value("SteerLatAccel", cast=float, condition=advanced_lateral_tuning and self.params.get("SteerLatAccel") != 0, default=latAccelFactor, minimum=latAccelFactor * 0.5, maximum=latAccelFactor * 1.5)
    toggle.use_custom_latAccelFactor = (round(toggle.latAccelFactor, 2) != round(latAccelFactor, 2) and is_torque_car and not toggle.has_auto_tune and not toggle.force_auto_tune) or toggle.force_auto_tune_off
    toggle.steerRatio = self.get_value("SteerRatio", cast=float, condition=advanced_lateral_tuning and self.params.get("SteerRatio") != 0, default=steerRatio, minimum=steerRatio * 0.5, maximum=steerRatio * 1.5)
    toggle.use_custom_steerRatio = (round(toggle.steerRatio, 2) != round(steerRatio, 2) and not toggle.force_auto_tune) or toggle.force_auto_tune_off

    advanced_longitudinal_tuning = toggle.openpilot_longitudinal and self.get_value("AdvancedLongitudinalTune")
    toggle.longitudinalActuatorDelay = self.get_value("LongitudinalActuatorDelay", cast=float, condition=advanced_longitudinal_tuning and (self.params.get("LongitudinalActuatorDelay") != 0 or self.params.get("LongitudinalActuatorDelayStock") != 0), default=longitudinalActuatorDelay, minimum=0, maximum=1)
    toggle.max_desired_acceleration = self.get_value("MaxDesiredAcceleration", cast=float, condition=advanced_longitudinal_tuning, minimum=0.1, maximum=MAX_ACCELERATION)
    toggle.startAccel = self.get_value("StartAccel", cast=float, condition=advanced_longitudinal_tuning and (self.params.get("StartAccel") != 0 or self.params.get("StartAccelStock") != 0), default=startAccel, minimum=0, maximum=MAX_ACCELERATION)
    toggle.stopAccel = self.get_value("StopAccel", cast=float, condition=advanced_longitudinal_tuning and (self.params.get("StopAccel") != 0 or self.params.get("StopAccelStock") != 0), default=stopAccel, minimum=-MAX_ACCELERATION, maximum=0)
    toggle.stoppingDecelRate = self.get_value("StoppingDecelRate", cast=float, condition=advanced_longitudinal_tuning and self.params.get("StoppingDecelRate") != 0, default=stoppingDecelRate, minimum=0.001, maximum=12)
    toggle.vEgoStarting = self.get_value("VEgoStarting", cast=float, condition=advanced_longitudinal_tuning and self.params.get("VEgoStarting") != 0, default=vEgoStarting, minimum=0.01, maximum=1)
    toggle.vEgoStopping = self.get_value("VEgoStopping", cast=float, condition=advanced_longitudinal_tuning and self.params.get("VEgoStopping") != 0, default=vEgoStopping, minimum=0.01, maximum=1)

    toggle.alert_volume_controller = self.get_value("AlertVolumeControl")
    toggle.disengage_volume = self.get_value("DisengageVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.engage_volume = self.get_value("EngageVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.prompt_volume = self.get_value("PromptVolume", cast=float, condition=toggle.alert_volume_controller, minimum=25)
    toggle.prompt_distracted_volume = self.get_value("PromptDistractedVolume", cast=float, condition=toggle.alert_volume_controller, minimum=25)
    toggle.refuse_volume = self.get_value("RefuseVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.warning_soft_volume = self.get_value("WarningSoftVolume", cast=float, condition=toggle.alert_volume_controller, minimum=25)
    toggle.warning_immediate_volume = self.get_value("WarningImmediateVolume", cast=float, condition=toggle.alert_volume_controller, minimum=25)

    toggle.always_on_lateral = self.get_value("AlwaysOnLateral")
    toggle.always_on_lateral_lkas = toggle.always_on_lateral and toggle.lkas_allowed_for_aol and self.get_value("AlwaysOnLateralLKAS")
    toggle.always_on_lateral_main = toggle.always_on_lateral and not prohibited_main_aol and not toggle.always_on_lateral_lkas
    toggle.always_on_lateral_pause_speed = self.get_value("PauseAOLOnBrake", cast=float, condition=toggle.always_on_lateral, conversion=speed_conversion)

    toggle.automatic_updates = self.get_value("AutomaticUpdates", condition=self.release_branch or self.vetting_branch or self.frogs_go_moo, default=True) and not frogpilot_paths.BACKUP_PATH.is_file()

    toggle.cluster_offset = self.get_value("ClusterOffset", cast=float, condition=toggle.car_make == "toyota")

    toggle.conditional_experimental_mode = toggle.openpilot_longitudinal and self.get_value("ConditionalExperimental")
    toggle.conditional_curves = self.get_value("CECurves", condition=toggle.conditional_experimental_mode)
    toggle.conditional_curves_lead = self.get_value("CECurvesLead", condition=toggle.conditional_curves)
    toggle.conditional_lead = self.get_value("CELead", condition=toggle.conditional_experimental_mode)
    toggle.conditional_slower_lead = self.get_value("CESlowerLead", condition=toggle.conditional_lead)
    toggle.conditional_stopped_lead = self.get_value("CEStoppedLead", condition=toggle.conditional_lead)
    toggle.conditional_limit = self.get_value("CESpeed", cast=float, condition=toggle.conditional_experimental_mode, conversion=speed_conversion)
    toggle.conditional_limit_lead = self.get_value("CESpeedLead", cast=float, condition=toggle.conditional_experimental_mode, conversion=speed_conversion)
    toggle.conditional_stop_lights = self.get_value("CEStopLights", condition=toggle.conditional_experimental_mode)
    toggle.conditional_signal = self.get_value("CESignalSpeed", cast=float, condition=toggle.conditional_experimental_mode, conversion=speed_conversion,
                                             default=self.default_values["CESignalSpeed"] * CV.MPH_TO_MS)
    toggle.conditional_signal_lane_detection = self.get_value("CESignalLaneDetection", condition=toggle.conditional_experimental_mode and toggle.conditional_signal != 0)
    toggle.cem_status = self.get_value("ShowCEMStatus", condition=toggle.conditional_experimental_mode) or toggle.debug_mode

    toggle.curve_speed_controller = toggle.openpilot_longitudinal and self.get_value("CurveSpeedController")
    toggle.curve_speed_profile = self.get_value("CurveSpeedProfile", cast=int, condition=toggle.curve_speed_controller)
    toggle.csc_status = self.get_value("ShowCSCStatus", condition=toggle.curve_speed_controller) or toggle.debug_mode

    custom_alerts = self.get_value("CustomAlerts")
    toggle.goat_scream_alert = self.get_value("GoatScream", condition=custom_alerts)
    toggle.green_light_alert = self.get_value("GreenLightAlert", condition=custom_alerts)
    toggle.lead_departing_alert = self.get_value("LeadDepartingAlert", condition=custom_alerts)
    toggle.loud_blindspot_alert = self.get_value("LoudBlindspotAlert", condition=has_bsm and custom_alerts)
    toggle.speed_limit_changed_alert = self.get_value("SpeedLimitChangedAlert", condition=custom_alerts)

    toggle.custom_personalities = toggle.openpilot_longitudinal and self.get_value("CustomPersonalities")
    toggle.aggressive_jerk_acceleration = self.get_value("AggressiveJerkAcceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.aggressive_jerk_deceleration = self.get_value("AggressiveJerkDeceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.aggressive_jerk_danger = self.get_value("AggressiveJerkDanger", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.aggressive_jerk_speed = self.get_value("AggressiveJerkSpeed", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.aggressive_jerk_speed_decrease = self.get_value("AggressiveJerkSpeedDecrease", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.aggressive_follow = self.get_value("AggressiveFollow", cast=float, condition=toggle.custom_personalities, minimum=1, maximum=MAX_T_FOLLOW)
    toggle.standard_jerk_acceleration = self.get_value("StandardJerkAcceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.standard_jerk_deceleration = self.get_value("StandardJerkDeceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.standard_jerk_danger = self.get_value("StandardJerkDanger", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.standard_jerk_speed = self.get_value("StandardJerkSpeed", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.standard_jerk_speed_decrease = self.get_value("StandardJerkSpeedDecrease", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.standard_follow = self.get_value("StandardFollow", cast=float, condition=toggle.custom_personalities, minimum=1, maximum=MAX_T_FOLLOW)
    toggle.relaxed_jerk_acceleration = self.get_value("RelaxedJerkAcceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.relaxed_jerk_deceleration = self.get_value("RelaxedJerkDeceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.relaxed_jerk_danger = self.get_value("RelaxedJerkDanger", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.relaxed_jerk_speed = self.get_value("RelaxedJerkSpeed", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.relaxed_jerk_speed_decrease = self.get_value("RelaxedJerkSpeedDecrease", cast=float, condition=toggle.custom_personalities, conversion=0.01, minimum=0.25, maximum=2.0)
    toggle.relaxed_follow = self.get_value("RelaxedFollow", cast=float, condition=toggle.custom_personalities, minimum=1, maximum=MAX_T_FOLLOW)

    custom_themes = self.get_value("CustomThemes")
    toggle.color_scheme = self.get_value("ColorScheme", cast=None, condition=custom_themes)
    theme_colors = json.loads(frogpilot_paths.THEME_COLORS_PATH.read_text()) if frogpilot_paths.THEME_COLORS_PATH.is_file() else {}
    toggle.lane_lines_color = self.get_color("LaneLines", theme_colors)
    toggle.lead_marker_color = self.get_color("LeadMarker", theme_colors)
    toggle.path_color = self.get_color("Path", theme_colors)
    toggle.path_edges_color = self.get_color("PathEdge", theme_colors)
    toggle.sidebar_color1 = self.get_color("Sidebar1", theme_colors)
    toggle.sidebar_color2 = self.get_color("Sidebar2", theme_colors)
    toggle.sidebar_color3 = self.get_color("Sidebar3", theme_colors)
    toggle.distance_icons = self.get_value("DistanceIconPack", cast=None, condition=custom_themes)
    toggle.icon_pack = self.get_value("IconPack", cast=None, condition=custom_themes)
    toggle.signal_icons = self.get_value("SignalAnimation", cast=None, condition=custom_themes)
    toggle.sound_pack = self.get_value("SoundPack", cast=None, condition=custom_themes)
    toggle.random_themes = self.get_value("RandomThemes", condition=custom_themes)
    if toggle.random_themes:
      toggle.color_scheme = "random"
    toggle.random_themes_holidays = self.get_value("RandomThemesHolidays", condition=toggle.random_themes)
    toggle.wheel_image = self.get_value("WheelIcon", cast=None, condition=custom_themes)

    custom_ui = self.get_value("CustomUI")
    toggle.acceleration_path = toggle.openpilot_longitudinal and (self.get_value("AccelerationPath", condition=custom_ui) or toggle.debug_mode)
    toggle.adjacent_paths = self.get_value("AdjacentPath", condition=custom_ui)
    toggle.blind_spot_path = has_bsm and self.get_value("BlindSpotPath", condition=custom_ui)
    toggle.compass = self.get_value("Compass", condition=custom_ui)
    toggle.onroad_distance_button = toggle.openpilot_longitudinal and (self.get_value("OnroadDistanceButton", condition=custom_ui) or toggle.debug_mode)
    toggle.pedals_on_ui = self.get_value("PedalsOnUI", condition=toggle.openpilot_longitudinal and custom_ui)
    toggle.dynamic_pedals_on_ui = self.get_value("DynamicPedalsOnUI", condition=toggle.pedals_on_ui)
    toggle.static_pedals_on_ui = self.get_value("StaticPedalsOnUI", condition=toggle.pedals_on_ui)
    toggle.rotating_wheel = self.get_value("RotatingWheel", condition=custom_ui)

    toggle.developer_ui = self.get_value("DeveloperUI")
    toggle.developer_metrics = self.get_value("DeveloperMetrics", condition=toggle.developer_ui)
    border_metrics = self.get_value("BorderMetrics", condition=toggle.developer_metrics)
    toggle.blind_spot_metrics = has_bsm and self.get_value("BlindSpotMetrics", condition=border_metrics)
    toggle.signal_metrics = self.get_value("SignalMetrics", condition=border_metrics) or toggle.debug_mode
    toggle.steering_metrics = not is_angle_car and (self.get_value("ShowSteering", condition=border_metrics) or toggle.debug_mode)
    toggle.show_fps = self.get_value("FPSCounter", condition=toggle.developer_metrics) or toggle.debug_mode
    toggle.adjacent_path_metrics = self.get_value("AdjacentPathMetrics", condition=toggle.developer_metrics) or toggle.debug_mode
    toggle.lead_info = self.get_value("LeadInfo", condition=toggle.developer_metrics) or toggle.debug_mode
    toggle.numerical_temp = self.get_value("NumericalTemp", condition=toggle.developer_metrics) or toggle.debug_mode
    toggle.fahrenheit = self.get_value("Fahrenheit", condition=toggle.numerical_temp and not toggle.debug_mode)
    toggle.cpu_metrics = self.get_value("ShowCPU", condition=toggle.developer_metrics) or toggle.debug_mode
    toggle.gpu_metrics = self.get_value("ShowGPU", condition=toggle.developer_metrics and not toggle.debug_mode)
    toggle.ip_metrics = self.get_value("ShowIP", condition=toggle.developer_metrics)
    toggle.memory_metrics = self.get_value("ShowMemoryUsage", condition=toggle.developer_metrics) or toggle.debug_mode
    toggle.storage_left_metrics = self.get_value("ShowStorageLeft", condition=toggle.developer_metrics and not toggle.debug_mode)
    toggle.storage_used_metrics = self.get_value("ShowStorageUsed", condition=toggle.developer_metrics and not toggle.debug_mode)
    toggle.use_si_metrics = self.get_value("UseSI", condition=toggle.developer_metrics) or toggle.debug_mode
    toggle.developer_sidebar = self.get_value("DeveloperSidebar", condition=toggle.developer_ui) or toggle.debug_mode
    toggle.developer_sidebar_metric1 = self.get_value("DeveloperSidebarMetric1", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["LONGITUDINAL_ACTUATOR_ACCELERATION"] if toggle.debug_mode else None)
    toggle.developer_sidebar_metric2 = self.get_value("DeveloperSidebarMetric2", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["ACCELERATION_CURRENT"] if toggle.debug_mode else None)
    toggle.developer_sidebar_metric3 = self.get_value("DeveloperSidebarMetric3", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["LATERAL_STEERING_ANGLE"] if toggle.debug_mode else None)
    toggle.developer_sidebar_metric4 = self.get_value("DeveloperSidebarMetric4", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["LATERAL_TORQUE_USED"] if toggle.debug_mode else None)
    toggle.developer_sidebar_metric5 = self.get_value("DeveloperSidebarMetric5", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["LONGITUDINAL_MPC_JERK_ACCELERATION"] if toggle.debug_mode else None)
    toggle.developer_sidebar_metric6 = self.get_value("DeveloperSidebarMetric6", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["LONGITUDINAL_MPC_JERK_DANGER_ZONE"] if toggle.debug_mode else None)
    toggle.developer_sidebar_metric7 = self.get_value("DeveloperSidebarMetric7", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["LONGITUDINAL_MPC_JERK_SPEED_CONTROL"] if toggle.debug_mode else None)
    developer_widgets = self.get_value("DeveloperWidgets", condition=toggle.developer_ui)
    toggle.adjacent_lead_tracking = has_radar and (self.get_value("AdjacentLeadsUI", condition=developer_widgets) or toggle.debug_mode)
    toggle.radar_tracks = has_radar and (self.get_value("RadarTracksUI", condition=developer_widgets) or toggle.debug_mode)
    toggle.show_stopping_point = toggle.openpilot_longitudinal and (self.get_value("ShowStoppingPoint", condition=developer_widgets) or toggle.debug_mode)
    toggle.show_stopping_point_metrics = self.get_value("ShowStoppingPointMetrics", condition=toggle.show_stopping_point) or toggle.debug_mode

    device_management = self.get_value("DeviceManagement")
    toggle.device_shutdown_time = DEVICE_SHUTDOWN_TIMES.get(self.get_value("DeviceShutdown", cast=int, condition=device_management))
    toggle.increase_thermal_limits = self.get_value("IncreaseThermalLimits", condition=device_management)
    toggle.low_voltage_shutdown = self.get_value("LowVoltageShutdown", cast=float, condition=device_management, minimum=VBATT_PAUSE_CHARGING, maximum=12.5)
    toggle.no_logging = self.get_value("NoLogging", condition=device_management and not self.vetting_branch) or toggle.force_onroad

    if not started:
      toggle.use_higher_bitrate = device_management
      toggle.use_higher_bitrate &= self.get_value("HigherBitrate")
      toggle.use_higher_bitrate &= self.get_value("NoUploads")
      toggle.use_higher_bitrate &= not self.get_value("DisableOnroadUploads")
      toggle.use_higher_bitrate &= not self.vetting_branch
      toggle.use_higher_bitrate |= self.development_branch

      toggle.use_konik_server = device_management
      toggle.use_konik_server &= self.get_value("UseKonikServer")
      toggle.use_konik_server |= frogpilot_paths.NOT_VETTED_PATH.is_file()

    toggle.no_uploads = self.get_value("NoUploads", condition=device_management and not self.vetting_branch) or toggle.use_higher_bitrate
    toggle.no_onroad_uploads = self.get_value("DisableOnroadUploads", condition=toggle.no_uploads and not toggle.use_higher_bitrate)
    toggle.uploads_disabled = toggle.no_uploads and not toggle.no_onroad_uploads

    toggle.distance_button_function = self.get_button_function("DistanceButtonControl")
    toggle.distance_long_button_function = self.get_button_function("LongDistanceButtonControl")
    toggle.distance_very_long_button_function = self.get_button_function("VeryLongDistanceButtonControl")

    toggle.frogpilot_telemetry = self.get_value("FrogPilotTelemetry", condition=self.params.get_bool("FrogPilotTelemetryConfirmed"))

    toggle.frogsgomoo_tweak = self.get_value("FrogsGoMoosTweak", condition=toggle.openpilot_longitudinal and toggle.car_make == "toyota")
    if toggle.frogsgomoo_tweak:
      toggle.stoppingDecelRate = 0.01
      toggle.vEgoStarting = 0.1
      toggle.vEgoStopping = 0.5

    toggle.holiday_themes = self.get_value("HolidayThemes")
    toggle.current_holiday_theme = holiday_theme if toggle.holiday_themes else "stock"
    if toggle.current_holiday_theme != "stock":
      toggle.color_scheme = toggle.current_holiday_theme
      toggle.distance_icons = toggle.current_holiday_theme
      toggle.icon_pack = toggle.current_holiday_theme
      toggle.signal_icons = toggle.current_holiday_theme
      toggle.sound_pack = toggle.current_holiday_theme
      toggle.wheel_image = toggle.current_holiday_theme

    honda_nidec_longitudinal = toggle.openpilot_longitudinal and toggle.is_honda_nidec
    toggle.honda_alt_tune = self.get_value("HondaAltTune", condition=honda_nidec_longitudinal)
    toggle.honda_low_speed_pedal = self.get_value("HondaLowSpeedPedal", condition=CP.enableGasInterceptorDEPRECATED and honda_nidec_longitudinal)
    toggle.honda_max_brake = self.get_value("HondaMaxBrake", condition=honda_nidec_longitudinal)

    toggle.lane_changes = self.get_value("LaneChanges")
    toggle.lane_change_delay = self.get_value("LaneChangeTime", cast=float, condition=toggle.lane_changes)
    toggle.minimum_lane_change_speed = self.get_value("MinimumLaneChangeSpeed", cast=float, conversion=speed_conversion, default=self.default_values["MinimumLaneChangeSpeed"] * CV.MPH_TO_MS)
    toggle.nudgeless = self.get_value("NudgelessLaneChange", condition=toggle.lane_changes)
    toggle.lane_detection_width = self.get_value("LaneDetectionWidth", cast=float, condition=toggle.nudgeless or toggle.conditional_signal_lane_detection, conversion=distance_conversion)
    toggle.one_lane_change = self.get_value("OneLaneChange", condition=toggle.lane_changes)

    lateral_tuning = self.get_value("LateralTune")
    toggle.nnff_selected = self.get_value("NNFF", condition=lateral_tuning)
    toggle.nnff_lite_selected = self.get_value("NNFFLite", condition=lateral_tuning)
    toggle.nnff = toggle.has_nnff and not is_angle_car and toggle.nnff_selected
    toggle.nnff_lite = not is_angle_car and not toggle.nnff and toggle.nnff_lite_selected
    toggle.use_turn_desires = self.get_value("TurnDesires", condition=lateral_tuning)

    if toggle.has_lkas_button and not toggle.always_on_lateral_lkas:
      toggle.lkas_button_function = self.get_button_function("LKASButtonControl")
    else:
      toggle.lkas_button_function = BUTTON_FUNCTIONS["NOTHING"]

    toggle.lock_doors_timer = self.get_value("LockDoorsTimer", cast=float, condition=toggle.car_make == "toyota")

    longitudinal_tuning = toggle.openpilot_longitudinal and self.get_value("LongitudinalTune")
    toggle.acceleration_profile = self.get_value("AccelerationProfile", cast=float, condition=longitudinal_tuning)
    toggle.deceleration_profile = self.get_value("DecelerationProfile", cast=float, condition=longitudinal_tuning)
    toggle.human_acceleration = self.get_value("HumanAcceleration", condition=longitudinal_tuning)
    toggle.human_following = self.get_value("HumanFollowing", condition=longitudinal_tuning)
    toggle.human_lane_changes = has_radar and self.get_value("HumanLaneChanges", condition=longitudinal_tuning)
    toggle.lead_detection_probability = self.get_value("LeadDetectionThreshold", cast=float, condition=longitudinal_tuning, conversion=0.01, minimum=0.25, maximum=0.5)
    toggle.taco_tune = self.get_value("TacoTune", condition=longitudinal_tuning)

    toggle.long_pitch = self.get_value("LongPitch", condition=toggle.openpilot_longitudinal and toggle.car_make == "gm" and not toggle.is_gm_cc_only)

    models = {model["id"]: model for model in get_models()}

    toggle.automatically_download_models = self.get_value("AutomaticallyDownloadModels")
    toggle.available_models = {model_id: model["name"] for model_id, model in models.items()}
    toggle.model_randomizer = self.get_value("ModelRandomizer")

    if not started:
      if toggle.model_randomizer:
        model_id = self.params.get("RandomizedModel")
      else:
        model_id = self.get_value("DrivingModel", cast=None)

      model = models.get(model_id) if is_model_complete(frogpilot_paths.MODELS_PATH / model_id) else None

      if model is None:
        model = DEFAULT_MODEL

      toggle.model = model["id"]
      toggle.model_name = model["name"]
      toggle.model_path = str(frogpilot_paths.STOCK_MODEL_PATH if model is DEFAULT_MODEL else frogpilot_paths.MODELS_PATH / model["id"])

    toggle.model_ui = self.get_value("ModelUI")
    toggle.dynamic_path_width = self.get_value("DynamicPathWidth", condition=toggle.model_ui and not toggle.debug_mode)
    toggle.lane_line_width = self.get_value("LaneLinesWidth", cast=float, condition=toggle.model_ui and not toggle.debug_mode, conversion=small_distance_conversion / 200, default=self.default_values["LaneLinesWidth"] * CV.INCH_TO_CM / 200)
    toggle.path_edge_width = self.get_value("PathEdgeWidth", cast=float, condition=toggle.model_ui and not toggle.debug_mode, default=self.default_values["PathEdgeWidth"] if toggle.debug_mode else None)
    toggle.path_width = self.get_value("PathWidth", cast=float, condition=toggle.model_ui and not toggle.debug_mode, conversion=distance_conversion / 2, default=self.default_values["PathWidth"] * CV.FOOT_TO_METER / 2)
    toggle.road_edge_width = self.get_value("RoadEdgesWidth", cast=float, condition=toggle.model_ui and not toggle.debug_mode, conversion=small_distance_conversion / 200, default=self.default_values["RoadEdgesWidth"] * CV.INCH_TO_CM / 200)

    navigation_ui = self.get_value("NavigationUI")
    toggle.road_name_ui = self.get_value("RoadNameUI", condition=navigation_ui) or toggle.debug_mode
    toggle.show_speed_limits = self.get_value("ShowSpeedLimits", condition=navigation_ui) or toggle.debug_mode
    toggle.speed_limit_vienna = self.get_value("UseVienna", condition=navigation_ui)

    quality_of_life_lateral = self.get_value("QOLLateral")
    toggle.pause_lateral_below_speed = self.get_value("PauseLateralSpeed", cast=float, condition=quality_of_life_lateral, conversion=speed_conversion)
    toggle.pause_lateral_below_signal = self.get_value("PauseLateralOnSignal", condition=toggle.pause_lateral_below_speed != 0)

    quality_of_life_longitudinal = toggle.openpilot_longitudinal and self.get_value("QOLLongitudinal")
    toggle.cruise_increase = self.get_value("CustomCruise", cast=float, condition=not pcm_cruise and quality_of_life_longitudinal)
    toggle.cruise_increase_long = self.get_value("CustomCruiseLong", cast=float, condition=not pcm_cruise and quality_of_life_longitudinal)
    toggle.force_stops = self.get_value("ForceStops", condition=quality_of_life_longitudinal)
    toggle.increase_stopped_distance = self.get_value("IncreasedStoppedDistance", cast=float, condition=quality_of_life_longitudinal, conversion=distance_conversion)
    toggle.can_map_gears = (toggle.car_make in MAP_GEARS_BRANDS or hyundai_canfd) and not CP.secOcRequired
    map_gears = self.get_value("MapGears", condition=toggle.can_map_gears and quality_of_life_longitudinal)
    toggle.map_acceleration = self.get_value("MapAcceleration", condition=map_gears)
    toggle.map_deceleration = self.get_value("MapDeceleration", condition=map_gears)
    toggle.reverse_cruise_increase = self.get_value("ReverseCruise", condition=toggle.car_make == "toyota" and pcm_cruise and quality_of_life_longitudinal)
    toggle.set_speed_offset = self.get_value("SetSpeedOffset", cast=float, condition=not pcm_cruise and quality_of_life_longitudinal, conversion=(1 if toggle.is_metric else CV.MPH_TO_KPH))
    toggle.weather_presets = self.get_value("WeatherPresets", condition=quality_of_life_longitudinal)
    toggle.increase_following_distance_low_visibility = self.get_value("IncreaseFollowingLowVisibility", cast=float, condition=toggle.weather_presets)
    toggle.increase_following_distance_rain = self.get_value("IncreaseFollowingRain", cast=float, condition=toggle.weather_presets)
    toggle.increase_following_distance_rain_storm = self.get_value("IncreaseFollowingRainStorm", cast=float, condition=toggle.weather_presets)
    toggle.increase_following_distance_snow = self.get_value("IncreaseFollowingSnow", cast=float, condition=toggle.weather_presets)
    toggle.increase_stopped_distance_low_visibility = self.get_value("IncreasedStoppedDistanceLowVisibility", cast=float, condition=toggle.weather_presets, conversion=distance_conversion)
    toggle.increase_stopped_distance_rain = self.get_value("IncreasedStoppedDistanceRain", cast=float, condition=toggle.weather_presets, conversion=distance_conversion)
    toggle.increase_stopped_distance_rain_storm = self.get_value("IncreasedStoppedDistanceRainStorm", cast=float, condition=toggle.weather_presets, conversion=distance_conversion)
    toggle.increase_stopped_distance_snow = self.get_value("IncreasedStoppedDistanceSnow", cast=float, condition=toggle.weather_presets, conversion=distance_conversion)
    toggle.reduce_acceleration_low_visibility = self.get_value("ReduceAccelerationLowVisibility", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_acceleration_rain = self.get_value("ReduceAccelerationRain", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_acceleration_rain_storm = self.get_value("ReduceAccelerationRainStorm", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_acceleration_snow = self.get_value("ReduceAccelerationSnow", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_lateral_acceleration_low_visibility = self.get_value("ReduceLateralAccelerationLowVisibility", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_lateral_acceleration_rain = self.get_value("ReduceLateralAccelerationRain", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_lateral_acceleration_rain_storm = self.get_value("ReduceLateralAccelerationRainStorm", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_lateral_acceleration_snow = self.get_value("ReduceLateralAccelerationSnow", cast=float, condition=toggle.weather_presets, conversion=0.01)

    quality_of_life_visuals = self.get_value("QOLVisuals")
    toggle.camera_view = self.get_value("CameraView", cast=float, condition=quality_of_life_visuals and not toggle.debug_mode, default=self.default_values["CameraView"] if toggle.debug_mode else None)
    toggle.driver_camera_in_reverse = self.get_value("DriverCamera", condition=quality_of_life_visuals)
    toggle.stopped_timer = self.get_value("StoppedTimer", condition=quality_of_life_visuals)

    toggle.rainbow_path = self.get_value("RainbowPath", condition=not toggle.debug_mode)

    toggle.random_events = self.get_value("RandomEvents")

    screen_management = self.get_value("ScreenManagement")
    toggle.instant_replay = self.get_value("InstantReplay", cast=int, condition=screen_management)
    toggle.screen_brightness = self.get_value("ScreenBrightness", cast=float, condition=screen_management, minimum=1)
    toggle.screen_brightness_onroad = self.get_value("ScreenBrightnessOnroad", cast=float, condition=screen_management and not toggle.force_onroad)
    toggle.screen_recorder = self.get_value("ScreenRecorder", condition=screen_management) or toggle.debug_mode
    toggle.screen_timeout = self.get_value("ScreenTimeout", cast=float, condition=screen_management)
    toggle.screen_timeout_onroad = self.get_value("ScreenTimeoutOnroad", cast=float, condition=screen_management)
    toggle.standby_mode = self.get_value("StandbyMode", condition=screen_management)

    toggle.sng_hack = self.get_value("SNGHack", condition=toggle.openpilot_longitudinal and toggle.car_make == "toyota" and not has_sng)

    toggle.speed_limit_controller = toggle.openpilot_longitudinal and self.get_value("SpeedLimitController")
    toggle.map_speed_lookahead_higher = self.get_value("SLCLookaheadHigher", cast=float, condition=toggle.speed_limit_controller)
    toggle.map_speed_lookahead_lower = self.get_value("SLCLookaheadLower", cast=float, condition=toggle.speed_limit_controller)
    toggle.set_speed_limit = self.get_value("SetSpeedLimit", condition=toggle.speed_limit_controller)
    toggle.show_speed_limit_offset = self.get_value("ShowSLCOffset", condition=toggle.speed_limit_controller) or toggle.debug_mode
    slc_fallback_method = self.get_value("SLCFallback", cast=float, condition=toggle.speed_limit_controller)
    toggle.slc_fallback_experimental_mode = toggle.speed_limit_controller and slc_fallback_method == 1
    toggle.slc_fallback_previous_speed_limit = toggle.speed_limit_controller and slc_fallback_method == 2
    toggle.slc_mapbox_filler = self.get_value("SLCMapboxFiller", condition=toggle.show_speed_limits or toggle.speed_limit_controller)
    speed_limit_confirmation = self.get_value("SLCConfirmation", condition=toggle.speed_limit_controller)
    toggle.speed_limit_confirmation_higher = self.get_value("SLCConfirmationHigher", condition=speed_limit_confirmation)
    toggle.speed_limit_confirmation_lower = self.get_value("SLCConfirmationLower", condition=speed_limit_confirmation)
    slc_override_method = self.get_value("SLCOverride", cast=float, condition=toggle.speed_limit_controller)
    toggle.speed_limit_controller_override_manual = slc_override_method == 1
    toggle.speed_limit_controller_override_set_speed = slc_override_method == 2
    toggle.speed_limit_offset1 = self.get_value("Offset1", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_offset2 = self.get_value("Offset2", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_offset3 = self.get_value("Offset3", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_offset4 = self.get_value("Offset4", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_offset5 = self.get_value("Offset5", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_offset6 = self.get_value("Offset6", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_offset7 = self.get_value("Offset7", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_priority1 = self.get_value("SLCPriority1", cast=None, condition=toggle.speed_limit_controller)
    toggle.speed_limit_priority2 = self.get_value("SLCPriority2", cast=None, condition=toggle.speed_limit_controller)
    toggle.speed_limit_priority3 = self.get_value("SLCPriority3", cast=None, condition=toggle.speed_limit_controller)
    toggle.speed_limit_priority_highest = toggle.speed_limit_priority1 == SPEED_LIMIT_PRIORITIES["HIGHEST"]
    toggle.speed_limit_priority_lowest = toggle.speed_limit_priority1 == SPEED_LIMIT_PRIORITIES["LOWEST"]
    toggle.speed_limit_sources = self.get_value("SpeedLimitSources", condition=toggle.speed_limit_controller) or toggle.debug_mode

    toggle.speed_limit_filler = self.get_value("SpeedLimitFiller")

    toggle.startup_alert_top = self.get_value("StartupMessageTop", cast=None)
    toggle.startup_alert_bottom = self.get_value("StartupMessageBottom", cast=None)

    toggle.subaru_sng = self.get_value("SubaruSNG")

    toggle.taco_tune_hacks = self.get_value("TacoTuneHacks")

    toggle.tailscale_installed = (frogpilot_paths.TAILSCALE_PATH / "tailscaled").is_file()

    toggle.reset_removed_keys = sorted(RESET_REMOVED_KEYS)

    toggle.can_use_dsu_bypass = toggle.car_make == "toyota" and not toggle.has_sdsu and not CP.flags & (ToyotaFlags.NO_DSU | ToyotaFlags.UNSUPPORTED_DSU)
    toggle.toyota_dsu_bypass = self.get_value("ToyotaDSUBypass")

    toyota_doors = self.get_value("ToyotaDoors", condition=toggle.car_make == "toyota")
    toggle.lock_doors = self.get_value("LockDoors", condition=toyota_doors)
    toggle.unlock_doors = self.get_value("UnlockDoors", condition=toyota_doors)

    toggle.volt_sng = self.get_value("VoltSNG", condition=toggle.car_model == "CHEVROLET_VOLT" and not has_sng)

    self.toggles_json = json.dumps(vars(toggle))
