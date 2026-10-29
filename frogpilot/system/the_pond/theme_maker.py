import asyncio
import hashlib
import io
import json
import re
import shutil
import tempfile
import uuid
import wave
import zipfile
import zlib

import xml.etree.ElementTree as ElementTree

from aiohttp import web
from PIL import Image

from openpilot.frogpilot.assets.theme_manager import THEME_PACK_COMPONENTS, WHEEL_EXTENSIONS, ThemeManager
from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.common.frogpilot_api import FrogPilotAPI, FrogPilotAPIError
from openpilot.frogpilot.system.the_pond import utilities

GIF_APPLICATION = 0xFF
GIF_APPLICATION_IDS = (b"ANIMEXTS1.0", b"NETSCAPE2.0")
GIF_GRAPHIC_CONTROL = 0xF9
GIF_HEADER_SIZE = 13
GIF_IMAGE = 0x2C
GIF_IMAGE_HEADER_SIZE = 11
GIF_TRAILER = 0x3B

MAX_FILE_SIZE = 10 * 1024 * 1024
MAX_SIGNAL_FRAMES = 30
MAX_SIGNAL_INTERVAL = 10000
MAX_SUBMISSION_SIZE = 32 * 1024 * 1024
MAX_THEME_NAME_LENGTH = 32
MAX_USERNAME_LENGTH = 128

SIGNAL_BLINDSPOT_NAME = "turn_signal_blindspot"
SIGNAL_FRAME_PATTERN = re.compile(r"^turn_signal_\d+$")
SIGNAL_STYLE_PATTERN = re.compile(r"^([a-z]+)_(\d+)$")

SOUND_CHANNELS = 1
SOUND_EXTENSION = ".wav"
SOUND_RATE = 48000
SOUND_SAMPLE_WIDTH = 2

SUBMISSION_IMAGE_TYPES = {".gif": "image/gif", ".png": "image/png", ".svg": "image/svg+xml"}

SVG_NAMESPACE = "http://www.w3.org/2000/svg"
SVG_DROPPED_ELEMENTS = ("desc", "metadata", "title")
SVG_KEPT_NAMESPACES = (f"{{{SVG_NAMESPACE}}}", "{http://www.w3.org/1999/xlink}", "{http://www.w3.org/XML/1998/namespace}")

THEME_NAME_PATTERN = re.compile(rf"^[A-Za-z0-9 ]{{1,{MAX_THEME_NAME_LENGTH}}}$")

WHEEL_FOLDER = "steering_wheel"
WHEEL_NONE = "none"

routes = web.RouteTableDef()

ElementTree.register_namespace("", SVG_NAMESPACE)
ElementTree.register_namespace("xlink", "http://www.w3.org/1999/xlink")

def get_theme_key(name):
  if not isinstance(name, str) or not THEME_NAME_PATTERN.match(name.strip()):
    raise web.HTTPBadRequest(text="invalid_name")

  key = f"{'_'.join(name.lower().split())}{frogpilot_variables.USER_CREATED_THEME_SUFFIX}"
  if theme_exists(key):
    raise web.HTTPConflict(text="name_in_use")
  return key


def get_user_theme(request):
  key = request.match_info["key"]
  if not key.endswith(frogpilot_variables.USER_CREATED_THEME_SUFFIX) or not theme_exists(key):
    raise web.HTTPNotFound(text="not_found")
  return key


def get_no_wheel(key):
  return frogpilot_paths.THEME_PACKS_PATH / key / WHEEL_FOLDER / WHEEL_NONE


def mark_no_wheel(key):
  no_wheel = get_no_wheel(key)
  no_wheel.parent.mkdir(parents=True, exist_ok=True)
  no_wheel.touch()


def get_wheel(key):
  return next((path for path in frogpilot_paths.STEERING_WHEELS_PATH.glob(f"{key}.*") if path.suffix in WHEEL_EXTENSIONS), None)


def get_wheel_param(key):
  if get_wheel(key) is not None:
    return key
  if get_no_wheel(key).is_file():
    return WHEEL_NONE
  return "stock"


def theme_exists(key):
  return (frogpilot_paths.THEME_PACKS_PATH / key).is_dir() or get_wheel(key) is not None


def is_selected(params, key):
  return any(params.get(param) == key for param in frogpilot_variables.THEME_PARAMS.values())


def is_theme_file(component, name):
  if component == "colors":
    return name == "colors.json"
  if component == "signals":
    stem, _, extension = name.rpartition(".")
    if stem and f".{extension}" in WHEEL_EXTENSIONS:
      return stem == SIGNAL_BLINDSPOT_NAME or SIGNAL_FRAME_PATTERN.match(stem) is not None
    match = SIGNAL_STYLE_PATTERN.match(name)
    return match is not None and match[1] in frogpilot_variables.THEME_SIGNAL_STYLES
  if component == "sounds":
    return name.endswith(SOUND_EXTENSION) and name.removesuffix(SOUND_EXTENSION) in frogpilot_variables.THEME_SLOTS[component]

  stem, _, extension = name.rpartition(".")
  return stem in frogpilot_variables.THEME_SLOTS[component] and f".{extension}" in WHEEL_EXTENSIONS


def read_colors(colors):
  if not isinstance(colors, dict) or not colors or any(key not in frogpilot_variables.THEME_COLOR_KEYS for key in colors):
    raise web.HTTPBadRequest(text="invalid_colors")

  for color in colors.values():
    if not isinstance(color, dict) or set(color) != {"alpha", "blue", "green", "red"}:
      raise web.HTTPBadRequest(text="invalid_colors")
    if any(type(value) is not int or not 0 <= value <= 255 for value in color.values()):
      raise web.HTTPBadRequest(text="invalid_colors")
  return colors


def get_color_table_size(flags):
  if flags & 0x80:
    return 3 << ((flags & 0x07) + 1)
  return 0


def skip_sub_blocks(data, position):
  while data[position] != 0:
    position += data[position] + 1
  return position + 1


def clean_gif(data):
  position = GIF_HEADER_SIZE + get_color_table_size(data[10])
  output = bytearray(data[:position])

  while data[position] != GIF_TRAILER:
    start = position
    if data[position] == GIF_IMAGE:
      position = skip_sub_blocks(data, position + GIF_IMAGE_HEADER_SIZE + get_color_table_size(data[position + 9]))
      output += data[start:position]
    else:
      position = skip_sub_blocks(data, position + 2)

      label = data[start + 1]
      is_loop = label == GIF_APPLICATION and data[start + 3:start + 14] in GIF_APPLICATION_IDS
      if label == GIF_GRAPHIC_CONTROL or is_loop:
        output += data[start:position]

  output.append(GIF_TRAILER)
  return bytes(output)


def clean_svg(svg):
  for element in list(svg.iter()):
    for child in list(element):
      if child.tag.rpartition("}")[2] in SVG_DROPPED_ELEMENTS or (child.tag.startswith("{") and not child.tag.startswith(SVG_KEPT_NAMESPACES)):
        element.remove(child)

    for name in list(element.attrib):
      if name.startswith("{") and not name.startswith(SVG_KEPT_NAMESPACES):
        del element.attrib[name]
  return ElementTree.tostring(svg)


def read_image(data):
  if len(data) > MAX_FILE_SIZE:
    raise web.HTTPBadRequest(text="file_too_large")

  try:
    svg = ElementTree.fromstring(data)
  except ElementTree.ParseError:
    svg = None

  if svg is not None and svg.tag.rpartition("}")[2] == "svg":
    return ".svg", clean_svg(svg)

  try:
    with Image.open(io.BytesIO(data)) as image:
      if image.format == "GIF":
        return ".gif", clean_gif(data)

      if image.format in ("JPEG", "PNG"):
        output = io.BytesIO()
        image.save(output, "PNG")
        return ".png", output.getvalue()
  except (IndexError, OSError, Image.DecompressionBombError):
    pass

  raise web.HTTPBadRequest(text="invalid_image")


def read_sound(data):
  try:
    with wave.open(io.BytesIO(data)) as sound:
      if (sound.getnchannels(), sound.getsampwidth(), sound.getframerate()) != (SOUND_CHANNELS, SOUND_SAMPLE_WIDTH, SOUND_RATE):
        raise web.HTTPBadRequest(text="invalid_sound")
      frames = sound.readframes(sound.getnframes())
  except (EOFError, wave.Error):
    raise web.HTTPBadRequest(text="invalid_sound") from None

  output = io.BytesIO()
  with wave.open(output, "wb") as sound:
    sound.setnchannels(SOUND_CHANNELS)
    sound.setsampwidth(SOUND_SAMPLE_WIDTH)
    sound.setframerate(SOUND_RATE)
    sound.writeframes(frames)
  return output.getvalue()


def read_theme_archive(file):
  try:
    theme_zip = zipfile.ZipFile(file)
  except zipfile.BadZipFile:
    raise web.HTTPBadRequest(text="invalid_file") from None

  files = {}
  with theme_zip:
    for member in theme_zip.infolist():
      if member.is_dir():
        continue

      component, _, name = member.filename.partition("/")
      if member.file_size > MAX_FILE_SIZE or "/" in name:
        raise web.HTTPBadRequest(text="invalid_file")

      try:
        data = theme_zip.read(member)
      except (NotImplementedError, RuntimeError, zipfile.BadZipFile, zlib.error):
        raise web.HTTPBadRequest(text="invalid_file") from None
      if component == WHEEL_FOLDER and name == WHEEL_NONE:
        files[(WHEEL_FOLDER, WHEEL_NONE)] = b""
      elif component == WHEEL_FOLDER and name.startswith("wheel."):
        extension, data = read_image(data)
        files[(WHEEL_FOLDER, f"wheel{extension}")] = data
      elif component in THEME_PACK_COMPONENTS and is_theme_file(component, name):
        if component == "colors":
          try:
            read_colors(json.loads(data))
          except ValueError:
            raise web.HTTPBadRequest(text="invalid_colors") from None
        elif component == "sounds":
          data = read_sound(data)
        elif name.endswith(WHEEL_EXTENSIONS):
          extension, data = read_image(data)
          if extension != f".{name.rpartition('.')[2]}":
            raise web.HTTPBadRequest(text="invalid_image")
        files[(component, name)] = data
      else:
        raise web.HTTPBadRequest(text="invalid_file")
  return files


async def read_upload(request):
  upload = (await request.post()).get("file")
  if not isinstance(upload, web.FileField):
    raise web.HTTPBadRequest(text="invalid_file")
  return upload.file.read()


async def convert_sound(the_pond, data, path):
  if len(data) > MAX_FILE_SIZE:
    raise web.HTTPBadRequest(text="file_too_large")

  with tempfile.TemporaryDirectory() as directory:
    source = f"{directory}/upload"
    with open(source, "wb") as file:
      file.write(data)

    cmd = ["ffmpeg", "-loglevel", "error", "-i", source, "-map_metadata", "-1", "-ac", str(SOUND_CHANNELS), "-ar", str(SOUND_RATE), "-sample_fmt", "s16", "-f", "wav"]
    try:
      await utilities.create_file(the_pond, path, cmd)
    except RuntimeError:
      raise web.HTTPBadRequest(text="invalid_sound") from None


async def edit_component(the_pond, key, component, change):
  destination = frogpilot_paths.THEME_PACKS_PATH / key / component
  staging = destination.with_name(f".{component}.new")
  backup = destination.with_name(f".{component}.old")

  async with the_pond.file_locks.setdefault(destination, asyncio.Lock()):
    await asyncio.to_thread(shutil.rmtree, staging, True)
    if destination.is_dir():
      await asyncio.to_thread(shutil.copytree, destination, staging)
    else:
      staging.mkdir(parents=True)

    try:
      await change(staging)
    except BaseException:
      await asyncio.to_thread(shutil.rmtree, staging, True)
      raise

    await asyncio.to_thread(shutil.rmtree, backup, True)
    if destination.exists():
      destination.rename(backup)
    staging.rename(destination)
    await asyncio.to_thread(shutil.rmtree, backup, True)


def get_source_files(component, source):
  if source == "active":
    folder = frogpilot_paths.ACTIVE_THEME_PATH / component
  else:
    folder = ThemeManager.get_theme_asset_path(component, source)

  if not folder.is_dir() or folder.resolve() == (frogpilot_paths.STOCK_THEME_PATH / component).resolve():
    return []
  return [path for path in folder.iterdir() if path.is_file() and is_theme_file(component, path.name)]


def copy_theme(source, key):
  for component in THEME_PACK_COMPONENTS:
    files = get_source_files(component, source)
    if files:
      destination = frogpilot_paths.THEME_PACKS_PATH / key / component
      destination.mkdir(parents=True, exist_ok=True)
      for path in files:
        shutil.copyfile(path, destination / path.name)

  if source == "active":
    wheel = next(iter((frogpilot_paths.ACTIVE_THEME_PATH / WHEEL_FOLDER).glob("wheel.*")), None)
  elif source != "none":
    wheel = ThemeManager.get_wheel_path(source)
  else:
    wheel = None

  if wheel is not None and wheel.resolve().parent != (frogpilot_paths.STOCK_THEME_PATH / WHEEL_FOLDER).resolve():
    frogpilot_paths.STEERING_WHEELS_PATH.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(wheel, frogpilot_paths.STEERING_WHEELS_PATH / f"{key}{wheel.suffix}")
  elif wheel is None and (source == "active" or get_no_wheel(source).is_file()):
    mark_no_wheel(key)


def build_export(key):
  archive = io.BytesIO()
  with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as theme_zip:
    for component in THEME_PACK_COMPONENTS:
      folder = frogpilot_paths.THEME_PACKS_PATH / key / component
      if folder.is_dir():
        for path in sorted(folder.iterdir()):
          theme_zip.write(path, f"{component}/{path.name}")

    wheel = get_wheel(key)
    if wheel is not None:
      theme_zip.write(wheel, f"{WHEEL_FOLDER}/wheel{wheel.suffix}")
    elif get_no_wheel(key).is_file():
      theme_zip.writestr(f"{WHEEL_FOLDER}/{WHEEL_NONE}", b"")
  return archive.getvalue()


def build_submission(key):
  assets = {}
  for component in THEME_PACK_COMPONENTS:
    folder = frogpilot_paths.THEME_PACKS_PATH / key / component
    if folder.is_dir() and any(folder.iterdir()):
      archive = io.BytesIO()
      with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as component_zip:
        for path in sorted(folder.iterdir()):
          component_zip.write(path, path.name)
      assets[component] = ("application/zip", archive.getvalue())

  wheel = get_wheel(key)
  if wheel is not None:
    extension, data = read_image(wheel.read_bytes())
    assets[WHEEL_FOLDER] = (SUBMISSION_IMAGE_TYPES[extension], data)
  return assets


def submit_theme(params, theme_name, discord_username, assets):
  frogpilot_api = FrogPilotAPI(params)

  manifest = []
  for kind, (content_type, data) in sorted(assets.items()):
    manifest.append({"content_type": content_type, "kind": kind, "sha256": hashlib.sha256(data).hexdigest(), "size_bytes": len(data)})

  submission = frogpilot_api.post_json("/v1/theme-submissions", {
    "assets": manifest,
    "client_submission_id": str(uuid.uuid4()),
    "discord_username": discord_username,
    "schema_version": 1,
    "theme_name": theme_name,
  })

  completed_assets = []
  for asset in submission["assets"]:
    data = assets[asset["kind"]][1]
    if "upload" in asset:
      frogpilot_api.put_upload(asset["upload"], data, f"Theme {asset['kind']}", "theme_submission")
    completed_assets.append({"asset_id": asset["asset_id"], "sha256": hashlib.sha256(data).hexdigest(), "size_bytes": len(data)})

  frogpilot_api.post_json(f"/v1/theme-submissions/{submission['submission_id']}/complete", {"assets": completed_assets, "schema_version": 1})
  return submission["submission_id"]


def refresh_if_selected(the_pond, key):
  if is_selected(the_pond.params, key):
    the_pond.send_request("refreshTheme")


def release_component(the_pond, key, component):
  param = frogpilot_variables.THEME_PARAMS[component]
  if the_pond.params.get(param) == key:
    the_pond.params.put(param, get_wheel_param(key) if component == "steering_wheels" else "stock")
    the_pond.send_request("refreshTheme")
  else:
    refresh_if_selected(the_pond, key)


@routes.post("/api/user_themes/{key}/apply")
async def apply_theme(request):
  the_pond = request.app[utilities.THE_POND]
  key = get_user_theme(request)

  for component, param in frogpilot_variables.THEME_PARAMS.items():
    if component == "steering_wheels":
      the_pond.params.put(param, get_wheel_param(key))
    elif (frogpilot_paths.THEME_PACKS_PATH / key / component).is_dir():
      the_pond.params.put(param, key)
    else:
      the_pond.params.put(param, "stock")

  the_pond.params.put_bool("CustomThemes", True)
  the_pond.params.put_bool("RandomThemes", False)
  the_pond.send_request("refreshTheme")
  return web.Response(status=204)


@routes.post("/api/user_themes/{key}/{component}/copy")
async def copy_component(request):
  the_pond = request.app[utilities.THE_POND]
  key = get_user_theme(request)
  component = request.match_info["component"]
  if component not in THEME_PACK_COMPONENTS:
    raise web.HTTPNotFound(text="not_found")

  source = (await utilities.read_json(request)).get("source")
  if not isinstance(source, str) or (source not in frogpilot_variables.BUILTIN_THEMES and not theme_exists(source)):
    raise web.HTTPBadRequest(text="invalid_source")

  files = get_source_files(component, source)
  if not files:
    raise web.HTTPBadRequest(text="invalid_source")

  async def copy_files(staging):
    for path in staging.iterdir():
      path.unlink()
    for path in files:
      shutil.copy2(path, staging / path.name)

  await edit_component(the_pond, key, component, copy_files)
  refresh_if_selected(the_pond, key)
  return web.Response(status=204)


@routes.post("/api/user_themes")
async def create_theme(request):
  body = await utilities.read_json(request)
  key = get_theme_key(body.get("name"))

  source = body.get("source")
  if source is not None and source != "active" and source not in frogpilot_variables.BUILTIN_THEMES and not theme_exists(source):
    raise web.HTTPBadRequest(text="invalid_source")

  (frogpilot_paths.THEME_PACKS_PATH / key).mkdir(parents=True)
  if source is not None:
    await asyncio.to_thread(copy_theme, source, key)
  return web.json_response({"key": key})


@routes.delete("/api/user_themes/{key}")
async def delete_theme(request):
  the_pond = request.app[utilities.THE_POND]
  key = get_user_theme(request)
  if is_selected(the_pond.params, key):
    raise web.HTTPConflict(text="in_use")

  await asyncio.to_thread(shutil.rmtree, frogpilot_paths.THEME_PACKS_PATH / key, True)
  wheel = get_wheel(key)
  if wheel is not None:
    wheel.unlink()
  the_pond.params.remove("ThemesDownloaded")
  return web.Response(status=204)


@routes.delete("/api/user_themes/{key}/{component}")
async def delete_component(request):
  the_pond = request.app[utilities.THE_POND]
  key = get_user_theme(request)
  component = request.match_info["component"]

  if component == "steering_wheels":
    wheel = get_wheel(key)
    if wheel is not None:
      wheel.unlink()
    get_no_wheel(key).unlink(missing_ok=True)
  elif component in THEME_PACK_COMPONENTS:
    destination = frogpilot_paths.THEME_PACKS_PATH / key / component
    async with the_pond.file_locks.setdefault(destination, asyncio.Lock()):
      await asyncio.to_thread(shutil.rmtree, destination, True)
  else:
    raise web.HTTPNotFound(text="not_found")

  release_component(the_pond, key, component)

  return web.Response(status=204)


@routes.delete("/api/user_themes/{key}/{component}/{slot}")
async def delete_slot(request):
  the_pond = request.app[utilities.THE_POND]
  key = get_user_theme(request)
  component = request.match_info["component"]
  slot = request.match_info["slot"]
  if slot not in frogpilot_variables.THEME_SLOTS.get(component, ()):
    raise web.HTTPNotFound(text="not_found")

  async def remove_slot(staging):
    for path in staging.glob(f"{slot}.*"):
      path.unlink()

  await edit_component(the_pond, key, component, remove_slot)
  refresh_if_selected(the_pond, key)
  return web.Response(status=204)


@routes.post("/api/user_themes/{key}/duplicate")
async def duplicate_theme(request):
  key = get_user_theme(request)
  new_key = get_theme_key((await utilities.read_json(request)).get("name"))

  (frogpilot_paths.THEME_PACKS_PATH / new_key).mkdir(parents=True)
  await asyncio.to_thread(copy_theme, key, new_key)
  return web.json_response({"key": new_key})


@routes.get("/api/user_themes")
async def get_user_themes(request):
  no_wheel = [path.parent.parent.name for path in frogpilot_paths.THEME_PACKS_PATH.glob(f"*/{WHEEL_FOLDER}/{WHEEL_NONE}")]

  return web.json_response({
    "limits": {
      "file_size": MAX_FILE_SIZE,
      "name_length": MAX_THEME_NAME_LENGTH,
      "submission_size": MAX_SUBMISSION_SIZE,
      "username_length": MAX_USERNAME_LENGTH,
    },
    "no_wheel": sorted(no_wheel),
  })


@routes.get("/api/user_themes/{key}/export")
async def export_theme(request):
  key = get_user_theme(request)
  name = utilities.format_theme_name(key.removesuffix(frogpilot_variables.USER_CREATED_THEME_SUFFIX), False)
  return web.Response(body=await asyncio.to_thread(build_export, key), content_type="application/zip",
                      headers={"Content-Disposition": f'attachment; filename="{name}.zip"'})


@routes.post("/api/user_themes/import")
async def import_theme(request):
  form = await request.post()
  key = get_theme_key(form.get("name"))

  upload = form.get("file")
  if not isinstance(upload, web.FileField):
    raise web.HTTPBadRequest(text="invalid_file")

  files = await asyncio.to_thread(read_theme_archive, upload.file)
  if not files:
    raise web.HTTPBadRequest(text="invalid_file")

  for (component, name), data in files.items():
    if (component, name) == (WHEEL_FOLDER, WHEEL_NONE):
      mark_no_wheel(key)
    elif component == WHEEL_FOLDER:
      frogpilot_paths.STEERING_WHEELS_PATH.mkdir(parents=True, exist_ok=True)
      (frogpilot_paths.STEERING_WHEELS_PATH / f"{key}{name.removeprefix('wheel')}").write_bytes(data)
    else:
      destination = frogpilot_paths.THEME_PACKS_PATH / key / component
      destination.mkdir(parents=True, exist_ok=True)
      (destination / name).write_bytes(data)
  (frogpilot_paths.THEME_PACKS_PATH / key).mkdir(parents=True, exist_ok=True)
  return web.json_response({"key": key})


@routes.put("/api/user_themes/{key}/name")
async def rename_theme(request):
  the_pond = request.app[utilities.THE_POND]
  key = get_user_theme(request)
  new_key = get_theme_key((await utilities.read_json(request)).get("name"))

  if (frogpilot_paths.THEME_PACKS_PATH / key).is_dir():
    (frogpilot_paths.THEME_PACKS_PATH / key).rename(frogpilot_paths.THEME_PACKS_PATH / new_key)

  wheel = get_wheel(key)
  if wheel is not None:
    wheel.rename(wheel.with_name(f"{new_key}{wheel.suffix}"))

  selected = False
  for param in frogpilot_variables.THEME_PARAMS.values():
    if the_pond.params.get(param) == key:
      the_pond.params.put(param, new_key)
      selected = True

  if selected:
    the_pond.send_request("refreshTheme")
  return web.json_response({"key": new_key})


@routes.put("/api/user_themes/{key}/colors")
async def set_colors(request):
  the_pond = request.app[utilities.THE_POND]
  key = get_user_theme(request)
  colors = read_colors((await utilities.read_json(request)).get("colors"))

  async def write_colors(staging):
    frogpilot_utilities.update_json_file(staging / "colors.json", colors)

  await edit_component(the_pond, key, "colors", write_colors)
  refresh_if_selected(the_pond, key)
  return web.Response(status=204)


@routes.put("/api/user_themes/{key}/signals")
async def set_signals(request):
  the_pond = request.app[utilities.THE_POND]
  key = get_user_theme(request)
  form = await request.post()

  style = form.get("style")
  interval = form.get("interval")
  frames = form.getall("frame", [])
  blindspot = form.get("blindspot")

  if style not in frogpilot_variables.THEME_SIGNAL_STYLES or not isinstance(interval, str) or not interval.isdigit():
    raise web.HTTPBadRequest(text="invalid_signals")
  if not 0 < int(interval) <= MAX_SIGNAL_INTERVAL or len(frames) > MAX_SIGNAL_FRAMES or (blindspot is not None and not frames):
    raise web.HTTPBadRequest(text="invalid_signals")
  if any(not isinstance(frame, web.FileField) for frame in frames) or (blindspot is not None and not isinstance(blindspot, web.FileField)):
    raise web.HTTPBadRequest(text="invalid_signals")

  images = [read_image(frame.file.read()) for frame in frames]
  blindspot_image = read_image(blindspot.file.read()) if blindspot is not None else None

  async def write_signals(staging):
    if not images and not any(SIGNAL_FRAME_PATTERN.match(path.stem) for path in staging.iterdir()):
      raise web.HTTPBadRequest(text="invalid_signals")

    for path in staging.iterdir():
      if images or SIGNAL_STYLE_PATTERN.match(path.name):
        path.unlink()

    for index, (extension, data) in enumerate(images, start=1):
      (staging / f"turn_signal_{index:02d}{extension}").write_bytes(data)
    if blindspot_image is not None:
      (staging / f"{SIGNAL_BLINDSPOT_NAME}{blindspot_image[0]}").write_bytes(blindspot_image[1])
    (staging / f"{style}_{int(interval)}").touch()

  await edit_component(the_pond, key, "signals", write_signals)
  refresh_if_selected(the_pond, key)
  return web.Response(status=204)


@routes.put("/api/user_themes/{key}/steering_wheels")
async def set_wheel(request):
  the_pond = request.app[utilities.THE_POND]
  key = get_user_theme(request)
  extension, data = read_image(await read_upload(request))

  frogpilot_paths.STEERING_WHEELS_PATH.mkdir(parents=True, exist_ok=True)
  wheel = get_wheel(key)
  if wheel is not None:
    wheel.unlink()
  (frogpilot_paths.STEERING_WHEELS_PATH / f"{key}{extension}").write_bytes(data)
  get_no_wheel(key).unlink(missing_ok=True)

  refresh_if_selected(the_pond, key)
  return web.Response(status=204)


@routes.put("/api/user_themes/{key}/steering_wheels/none")
async def set_no_wheel(request):
  the_pond = request.app[utilities.THE_POND]
  key = get_user_theme(request)

  wheel = get_wheel(key)
  if wheel is not None:
    wheel.unlink()
  mark_no_wheel(key)

  release_component(the_pond, key, "steering_wheels")
  return web.Response(status=204)


@routes.put("/api/user_themes/{key}/{component}/{slot}")
async def set_slot(request):
  the_pond = request.app[utilities.THE_POND]
  key = get_user_theme(request)
  component = request.match_info["component"]
  slot = request.match_info["slot"]
  if slot not in frogpilot_variables.THEME_SLOTS.get(component, ()):
    raise web.HTTPNotFound(text="not_found")

  data = await read_upload(request)

  if component == "sounds":
    async def write_slot(staging):
      await convert_sound(the_pond, data, staging / f"{slot}{SOUND_EXTENSION}")
  else:
    extension, image = read_image(data)

    async def write_slot(staging):
      for path in staging.glob(f"{slot}.*"):
        path.unlink()
      (staging / f"{slot}{extension}").write_bytes(image)

  await edit_component(the_pond, key, component, write_slot)
  refresh_if_selected(the_pond, key)
  return web.Response(status=204)


@routes.post("/api/user_themes/{key}/submit")
async def submit_theme_route(request):
  the_pond = request.app[utilities.THE_POND]
  key = get_user_theme(request)

  discord_username = (await utilities.read_json(request)).get("discord_username")
  if not isinstance(discord_username, str) or not 0 < len(discord_username.strip().encode()) <= MAX_USERNAME_LENGTH:
    raise web.HTTPBadRequest(text="invalid_username")

  assets = await asyncio.to_thread(build_submission, key)
  if not assets:
    raise web.HTTPConflict(text="empty_theme")
  if sum(len(data) for _, data in assets.values()) > MAX_SUBMISSION_SIZE:
    raise web.HTTPConflict(text="theme_too_large")

  theme_name = utilities.format_theme_name(key.removesuffix(frogpilot_variables.USER_CREATED_THEME_SUFFIX), False)
  try:
    submission_id = await asyncio.to_thread(submit_theme, the_pond.params, theme_name, discord_username.strip(), assets)
  except FrogPilotAPIError:
    raise web.HTTPBadGateway(text="submission_failed") from None

  the_pond.params.put("DiscordUsername", discord_username.strip())
  return web.json_response({"submission_id": submission_id})
