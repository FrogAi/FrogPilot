import asyncio
import time

from aiohttp import web

from openpilot.common.basedir import BASEDIR
from openpilot.selfdrive.selfdrived.alertmanager import OFFROAD_ALERTS
from openpilot.system.hardware import HARDWARE

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

STORAGE_CACHE_TIME = 10

routes = web.RouteTableDef()

def get_storage_usage():
  return {
    "dashcam": {log_root.name: frogpilot_utilities.get_directory_size(log_root) for log_root in frogpilot_paths.LOG_ROOTS},
    "frogpilot_backups": frogpilot_utilities.get_directory_size(frogpilot_paths.FROGPILOT_BACKUPS),
    "maps": frogpilot_utilities.get_directory_size(frogpilot_paths.MAPS_PATH),
    "models": frogpilot_utilities.get_directory_size(frogpilot_paths.MODELS_PATH.parent),
    "screen_recordings": frogpilot_utilities.get_directory_size(frogpilot_paths.SCREEN_RECORDINGS_PATH),
    "software": frogpilot_utilities.get_directory_size(BASEDIR),
    "the_pond": frogpilot_utilities.get_directory_size(frogpilot_paths.THE_POND_PATH),
    "themes": frogpilot_utilities.get_directory_size(frogpilot_paths.THEME_SAVE_PATH),
  }


@routes.get("/api/device")
async def get_device(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params

  device_state = the_pond.sm["deviceState"]
  frogpilot_device_state = the_pond.sm["frogpilotDeviceState"]

  alerts = []
  for key in OFFROAD_ALERTS:
    alert = params.get(key)
    if alert is not None:
      alerts.append({"key": key, "severity": alert["severity"], "text": OFFROAD_ALERTS[key]["text"].replace("%1", alert["extra"])})

  return web.json_response({
    "alerts": sorted(alerts, key=lambda alert: (-alert["severity"], alert["key"])),
    "branch": params.get("GitBranch"),
    "commit": params.get("GitCommit"),
    "cpu_usage": list(device_state.cpuUsagePercent),
    "gpu_usage": device_state.gpuUsagePercent,
    "model": frogpilot_variables.DEVICE_MODELS[HARDWARE.get_device_type()],
    "network": {"metered": device_state.networkMetered, "strength": str(device_state.networkStrength), "type": str(device_state.networkType)},
    "storage": {"free_space": frogpilot_device_state.freeSpace, "used_space": frogpilot_device_state.usedSpace},
    "temperature": device_state.maxTempC,
    "thermal_status": str(device_state.thermalStatus),
    "version": params.get("Version"),
    "voltage": HARDWARE.get_voltage() / 1000,
  })


@routes.post("/api/device/power_off")
async def power_off(request):
  the_pond = request.app[utilities.THE_POND]
  if the_pond.sm["deviceState"].started:
    raise web.HTTPConflict(text="car_on")

  the_pond.params.put_bool("DoShutdown", True)
  return web.Response(status=204)


@routes.post("/api/device/reboot")
async def reboot(request):
  the_pond = request.app[utilities.THE_POND]
  if frogpilot_utilities.is_openpilot_steering(the_pond.sm):
    raise web.HTTPConflict(text="openpilot_steering")

  the_pond.params.put_bool("DoReboot", True)
  return web.Response(status=204)


@routes.post("/api/device/soft_reboot")
async def soft_reboot(request):
  the_pond = request.app[utilities.THE_POND]
  if frogpilot_utilities.is_openpilot_steering(the_pond.sm):
    raise web.HTTPConflict(text="openpilot_steering")

  the_pond.params.put_bool("DoSoftReboot", True)
  return web.Response(status=204)


@routes.get("/api/device/storage")
async def get_storage(request):
  the_pond = request.app[utilities.THE_POND]
  if time.monotonic() >= the_pond.storage_usage_expiry:
    the_pond.storage_usage = await asyncio.to_thread(get_storage_usage)
    the_pond.storage_usage_expiry = time.monotonic() + STORAGE_CACHE_TIME
  return web.json_response(the_pond.storage_usage)
