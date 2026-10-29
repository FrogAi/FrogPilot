import asyncio
import shutil

from aiohttp import web

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

routes = web.RouteTableDef()

def get_blacklist(the_pond):
  return [model_id for model_id in the_pond.params.get("BlacklistedModels").split(",") if model_id]


def download_error(the_pond, missing_models):
  if not missing_models:
    return "all_downloaded"
  if not is_online(the_pond):
    return "offline"
  if is_downloading(the_pond):
    return "busy"
  return None


def get_downloaded_models():
  return [model["id"] for model in frogpilot_variables.get_models() if (frogpilot_paths.MODELS_PATH / model["id"]).is_dir()]


def get_missing_models():
  return [model["id"] for model in frogpilot_variables.get_models() if not (frogpilot_paths.MODELS_PATH / model["id"]).is_dir()]


def get_deletable_models(the_pond):
  in_use = frogpilot_variables.get_frogpilot_toggles(the_pond.sm).model
  return [model_id for model_id in get_downloaded_models() if model_id not in (the_pond.params.get("DrivingModel"), in_use)]


def is_downloading(the_pond):
  return the_pond.sm["frogpilotProcessState"].downloadingModels


def is_online(the_pond):
  return frogpilot_utilities.is_online(the_pond.sm["deviceState"])


@routes.post("/api/models/cancel")
async def cancel_download(request):
  request.app[utilities.THE_POND].send_request("cancelModelDownload")
  return web.Response(status=204)


@routes.delete("/api/models")
async def delete_models(request):
  the_pond = request.app[utilities.THE_POND]

  model_ids = (await utilities.read_json(request)).get("ids")
  deletable_models = get_deletable_models(the_pond)
  if not isinstance(model_ids, list) or not model_ids or any(model_id not in deletable_models for model_id in model_ids):
    raise web.HTTPBadRequest(text="invalid_models")
  if is_downloading(the_pond):
    raise web.HTTPConflict(text="busy")

  for model_id in dict.fromkeys(model_ids):
    await asyncio.to_thread(shutil.rmtree, frogpilot_paths.MODELS_PATH / model_id)

  the_pond.send_request("updateToggles")
  return web.Response(status=204)


@routes.post("/api/models/download")
async def download_models(request):
  the_pond = request.app[utilities.THE_POND]

  model_ids = (await utilities.read_json(request)).get("ids")
  missing_models = get_missing_models()
  if model_ids is not None:
    if not isinstance(model_ids, list) or not model_ids or any(model_id not in missing_models for model_id in model_ids):
      raise web.HTTPBadRequest(text="invalid_models")

  error = download_error(the_pond, missing_models)
  if error is not None:
    raise web.HTTPConflict(text=error)

  if model_ids is None:
    request_time = the_pond.send_request("downloadAllModels")
  else:
    request_time = the_pond.send_request("downloadModels", model_ids)
  return web.json_response({"request_time": request_time})


@routes.get("/api/models")
async def get_models(request):
  the_pond = request.app[utilities.THE_POND]
  frogpilot_toggles = frogpilot_variables.get_frogpilot_toggles(the_pond.sm)
  process_state = the_pond.sm["frogpilotProcessState"]

  downloaded_models = get_downloaded_models()
  ratings = the_pond.params.get("ModelDrivesAndScores")

  models = []
  for model in frogpilot_variables.get_models():
    rating = ratings.get(model["name"], {})
    models.append({
      "downloaded": model["id"] in downloaded_models,
      "drives": rating.get("Drives", 0),
      "id": model["id"],
      "name": model["name"],
      "score": rating.get("Score"),
    })

  default_rating = ratings.get(frogpilot_variables.DEFAULT_MODEL["name"], {})
  randomizer_changeable = frogpilot_toggles.tuning_level >= the_pond.params.get_tuning_level("ModelRandomizer")
  selected_model = the_pond.params.get("DrivingModel")

  return web.json_response({
    "blacklist": get_blacklist(the_pond),
    "default": {"drives": default_rating.get("Drives", 0), "id": frogpilot_variables.DEFAULT_MODEL["id"],
                "name": frogpilot_variables.DEFAULT_MODEL["name"], "score": default_rating.get("Score")},
    "deletable": get_deletable_models(the_pond),
    "download": {"downloading": is_downloading(the_pond), "progress": process_state.modelDownloadProgress,
                 "request_time": process_state.modelDownloadRequestTime},
    "in_use": frogpilot_toggles.model,
    "models": models,
    "online": is_online(the_pond),
    "randomizer": randomizer_changeable and the_pond.params.get_bool("ModelRandomizer"),
    "randomizer_changeable": randomizer_changeable,
    "selected": selected_model if selected_model in downloaded_models else frogpilot_variables.DEFAULT_MODEL["id"],
  })


@routes.delete("/api/models/ratings")
async def reset_ratings(request):
  request.app[utilities.THE_POND].params.remove("ModelDrivesAndScores")
  return web.Response(status=204)


@routes.put("/api/models/blacklist")
async def set_blacklist(request):
  the_pond = request.app[utilities.THE_POND]

  model_ids = (await utilities.read_json(request)).get("ids")
  allowed_models = [frogpilot_variables.DEFAULT_MODEL["id"], *get_downloaded_models(), *get_blacklist(the_pond)]
  if not isinstance(model_ids, list) or any(not isinstance(model_id, str) or model_id not in allowed_models for model_id in model_ids):
    raise web.HTTPBadRequest(text="invalid_models")

  the_pond.params.put("BlacklistedModels", ",".join(dict.fromkeys(model_ids)))
  the_pond.send_request("updateToggles")
  return web.Response(status=204)


@routes.put("/api/models/randomizer")
async def set_randomizer(request):
  the_pond = request.app[utilities.THE_POND]

  enabled = (await utilities.read_json(request)).get("enabled")
  if not isinstance(enabled, bool):
    raise web.HTTPBadRequest(text="invalid_enabled")

  the_pond.params.put_bool("ModelRandomizer", enabled)
  the_pond.send_request("updateToggles")
  return web.Response(status=204)


@routes.put("/api/models/selected")
async def select_model(request):
  the_pond = request.app[utilities.THE_POND]

  model_id = (await utilities.read_json(request)).get("id")
  if model_id != frogpilot_variables.DEFAULT_MODEL["id"] and model_id not in get_downloaded_models():
    raise web.HTTPBadRequest(text="invalid_model")

  the_pond.params.put("DrivingModel", model_id)
  the_pond.send_request("updateToggles")
  return web.json_response({"reboot_required": the_pond.sm["deviceState"].started})
