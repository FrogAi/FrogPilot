from cereal import car, custom

from openpilot.frogpilot.common import frogpilot_utilities
from openpilot.frogpilot.common.frogpilot_api import API_VERSION
from openpilot.frogpilot.assets.city_lookup import get_location


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


def send_stats(params, frogpilot_toggles, frogpilot_api):
  if not frogpilot_toggles.frogpilot_telemetry:
    return

  if frogpilot_toggles.car_make == "mock":
    return

  city, country, state = get_location(params.get("LastGPSPosition"))

  payload = {
    "model_scores": get_model_scores(params),
    "stats_schema_version": API_VERSION,
    "user_stats": {
      "calibrated_lateral_acceleration": params.get("CalibratedLateralAcceleration"),
      "car_params": get_car_params(params),
      "city": city,
      "country": country,
      "frogpilot_car_params": get_frogpilot_car_params(params),
      "frogpilot_stats": params.get("FrogPilotStats") or {},
      "state": state,
      "toggles": vars(frogpilot_toggles),
      "using_default_model": (params.get("DrivingModel") or "").endswith("_default"),
    },
  }

  response = frogpilot_api.post("/v1/stats", json=payload, timeout=30)
  if response is None or not 200 <= response.status_code < 300:
    status = "no_response" if response is None else response.status_code
    print(f"Error sending stats (status={status})")
