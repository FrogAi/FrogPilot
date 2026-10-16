import asyncio
import datetime
import re

from aiohttp import web

from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.system.the_pond import dashcam, utilities

CACHE_KINDS = ("thumbnail.jpg", "timelapse.mp4")
CACHE_PREFIX = "screen_recording--"

MAX_FILE_NAME_LENGTH = 255

PREVIEW_KEYFRAME_SPACING = 0.4

RECORDER_NAME_PATTERN = re.compile(r"\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}(_(replay_)?\d+)?(?=_|$)")

routes = web.RouteTableDef()

def delete_recording(path):
  for kind in CACHE_KINDS:
    get_cache_path(path, kind).unlink(missing_ok=True)

  for file in get_recording_files(path):
    file.unlink(missing_ok=True)


def get_cache_path(path, kind):
  stat = path.stat()
  return frogpilot_variables.THE_POND_THUMBNAILS_PATH / f"{CACHE_PREFIX}{stat.st_mtime_ns}-{stat.st_size}-{kind}"


def get_recording(request):
  name = request.match_info["name"]
  if not name.lower().endswith(".mp4"):
    raise web.HTTPNotFound(text="not_found")
  return utilities.get_file(frogpilot_variables.SCREEN_RECORDINGS_PATH, name)


def get_recording_files(path):
  return [path] + [path.with_suffix(suffix) for suffix in frogpilot_variables.SCREEN_RECORDING_COMPANION_SUFFIXES]


def get_recording_info(file):
  clean_name = file["name"].replace(".mp4", "")
  parts = clean_name.split("_")

  recorded = None
  if len(parts) >= 2:
    try:
      recorded = datetime.datetime.strptime(f"{parts[0]}_{parts[1]}", "%Y-%m-%d_%H-%M-%S").isoformat()
    except ValueError:
      pass

  return {**file, "recorded": recorded, "replay": "_replay_" in clean_name}


def get_recording_paths():
  return {path for path in frogpilot_variables.SCREEN_RECORDINGS_PATH.glob("*.mp4") if path.is_file()}


async def prepare_preview(the_pond, path):
  preview_path = get_cache_path(path, "timelapse.mp4")
  async with the_pond.file_locks.setdefault(preview_path, asyncio.Lock()):
    if not preview_path.is_file():
      duration = await dashcam.get_duration(path)
      await dashcam.create_timelapse(the_pond, preview_path, str(path), duration, PREVIEW_KEYFRAME_SPACING, f",scale={dashcam.THUMBNAIL_WIDTH}:-2")
  return preview_path


async def prepare_recordings(the_pond, previous_recordings):
  for path in get_recording_paths() - previous_recordings:
    await utilities.run_and_report(prepare_thumbnail(the_pond, path))
    await utilities.run_and_report(prepare_preview(the_pond, path))


async def prepare_thumbnail(the_pond, path):
  thumbnail_path = get_cache_path(path, "thumbnail.jpg")
  async with the_pond.file_locks.setdefault(thumbnail_path, asyncio.Lock()):
    if not thumbnail_path.is_file():
      await dashcam.create_thumbnail(the_pond, thumbnail_path, path, ["-vf", f"scale={dashcam.THUMBNAIL_WIDTH}:-2"])
  return thumbnail_path


def remove_stale_files():
  current = set()
  for path in frogpilot_variables.SCREEN_RECORDINGS_PATH.glob("*"):
    if path.is_file() and path.name.lower().endswith(".mp4"):
      try:
        current.update(get_cache_path(path, kind).name for kind in CACHE_KINDS)
      except FileNotFoundError:
        continue

  for path in frogpilot_variables.THE_POND_THUMBNAILS_PATH.glob(f"{CACHE_PREFIX}*"):
    if path.name not in current:
      path.unlink(missing_ok=True)


@routes.delete("/api/screen_recordings/{name}")
async def delete_single_recording(request):
  await asyncio.to_thread(delete_recording, get_recording(request))
  return web.Response(status=204)


@routes.get("/api/screen_recordings")
async def get_recordings(request):
  await asyncio.to_thread(remove_stale_files)

  recordings = []
  for file in utilities.list_files(frogpilot_variables.SCREEN_RECORDINGS_PATH):
    if file["name"].lower().endswith(".mp4"):
      recordings.append(get_recording_info(file))
  return web.json_response(recordings)


@routes.get("/api/screen_recordings/{name}")
async def get_video(request):
  path = get_recording(request)

  headers = {}
  if "download" in request.query:
    headers["Content-Disposition"] = f'attachment; filename="{path.name}"'
  return web.FileResponse(path, headers=headers)


@routes.get("/api/screen_recordings/{name}/preview")
async def get_preview(request):
  the_pond = request.app[utilities.THE_POND]
  path = get_recording(request)
  return web.FileResponse(await prepare_preview(the_pond, path))


@routes.get("/api/screen_recordings/{name}/thumbnail")
async def get_thumbnail(request):
  path = get_recording(request)
  return web.FileResponse(await prepare_thumbnail(request.app[utilities.THE_POND], path))


@routes.put("/api/screen_recordings/{name}/name")
async def rename_recording(request):
  path = get_recording(request)

  name = (await utilities.read_json(request)).get("name")
  if not isinstance(name, str):
    raise web.HTTPBadRequest(text="invalid_name")

  name = name.strip().replace(" ", "_")
  new_name = f"{name}.mp4"
  recorder_name = RECORDER_NAME_PATTERN.match(path.stem)
  if recorder_name is not None:
    new_name = f"{recorder_name[0]}_{new_name}"
  if len(new_name) > MAX_FILE_NAME_LENGTH or not utilities.is_valid_name(name):
    raise web.HTTPBadRequest(text="invalid_name")

  targets = get_recording_files(path.with_name(new_name))
  if any(target.exists() for target in targets):
    raise web.HTTPConflict(text="name_taken")

  for source, target in zip(get_recording_files(path), targets, strict=True):
    if source.exists():
      source.rename(target)

  return web.json_response({"name": new_name})
