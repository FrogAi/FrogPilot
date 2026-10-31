import asyncio
import shutil

from aiohttp import web

from openpilot.frogpilot.assets.theme_manager import ThemeManager
from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

ACTIVE_FOLDERS = {
  "colors": "colors",
  "distance_icons": "distance_icons",
  "icons": "icons",
  "signals": "signals",
  "sounds": "sounds",
  "steering_wheels": "steering_wheel",
}

routes = web.RouteTableDef()

def get_component(request):
  component = request.match_info["component"]
  if component not in frogpilot_variables.THEME_PARAMS:
    raise web.HTTPNotFound(text="not_found")
  return component


def get_installed(component):
  if component == "steering_wheels":
    keys = [path.stem for path in frogpilot_paths.STEERING_WHEELS_PATH.glob("*") if path.is_file()]
  else:
    keys = [path.parent.name for path in frogpilot_paths.THEME_PACKS_PATH.glob(f"*/{component}") if path.is_dir()]
  return sorted(keys)


def get_theme_folder(component, key):
  if key == "active":
    return frogpilot_paths.ACTIVE_THEME_PATH / ACTIVE_FOLDERS[component]

  if component == "steering_wheels":
    if key == "none":
      return None
    wheel_path = ThemeManager.get_wheel_path(key)
    return wheel_path.parent if wheel_path is not None else None

  if key not in frogpilot_variables.BUILTIN_THEMES and key not in get_installed(component):
    raise web.HTTPNotFound(text="not_found")
  return ThemeManager.get_theme_asset_path(component, key)


def get_theme_files(component, key):
  folder = get_theme_folder(component, key)
  if folder is None or not folder.is_dir():
    return []

  if component == "steering_wheels" and key != "active":
    return [ThemeManager.get_wheel_path(key).name]
  return sorted(path.name for path in folder.iterdir() if path.is_file())


def is_random_themes(the_pond, frogpilot_toggles):
  return frogpilot_toggles.tuning_level >= frogpilot_variables.get_tuning_level("RandomThemes") and the_pond.params.get_bool("RandomThemes")


def is_selectable(component, key):
  if key in frogpilot_variables.BUILTIN_THEMES:
    return key not in frogpilot_variables.THEME_EXCLUDED_BUILTINS[component]
  return key in get_installed(component)


def get_download(process_state):
  return {
    "count": process_state.themeDownloadCount,
    "failed": process_state.themeDownloadFailedCount,
    "progress": process_state.themeDownloadProgress,
    "request_time": process_state.downloadThemeRequestTime,
    "succeeded": process_state.themeDownloadSuccessCount,
  }


@routes.post("/api/themes/download/cancel")
async def cancel_download(request):
  request.app[utilities.THE_POND].send_request("cancelThemeDownload")
  return web.Response(status=204)


@routes.delete("/api/themes/{component}/{key}")
async def delete_theme(request):
  params = request.app[utilities.THE_POND].params
  component = get_component(request)
  key = request.match_info["key"]

  if key not in get_installed(component):
    raise web.HTTPNotFound(text="not_found")
  if params.get(frogpilot_variables.THEME_PARAMS[component], encoding="utf-8") == key:
    raise web.HTTPConflict(text="in_use")

  if component == "steering_wheels":
    for path in frogpilot_paths.STEERING_WHEELS_PATH.glob("*"):
      if path.is_file() and path.stem == key:
        path.unlink()
  else:
    await asyncio.to_thread(shutil.rmtree, frogpilot_paths.THEME_PACKS_PATH / key / component)

  downloadable = {}
  for downloadable_component, param in frogpilot_variables.DOWNLOADABLE_PARAMS.items():
    downloadable[downloadable_component] = [name for name in (params.get(param, encoding="utf-8") or "").split(",") if name]
  if not key.endswith(frogpilot_variables.USER_CREATED_THEME_SUFFIX):
    downloadable[component].append(ThemeManager.format_name(key))
  await asyncio.to_thread(ThemeManager(params).update_theme_params, downloadable)
  return web.Response(status=204)


@routes.post("/api/themes/download")
async def download_themes(request):
  the_pond = request.app[utilities.THE_POND]

  body = await utilities.read_json(request)
  component = body.get("component")
  names = body.get("names")
  if component not in frogpilot_variables.DOWNLOADABLE_PARAMS:
    raise web.HTTPBadRequest(text="invalid_component")

  downloadable = (the_pond.params.get(frogpilot_variables.DOWNLOADABLE_PARAMS[component], encoding="utf-8") or "").split(",")
  if not isinstance(names, list) or not names or any(not isinstance(name, str) or name not in downloadable for name in names):
    raise web.HTTPBadRequest(text="invalid_names")
  if not frogpilot_utilities.is_online(the_pond.sm["deviceState"]):
    raise web.HTTPConflict(text="offline")

  themes = [{"component": component, "theme": ThemeManager.unformat_name(name)} for name in names]
  return web.json_response({"request_time": the_pond.send_request("downloadTheme", themes)})


@routes.get("/api/theme_files/{component}/{key}")
async def get_files(request):
  return web.json_response(get_theme_files(get_component(request), request.match_info["key"]))


@routes.get("/api/theme_files/{component}/{key}/{file}")
async def get_file(request):
  component = get_component(request)
  folder = get_theme_folder(component, request.match_info["key"])
  if folder is None or request.match_info["file"] not in get_theme_files(component, request.match_info["key"]):
    raise web.HTTPNotFound(text="not_found")
  return web.FileResponse(folder / request.match_info["file"])


@routes.get("/api/themes/download")
async def get_download_status(request):
  return web.json_response(get_download(request.app[utilities.THE_POND].sm["frogpilotProcessState"]))


@routes.get("/api/themes")
async def get_themes(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params
  frogpilot_toggles = frogpilot_variables.get_frogpilot_toggles(the_pond.sm)

  components = {}
  for component, param in frogpilot_variables.THEME_PARAMS.items():
    installed = []
    for key in get_installed(component):
      user_created = key.endswith(frogpilot_variables.USER_CREATED_THEME_SUFFIX)
      name = utilities.format_theme_name(key.removesuffix(frogpilot_variables.USER_CREATED_THEME_SUFFIX), component == "steering_wheels")
      installed.append({"key": key, "name": name, "user_created": user_created})

    components[component] = {
      "downloadable": [name for name in (params.get(frogpilot_variables.DOWNLOADABLE_PARAMS[component], encoding="utf-8") or "").split(",") if name],
      "excluded": sorted(frogpilot_variables.THEME_EXCLUDED_BUILTINS[component]),
      "installed": installed,
      "selected": params.get(param, encoding="utf-8"),
    }

  startup_alert = {}
  for key, maximum_length in frogpilot_variables.STARTUP_ALERT_MAX_LENGTHS.items():
    startup_alert[key] = {"current": params.get(key, encoding="utf-8"), "frogpilot": frogpilot_variables.get_default_value(key), "max_length": maximum_length,
                          "stock": frogpilot_variables.get_stock_value(key)}

  user_themes = sorted(path.name for path in frogpilot_paths.THEME_PACKS_PATH.glob(f"*{frogpilot_variables.USER_CREATED_THEME_SUFFIX}") if path.is_dir())

  return web.json_response({
    "builtins": frogpilot_variables.BUILTIN_THEMES,
    "components": components,
    "custom_themes": params.get_bool("CustomThemes"),
    "download": get_download(the_pond.sm["frogpilotProcessState"]),
    "holiday": frogpilot_toggles.current_holiday_theme,
    "holiday_themes": params.get_bool("HolidayThemes"),
    "online": frogpilot_utilities.is_online(the_pond.sm["deviceState"]),
    "random_themes": is_random_themes(the_pond, frogpilot_toggles),
    "startup_alert": startup_alert,
    "user_themes": user_themes,
  })


@routes.put("/api/themes/selection")
async def select_theme(request):
  the_pond = request.app[utilities.THE_POND]

  body = await utilities.read_json(request)
  component = body.get("component")
  key = body.get("key")
  if component not in frogpilot_variables.THEME_PARAMS or not isinstance(key, str) or not is_selectable(component, key):
    raise web.HTTPBadRequest(text="invalid_theme")
  if is_random_themes(the_pond, frogpilot_variables.get_frogpilot_toggles(the_pond.sm)):
    raise web.HTTPConflict(text="random_themes")

  the_pond.params.put(frogpilot_variables.THEME_PARAMS[component], key)
  the_pond.send_request("updateToggles")
  return web.Response(status=204)


@routes.put("/api/themes/startup_alert")
async def set_startup_alert(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params

  body = await utilities.read_json(request)
  for key, maximum_length in frogpilot_variables.STARTUP_ALERT_MAX_LENGTHS.items():
    text = body.get(key)
    if not isinstance(text, str) or len(text) > maximum_length or not text.strip():
      raise web.HTTPBadRequest(text="invalid_text")

  for key in frogpilot_variables.STARTUP_ALERT_MAX_LENGTHS:
    params.put(key, body[key].strip())
  the_pond.send_request("updateToggles")
  return web.Response(status=204)
