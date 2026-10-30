import json
import secrets

from aiohttp import web

from openpilot.frogpilot.system.the_pond import utilities

FAVORITE_TYPES = {"home": "is_home", "work": "is_work"}

MAX_FAVORITES = 100
MAX_NAME_LENGTH = 256

UNNAMED_PLACE = "Unnamed place"

routes = web.RouteTableDef()

def load_favorites(params):
  favorites = params.get("FavoriteDestinations")
  changed = False

  for favorite in favorites:
    if "id" not in favorite:
      favorite["id"] = secrets.token_hex(4)
      changed = True

    if not favorite.get("name"):
      favorite["name"] = UNNAMED_PLACE
      changed = True

  if changed:
    params.put("FavoriteDestinations", favorites)

  return favorites


def read_coordinate(value, limit):
  if type(value) not in (int, float) or not -limit <= value <= limit:
    raise web.HTTPBadRequest(text="invalid_location")
  return value


def read_name(name):
  if not isinstance(name, str) or not 0 < len(name.strip()) <= MAX_NAME_LENGTH or not name.strip().isprintable():
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
  params.put("FavoriteDestinations", favorites)
  return web.json_response({"id": favorite["id"]})


@routes.delete("/api/navigation/destination")
async def cancel_destination(request):
  request.app[utilities.THE_POND].params.remove("NavDestination")
  return web.Response(status=204)


@routes.delete("/api/navigation/favorites/{id}")
async def delete_favorite(request):
  params = request.app[utilities.THE_POND].params

  favorites = load_favorites(params)
  remaining = [favorite for favorite in favorites if favorite["id"] != request.match_info["id"]]
  if len(remaining) == len(favorites):
    raise web.HTTPNotFound(text="not_found")

  params.put("FavoriteDestinations", remaining)
  return web.Response(status=204)


@routes.get("/api/navigation")
async def get_navigation(request):
  params = request.app[utilities.THE_POND].params
  last_position = params.get("LastGPSPosition")
  return web.json_response({
    "destination": params.get("NavDestination"),
    "favorites": load_favorites(params),
    "is_metric": params.get_bool("IsMetric"),
    "last_position": json.loads(last_position) if last_position else None,
  })


@routes.put("/api/navigation/destination")
async def set_destination(request):
  place = read_place(await utilities.read_json(request))
  request.app[utilities.THE_POND].params.put("NavDestination", {"latitude": place["latitude"], "longitude": place["longitude"], "place_name": place["name"]})
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

  params.put("FavoriteDestinations", favorites)
  return web.Response(status=204)
