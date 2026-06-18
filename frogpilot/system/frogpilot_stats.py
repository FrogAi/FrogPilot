from cereal import car, custom

from openpilot.frogpilot.assets.city_lookup import get_location
from openpilot.frogpilot.common import frogpilot_api, frogpilot_utilities


STATS_PAYLOAD_SCHEMA_VERSION = 1


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


def send_stats(params, frogpilot_toggles):
  if not frogpilot_toggles.frogpilot_telemetry:
    return

  if frogpilot_toggles.car_make == "mock":
    return

  if not frogpilot_api.get_token() or not params.get("FrogPilotDongleId"):
    return

  city, country, state = get_location(params.get("LastGPSPosition"))

  payload = {
    "model_scores": get_model_scores(params),
    "stats_schema_version": STATS_PAYLOAD_SCHEMA_VERSION,
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
  if response is not None and response.status_code == 204:
    print("Successfully sent FrogPilot stats!")
  else:
    status = response.status_code if response is not None else "unavailable"
    print(f"Failed to send stats: {status}")
