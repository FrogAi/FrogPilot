import asyncio
import datetime
import subprocess

from aiohttp import web

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

routes = web.RouteTableDef()

def get_branches(params):
  available_branches = params.get("UpdaterAvailableBranches")
  if available_branches is None:
    return []
  return frogpilot_utilities.get_target_branches(available_branches, params.get("GitBranch"))


def get_failed_count(params):
  return params.get("UpdateFailedCount") or 0


def is_updater_idle(params):
  return params.get("UpdaterState") == "idle"


async def signal_updater(signal):
  await asyncio.to_thread(subprocess.run, ["pkill", signal, "-f", frogpilot_variables.UPDATER_PROCESS], check=True)


@routes.get("/api/software")
async def get_software(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params

  last_update_time = params.get("LastUpdateTime")
  if last_update_time is not None:
    last_update_time = int(last_update_time.replace(tzinfo=datetime.UTC).timestamp())

  return web.json_response({
    "automatic_updates": params.get_bool("AutomaticUpdates"),
    "automatic_updates_changeable": params.get_bool("IsReleaseBranch") or frogpilot_utilities.is_FrogsGoMoo(),
    "branch": params.get("GitBranch"),
    "branches": get_branches(params),
    "current_description": params.get("UpdaterCurrentDescription") or f"{params.get('GitBranch')} / {params.get('GitCommit')[:7]}",
    "download_progress": params.get("UpdaterDownloadProgress"),
    "failed_count": get_failed_count(params),
    "fetch_available": params.get_bool("UpdaterFetchAvailable"),
    "last_update_time": last_update_time,
    "new_description": params.get("UpdaterNewDescription"),
    "new_release_notes": (params.get("UpdaterNewReleaseNotes") or b"").decode(),
    "state": params.get("UpdaterState"),
    "target_branch": params.get("UpdaterTargetBranch"),
    "update_available": params.get_bool("UpdateAvailable"),
  })


@routes.put("/api/software/automatic_updates")
async def set_automatic_updates(request):
  the_pond = request.app[utilities.THE_POND]

  enabled = (await utilities.read_json(request)).get("enabled")
  if not isinstance(enabled, bool):
    raise web.HTTPBadRequest(text="invalid_enabled")

  the_pond.params.put_bool("AutomaticUpdates", enabled)
  the_pond.send_request("updateToggles")
  return web.Response(status=204)


@routes.put("/api/software/branch")
async def select_branch(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params

  body = await utilities.read_json(request)
  branch = body.get("branch")
  download = body.get("download", False)
  if not isinstance(branch, str) or branch not in get_branches(params):
    raise web.HTTPBadRequest(text="invalid_branch")
  if not isinstance(download, bool):
    raise web.HTTPBadRequest(text="invalid_download")

  params.put("UpdaterTargetBranch", branch)

  if download and branch != params.get("GitBranch"):
    await signal_updater("-SIGHUP")
    the_pond.send_request("updateChecks")
  else:
    await signal_updater("-SIGUSR1")
  return web.Response(status=204)


@routes.post("/api/software/check")
async def check_for_updates(request):
  the_pond = request.app[utilities.THE_POND]
  if not frogpilot_utilities.is_online(the_pond.sm["deviceState"]):
    raise web.HTTPConflict(text="offline")
  if the_pond.sm["deviceState"].started:
    raise web.HTTPConflict(text="car_on")
  if not is_updater_idle(the_pond.params):
    raise web.HTTPConflict(text="updater_busy")

  await signal_updater("-SIGUSR1")
  return web.Response(status=204)


@routes.post("/api/software/download")
async def download_update(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params
  if not is_updater_idle(params):
    raise web.HTTPConflict(text="updater_busy")

  if get_failed_count(params) > 0 or not params.get_bool("UpdaterFetchAvailable"):
    await signal_updater("-SIGUSR1")
  else:
    await signal_updater("-SIGHUP")

  the_pond.send_request("updateChecks")
  return web.Response(status=204)


@routes.post("/api/software/install")
async def install_update(request):
  params = request.app[utilities.THE_POND].params
  if not params.get_bool("UpdateAvailable"):
    raise web.HTTPConflict(text="no_update")

  params.put_bool("DoReboot", True)
  return web.Response(status=204)
