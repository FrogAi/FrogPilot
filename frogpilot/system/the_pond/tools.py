import asyncio
import datetime
import hashlib
import json
import string
import time

from aiohttp import web

from openpilot.common.basedir import BASEDIR
from openpilot.system.loggerd.config import SEGMENT_LENGTH

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import dashcam, utilities

MAX_NAME_LENGTH = 64
MAX_SHELL_IDLE_TIME = 300
MAX_TMUX_CAPTURES = 4
MAX_TMUX_TEXT_LENGTH = 1000

REPORT_EDGE_TIME = 10
REPORT_FILES = {"qcamera": dashcam.CAMERA_FILES["low"], "qlog": dashcam.LOG_FILES["qlog"], "rlog": dashcam.LOG_FILES["rlog"]}
REPORT_TYPES = ("full", "quick")

TMUX_KEYS = {"Down", "Enter", "Escape", "Left", "Right", "Tab", "Up", *(f"C-{letter}" for letter in string.ascii_lowercase)}
TMUX_LOG_EXTENSION = ".log"
TMUX_SESSIONS = {"openpilot": "comma", "shell": "the_pond_shell"}

routes = web.RouteTableDef()


async def close_idle_shell(the_pond):
  if the_pond.shell_used is not None and (the_pond.locked or time.monotonic() - the_pond.shell_used > MAX_SHELL_IDLE_TIME):
    await close_shell(the_pond)


async def close_shell(the_pond):
  the_pond.shell_used = None

  process = await asyncio.create_subprocess_exec("tmux", "kill-session", "-t", f"={TMUX_SESSIONS['shell']}", stderr=asyncio.subprocess.DEVNULL)
  await process.wait()


async def get_tmux_session(the_pond, name):
  session = TMUX_SESSIONS.get(name)
  if session is None:
    raise web.HTTPBadRequest(text="invalid_session")

  if name == "shell":
    the_pond.shell_used = time.monotonic()

    process = await asyncio.create_subprocess_exec("tmux", "has-session", "-t", f"={session}", stderr=asyncio.subprocess.DEVNULL)
    if await process.wait() != 0:
      pythonpath = f"{BASEDIR}/frogpilot/third_party:{BASEDIR}"
      await utilities.run_process(["tmux", "new-session", "-d", "-s", session, "-c", BASEDIR, "-e", f"PYTHONPATH={pythonpath}"])

  return session


def check_issue_report_available(the_pond):
  if not frogpilot_variables.get_frogpilot_toggles(the_pond.sm).issue_report_available:
    raise web.HTTPNotFound(text="not_found")


def get_issue_report_state(frogpilot_process_state):
  if not frogpilot_process_state.issueReport:
    return {"status": ""}
  return {"status": "", **json.loads(frogpilot_process_state.issueReport)}


def get_log(request, directory):
  path = utilities.get_file(directory, request.match_info["name"])

  headers = {"Content-Type": "text/plain; charset=utf-8"}
  if "download" in request.query:
    headers["Content-Disposition"] = f'attachment; filename="{path.name}"'
  return web.FileResponse(path, headers=headers)


def get_marked_segments(segments, moment):
  positions = [moment // SEGMENT_LENGTH]
  offset = moment % SEGMENT_LENGTH

  if offset <= REPORT_EDGE_TIME and positions[0] > 0:
    positions.insert(0, positions[0] - 1)
  if SEGMENT_LENGTH - offset <= REPORT_EDGE_TIME and positions[-1] + 1 < len(segments):
    positions.append(positions[-1] + 1)

  segment_numbers = list(segments)
  return [segment_numbers[position] for position in positions]


def get_report_files(segments, recording, report_type, moment):
  finished = set(dashcam.finished_segments(segments, recording))

  wanted = [("qlog", segment) for segment in segments]
  if report_type == "full":
    for segment in get_marked_segments(segments, moment):
      wanted += [("qcamera", segment), ("rlog", segment)]

  files = []
  for kind, segment in wanted:
    path = segments[segment] / REPORT_FILES[kind]
    if segments[segment] in finished and path.is_file():
      files.append({"kind": kind, "path": str(path), "segment": segment, "size_bytes": path.stat().st_size})
  return files


def get_report_issues():
  issues = list(frogpilot_variables.REPORT_ISSUES)
  if frogpilot_paths.ERROR_LOG_PATH.is_file():
    issues.insert(0, frogpilot_variables.REPORT_CRASH_ISSUE)
  return issues


async def read_report_logs(the_pond, body):
  route = body.get("route")
  if not isinstance(route, str) or dashcam.ROUTE_PATTERN.match(route) is None:
    raise web.HTTPBadRequest(text="invalid_route")

  recording = dashcam.is_recording(the_pond, route)

  segments = (await asyncio.to_thread(dashcam.get_routes, route)).get(route)
  if segments is None:
    raise web.HTTPBadRequest(text="invalid_route")

  report_type = body.get("type")
  if report_type not in REPORT_TYPES:
    raise web.HTTPBadRequest(text="invalid_type")

  moment = None
  if report_type == "full":
    moment = body.get("moment")
    if type(moment) is not int or not 0 <= moment < len(segments) * SEGMENT_LENGTH:
      raise web.HTTPBadRequest(text="invalid_moment")

  files = await asyncio.to_thread(get_report_files, segments, recording, report_type, moment)
  if not files:
    raise web.HTTPConflict(text="no_logs")
  return route, report_type, files


@routes.delete("/api/error_logs")
async def delete_error_logs(request):
  for path in frogpilot_paths.ERROR_LOGS_PATH.glob("*"):
    if path.is_file():
      path.unlink()
  return web.Response(status=204)


@routes.delete("/api/error_logs/{name}")
async def delete_error_log(request):
  utilities.get_file(frogpilot_paths.ERROR_LOGS_PATH, request.match_info["name"]).unlink()
  return web.Response(status=204)


@routes.get("/api/error_logs")
async def get_error_logs(request):
  return web.json_response(utilities.list_files(frogpilot_paths.ERROR_LOGS_PATH))


@routes.get("/api/error_logs/{name}")
async def get_error_log(request):
  return get_log(request, frogpilot_paths.ERROR_LOGS_PATH)


@routes.get("/api/issue_report")
async def get_issue_report(request):
  the_pond = request.app[utilities.THE_POND]
  check_issue_report_available(the_pond)

  request_time = request.query.get("request_time", "0")
  if not request_time.isdecimal():
    raise web.HTTPBadRequest(text="invalid_request_time")

  frogpilot_process_state = the_pond.sm["frogpilotProcessState"]

  issue_report = get_issue_report_state(frogpilot_process_state)
  if int(request_time) > frogpilot_process_state.issueReportRequestTime:
    issue_report = {"status": "pending"}

  return web.json_response({
    "described_issues": frogpilot_variables.REPORT_DESCRIBED_ISSUES,
    "description_max_length": frogpilot_variables.REPORT_DESCRIPTION_MAX_LENGTH,
    "description_min_length": frogpilot_variables.REPORT_DESCRIPTION_MIN_LENGTH,
    "discord_user": the_pond.params.get("DiscordUsername"),
    "discord_user_max_length": frogpilot_variables.REPORT_DISCORD_USER_MAX_LENGTH,
    "issues": get_report_issues(),
    **issue_report,
  })


@routes.post("/api/issue_report")
async def send_issue_report(request):
  the_pond = request.app[utilities.THE_POND]
  check_issue_report_available(the_pond)

  if not frogpilot_utilities.is_online(the_pond.sm["deviceState"]):
    raise web.HTTPConflict(text="offline")
  if get_issue_report_state(the_pond.sm["frogpilotProcessState"])["status"] in ("pending", "uploading"):
    raise web.HTTPConflict(text="pending")

  body = await utilities.read_json(request)

  issue = body.get("issue")
  if issue not in get_report_issues():
    raise web.HTTPBadRequest(text="invalid_issue")

  if issue in frogpilot_variables.REPORT_DESCRIBED_ISSUES:
    description = body.get("description")
    if not isinstance(description, str):
      raise web.HTTPBadRequest(text="invalid_description")
    if not frogpilot_variables.REPORT_DESCRIPTION_MIN_LENGTH <= len(description) <= frogpilot_variables.REPORT_DESCRIPTION_MAX_LENGTH:
      raise web.HTTPBadRequest(text="invalid_description")

    description = description.strip()
    if not description or not description.isprintable():
      raise web.HTTPBadRequest(text="invalid_description")

    if issue.endswith(":"):
      issue = f"{issue} {description}"
    else:
      issue = f"{issue}: {description}"

  discord_user = body.get("discord_user")
  if not isinstance(discord_user, str) or len(discord_user) > frogpilot_variables.REPORT_DISCORD_USER_MAX_LENGTH:
    raise web.HTTPBadRequest(text="invalid_discord_user")

  discord_user = discord_user.strip()
  if not discord_user or not discord_user.isprintable():
    raise web.HTTPBadRequest(text="invalid_discord_user")

  report = {"DiscordUser": discord_user, "Issue": issue}
  if "route" in body:
    route, report_type, files = await read_report_logs(the_pond, body)

    logged_files = []
    for file in files:
      logged_files.append({"kind": file["kind"], "path": file["path"], "segment": file["segment"]})

    report["Logs"] = {"files": logged_files, "type": report_type}
    report["Route"] = route

  the_pond.params.put_nonblocking("DiscordUsername", discord_user)

  request_time = the_pond.send_request("issueReport", json.dumps(report))
  return web.json_response({"request_time": request_time})


@routes.post("/api/issue_report/files")
async def get_issue_report_files(request):
  the_pond = request.app[utilities.THE_POND]
  check_issue_report_available(the_pond)

  _, _, files = await read_report_logs(the_pond, await utilities.read_json(request))

  sizes = []
  for file in files:
    sizes.append({"kind": file["kind"], "segment": file["segment"], "size_bytes": file["size_bytes"]})

  return web.json_response({
    "files": sizes,
    "metered": the_pond.sm["deviceState"].networkMetered,
    "size_bytes": sum(file["size_bytes"] for file in files),
  })


@routes.post("/api/issue_report/retry")
async def retry_issue_report(request):
  the_pond = request.app[utilities.THE_POND]
  check_issue_report_available(the_pond)

  if not frogpilot_utilities.is_online(the_pond.sm["deviceState"]):
    raise web.HTTPConflict(text="offline")

  if get_issue_report_state(the_pond.sm["frogpilotProcessState"])["status"] != "failed":
    raise web.HTTPConflict(text="no_failed_report")

  request_time = the_pond.send_request("retryIssueReport")
  return web.json_response({"request_time": request_time})


@routes.get("/api/tmux")
async def get_tmux_pane(request):
  the_pond = request.app[utilities.THE_POND]
  session = await get_tmux_session(the_pond, request.query.get("session", "openpilot"))
  pane = await utilities.run_process(["tmux", "capture-pane", "-p", "-J", "-S", "-", "-t", session])

  capture_id = hashlib.sha256(pane).hexdigest()
  since = request.query.get("since")
  if since == capture_id:
    return web.Response(status=204)

  lines = pane.decode(errors="replace").rstrip().split("\n")
  previous_lines = the_pond.tmux_captures.get(since, [])

  kept_lines = 0
  for previous_line, line in zip(previous_lines, lines, strict=False):
    if previous_line != line:
      break
    kept_lines += 1

  the_pond.tmux_captures[capture_id] = lines
  while len(the_pond.tmux_captures) > MAX_TMUX_CAPTURES:
    del the_pond.tmux_captures[next(iter(the_pond.tmux_captures))]

  return web.json_response({"id": capture_id, "keep": kept_lines, "lines": lines[kept_lines:]})


@routes.post("/api/tmux")
async def send_tmux_input(request):
  body = await utilities.read_json(request)
  session = await get_tmux_session(request.app[utilities.THE_POND], body.get("session", "openpilot"))

  if "key" in body:
    key = body["key"]
    if not isinstance(key, str) or key not in TMUX_KEYS:
      raise web.HTTPBadRequest(text="invalid_key")

    await utilities.run_process(["tmux", "send-keys", "-t", session, key])
    return web.Response(status=204)

  text = body.get("text")
  if not isinstance(text, str) or not 0 < len(text) <= MAX_TMUX_TEXT_LENGTH or not text.isprintable():
    raise web.HTTPBadRequest(text="invalid_text")

  await utilities.run_process(["tmux", "send-keys", "-t", session, "-l", "--", text])
  await utilities.run_process(["tmux", "send-keys", "-t", session, "Enter"])
  return web.Response(status=204)


@routes.delete("/api/tmux/shell")
async def delete_shell(request):
  await close_shell(request.app[utilities.THE_POND])
  return web.Response(status=204)


@routes.delete("/api/tmux_logs")
async def delete_tmux_logs(request):
  for path in frogpilot_paths.TMUX_LOGS_PATH.glob(f"*{TMUX_LOG_EXTENSION}"):
    path.unlink()
  return web.Response(status=204)


@routes.delete("/api/tmux_logs/{name}")
async def delete_tmux_log(request):
  utilities.get_file(frogpilot_paths.TMUX_LOGS_PATH, request.match_info["name"]).unlink()
  return web.Response(status=204)


@routes.get("/api/tmux_logs")
async def get_tmux_logs(request):
  return web.json_response(utilities.list_files(frogpilot_paths.TMUX_LOGS_PATH, f"*{TMUX_LOG_EXTENSION}"))


@routes.get("/api/tmux_logs/{name}")
async def get_tmux_log(request):
  return get_log(request, frogpilot_paths.TMUX_LOGS_PATH)


@routes.post("/api/tmux_logs")
async def capture_tmux_log(request):
  session = await get_tmux_session(request.app[utilities.THE_POND], (await utilities.read_json(request)).get("session", "openpilot"))
  output = await utilities.run_process(["tmux", "capture-pane", "-p", "-J", "-S", "-", "-t", session])

  path = frogpilot_paths.TMUX_LOGS_PATH / f"tmux_log_{datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}{TMUX_LOG_EXTENSION}"
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_bytes(output)
  return web.json_response({"name": path.name})


@routes.put("/api/tmux_logs/{name}")
async def rename_tmux_log(request):
  path = utilities.get_file(frogpilot_paths.TMUX_LOGS_PATH, request.match_info["name"])

  name = (await utilities.read_json(request)).get("name")
  if not isinstance(name, str) or len(name) > MAX_NAME_LENGTH or not utilities.is_valid_name(name):
    raise web.HTTPBadRequest(text="invalid_name")

  new_path = path.with_name(f"{name}{TMUX_LOG_EXTENSION}")
  if new_path.exists():
    raise web.HTTPConflict(text="name_taken")

  path.rename(new_path)
  return web.Response(status=204)
