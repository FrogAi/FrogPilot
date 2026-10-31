import aiohttp
import datetime
import json

from aiohttp import web

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

INVALID_KEY_STATUSES = {
  "amap": (),
  "mapbox_public": (401,),
  "mapbox_secret": (401,),
  "weather": (401, 403),
}

JSON_TEST_KINDS = ("amap", "weather")

KEYS = {
  "amap": "AMapKey1",
  "mapbox_public": "MapboxPublicKey",
  "mapbox_secret": "MapboxSecretKey",
  "weather": "WeatherToken",
}

TEST_TIMEOUT = 15
TEST_URLS = {
  "amap": frogpilot_variables.AMAP_KEY_TEST_URL,
  "mapbox_public": frogpilot_variables.MAPBOX_PUBLIC_KEY_TEST_URL,
  "mapbox_secret": frogpilot_variables.MAPBOX_SECRET_KEY_TEST_URL,
  "weather": frogpilot_variables.WEATHER_KEY_TEST_URL,
}

routes = web.RouteTableDef()

def amap_test_result(data, status):
  if not data:
    return {"result": "unreachable", "status": 0}

  if data["status"] == "1":
    result = "valid"
  elif data["infocode"] == frogpilot_variables.AMAP_INVALID_KEY_CODE:
    result = "invalid"
  elif data["infocode"] == frogpilot_variables.AMAP_WRONG_KEY_TYPE_CODE:
    result = "wrong_key_type"
  else:
    result = "failed"
  return {"message": data["info"], "result": result, "status": status}


def get_kind(request):
  kind = request.match_info["kind"]
  if kind not in KEYS:
    raise web.HTTPNotFound(text="not_found")
  return kind


def is_valid_integer(value):
  return isinstance(value, int) and not isinstance(value, bool)


def is_valid_weather(data):
  if not isinstance(data, dict) or not isinstance(data.get("data"), list) or not data["data"]:
    return False

  current = data["data"][0]
  if not isinstance(current, dict):
    return False

  for field in ("sunrise", "sunset"):
    if field in current and not is_valid_integer(current[field]):
      return False

  weather = current.get("weather")
  return isinstance(weather, list) and len(weather) > 0 and isinstance(weather[0], dict) and is_valid_integer(weather[0].get("id"))


def navigation_keys(the_pond):
  params = the_pond.params
  mapbox_secret = params.get("MapboxSecretKey")

  return {
    "amap_search": params.get_int("SearchInput") == frogpilot_variables.AMAP_SEARCH_INPUT,
    "map_reboot_required": bool(mapbox_secret) and mapbox_secret != the_pond.mapbox_secret_at_start,
    "mapbox_public": params.get("MapboxPublicKey", encoding="utf-8") or None,
    "mapbox_secret": bool(mapbox_secret),
  }


@routes.delete("/api/keys/{kind}")
async def delete_key(request):
  request.app[utilities.THE_POND].params.remove(KEYS[get_kind(request)])
  return web.Response(status=204)


@routes.get("/api/keys")
async def get_keys(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params

  now = datetime.datetime.now()
  mapbox_requests = json.loads(params.get("MapBoxRequests") or "{}")
  if mapbox_requests.get("month") == now.month and mapbox_requests.get("year", now.year) == now.year:
    request_count = mapbox_requests.get("total_requests", 0)
  else:
    request_count = 0

  return web.json_response({
    **navigation_keys(the_pond),
    "amap": bool(params.get("AMapKey1")),
    "mapbox_request_limit": frogpilot_variables.MAPBOX_MONTHLY_REQUEST_LIMIT,
    "mapbox_requests": request_count,
    "weather": bool(params.get("WeatherToken")),
  })


@routes.put("/api/keys/{kind}")
async def set_key(request):
  the_pond = request.app[utilities.THE_POND]
  kind = get_kind(request)

  key = (await utilities.read_json(request)).get("key")
  if not isinstance(key, str) or not key.strip():
    raise web.HTTPBadRequest(text="invalid_key")
  key = key.strip()

  if kind == "weather":
    if len(key) > frogpilot_variables.WEATHER_KEY_MAX_LENGTH:
      raise web.HTTPBadRequest(text="invalid_key")
  elif kind != "amap":
    if kind == "mapbox_public":
      prefix = frogpilot_variables.MAPBOX_PUBLIC_KEY_PREFIX
      other_prefix = frogpilot_variables.MAPBOX_SECRET_KEY_PREFIX
    else:
      prefix = frogpilot_variables.MAPBOX_SECRET_KEY_PREFIX
      other_prefix = frogpilot_variables.MAPBOX_PUBLIC_KEY_PREFIX

    if key.startswith(other_prefix):
      raise web.HTTPBadRequest(text="wrong_key_type")
    if not key.startswith(prefix):
      key = f"{prefix}{key}"
    if len(key) < frogpilot_variables.MAPBOX_KEY_MIN_LENGTH:
      raise web.HTTPBadRequest(text="invalid_key")

  the_pond.params.put(KEYS[kind], key)

  if kind == "mapbox_public":
    reboot_required = the_pond.sm["deviceState"].started
  else:
    reboot_required = kind == "mapbox_secret"
  return web.json_response({"reboot_required": reboot_required})


@routes.post("/api/keys/{kind}/test")
async def test_key(request):
  the_pond = request.app[utilities.THE_POND]
  kind = get_kind(request)

  key = the_pond.params.get(KEYS[kind], encoding="utf-8")
  if not key:
    raise web.HTTPNotFound(text="not_found")
  if not frogpilot_utilities.is_online(the_pond.sm["deviceState"]):
    raise web.HTTPConflict(text="offline")

  try:
    async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=TEST_TIMEOUT)) as session:
      async with session.get(TEST_URLS[kind].format(key), allow_redirects=False) as response:
        status = response.status

        data = None
        if kind in JSON_TEST_KINDS and (200 <= status < 300 or status in INVALID_KEY_STATUSES[kind]):
          data = await response.json(content_type=None)
  except (aiohttp.ClientError, TimeoutError, ValueError):
    return web.json_response({"result": "unreachable", "status": 0})

  if kind == "amap" and status < 400:
    return web.json_response(amap_test_result(data, status))

  message = data.get("message") if isinstance(data, dict) and status in INVALID_KEY_STATUSES[kind] else None

  if 200 <= status < 300 and (kind != "weather" or is_valid_weather(data)):
    result = "valid"
  elif status in INVALID_KEY_STATUSES[kind]:
    result = "invalid"
  elif status == 429:
    result = "rate_limited"
  elif 200 <= status < 300:
    result = "incomplete"
  else:
    result = "failed"
  return web.json_response({"message": message if isinstance(message, str) else None, "result": result, "status": status})
