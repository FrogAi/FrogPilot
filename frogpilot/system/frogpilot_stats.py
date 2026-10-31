import json
import math
import zlib

from cereal import car, custom

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.common.frogpilot_api import API_VERSION

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
  if last_gps_position is None:
    return "N/A", "N/A", "N/A"

  position = json.loads(last_gps_position)

  if (position["latitude"], position["longitude"]) == (0, 0):
    return "N/A", "N/A", "N/A"

  latitude = position["latitude"]
  longitude = position["longitude"]

  with frogpilot_paths.BORDERS_PATH.open(encoding="utf-8") as file:
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

  if inside:
    region = borders["regions"][min(inside)]
    location = ("N/A", *region)
  else:
    region = None
    location = ("N/A", "N/A", "N/A")

  with frogpilot_paths.CITIES_PATH.open(encoding="utf-8") as file:
    countries = json.load(file)

  best_distance = math.inf
  for country, cities in countries.items():
    for city, *state, city_latitude, city_longitude in cities:
      state_name = "N/A"
      if state:
        state_name = state[0]

      if region is not None and [country, state_name] != region:
        continue

      distance = frogpilot_utilities.calculate_distance_to_point(latitude, longitude, city_latitude, city_longitude)
      if distance < best_distance:
        best_distance = distance
        location = city, country, state_name

  return location


def get_model_scores(params):
  model_scores = []

  for model_name, model_data in sorted((json.loads(params.get("ModelDrivesAndScores") or "{}")).items()):
    drives = int(model_data.get("Drives", 0) or 0)
    if drives <= 0:
      continue

    model_scores.append({
      "drives": drives,
      "model_name": frogpilot_utilities.clean_model_name(model_name),
      "score": round(model_data.get("Score", 0) or 0),
    })

  return model_scores


def send_stats(params, frogpilot_toggles, frogpilot_api):
  if not params.get_bool("FrogPilotTelemetryConfirmed"):
    return

  if frogpilot_toggles.car_make in ("body", "mock"):
    return

  payload = {
    "model_scores": [],
    "stats_schema_version": API_VERSION,
    "user_stats": {},
  }

  if frogpilot_toggles.frogpilot_telemetry:
    city, country, state = get_location(params.get("LastGPSPosition"))

    payload["model_scores"] = get_model_scores(params)
    payload["user_stats"] = {
      "calibrated_lateral_acceleration": params.get_float("CalibratedLateralAcceleration"),
      "car_params": get_car_params(params),
      "city": city,
      "country": country,
      "frogpilot_car_params": get_frogpilot_car_params(params),
      "frogpilot_stats": json.loads(params.get("FrogPilotStats") or "{}"),
      "state": state,
      "toggles": vars(frogpilot_toggles),
      "using_default_model": frogpilot_toggles.model == frogpilot_variables.DEFAULT_MODEL,
    }
  else:
    params.remove("SpeedLimitFillerUploadedHash")

  data = json.dumps(payload, separators=(",", ":"), sort_keys=True).encode()

  compressor = zlib.compressobj(level=9, wbits=-15, zdict=frogpilot_paths.STATS_DICTIONARY_PATH.read_bytes())
  body = compressor.compress(data) + compressor.flush()

  headers = {"Content-Encoding": "frogpilot-stats-1", "Content-Type": "application/json"}
  response = frogpilot_api.post("/v1/stats", data=body, headers=headers, timeout=30)
  if response is None or not 200 <= response.status_code < 300:
    status = "no_response" if response is None else response.status_code
    print(f"Error sending stats (status={status})")
