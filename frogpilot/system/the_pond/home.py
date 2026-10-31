import asyncio
import datetime
import json
import shutil

from aiohttp import web

from openpilot.common.conversions import Conversions as CV
from openpilot.system.hardware.hw import Paths
from openpilot.system.loggerd.deleter import MIN_BYTES, MIN_PERCENT

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import dashcam, utilities

routes = web.RouteTableDef()

def get_drive_stats(stats, is_metric):
  distance = stats.get("distance", 0)
  if is_metric:
    distance *= CV.MPH_TO_KPH

  return {"distance": int(distance), "drives": int(stats.get("routes", 0)), "hours": int(stats.get("minutes", 0) / 60)}


def get_frogpilot_drive_stats(stats, is_metric):
  meters = stats.get("FrogPilotMeters", 0)
  if is_metric:
    distance = meters * 0.001
  else:
    distance = meters * (CV.KPH_TO_MPH / 1000)

  return {"distance": int(distance), "drives": stats.get("FrogPilotDrives", 0), "hours": int(stats.get("FrogPilotSeconds", 0) / 3600)}


def get_favorite_speed(speeds):
  if not speeds:
    return None

  speed = max(sorted(speeds), key=speeds.get)
  return {"speed": float(speed), "time": speeds[speed]}


def get_frogpilot_stats(stats):
  frogpilot_stats = {}
  for key, stat_type in frogpilot_variables.FROGPILOT_STATS_TYPES.items():
    if key == "AEBEvents":
      total_events = stats.get("TotalEvents", {})
      value = total_events.get("stockAeb", 0) + total_events.get("fcw", 0)
    elif key == "CruiseSpeedTimes":
      value = get_favorite_speed(stats.get(key, {}))
    elif key == "CurrentMonthsMeters" and stats.get("Month") != datetime.datetime.now(datetime.UTC).month:
      value = 0
    elif stat_type == "parent":
      value = {name: amount for name, amount in stats.get(key, {}).items() if name.lower() != "unknown"}
    else:
      value = stats.get(key, 0)

    if key in ("DayTime", "NightTime") and value <= 0:
      continue

    frogpilot_stats[key] = {"type": stat_type, "value": value}
  return frogpilot_stats


def get_repository(remote):
  address = remote.removesuffix(".git").split("://")[-1].split("@")[-1]
  parts = address.replace(":", "/", 1).split("/")
  if len(parts) < 3:
    return None

  return {"owner": parts[1], "url": f"https://{'/'.join(parts[:3])}"}


def get_deletion_threshold():
  total = shutil.disk_usage(Paths.log_root()).total
  return min(100 - MIN_PERCENT, (total - MIN_BYTES) / total * 100)


@routes.delete("/api/frogpilot_stats")
async def reset_frogpilot_stats(request):
  the_pond = request.app[utilities.THE_POND]
  if the_pond.sm["deviceState"].started:
    raise web.HTTPConflict(text="car_on")

  the_pond.params.remove("FrogPilotStats")
  return web.Response(status=204)


def get_last_drive(recording_route):
  for route, segments in sorted(dashcam.get_routes().items(), reverse=True):
    if route != recording_route:
      return dashcam.get_route_info(route, segments, False)
  return None


@routes.get("/api/home")
async def get_home(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params

  drive_stats = json.loads(params.get("ApiCache_DriveStatsKonik" if frogpilot_utilities.use_konik_server() else "ApiCache_DriveStats") or "{}")
  frogpilot_stats = json.loads(params.get("FrogPilotStats") or "{}")
  is_metric = params.get_bool("IsMetric")

  return web.json_response({
    "drive_stats": {
      "all": get_drive_stats(drive_stats.get("all", {}), is_metric),
      "frogpilot": get_frogpilot_drive_stats(frogpilot_stats, is_metric),
      "week": get_drive_stats(drive_stats.get("week", {}), is_metric),
    },
    "frogpilot_stats": get_frogpilot_stats(frogpilot_stats),
    "is_metric": is_metric,
    "last_drive": await asyncio.to_thread(get_last_drive, dashcam.get_recording_route(the_pond)),
    "software": {
      "repository": get_repository(params.get("GitRemote", encoding="utf-8") or ""),
    },
    "storage": {
      "deletion_threshold": get_deletion_threshold(),
      "free_space": the_pond.sm["frogpilotDeviceState"].freeSpace,
      "used_space": the_pond.sm["frogpilotDeviceState"].usedSpace,
    },
  })
