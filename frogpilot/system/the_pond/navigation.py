import aiohttp
import json
import math
import re
import secrets
import time

from aiohttp import web

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import keys, utilities

AMAP_LIMIT_CODES = ("10003", "10004", "10010", "10014", "10015", "10019", "10020", "10021", "10029", "10044", "10045", "40000", "40003")
AMAP_TIMEOUT = 15

FAVORITE_TYPES = {"home": "is_home", "work": "is_work"}

GCJ02_ECCENTRICITY_SQUARED = 0.00669342162296594323
GCJ02_SEMI_MAJOR_AXIS = 6378245.0

MAX_FAVORITES = 100
MAX_NAME_LENGTH = 256

UNNAMED_PLACE = "Unnamed place"
UNSAFE_NAME_PATTERN = re.compile(r"[\x00-\x1f\x7f-\x9f  \ud800-\udfff]")

routes = web.RouteTableDef()

def gcj02_latitude_shift(x, y):
  shift = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * x * y + 0.2 * math.sqrt(abs(x))
  shift += (20.0 * math.sin(6.0 * x * math.pi) + 20.0 * math.sin(2.0 * x * math.pi)) * 2.0 / 3.0
  shift += (20.0 * math.sin(y * math.pi) + 40.0 * math.sin(y / 3.0 * math.pi)) * 2.0 / 3.0
  shift += (160.0 * math.sin(y / 12.0 * math.pi) + 320.0 * math.sin(y * math.pi / 30.0)) * 2.0 / 3.0
  return shift


def gcj02_longitude_shift(x, y):
  shift = 300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * x * y + 0.1 * math.sqrt(abs(x))
  shift += (20.0 * math.sin(6.0 * x * math.pi) + 20.0 * math.sin(2.0 * x * math.pi)) * 2.0 / 3.0
  shift += (20.0 * math.sin(x * math.pi) + 40.0 * math.sin(x / 3.0 * math.pi)) * 2.0 / 3.0
  shift += (150.0 * math.sin(x / 12.0 * math.pi) + 300.0 * math.sin(x / 30.0 * math.pi)) * 2.0 / 3.0
  return shift


def gcj02_to_wgs84(longitude, latitude):
  latitude_radians = math.radians(latitude)
  radius_factor = 1 - GCJ02_ECCENTRICITY_SQUARED * math.sin(latitude_radians) ** 2
  radius_factor_root = math.sqrt(radius_factor)

  latitude_shift = gcj02_latitude_shift(longitude - 105.0, latitude - 35.0) * 180.0
  latitude_shift /= GCJ02_SEMI_MAJOR_AXIS * (1 - GCJ02_ECCENTRICITY_SQUARED) / (radius_factor * radius_factor_root) * math.pi

  longitude_shift = gcj02_longitude_shift(longitude - 105.0, latitude - 35.0) * 180.0
  longitude_shift /= GCJ02_SEMI_MAJOR_AXIS / radius_factor_root * math.cos(latitude_radians) * math.pi

  return longitude - longitude_shift, latitude - latitude_shift


def load_destination(params):
  try:
    destination = json.loads(params.get("NavDestination") or "null")
  except ValueError:
    return None

  if not isinstance(destination, dict):
    return None

  if type(destination.get("latitude")) not in (int, float) or type(destination.get("longitude")) not in (int, float):
    return None

  return destination


def load_favorites(params):
  favorites = json.loads(params.get("FavoriteDestinations") or "[]")
  changed = False

  for favorite in favorites:
    if "id" not in favorite:
      favorite["id"] = secrets.token_hex(4)
      changed = True

    if not favorite.get("name"):
      favorite["name"] = UNNAMED_PLACE
      changed = True

  if changed:
    params.put("FavoriteDestinations", json.dumps(favorites))

  return favorites


def load_connect_places(params):
  locations = json.loads(params.get("ApiCache_NavDestinations") or "null")
  if isinstance(locations, dict):
    locations = locations.get("locations")
  if not isinstance(locations, list):
    return [], []

  connect_favorites = []
  recents = []

  for location in sorted(locations, key=lambda location: location.get("modified") or "", reverse=True):
    place = {"address": location.get("place_details") or "", "latitude": location["latitude"], "longitude": location["longitude"],
             "name": location.get("place_name") or UNNAMED_PLACE}

    if location.get("save_type") == "favorite":
      connect_favorites.append({**place, "is_home": location.get("label") == "home", "is_work": location.get("label") == "work"})
    elif location.get("save_type") == "recent":
      recents.append(place)

  return connect_favorites, recents


def read_amap_tip(tip):
  longitude, latitude = tip["location"].split(",")
  longitude, latitude = gcj02_to_wgs84(float(longitude), float(latitude))

  address = ""
  for part in (tip.get("district"), tip.get("address")):
    if part:
      address += part

  return {"address": address, "latitude": latitude, "longitude": longitude, "name": tip["name"]}


def read_coordinate(value, limit):
  if type(value) not in (int, float) or not -limit <= value <= limit:
    raise web.HTTPBadRequest(text="invalid_location")
  return value


def read_name(name):
  if not isinstance(name, str) or not 0 < len(name.strip()) <= MAX_NAME_LENGTH or UNSAFE_NAME_PATTERN.search(name.strip()):
    raise web.HTTPBadRequest(text="invalid_name")
  return name.strip()


def read_place(body):
  return {"latitude": read_coordinate(body.get("latitude"), 90), "longitude": read_coordinate(body.get("longitude"), 180), "name": read_name(body.get("name"))}


def read_type(body):
  favorite_type = body.get("type")
  if favorite_type is None:
    return None

  if not isinstance(favorite_type, str) or favorite_type not in FAVORITE_TYPES:
    raise web.HTTPBadRequest(text="invalid_type")

  return favorite_type


def route_state(the_pond):
  sm = the_pond.sm

  if sm.logMonoTime["navRoute"] < the_pond.destination_set_time:
    return "finding"

  if sm.seen["navInstruction"] and sm.valid["navInstruction"]:
    return "routed"

  if sm.seen["navRoute"] and len(sm["navRoute"].coordinates) == 0:
    return "failed"

  return "finding"


def set_type(favorites, favorite, favorite_type):
  for key in FAVORITE_TYPES.values():
    favorite.pop(key, None)

  if favorite_type is not None:
    key = FAVORITE_TYPES[favorite_type]

    for other in favorites:
      other.pop(key, None)

    favorite[key] = True


@routes.post("/api/navigation/favorites")
async def add_favorite(request):
  params = request.app[utilities.THE_POND].params
  body = await utilities.read_json(request)

  favorites = load_favorites(params)
  if len(favorites) >= MAX_FAVORITES:
    raise web.HTTPConflict(text="too_many_favorites")

  favorite = {**read_place(body), "id": secrets.token_hex(4)}
  favorites.append(favorite)
  params.put("FavoriteDestinations", json.dumps(favorites))
  return web.json_response({"id": favorite["id"]})


@routes.delete("/api/navigation/destination")
async def cancel_destination(request):
  params = request.app[utilities.THE_POND].params
  params.remove("NavDestination")
  params.remove("NavDestinationWaypoints")
  return web.Response(status=204)


@routes.delete("/api/navigation/favorites/{id}")
async def delete_favorite(request):
  params = request.app[utilities.THE_POND].params

  favorites = load_favorites(params)
  remaining = [favorite for favorite in favorites if favorite["id"] != request.match_info["id"]]
  if len(remaining) == len(favorites):
    raise web.HTTPNotFound(text="not_found")

  params.put("FavoriteDestinations", json.dumps(remaining))
  return web.Response(status=204)


@routes.get("/api/navigation")
async def get_navigation(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params

  connect_favorites, recents = load_connect_places(params)
  destination = load_destination(params)
  last_position = params.get("LastGPSPosition")

  route = None
  if destination is not None and the_pond.sm["deviceState"].started:
    route = route_state(the_pond)

  return web.json_response({
    **keys.navigation_keys(the_pond),
    "connect_favorites": connect_favorites,
    "destination": destination,
    "favorites": load_favorites(params),
    "is_metric": params.get_bool("IsMetric"),
    "last_position": json.loads(last_position) if last_position else None,
    "recents": recents,
    "route": route,
  })


@routes.get("/api/navigation/amap_search")
async def search_amap(request):
  the_pond = request.app[utilities.THE_POND]

  key = the_pond.params.get("AMapKey1", encoding="utf-8")
  if not key:
    raise web.HTTPConflict(text="no_amap_key")
  if not frogpilot_utilities.is_online(the_pond.sm["deviceState"]):
    raise web.HTTPConflict(text="offline")

  parameters = {"datatype": "poi", "key": key, "keywords": request.query["query"]}

  try:
    async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=AMAP_TIMEOUT)) as session:
      async with session.get(frogpilot_variables.AMAP_INPUT_TIPS_URL, allow_redirects=False, params=parameters, raise_for_status=True) as response:
        data = await response.json(content_type=None)
  except (aiohttp.ClientError, TimeoutError, ValueError):
    raise web.HTTPBadGateway(text="amap_unreachable") from None

  if not data or (data["status"] == "1" and "tips" not in data):
    raise web.HTTPBadGateway(text="amap_unreachable")

  if data["status"] != "1":
    if data["infocode"] == frogpilot_variables.AMAP_INVALID_KEY_CODE:
      raise web.HTTPBadGateway(text="amap_invalid_key")
    if data["infocode"] in AMAP_LIMIT_CODES:
      raise web.HTTPBadGateway(text="amap_over_limit")
    if data["infocode"] == frogpilot_variables.AMAP_WRONG_KEY_TYPE_CODE:
      raise web.HTTPBadGateway(text="amap_wrong_key_type")
    raise web.HTTPBadGateway(text=f"Amap said {data['info']}")

  places = []
  for tip in data["tips"]:
    if tip.get("location"):
      places.append(read_amap_tip(tip))

  return web.json_response(places)


@routes.put("/api/navigation/destination")
async def set_destination(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params
  place = read_place(await utilities.read_json(request))

  destination = load_destination(params)
  if destination is None or destination["latitude"] != place["latitude"] or destination["longitude"] != place["longitude"]:
    the_pond.destination_set_time = time.monotonic_ns()

  params.remove("NavDestinationWaypoints")
  params.put("NavDestination", json.dumps({"latitude": place["latitude"], "longitude": place["longitude"], "place_name": place["name"]}))
  return web.Response(status=204)


@routes.put("/api/navigation/favorites/{id}")
async def update_favorite(request):
  params = request.app[utilities.THE_POND].params
  body = await utilities.read_json(request)

  favorites = load_favorites(params)
  favorite = next((favorite for favorite in favorites if favorite["id"] == request.match_info["id"]), None)
  if favorite is None:
    raise web.HTTPNotFound(text="not_found")

  if "latitude" in body:
    favorite["latitude"] = read_coordinate(body["latitude"], 90)

  if "longitude" in body:
    favorite["longitude"] = read_coordinate(body["longitude"], 180)

  if "name" in body:
    favorite["name"] = read_name(body["name"])

  if "type" in body:
    set_type(favorites, favorite, read_type(body))

  params.put("FavoriteDestinations", json.dumps(favorites))
  return web.Response(status=204)
