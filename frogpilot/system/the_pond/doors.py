from aiohttp import web

from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

routes = web.RouteTableDef()

def is_toyota(the_pond):
  return frogpilot_variables.get_frogpilot_toggles(the_pond.sm).car_make == "toyota"


@routes.get("/api/doors")
async def get_doors(request):
  the_pond = request.app[utilities.THE_POND]
  process_state = the_pond.sm["frogpilotProcessState"]
  return web.json_response({
    "available": is_toyota(the_pond),
    "lock_status": str(the_pond.sm["frogpilotCarState"].doorLockStatus) if the_pond.sm.alive["frogpilotCarState"] else "unavailable",
    "request_time": process_state.doorCommandRequestTime,
    "status": process_state.doorCommand,
  })


@routes.post("/api/doors")
async def send_door_command(request):
  the_pond = request.app[utilities.THE_POND]

  lock = (await utilities.read_json(request)).get("lock")
  if not isinstance(lock, bool):
    raise web.HTTPBadRequest(text="invalid_command")
  if not is_toyota(the_pond):
    raise web.HTTPNotFound(text="not_found")
  if the_pond.sm["deviceState"].started:
    raise web.HTTPConflict(text="car_on")

  return web.json_response({"request_time": the_pond.send_request("lockDoors", lock)})
