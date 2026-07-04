import json
import math
import requests
import sqlite3

from cereal import car, custom

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables


STATS_PAYLOAD_SCHEMA_VERSION = 1

LOCATION_UNAVAILABLE = ("N/A", "N/A", "N/A")

COUNTRY_AMBIGUITY_DISTANCE = 5000
COUNTRY_AMBIGUITY_MARGIN = 1000
MAX_NEAREST_CITY_DISTANCE = 100000
MIN_CITY_POPULATION = 100000
NEARBY_MAJOR_CITY_DISTANCE = 50000


def format_location_name(name):
  return "N/A" if name in (None, "", "00") else name


def get_city_candidates(connection, latitude, longitude):
  sql = """
    SELECT name, latitude, longitude, country_code, admin1_code, population
    FROM (
      SELECT
        name,
        latitude,
        longitude,
        country_code,
        admin1_code,
        population,
        latitude - :latitude AS latitude_delta,
        CASE
          WHEN ABS(longitude - :longitude) > 180 THEN 360 - ABS(longitude - :longitude)
          ELSE ABS(longitude - :longitude)
        END AS longitude_delta
      FROM cities
    )
    ORDER BY latitude_delta * latitude_delta + longitude_delta * longitude_delta * :longitude_weight ASC
    LIMIT :limit
  """

  connection.row_factory = sqlite3.Row
  return connection.execute(
    sql,
    {
      "latitude": latitude,
      "longitude": longitude,
      "longitude_weight": max(math.cos(math.radians(latitude)) ** 2, 0.01),
      "limit": 250,
    },
  ).fetchall()


def get_fallback_city(connection, country_code, admin1_code):
  return connection.execute(
    """
    SELECT city_fallbacks.name, admin1_regions.admin1_name, countries.country_name
    FROM city_fallbacks
    JOIN countries
      ON countries.country_code = city_fallbacks.country_code
    LEFT JOIN admin1_regions
      ON admin1_regions.country_code = city_fallbacks.country_code
      AND admin1_regions.admin1_code = city_fallbacks.fallback_admin1_code
    WHERE city_fallbacks.country_code = :country_code
      AND city_fallbacks.admin1_code = :admin1_code
    LIMIT 1
    """,
    {
      "country_code": country_code,
      "admin1_code": admin1_code,
    },
  ).fetchone()


def get_location_names(connection, country_code, admin1_code):
  location = connection.execute(
    """
    SELECT admin1_regions.admin1_name, countries.country_name
    FROM countries
    LEFT JOIN admin1_regions
      ON admin1_regions.country_code = countries.country_code
      AND admin1_regions.admin1_code = :admin1_code
    WHERE countries.country_code = :country_code
    LIMIT 1
    """,
    {
      "country_code": country_code,
      "admin1_code": admin1_code,
    },
  ).fetchone()

  if location is None:
    return "N/A", "N/A"

  return format_location_name(location["admin1_name"]), location["country_name"] or "N/A"


def get_nearby_major_city(connection, latitude, longitude, best_city):
  nearby_major_cities = connection.execute(
    """
    SELECT name, latitude, longitude, country_code, admin1_code, population
    FROM cities
    WHERE country_code = :country_code
      AND admin1_code = :admin1_code
      AND population >= :min_city_population
    """,
    {
      "country_code": best_city["country_code"],
      "admin1_code": best_city["admin1_code"],
      "min_city_population": MIN_CITY_POPULATION,
    },
  ).fetchall()
  if not nearby_major_cities:
    return None

  nearby_major_city = min(
    nearby_major_cities,
    key=lambda city: frogpilot_utilities.calculate_distance_to_point(latitude, longitude, city["latitude"], city["longitude"]),
  )
  distance = frogpilot_utilities.calculate_distance_to_point(latitude, longitude, nearby_major_city["latitude"], nearby_major_city["longitude"])
  return nearby_major_city if distance <= NEARBY_MAJOR_CITY_DISTANCE else None


def is_country_ambiguous(ranked_candidates):
  best_distance, _, best_city = ranked_candidates[0]

  for distance, _, city in ranked_candidates[1:]:
    if distance > COUNTRY_AMBIGUITY_DISTANCE or distance - best_distance > COUNTRY_AMBIGUITY_MARGIN:
      break
    if city["country_code"] != best_city["country_code"]:
      return True

  return False


def get_city(gps_position):
  coordinates = frogpilot_utilities.parse_gps_position(gps_position)
  if coordinates is None:
    return LOCATION_UNAVAILABLE

  latitude, longitude = coordinates
  if latitude == 0 and longitude == 0:
    return LOCATION_UNAVAILABLE

  try:
    city_lookup_path = frogpilot_variables.CITY_LOOKUP_PATH
    if not city_lookup_path.is_file():
      return LOCATION_UNAVAILABLE

    connection = sqlite3.connect(f"{city_lookup_path.resolve().as_uri()}?mode=ro", uri=True)
    try:
      candidates = get_city_candidates(connection, latitude, longitude)
      if not candidates:
        return LOCATION_UNAVAILABLE

      ranked_candidates = sorted(
        (
          frogpilot_utilities.calculate_distance_to_point(latitude, longitude, city["latitude"], city["longitude"]),
          index,
          city,
        )
        for index, city in enumerate(candidates)
      )
      if is_country_ambiguous(ranked_candidates):
        return LOCATION_UNAVAILABLE

      best_city = ranked_candidates[0][2]
      if ranked_candidates[0][0] > MAX_NEAREST_CITY_DISTANCE:
        return LOCATION_UNAVAILABLE

      if best_city["population"] < MIN_CITY_POPULATION:
        nearby_major_city = get_nearby_major_city(connection, latitude, longitude, best_city)
        if nearby_major_city is not None:
          admin1_name, country_name = get_location_names(connection, nearby_major_city["country_code"], nearby_major_city["admin1_code"])
          return nearby_major_city["name"], admin1_name, country_name

        fallback_city = get_fallback_city(connection, best_city["country_code"], best_city["admin1_code"])
        if fallback_city is None:
          return LOCATION_UNAVAILABLE

        return fallback_city["name"], format_location_name(fallback_city["admin1_name"]), fallback_city["country_name"] or "N/A"

      admin1_name, country_name = get_location_names(connection, best_city["country_code"], best_city["admin1_code"])
      return best_city["name"], admin1_name, country_name
    finally:
      connection.close()
  except (sqlite3.Error, OSError, ValueError) as error:
    print(f"Failed to get city: {error}")
    return LOCATION_UNAVAILABLE


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

  for model_name, model_data in sorted((json.loads(params.get("ModelDrivesAndScores") or "{}")).items()):
    drives = int(model_data.get("Drives", 0) or 0)
    if drives <= 0:
      continue

    model_scores.append({
      "drives": drives,
      "model_name": frogpilot_utilities.clean_model_name(model_name),
      "score": int(model_data.get("Score", 0) or 0),
    })

  return model_scores


def send_stats(gps_position, params, frogpilot_toggles):
  if not frogpilot_toggles.frogpilot_telemetry:
    return

  if frogpilot_toggles.car_make == "mock":
    return

  api_info = frogpilot_utilities.get_frogpilot_api_info()
  if not api_info.api_token or not api_info.dongle_id:
    return

  city, state, country = get_city(gps_position)

  using_default_model = (params.get("Model", encoding="utf-8") or "").endswith("_default")

  payload = {
    "api_token": api_info.api_token,
    "build_metadata": api_info.build_metadata,
    "device": api_info.device_type,
    "frogpilot_dongle_id": api_info.dongle_id,
    "model_scores": get_model_scores(params),
    "os_version": api_info.os_version,
    "stats_schema_version": STATS_PAYLOAD_SCHEMA_VERSION,
    "user_stats": {
      "calibrated_lateral_acceleration": params.get_float("CalibratedLateralAcceleration"),
      "car_params": get_car_params(params),
      "city": city,
      "country": country,
      "device": api_info.device_type,
      "frogpilot_car_params": get_frogpilot_car_params(params),
      "frogpilot_dongle_id": api_info.dongle_id,
      "frogpilot_stats": json.loads(params.get("FrogPilotStats") or "{}"),
      "state": state,
      "toggles": vars(frogpilot_toggles),
      "using_default_model": using_default_model,
    },
  }

  try:
    response = requests.post(
      f"{frogpilot_variables.FROGPILOT_API}/stats",
      json=payload,
      headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"},
      timeout=30,
    )
    response.raise_for_status()
    print("Successfully sent FrogPilot stats!")
  except requests.exceptions.RequestException as error:
    print(f"Failed to send stats: {error}")
