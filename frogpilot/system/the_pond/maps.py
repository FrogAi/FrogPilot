import asyncio
import json
import os
import shutil

from aiohttp import web

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

SCHEDULES = (0, 1, 2)

DOWNLOAD = {}

routes = web.RouteTableDef()

def get_download(the_pond):
  downloading = is_downloading()
  progress = get_progress(the_pond.params)
  update_stamp = get_param_stamp(the_pond.params, "LastMapsUpdate")

  download_id = get_download_id()
  if downloading and DOWNLOAD.get("id") != download_id:
    DOWNLOAD.update(id=download_id, update_stamp=update_stamp)

  if progress:
    succeeded = progress["downloaded_files"] == progress["total_files"] > 0
  else:
    succeeded = update_stamp is not None and update_stamp != DOWNLOAD.get("update_stamp")

  return {"active": downloading and bool(progress), "downloaded_files": progress.get("downloaded_files", 0), "downloading": downloading,
          "succeeded": succeeded, "total_files": progress.get("total_files", 0)}


def get_download_id():
  return get_param_stamp(frogpilot_variables.params_memory, "OSMDownloadLocations")


def get_locations(regions):
  nations = set()
  states = set()
  for region in regions:
    if region.startswith(frogpilot_variables.MAP_COUNTRY_PREFIX):
      nations.add(region.removeprefix(frogpilot_variables.MAP_COUNTRY_PREFIX))
    else:
      states.add(region.removeprefix(frogpilot_variables.MAP_STATE_PREFIX))
  return json.dumps({"nations": sorted(nations), "states": sorted(states)})


def get_param_stamp(params, key):
  try:
    return os.stat(params.get_param_path(key)).st_mtime_ns
  except FileNotFoundError:
    return None


def get_progress(params):
  return json.loads(params.get("OSMDownloadProgress", encoding="utf-8") or "{}")


def get_region_names():
  names = {}
  for regions in frogpilot_variables.MAP_COUNTRIES.values():
    names.update({f"{frogpilot_variables.MAP_COUNTRY_PREFIX}{code}": name for code, name in regions.items()})
  for regions in frogpilot_variables.MAP_STATES.values():
    names.update({f"{frogpilot_variables.MAP_STATE_PREFIX}{code}": name for code, name in regions.items()})
  return names


def get_regions(maps_selected):
  maps_selected = json.loads(maps_selected or "{}")

  regions = [f"{frogpilot_variables.MAP_COUNTRY_PREFIX}{code}" for code in maps_selected.get("nations", [])]
  regions += [f"{frogpilot_variables.MAP_STATE_PREFIX}{code}" for code in maps_selected.get("states", [])]

  region_names = get_region_names()
  return [region for region in regions if region in region_names]


def get_selected_regions(params):
  return get_regions(params.get("MapsSelected", encoding="utf-8"))


def is_downloading():
  return bool(frogpilot_variables.params_memory.get("OSMDownloadLocations"))


@routes.post("/api/maps/cancel")
async def cancel_download(request):
  the_pond = request.app[utilities.THE_POND]
  the_pond.params.remove("OSMDownloadProgress")
  frogpilot_variables.params_memory.remove("OSMDownloadLocations")
  frogpilot_variables.params_memory.put_bool("RestartMapd", True)
  return web.Response(status=204)


@routes.post("/api/maps/download")
async def download_maps(request):
  the_pond = request.app[utilities.THE_POND]
  regions = get_selected_regions(the_pond.params)
  if not regions:
    raise web.HTTPConflict(text="no_maps_selected")
  if not frogpilot_utilities.is_online(the_pond.sm["deviceState"]):
    raise web.HTTPConflict(text="offline")
  if is_downloading():
    raise web.HTTPConflict(text="busy")

  update_stamp = get_param_stamp(the_pond.params, "LastMapsUpdate")
  frogpilot_variables.params_memory.put("OSMDownloadLocations", get_locations(regions))
  DOWNLOAD.update(id=get_download_id(), update_stamp=update_stamp)
  return web.Response(status=204)


@routes.get("/api/maps/download")
async def get_download_status(request):
  return web.json_response(get_download(request.app[utilities.THE_POND]))


@routes.get("/api/maps")
async def get_maps(request):
  the_pond = request.app[utilities.THE_POND]

  return web.json_response({
    "countries": frogpilot_variables.MAP_COUNTRIES,
    "country_prefix": frogpilot_variables.MAP_COUNTRY_PREFIX,
    "download": get_download(the_pond),
    "last_updated": the_pond.params.get("LastMapsUpdate", encoding="utf-8"),
    "schedule": the_pond.params.get_int("PreferredSchedule"),
    "selected": get_selected_regions(the_pond.params),
    "state_prefix": frogpilot_variables.MAP_STATE_PREFIX,
    "states": frogpilot_variables.MAP_STATES,
    "storage_used": await asyncio.to_thread(frogpilot_utilities.get_directory_size, frogpilot_paths.MAPS_PATH),
  })


@routes.delete("/api/maps")
async def remove_maps(request):
  the_pond = request.app[utilities.THE_POND]
  if is_downloading():
    raise web.HTTPConflict(text="busy")

  the_pond.params.remove("MapsSelected")
  the_pond.params.remove("LastMapsUpdate")
  await asyncio.to_thread(shutil.rmtree, frogpilot_paths.MAPS_PATH, True)
  return web.Response(status=204)


@routes.put("/api/maps/schedule")
async def set_schedule(request):
  schedule = (await utilities.read_json(request)).get("schedule")
  if type(schedule) is not int or schedule not in SCHEDULES:
    raise web.HTTPBadRequest(text="invalid_schedule")

  request.app[utilities.THE_POND].params.put("PreferredSchedule", str(schedule))
  return web.Response(status=204)


@routes.put("/api/maps/selection")
async def set_selection(request):
  params = request.app[utilities.THE_POND].params

  regions = (await utilities.read_json(request)).get("regions")
  region_names = get_region_names()
  if not isinstance(regions, list) or any(not isinstance(region, str) or region not in region_names for region in regions):
    raise web.HTTPBadRequest(text="invalid_regions")

  if regions:
    params.put("MapsSelected", get_locations(regions))
  else:
    params.remove("MapsSelected")
  return web.Response(status=204)
