import json
import math

from pathlib import Path

from cereal import car, custom

from openpilot.frogpilot.common import frogpilot_variables

BORDERS_PATH = Path(__file__).parents[1] / "assets/borders.json"
# City data from GeoNames (https://www.geonames.org/), CC BY 4.0
CITIES_PATH = Path(__file__).parents[1] / "assets/cities.json"

SEA_LIMIT = 0.3


def coast_distance(points, latitude, longitude):
  scale = math.cos(math.radians(latitude))
  ys = [point_latitude - latitude for point_latitude in points[0::2]]
  xs = [(point_longitude - longitude) * scale for point_longitude in points[1::2]]

  nearest = math.inf
  for x1, y1, x2, y2 in zip(xs, ys, xs[1:], ys[1:], strict=False):
    dx, dy = x2 - x1, y2 - y1
    along = min(max(-(x1 * dx + y1 * dy) / (dx * dx + dy * dy), 0), 1)
    nearest = min(nearest, (x1 + along * dx) ** 2 + (y1 + along * dy) ** 2)
  return nearest


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


def get_location(last_gps_position):
  position = json.loads(last_gps_position) if last_gps_position is not None else None

  if position is None or (position["latitude"], position["longitude"]) == (0, 0):
    return "N/A", "N/A", "N/A"

  latitude = position["latitude"]
  longitude = position["longitude"]

  with BORDERS_PATH.open(encoding="utf-8") as file:
    borders = json.load(file)

  inside = set()
  for sides, *points in borders["outlines"]:
    latitudes = points[0::2]
    longitudes = points[1::2]
    for latitude_1, longitude_1, latitude_2, longitude_2 in zip(latitudes, longitudes, latitudes[1:], longitudes[1:], strict=False):
      if (latitude_1 > latitude) != (latitude_2 > latitude):
        if longitude < longitude_1 + (latitude - latitude_1) * (longitude_2 - longitude_1) / (latitude_2 - latitude_1):
          inside ^= set(sides)

  if not inside:
    distance, sides = min((coast_distance(points, latitude, longitude), sides) for sides, *points in borders["outlines"] if len(sides) == 1)
    if distance < SEA_LIMIT ** 2:
      inside = set(sides)

  region = borders["regions"][min(inside)] if inside else None

  with CITIES_PATH.open(encoding="utf-8") as file:
    countries = json.load(file)

  latitude = math.radians(latitude)
  longitude = math.radians(longitude)

  best_similarity = -2
  location = ("N/A", *region) if region else ("N/A", "N/A", "N/A")
  for country, cities in countries.items():
    for city, *state, city_latitude, city_longitude in cities:
      state = state[0] if state else "N/A"
      if region is not None and [country, state] != region:
        continue

      city_latitude = math.radians(city_latitude)
      city_longitude = math.radians(city_longitude)

      similarity = math.sin(latitude) * math.sin(city_latitude) + math.cos(latitude) * math.cos(city_latitude) * math.cos(longitude - city_longitude)
      if similarity > best_similarity:
        best_similarity = similarity
        location = city, country, state

  return location


def get_model_scores(params):
  model_scores = []

  for model_name, model_data in sorted(params.get("ModelDrivesAndScores").items()):
    drives = int(model_data.get("Drives", 0))
    if drives <= 0:
      continue

    model_scores.append({
      "drives": drives,
      "model_name": model_name,
      "score": int(model_data.get("Score", 0)),
    })

  return model_scores


def send_stats(params, frogpilot_toggles, frogpilot_api):
  if not params.get_bool("FrogPilotTelemetryConfirmed"):
    return

  if frogpilot_toggles.car_make == "mock":
    return

  payload = {
    "model_scores": [],
    "stats_schema_version": 1,
    "user_stats": {},
  }

  if frogpilot_toggles.frogpilot_telemetry:
    city, country, state = get_location(params.get("LastGPSPosition"))

    payload["model_scores"] = get_model_scores(params)
    payload["user_stats"] = {
      "calibrated_lateral_acceleration": params.get("CalibratedLateralAcceleration"),
      "car_params": get_car_params(params),
      "city": city,
      "country": country,
      "frogpilot_car_params": get_frogpilot_car_params(params),
      "frogpilot_stats": params.get("FrogPilotStats"),
      "state": state,
      "toggles": vars(frogpilot_toggles),
      "using_default_model": frogpilot_toggles.model == frogpilot_variables.DEFAULT_MODEL["id"],
    }

  response = frogpilot_api.post_gzip("/v1/stats", json.dumps(payload, separators=(",", ":")).encode(), timeout=30)
  if response is None or not 200 <= response.status_code < 300:
    status = "no_response" if response is None else response.status_code
    print(f"Error sending stats (status={status})")
