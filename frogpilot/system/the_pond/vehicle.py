import json
import math

from aiohttp import web

from cereal import log, messaging

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system import frogpilot_stats
from openpilot.frogpilot.system.the_pond import utilities

MAX_KEY_NAME_LENGTH = 64

routes = web.RouteTableDef()

def get_calibration(params):
  pitch = None
  yaw = None
  msg_bytes = params.get("CalibrationParams")
  if msg_bytes:
    calibration = messaging.log_from_bytes(msg_bytes).liveCalibration
    if calibration.calStatus != log.LiveCalibrationData.Status.uncalibrated:
      pitch = math.degrees(calibration.rpyCalib[1])
      yaw = math.degrees(calibration.rpyCalib[2])

  return {"pitch": pitch, "yaw": yaw}


def get_capabilities(params, car_params, frogpilot_car_params):
  subaru_sng = params.get_int("TuningLevel") < frogpilot_variables.get_tuning_level("SubaruSNG") or params.get_bool("SubaruSNG")

  return {
    "blind_spot_support": car_params["enableBsm"],
    "openpilot_longitudinal": frogpilot_utilities.has_openpilot_longitudinal(params, car_params),
    "pedal_support": frogpilot_car_params.get("canUsePedal", False),
    "radar_support": not car_params["radarUnavailable"],
    "sdsu_support": frogpilot_car_params.get("canUseSDSU", False),
    "sng_support": car_params["autoResumeSng"] and (car_params["carName"] != "subaru" or subaru_sng),
  }


@routes.get("/api/vehicle")
async def get_vehicle(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params
  frogpilot_toggles = frogpilot_variables.get_frogpilot_toggles(the_pond.sm)

  car_params = frogpilot_stats.get_car_params(params)
  if car_params:
    capabilities = get_capabilities(params, car_params, frogpilot_stats.get_frogpilot_car_params(params))
  else:
    capabilities = None

  return web.json_response({
    "calibration": get_calibration(params),
    "capabilities": capabilities,
    "curve_speed": {
      "calibrated_lateral_acceleration": min(params.get_float("CalibratedLateralAcceleration"), frogpilot_toggles.maxLateralAccel),
      "calibration_progress": params.get_float("CalibrationProgress"),
      "max_lateral_acceleration": frogpilot_toggles.maxLateralAccel,
    },
    "fingerprint": car_params.get("carFingerprint"),
    "model": params.get("CarModelName", encoding="utf-8") or params.get("CarModel", encoding="utf-8"),
  })


@routes.delete("/api/vehicle/calibration")
async def reset_calibration(request):
  the_pond = request.app[utilities.THE_POND]
  if frogpilot_utilities.is_openpilot_steering(the_pond.sm):
    raise web.HTTPConflict(text="openpilot_steering")

  for key in frogpilot_variables.CALIBRATION_KEYS:
    the_pond.params.remove(key)
  return web.Response(status=204)


@routes.delete("/api/vehicle/curve_data")
async def reset_curve_data(request):
  the_pond = request.app[utilities.THE_POND]
  if the_pond.sm["deviceState"].started:
    raise web.HTTPConflict(text="car_on")

  utilities.reset_curve_data(the_pond.params)

  the_pond.send_request("updateToggles")
  return web.Response(status=204)


@routes.put("/api/secoc_key")
async def apply_secoc_key(request):
  the_pond = request.app[utilities.THE_POND]
  name = (await utilities.read_json(request)).get("name")

  for key in json.loads(the_pond.params.get("SecOCKeys") or "[]"):
    if key["name"] == name:
      the_pond.params.put("SecOCKey", key["value"])

      cache_key_path = frogpilot_paths.PARAMS_CACHE_PATH / "SecOCKey"
      if cache_key_path.is_file():
        cache_key_path.write_text(key["value"])
      return web.json_response({"restart_required": the_pond.sm["deviceState"].started})

  raise web.HTTPNotFound(text="not_found")


@routes.get("/api/secoc_keys")
async def get_secoc_keys(request):
  params = request.app[utilities.THE_POND].params
  applied_key = params.get("SecOCKey", encoding="utf-8")

  keys = []
  for key in json.loads(params.get("SecOCKeys") or "[]"):
    keys.append({"applied": key["value"] == applied_key, "name": key["name"]})

  return web.json_response({
    "keys": keys,
    "required": frogpilot_stats.get_car_params(params).get("secOcRequired", False),
  })


@routes.post("/api/secoc_keys")
async def save_secoc_key(request):
  params = request.app[utilities.THE_POND].params
  body = await utilities.read_json(request)

  name = body.get("name")
  if not isinstance(name, str):
    raise web.HTTPBadRequest(text="invalid_name")

  name = name.strip()
  if not 0 < len(name) <= MAX_KEY_NAME_LENGTH or not name.isprintable():
    raise web.HTTPBadRequest(text="invalid_name")

  value = body.get("value")
  if not isinstance(value, str) or frogpilot_variables.SECOC_KEY_PATTERN.fullmatch(value) is None:
    raise web.HTTPBadRequest(text="invalid_key")

  keys = json.loads(params.get("SecOCKeys") or "[]")
  if any(key["name"] == name for key in keys):
    raise web.HTTPConflict(text="name_taken")

  keys.append({"name": name, "value": value})
  params.put("SecOCKeys", json.dumps(keys))
  return web.Response(status=204)


@routes.delete("/api/secoc_keys/{name}")
async def delete_secoc_key(request):
  params = request.app[utilities.THE_POND].params

  keys = json.loads(params.get("SecOCKeys") or "[]")
  remaining = [key for key in keys if key["name"] != request.match_info["name"]]
  if len(remaining) == len(keys):
    raise web.HTTPNotFound(text="not_found")

  params.put("SecOCKeys", json.dumps(remaining))
  return web.Response(status=204)
