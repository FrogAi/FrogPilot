import asyncio
import shutil

from aiohttp import web

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

SCHEDULES = (0, 1, 2)

routes = web.RouteTableDef()

def get_region_codes():
  codes = set()
  for regions in frogpilot_variables.MAP_COUNTRIES.values():
    codes.update(f"{frogpilot_variables.MAP_COUNTRY_PREFIX}{code}" for code in regions)
  for regions in frogpilot_variables.MAP_STATES.values():
    codes.update(f"{frogpilot_variables.MAP_STATE_PREFIX}{code}" for code in regions)
  return codes


def get_download(the_pond):
  progress = the_pond.sm["mapdExtendedOut"].downloadProgress
  process_state = the_pond.sm["frogpilotProcessState"]
  return {"active": progress.active, "cancelled": progress.cancelled, "downloaded_files": progress.downloadedFiles,
          "downloading": process_state.downloadingMaps, "request_time": process_state.downloadMapsRequestTime, "total_files": progress.totalFiles}


@routes.post("/api/maps/cancel")
async def cancel_download(request):
  request.app[utilities.THE_POND].send_request("cancelMapsDownload")
  return web.Response(status=204)


@routes.post("/api/maps/download")
async def download_maps(request):
  the_pond = request.app[utilities.THE_POND]
  if not the_pond.params.get("MapsSelected"):
    raise web.HTTPConflict(text="no_maps_selected")
  if not frogpilot_utilities.is_online(the_pond.sm["deviceState"]):
    raise web.HTTPConflict(text="offline")
  if the_pond.sm["frogpilotProcessState"].downloadingMaps:
    raise web.HTTPConflict(text="busy")

  return web.json_response({"request_time": the_pond.send_request("downloadMaps")})


@routes.get("/api/maps/download")
async def get_download_status(request):
  return web.json_response(get_download(request.app[utilities.THE_POND]))


@routes.get("/api/maps")
async def get_maps(request):
  the_pond = request.app[utilities.THE_POND]
  selected_maps = the_pond.params.get("MapsSelected")

  return web.json_response({
    "countries": frogpilot_variables.MAP_COUNTRIES,
    "country_prefix": frogpilot_variables.MAP_COUNTRY_PREFIX,
    "download": get_download(the_pond),
    "last_updated": the_pond.params.get("LastMapsUpdate"),
    "schedule": the_pond.params.get("PreferredSchedule"),
    "selected": selected_maps.split(",") if selected_maps else [],
    "state_prefix": frogpilot_variables.MAP_STATE_PREFIX,
    "states": frogpilot_variables.MAP_STATES,
    "storage_used": await asyncio.to_thread(frogpilot_utilities.get_directory_size, frogpilot_paths.MAPS_PATH),
  })


@routes.delete("/api/maps")
async def remove_maps(request):
  the_pond = request.app[utilities.THE_POND]
  if the_pond.sm["frogpilotProcessState"].downloadingMaps:
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

  request.app[utilities.THE_POND].params.put("PreferredSchedule", schedule)
  return web.Response(status=204)


@routes.put("/api/maps/selection")
async def set_selection(request):
  params = request.app[utilities.THE_POND].params

  regions = (await utilities.read_json(request)).get("regions")
  region_codes = get_region_codes()
  if not isinstance(regions, list) or any(not isinstance(region, str) or region not in region_codes for region in regions):
    raise web.HTTPBadRequest(text="invalid_regions")

  if regions:
    params.put("MapsSelected", ",".join(sorted(set(regions))))
  else:
    params.remove("MapsSelected")
  return web.Response(status=204)
