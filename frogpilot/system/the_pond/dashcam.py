import asyncio
import json
import os
import re
import shutil
import sys
import xattr

from aiohttp import web

from openpilot.system import sentry
from openpilot.system.loggerd.config import SEGMENT_LENGTH
from openpilot.system.loggerd.deleter import PRESERVE_ATTR_NAME, PRESERVE_ATTR_VALUE
from openpilot.system.loggerd.uploader import UPLOAD_ATTR_NAME, UPLOAD_ATTR_VALUE

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

CAMERA_FILES = {"driver": "dcamera.hevc", "forward": "fcamera.hevc", "low": "qcamera.ts", "wide": "ecamera.hevc"}
CAMERA_FPS = 20
DRIVE_SUMMARY_VERSION = 6
LOG_FILES = {"qlog": "qlog.zst", "rlog": "rlog.zst"}

MAX_NAME_LENGTH = 64
MAX_VIDEO_CACHE_SIZE = 1024 * 1024 * 1024

NAME_ATTR_NAME = "user.frogpilot_name"

PREVIEW_FPS = 20
PREVIEW_FRAMES = 100
PREVIEW_KEYFRAME_SPACING = 1
PREVIEW_SEEK_SPACING = 12
THUMBNAIL_WIDTH = 640

CACHE_FILE_PATTERN = re.compile(r"^([0-9a-f]{8}--[0-9a-f]{10})[.-]")
ROUTE_PATTERN = re.compile(r"^[0-9a-f]{8}--[0-9a-f]{10}$")
SEGMENT_PATTERN = re.compile(r"^([0-9a-f]{8}--[0-9a-f]{10})--(\d+)$")

routes = web.RouteTableDef()

def get_xattr(path, name):
  try:
    return xattr.getxattr(path, name)
  except OSError:
    return None


def get_routes(route="*--*"):
  segments = {}
  for log_root in frogpilot_variables.LOG_ROOTS:
    for path in log_root.glob(f"{route}--*"):
      match = SEGMENT_PATTERN.match(path.name)
      if match is not None and path.is_dir():
        segments.setdefault(match[1], {})[int(match[2])] = path
  return {route: dict(sorted(route_segments.items())) for route, route_segments in segments.items()}


def get_route(request):
  route = request.match_info["route"]
  if ROUTE_PATTERN.match(route) is None:
    raise web.HTTPNotFound(text="not_found")

  segments = get_routes(route).get(route)
  if segments is None:
    raise web.HTTPNotFound(text="not_found")
  return route, segments


def get_route_info(route, segments, recording):
  first_segment = next(iter(segments.values()))
  name = get_xattr(first_segment, NAME_ATTR_NAME)

  size = 0
  video = False
  for path in segments.values():
    for file in path.iterdir():
      size += file.stat().st_size
      video |= file.name == CAMERA_FILES["low"]

  return {
    "bookmarked": any(is_bookmarked(path) for path in segments.values()),
    "date": int(first_segment.stat().st_mtime),
    "name": name.decode() if name is not None else "",
    "preserved": frogpilot_utilities.is_route_preserved(first_segment),
    "recording": recording,
    "route": route,
    "segment_length": SEGMENT_LENGTH,
    "segments": len(segments),
    "size": size,
    "video": video,
  }


def list_routes(recording_route):
  all_routes = get_routes()
  remove_stale_files(all_routes)

  route_list = []
  unsummarized = {}
  for route, segments in all_routes.items():
    route_info = get_route_info(route, segments, route == recording_route)
    qlogs = get_qlogs(segments, route_info["recording"])
    drive = load_drive(route, qlogs) if qlogs else None
    if drive is None:
      if qlogs and not route_info["recording"]:
        unsummarized[route] = qlogs
    elif not drive.get("failed"):
      route_info["distance"] = drive["distance"]
    route_list.append(route_info)
  return sorted(route_list, key=lambda route_info: route_info["route"], reverse=True), unsummarized


def get_segment(segments, segment):
  path = segments.get(int(segment))
  if path is None:
    raise web.HTTPNotFound(text="not_found")
  return path


def is_bookmarked(path):
  return get_xattr(path, PRESERVE_ATTR_NAME) == PRESERVE_ATTR_VALUE


def is_drive_active(the_pond):
  loggerd_running = any(process.name == "loggerd" and process.running for process in the_pond.sm["managerState"].processes)
  return the_pond.sm["deviceState"].started or loggerd_running


def get_recording_route(the_pond):
  if is_drive_active(the_pond):
    return the_pond.params.get("CurrentRoute")
  return None


def is_recording(the_pond, route):
  return route == get_recording_route(the_pond)


def finished_segments(segments, recording):
  if not recording:
    return list(segments.values())
  return [path for path in segments.values() if not any(path.glob("*.lock"))]


def get_qlogs(segments, recording):
  finished = set(finished_segments(segments, recording))

  qlogs = {}
  for position, path in enumerate(segments.values()):
    if path in finished and (path / LOG_FILES["qlog"]).is_file():
      qlogs[position] = path / LOG_FILES["qlog"]
  return qlogs


def get_low_camera_sources(segments):
  return [path / CAMERA_FILES["low"] for path in segments.values() if (path / CAMERA_FILES["low"]).is_file()]


def get_drive_source(qlogs):
  return [DRIVE_SUMMARY_VERSION, len(qlogs), int(qlogs[max(qlogs)].stat().st_mtime)]


def load_drive(route, qlogs):
  drive = frogpilot_utilities.load_json_file(frogpilot_variables.THE_POND_DRIVES_PATH / f"{route}.json")
  return drive if drive.get("source") == get_drive_source(qlogs) else None


def save_drive(route, drive):
  drive_path = frogpilot_variables.THE_POND_DRIVES_PATH / f"{route}.json"
  drive_path.parent.mkdir(parents=True, exist_ok=True)
  frogpilot_utilities.update_json_file(drive_path, drive)


async def summarize_drive(the_pond, route, qlogs):
  drive_path = frogpilot_variables.THE_POND_DRIVES_PATH / f"{route}.json"
  async with the_pond.file_locks.setdefault(drive_path, asyncio.Lock()):
    drive = load_drive(route, qlogs)
    if drive is None or drive.get("failed"):
      source = get_drive_source(qlogs)
      qlog_list = json.dumps([[position, str(qlog)] for position, qlog in qlogs.items()])
      output = await utilities.run_heavy_process(the_pond, [sys.executable, "-m", "openpilot.frogpilot.system.the_pond.drive_summary", qlog_list])
      drive = {**json.loads(output), "source": source}

      save_drive(route, drive)
  return drive


async def summarize_drives(the_pond, drives):
  for route, qlogs in sorted(drives.items(), reverse=True):
    if load_drive(route, qlogs) is None:
      try:
        await summarize_drive(the_pond, route, qlogs)
      except Exception as exception:
        sentry.capture_exception(exception, crash_log=False)
        save_drive(route, {"failed": True, "source": get_drive_source(qlogs)})


def remove_route_files(route):
  for path in (frogpilot_variables.THE_POND_DRIVES_PATH / f"{route}.json", frogpilot_variables.THE_POND_THUMBNAILS_PATH / f"{route}--thumbnail.jpg",
               frogpilot_variables.THE_POND_THUMBNAILS_PATH / f"{route}--timelapse.mp4"):
    path.unlink(missing_ok=True)
  for path in frogpilot_variables.THE_POND_VIDEOS_PATH.glob(f"{route}--*.mp4"):
    path.unlink()


def remove_stale_files(all_routes):
  for directory in (frogpilot_variables.THE_POND_DRIVES_PATH, frogpilot_variables.THE_POND_THUMBNAILS_PATH, frogpilot_variables.THE_POND_VIDEOS_PATH):
    for path in directory.glob("*"):
      match = CACHE_FILE_PATTERN.match(path.name)
      if match is not None and match[1] not in all_routes:
        path.unlink(missing_ok=True)


def delete_driving_data(recording_route):
  deleted = True
  deleted_routes = set()
  for log_root in frogpilot_variables.LOG_ROOTS:
    if not log_root.is_dir():
      continue

    for path in log_root.iterdir():
      if not path.is_dir() or is_bookmarked(path) or frogpilot_utilities.is_route_preserved(path):
        continue

      match = SEGMENT_PATTERN.match(path.name)
      if match is not None and match[1] == recording_route:
        continue

      try:
        shutil.rmtree(path)
      except OSError:
        deleted = False

      if match is not None:
        deleted_routes.add(match[1])

  for route in deleted_routes:
    remove_route_files(route)
  return deleted


def delete_route(route, segments):
  for path in segments.values():
    shutil.rmtree(path)
  remove_route_files(route)


async def create_preview(the_pond, preview_path, inputs, frame_filter):
  cmd = ["ffmpeg", "-loglevel", "error", *inputs, "-filter_complex", f"{frame_filter},setpts=N/{PREVIEW_FPS}/TB", "-frames:v", str(PREVIEW_FRAMES),
         "-r", str(PREVIEW_FPS), "-an", "-c:v", "libx264", "-preset", "veryfast", "-crf", "28", "-pix_fmt", "yuv420p", "-movflags", "+faststart", "-f", "mp4"]
  await utilities.create_file(the_pond, preview_path, cmd)


async def create_route_preview(the_pond, preview_path, sources):
  length = (len(sources) - 1) * SEGMENT_LENGTH + await get_duration(sources[-1])

  if length / PREVIEW_FRAMES >= PREVIEW_SEEK_SPACING:
    inputs = []
    for index in range(PREVIEW_FRAMES):
      time = length * (index + 0.5) / PREVIEW_FRAMES
      position = min(int(time // SEGMENT_LENGTH), len(sources) - 1)
      offset = time - position * SEGMENT_LENGTH
      seek_sources = "|".join(str(source) for source in sources[position:position + 2])
      inputs += ["-threads", "1", "-skip_frame", "nokey", "-noaccurate_seek", "-ss", f"{offset:.2f}", "-t", "1", "-i", f"concat:{seek_sources}"]

    trimmed_frames = "".join(f"[{index}:v]trim=end_frame=1[frame{index}];" for index in range(PREVIEW_FRAMES))
    frame_list = "".join(f"[frame{index}]" for index in range(PREVIEW_FRAMES))
    await create_preview(the_pond, preview_path, inputs, f"{trimmed_frames}{frame_list}concat=n={PREVIEW_FRAMES}")
  else:
    await create_timelapse(the_pond, preview_path, f"concat:{'|'.join(str(source) for source in sources)}", length, PREVIEW_KEYFRAME_SPACING)


async def create_timelapse(the_pond, preview_path, source, length, keyframe_spacing, video_filter=""):
  decode_options = ["-threads", "1", "-skip_frame", "nokey"] if length / PREVIEW_FRAMES >= keyframe_spacing else []
  frame_filter = f"[0:v]setpts=(PTS-STARTPTS)*{PREVIEW_FRAMES / PREVIEW_FPS / length},fps={PREVIEW_FPS}{video_filter}"
  await create_preview(the_pond, preview_path, [*decode_options, "-i", source], frame_filter)


async def create_thumbnail(the_pond, thumbnail_path, source, output_options):
  middle = await get_duration(source) / 2
  cmd = ["ffmpeg", "-loglevel", "error", "-skip_frame", "nokey", "-noaccurate_seek", "-ss", f"{middle:.2f}", "-i", str(source), *output_options,
         "-frames:v", "1", "-q:v", "4", "-f", "image2"]
  await utilities.create_file(the_pond, thumbnail_path, cmd)


async def prepare_preview(the_pond, route, sources):
  preview_path = frogpilot_variables.THE_POND_THUMBNAILS_PATH / f"{route}--timelapse.mp4"
  async with the_pond.file_locks.setdefault(preview_path, asyncio.Lock()):
    if not preview_path.is_file():
      try:
        await create_route_preview(the_pond, preview_path, sources)
      except (RuntimeError, ValueError):
        readable = [source for source in sources if await is_readable(source)]
        if not readable:
          raise
        await create_route_preview(the_pond, preview_path, readable)
  return preview_path


async def prepare_thumbnail(the_pond, route, sources):
  thumbnail_path = frogpilot_variables.THE_POND_THUMBNAILS_PATH / f"{route}--thumbnail.jpg"
  async with the_pond.file_locks.setdefault(thumbnail_path, asyncio.Lock()):
    if not thumbnail_path.is_file():
      middle = (len(sources) - 1) / 2
      positions = sorted(range(len(sources)), key=lambda position: abs(position - middle))
      for position in positions:
        try:
          await create_thumbnail(the_pond, thumbnail_path, sources[position], [])
          break
        except (RuntimeError, ValueError):
          if position == positions[-1]:
            raise
  return thumbnail_path


async def prepare_routes(the_pond):
  for route, segments in sorted(get_routes().items(), reverse=True):
    sources = get_low_camera_sources(segments)
    if sources:
      await utilities.run_and_report(prepare_thumbnail(the_pond, route, sources))

    qlogs = get_qlogs(segments, False)
    if qlogs:
      await summarize_drives(the_pond, {route: qlogs})

    if sources:
      await utilities.run_and_report(prepare_preview(the_pond, route, sources))


async def get_duration(path):
  return float(await utilities.run_process(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(path)]))


async def is_readable(path):
  try:
    await get_duration(path)
    return True
  except (RuntimeError, ValueError):
    return False


def trim_video_cache():
  videos = sorted(frogpilot_variables.THE_POND_VIDEOS_PATH.glob("*.mp4"), key=lambda path: path.stat().st_mtime, reverse=True)

  total_size = 0
  for path in videos:
    total_size += path.stat().st_size
    if total_size > MAX_VIDEO_CACHE_SIZE:
      path.unlink()


@routes.post("/api/routes/delete")
async def delete_selected_routes(request):
  selected_routes = (await utilities.read_json(request)).get("routes")
  if not isinstance(selected_routes, list):
    raise web.HTTPBadRequest(text="invalid_routes")

  all_routes = get_routes()
  if any(not isinstance(route, str) or route not in all_routes for route in selected_routes):
    raise web.HTTPNotFound(text="not_found")
  if get_recording_route(request.app[utilities.THE_POND]) in selected_routes:
    raise web.HTTPConflict(text="recording")

  for route in selected_routes:
    await asyncio.to_thread(delete_route, route, all_routes[route])
  return web.Response(status=204)


@routes.delete("/api/routes/{route}")
async def delete_single_route(request):
  route, segments = get_route(request)
  if is_recording(request.app[utilities.THE_POND], route):
    raise web.HTTPConflict(text="recording")

  await asyncio.to_thread(delete_route, route, segments)
  return web.Response(status=204)


@routes.get("/api/routes")
async def get_route_list(request):
  the_pond = request.app[utilities.THE_POND]
  route_list, unsummarized = await asyncio.to_thread(list_routes, get_recording_route(the_pond))

  job = utilities.get_job(the_pond, "drive_summaries")
  if unsummarized and (job is None or job["state"] != "running"):
    utilities.start_job(the_pond, "drive_summaries", summarize_drives(the_pond, unsummarized), {})
  return web.json_response(route_list)


@routes.get("/api/routes/{route}")
async def get_route_details(request):
  route, segments = get_route(request)
  recording = is_recording(request.app[utilities.THE_POND], route)

  uploaded = {}
  for kind, file_name in {**CAMERA_FILES, **LOG_FILES}.items():
    uploaded[kind] = sum(get_xattr(path / file_name, UPLOAD_ATTR_NAME) == UPLOAD_ATTR_VALUE for path in segments.values())

  audio = False
  for path in finished_segments(segments, recording):
    low_camera = path / CAMERA_FILES["low"]
    if not low_camera.is_file():
      continue
    try:
      output = await utilities.run_process(["ffprobe", "-v", "error", "-select_streams", "a", "-show_entries", "stream=index",
                                            "-of", "csv=p=0", str(low_camera)])
    except RuntimeError:
      continue
    audio = bool(output.strip())
    break

  return web.json_response({
    **await asyncio.to_thread(get_route_info, route, segments, recording),
    "audio": audio,
    "cameras": [camera for camera, file_name in CAMERA_FILES.items() if any((path / file_name).is_file() for path in segments.values())],
    "route_ids": sorted(await asyncio.to_thread(get_routes), reverse=True),
    "segment_numbers": list(segments),
    "uploaded": uploaded,
  })


@routes.get("/api/routes/{route}/download/{kind}")
async def download_route(request):
  route, segments = get_route(request)
  kind = request.match_info["kind"]
  recording = is_recording(request.app[utilities.THE_POND], route)

  try:
    start = int(request.query.get("start", min(segments)))
    end = int(request.query.get("end", max(segments)))
  except ValueError:
    raise web.HTTPBadRequest(text="invalid_range") from None
  segments = {number: path for number, path in segments.items() if start <= number <= end}

  if kind in LOG_FILES:
    members = []
    for path in finished_segments(segments, recording):
      if (path / LOG_FILES[kind]).is_file():
        members += ["-C", str(path.parent), f"{path.name}/{LOG_FILES[kind]}"]
    if not members:
      raise web.HTTPNotFound(text="not_found")
    return await utilities.stream_process(request, ["tar", "-cf", "-", *members], f"{route}-{kind}.tar")

  if kind not in CAMERA_FILES:
    raise web.HTTPNotFound(text="not_found")

  files = [str(path / CAMERA_FILES[kind]) for path in finished_segments(segments, recording) if (path / CAMERA_FILES[kind]).is_file()]
  if not files:
    raise web.HTTPNotFound(text="not_found")

  if kind == "low":
    source = ["-f", "concat", "-safe", "0", "-protocol_whitelist", "file,pipe", "-i", "pipe:0"]
    concat_list = "".join(f"file 'file:{file}'\n" for file in files).encode()
  else:
    source = ["-framerate", str(CAMERA_FPS), "-i", f"concat:{'|'.join(files)}", "-tag:v", "hvc1"]
    concat_list = None

  cmd = ["ffmpeg", "-loglevel", "error", *source, "-c", "copy", "-movflags", "frag_keyframe+empty_moov", "-f", "mp4", "pipe:1"]
  return await utilities.stream_process(request, cmd, f"{route}-{kind}.mp4", concat_list)


@routes.get("/api/routes/{route}/drive")
async def get_drive(request):
  the_pond = request.app[utilities.THE_POND]
  route, segments = get_route(request)

  qlogs = get_qlogs(segments, is_recording(the_pond, route))
  if not qlogs:
    raise web.HTTPNotFound(text="not_found")

  drive = await summarize_drive(the_pond, route, qlogs)
  return web.json_response({key: value for key, value in drive.items() if key != "source"})


@routes.get("/api/routes/{route}/preview")
async def get_preview(request):
  the_pond = request.app[utilities.THE_POND]
  route, segments = get_route(request)

  sources = get_low_camera_sources(segments)
  if not sources:
    raise web.HTTPNotFound(text="not_found")
  if is_recording(the_pond, route):
    raise web.HTTPConflict(text="recording")

  return web.FileResponse(await prepare_preview(the_pond, route, sources))


@routes.get("/api/routes/{route}/thumbnail")
async def get_thumbnail(request):
  the_pond = request.app[utilities.THE_POND]
  route, segments = get_route(request)
  if is_recording(the_pond, route):
    return web.Response(status=202)

  sources = get_low_camera_sources(segments)
  if not sources:
    raise web.HTTPNotFound(text="not_found")

  return web.FileResponse(await prepare_thumbnail(the_pond, route, sources))


@routes.get("/api/routes/{route}/{segment:\\d+}/{camera}")
async def get_video(request):
  the_pond = request.app[utilities.THE_POND]
  route, segments = get_route(request)
  path = get_segment(segments, request.match_info["segment"])

  camera = request.match_info["camera"]
  if camera not in CAMERA_FILES or not (path / CAMERA_FILES[camera]).is_file():
    raise web.HTTPNotFound(text="not_found")
  if is_recording(the_pond, route) and any(path.glob("*.lock")):
    raise web.HTTPConflict(text="recording")

  source = path / CAMERA_FILES[camera]
  video_path = frogpilot_variables.THE_POND_VIDEOS_PATH / f"{path.name}--{camera}.mp4"

  async with the_pond.file_locks.setdefault(video_path, asyncio.Lock()):
    if not video_path.is_file() or video_path.stat().st_mtime < source.stat().st_mtime:
      if camera == "low":
        cmd = ["ffmpeg", "-loglevel", "error", "-i", str(source), "-c", "copy", "-movflags", "+faststart", "-f", "mp4"]
      else:
        cmd = ["ffmpeg", "-loglevel", "error", "-framerate", str(CAMERA_FPS), "-i", str(source), "-c", "copy", "-tag:v", "hvc1",
               "-movflags", "+faststart", "-f", "mp4"]
      await utilities.create_file(the_pond, video_path, cmd)
      await asyncio.to_thread(trim_video_cache)
    else:
      os.utime(video_path)

  return web.FileResponse(video_path)


@routes.put("/api/routes/{route}/name")
async def rename_route(request):
  route, segments = get_route(request)
  if is_recording(request.app[utilities.THE_POND], route):
    raise web.HTTPConflict(text="recording")

  name = (await utilities.read_json(request)).get("name")
  if not isinstance(name, str):
    raise web.HTTPBadRequest(text="invalid_name")

  name = name.strip()
  if not 0 < len(name) <= MAX_NAME_LENGTH or not name.isprintable():
    raise web.HTTPBadRequest(text="invalid_name")

  xattr.setxattr(next(iter(segments.values())), NAME_ATTR_NAME, name.encode())
  return web.Response(status=204)


@routes.delete("/api/routes/{route}/name")
async def reset_route_name(request):
  route, segments = get_route(request)

  first_segment = next(iter(segments.values()))
  if get_xattr(first_segment, NAME_ATTR_NAME) is not None:
    xattr.removexattr(first_segment, NAME_ATTR_NAME)
  return web.Response(status=204)


@routes.put("/api/routes/{route}/preserve")
async def preserve_route(request):
  route, segments = get_route(request)
  if is_recording(request.app[utilities.THE_POND], route):
    raise web.HTTPConflict(text="recording")

  for path in segments.values():
    xattr.setxattr(path, frogpilot_variables.ROUTE_PRESERVE_ATTR_NAME, frogpilot_variables.ROUTE_PRESERVED)
  return web.Response(status=204)


@routes.delete("/api/routes/{route}/preserve")
async def unpreserve_route(request):
  route, segments = get_route(request)
  for path in segments.values():
    if get_xattr(path, frogpilot_variables.ROUTE_PRESERVE_ATTR_NAME) is not None:
      xattr.removexattr(path, frogpilot_variables.ROUTE_PRESERVE_ATTR_NAME)
  return web.Response(status=204)
