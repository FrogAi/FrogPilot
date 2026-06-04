import gzip
import json
import math
import requests

from functools import cache
from pathlib import Path

from cereal import car, custom


LOCATION_DATA_SCHEMA_VERSION = 1
STATS_PAYLOAD_SCHEMA_VERSION = 1

GRID_DEGREES = 1.0

MAX_NEAREST_CITY_KM = 100.0
NEAR_TIE_KM = 0.25

MINIMUM_POPULATION = 100_000

LATITUDE_BINS = int(180 / GRID_DEGREES)
LONGITUDE_BINS = int(360 / GRID_DEGREES)

LOCATION_UNAVAILABLE = ("N/A", "N/A", "N/A")

RECORD_GEONAME_ID = 0
RECORD_CITY = 1
RECORD_STATE = 2
RECORD_COUNTRY = 3
RECORD_LATITUDE = 4
RECORD_LONGITUDE = 5
RECORD_POPULATION = 6
RECORD_FEATURE_CODE = 7
RECORD_COUNTRY_CODE = 8
RECORD_ADMIN1_CODE = 9

DEFAULT_FEATURE_RANK = 3

FEATURE_RANK = {
  "PPLC": 0,
  "PPLA": 1,
  "PPLG": 2,
}

def candidate_grid_keys(latitude, longitude):
  lat_bin = math.floor((latitude + 90.0) / GRID_DEGREES)
  lat_bin = min(max(lat_bin, 0), LATITUDE_BINS - 1)
  lon_bin = math.floor((longitude + 180.0) / GRID_DEGREES) % LONGITUDE_BINS

  lat_radius = math.ceil(MAX_NEAREST_CITY_KM / 111.0 / GRID_DEGREES) + 1
  cos_latitude = max(math.cos(math.radians(latitude)), 0.05)
  lon_radius = math.ceil(MAX_NEAREST_CITY_KM / (111.0 * cos_latitude) / GRID_DEGREES) + 1

  lat_start = max(lat_bin - lat_radius, 0)
  lat_end = min(lat_bin + lat_radius, LATITUDE_BINS - 1)

  for candidate_lat_bin in range(lat_start, lat_end + 1):
    for candidate_lon_bin in range(lon_bin - lon_radius, lon_bin + lon_radius + 1):
      yield f"{candidate_lat_bin}:{candidate_lon_bin % LONGITUDE_BINS}"


def fallback_location(record, location_data):
  records = location_data["records"]
  fallback_indexes = (
    location_data["admin1_capitals"].get(f"{record[RECORD_COUNTRY_CODE]}.{record[RECORD_ADMIN1_CODE]}"),
    location_data["country_capitals"].get(record[RECORD_COUNTRY_CODE]),
  )

  for fallback_index in fallback_indexes:
    if fallback_index is None:
      continue

    fallback_record = records[fallback_index]
    if fallback_record[RECORD_POPULATION] < MINIMUM_POPULATION:
      continue
    if (
      record[RECORD_COUNTRY_CODE],
      record[RECORD_ADMIN1_CODE],
      record[RECORD_CITY].casefold(),
    ) == (
      fallback_record[RECORD_COUNTRY_CODE],
      fallback_record[RECORD_ADMIN1_CODE],
      fallback_record[RECORD_CITY].casefold(),
    ):
      continue

    return record_location(fallback_record)

  return LOCATION_UNAVAILABLE[0], LOCATION_UNAVAILABLE[1], record[RECORD_COUNTRY] or LOCATION_UNAVAILABLE[2]


@cache
def load_location_data():
  try:
    with gzip.open(Path(__file__).with_name("location_data.json.gz"), "rt", encoding="utf-8") as location_file:
      location_data = json.load(location_file)

    if location_data.get("schema_version") != LOCATION_DATA_SCHEMA_VERSION:
      raise ValueError(f"Unsupported location data schema: {location_data.get('schema_version')}")

    return location_data
  except (OSError, EOFError, gzip.BadGzipFile, json.JSONDecodeError, ValueError) as error:
    print(f"Failed to load FrogPilot location data: {error}")
    return None


def nearest_record(latitude, longitude, location_data):
  grid = location_data["grid"]
  records = location_data["records"]

  candidates = []
  for key in candidate_grid_keys(latitude, longitude):
    for record_index in grid.get(key, []):
      record = records[record_index]
      distance_km = frogpilot_utilities.calculate_distance_to_point(latitude, longitude, record[RECORD_LATITUDE], record[RECORD_LONGITUDE]) / 1000
      if distance_km <= MAX_NEAREST_CITY_KM:
        candidates.append((distance_km, record))

  if not candidates:
    return None

  nearest_distance = min(candidate[0] for candidate in candidates)
  return min(
    (record for distance, record in candidates if distance <= nearest_distance + NEAR_TIE_KM),
    key=lambda record: (-record[RECORD_POPULATION], FEATURE_RANK.get(record[RECORD_FEATURE_CODE], DEFAULT_FEATURE_RANK), record[RECORD_GEONAME_ID]),
  )


def record_location(record):
  city = record[RECORD_CITY] or LOCATION_UNAVAILABLE[0]
  state = record[RECORD_STATE] or LOCATION_UNAVAILABLE[1]
  country = record[RECORD_COUNTRY] or LOCATION_UNAVAILABLE[2]
  return city, state, country


def get_city_center(latitude, longitude):
  try:
    latitude = float(latitude)
    longitude = float(longitude)
  except (TypeError, ValueError):
    return LOCATION_UNAVAILABLE

  if not math.isfinite(latitude) or not math.isfinite(longitude):
    return LOCATION_UNAVAILABLE
  if not (-90.0 <= latitude <= 90.0 and -180.0 <= longitude <= 180.0):
    return LOCATION_UNAVAILABLE
  if latitude == 0 and longitude == 0:
    return LOCATION_UNAVAILABLE

  location_data = load_location_data()
  if location_data is None:
    return LOCATION_UNAVAILABLE

  nearest = nearest_record(latitude, longitude, location_data)
  if nearest is None:
    return LOCATION_UNAVAILABLE

  if nearest[RECORD_POPULATION] >= MINIMUM_POPULATION:
    return record_location(nearest)

  return fallback_location(nearest, location_data)


def get_car_params(params):
  msg_bytes = params.get("CarParamsPersistent")
  if not msg_bytes:
    return {}

  with car.CarParams.from_bytes(msg_bytes) as CP:
    car_params = CP.to_dict()

  car_params.pop("carFw", None)
  car_params.pop("carVin", None)
  return car_params


def get_frogpilot_car_params(params):
  msg_bytes = params.get("FrogPilotCarParamsPersistent")
  if not msg_bytes:
    return {}

  with custom.FrogPilotCarParams.from_bytes(msg_bytes) as FPCP:
    return FPCP.to_dict()


def get_model_scores(params):
  model_scores = []

  for model_name, model_data in sorted((params.get("ModelDrivesAndScores") or {}).items()):
    drives = int(model_data.get("Drives", 0) or 0)
    if drives <= 0:
      continue

    model_scores.append({
      "drives": drives,
      "model_name": frogpilot_utilities.clean_model_name(model_name),
      "score": int(model_data.get("Score", 0) or 0),
    })

  return model_scores


def send_stats(gps_position, params):
  api_info = frogpilot_utilities.get_frogpilot_api_info()
  if not api_info.api_token or not api_info.dongle_id:
    return

  city, state, country = LOCATION_UNAVAILABLE
  if isinstance(gps_position, dict) and gps_position.get("valid") is True:
    city, state, country = get_city_center(gps_position.get("latitude"), gps_position.get("longitude"))

  payload = {
    "api_token": api_info.api_token,
    "build_metadata": api_info.build_metadata,
    "device": api_info.device_type,
    "frogpilot_dongle_id": api_info.dongle_id,
    "model_scores": get_model_scores(params),
    "os_version": api_info.os_version,
    "stats_schema_version": STATS_PAYLOAD_SCHEMA_VERSION,
    "user_stats": {
      "calibrated_lateral_acceleration": params.get("CalibratedLateralAcceleration"),
      "car_params": get_car_params(params),
      "city": city,
      "country": country,
      "device": api_info.device_type,
      "frogpilot_car_params": get_frogpilot_car_params(params),
      "frogpilot_dongle_id": api_info.dongle_id,
      "frogpilot_stats": params.get("FrogPilotStats"),
      "state": state,
      "toggles": vars(frogpilot_toggles),
      "using_default_model": params.get("DrivingModel").endswith("_default"),
    },
  }

  try:
    response = requests.post(
      f"{FROGPILOT_API}/stats",
      json=payload,
      headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"},
      timeout=30,
    )
    response.raise_for_status()
    print("Successfully sent FrogPilot stats!")
  except requests.exceptions.RequestException as error:
    print(f"Failed to send stats: {error}")
