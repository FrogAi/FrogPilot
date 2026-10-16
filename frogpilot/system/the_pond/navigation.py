import json
import secrets

from aiohttp import web

from openpilot.frogpilot.system.the_pond import utilities

FAVORITE_TYPES = ("home", "work")

MAX_FAVORITES = 100
MAX_NAME_LENGTH = 256

routes = web.RouteTableDef()

def read_place(body):
  name = body.get("name")
  latitude = body.get("latitude")
  longitude = body.get("longitude")

  if not isinstance(name, str) or not 0 < len(name.strip()) <= MAX_NAME_LENGTH or not name.strip().isprintable():
    raise web.HTTPBadRequest(text="invalid_name")
  if type(latitude) not in (int, float) or not -90 <= latitude <= 90:
    raise web.HTTPBadRequest(text="invalid_location")
  if type(longitude) not in (int, float) or not -180 <= longitude <= 180:
    raise web.HTTPBadRequest(text="invalid_location")
  return {"latitude": latitude, "longitude": longitude, "name": name.strip()}


def read_type(body):
  favorite_type = body.get("type")
  if favorite_type is not None and favorite_type not in FAVORITE_TYPES:
    raise web.HTTPBadRequest(text="invalid_type")
  return favorite_type


def save_favorites(params, favorites, favorite):
  if favorite["type"] is not None:
    for other in favorites:
      if other is not favorite and other["type"] == favorite["type"]:
        other["type"] = None
  params.put("FavoriteDestinations", favorites)


@routes.post("/api/navigation/favorites")
async def add_favorite(request):
  params = request.app[utilities.THE_POND].params
  body = await utilities.read_json(request)

  favorites = params.get("FavoriteDestinations")
  if len(favorites) >= MAX_FAVORITES:
    raise web.HTTPConflict(text="too_many_favorites")

  favorite = {**read_place(body), "id": secrets.token_hex(4), "type": read_type(body)}
  favorites.append(favorite)
  save_favorites(params, favorites, favorite)
  return web.json_response({"id": favorite["id"]})


@routes.delete("/api/navigation/destination")
async def cancel_destination(request):
  request.app[utilities.THE_POND].params.remove("NavDestination")
  return web.Response(status=204)


@routes.delete("/api/navigation/favorites/{id}")
async def delete_favorite(request):
  params = request.app[utilities.THE_POND].params

  favorites = params.get("FavoriteDestinations")
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
    "favorites": params.get("FavoriteDestinations"),
    "is_metric": params.get_bool("IsMetric"),
    "last_position": json.loads(last_position) if last_position else None,
  })


@routes.put("/api/navigation/destination")
async def set_destination(request):
  request.app[utilities.THE_POND].params.put("NavDestination", read_place(await utilities.read_json(request)))
  return web.Response(status=204)


@routes.put("/api/navigation/favorites/{id}")
async def update_favorite(request):
  params = request.app[utilities.THE_POND].params
  body = await utilities.read_json(request)

  favorites = params.get("FavoriteDestinations")
  favorite = next((favorite for favorite in favorites if favorite["id"] == request.match_info["id"]), None)
  if favorite is None:
    raise web.HTTPNotFound(text="not_found")

  favorite.update(read_place({**favorite, **body}))
  if "type" in body:
    favorite["type"] = read_type(body)
  save_favorites(params, favorites, favorite)
  return web.Response(status=204)
