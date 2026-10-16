import datetime
import hashlib
import json
import string

from aiohttp import web

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond import dashcam, utilities

MAX_NAME_LENGTH = 64
MAX_TMUX_CAPTURES = 4
MAX_TMUX_TEXT_LENGTH = 1000

TMUX_KEYS = {"Down", "Enter", "Escape", "Left", "Right", "Tab", "Up", *(f"C-{letter}" for letter in string.ascii_lowercase)}
TMUX_LOG_EXTENSION = ".log"

routes = web.RouteTableDef()


def check_issue_report_available(the_pond):
  if not utilities.is_issue_report_available(the_pond.params):
    raise web.HTTPNotFound(text="not_found")


def get_issue_report_status(frogpilot_process_state):
  if not frogpilot_process_state.issueReport:
    return ""
  return json.loads(frogpilot_process_state.issueReport).get("status", "")


def get_log(request, directory):
  path = utilities.get_file(directory, request.match_info["name"])

  headers = {"Content-Type": "text/plain; charset=utf-8"}
  if "download" in request.query:
    headers["Content-Disposition"] = f'attachment; filename="{path.name}"'
  return web.FileResponse(path, headers=headers)


def get_report_issues():
  issues = list(frogpilot_variables.REPORT_ISSUES)
  if frogpilot_variables.ERROR_LOG_PATH.is_file():
    issues.insert(0, frogpilot_variables.REPORT_CRASH_ISSUE)
  return issues


@routes.delete("/api/error_logs")
async def delete_error_logs(request):
  for path in frogpilot_variables.ERROR_LOGS_PATH.glob("*"):
    if path.is_file():
      path.unlink()
  return web.Response(status=204)


@routes.delete("/api/error_logs/{name}")
async def delete_error_log(request):
  utilities.get_file(frogpilot_variables.ERROR_LOGS_PATH, request.match_info["name"]).unlink()
  return web.Response(status=204)


@routes.get("/api/error_logs")
async def get_error_logs(request):
  return web.json_response(utilities.list_files(frogpilot_variables.ERROR_LOGS_PATH))


@routes.get("/api/error_logs/{name}")
async def get_error_log(request):
  return get_log(request, frogpilot_variables.ERROR_LOGS_PATH)


@routes.get("/api/issue_report")
async def get_issue_report(request):
  the_pond = request.app[utilities.THE_POND]
  check_issue_report_available(the_pond)

  request_time = request.query.get("request_time", "0")
  if not request_time.isdecimal():
    raise web.HTTPBadRequest(text="invalid_request_time")

  frogpilot_process_state = the_pond.sm["frogpilotProcessState"]

  status = get_issue_report_status(frogpilot_process_state)
  if int(request_time) > frogpilot_process_state.issueReportRequestTime:
    status = "pending"

  return web.json_response({
    "described_issues": frogpilot_variables.REPORT_DESCRIBED_ISSUES,
    "description_max_length": frogpilot_variables.REPORT_DESCRIPTION_MAX_LENGTH,
    "description_min_length": frogpilot_variables.REPORT_DESCRIPTION_MIN_LENGTH,
    "discord_user": the_pond.params.get("DiscordUsername"),
    "discord_user_max_length": frogpilot_variables.REPORT_DISCORD_USER_MAX_LENGTH,
    "issues": get_report_issues(),
    "status": status,
  })


@routes.post("/api/issue_report")
async def send_issue_report(request):
  the_pond = request.app[utilities.THE_POND]
  check_issue_report_available(the_pond)

  if not frogpilot_utilities.is_online(the_pond.sm["deviceState"]):
    raise web.HTTPConflict(text="offline")
  if get_issue_report_status(the_pond.sm["frogpilotProcessState"]) == "pending":
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
    route = body["route"]
    if not isinstance(route, str) or dashcam.ROUTE_PATTERN.match(route) is None or route not in dashcam.get_routes(route):
      raise web.HTTPBadRequest(text="invalid_route")
    report["Route"] = route

  the_pond.params.put_nonblocking("DiscordUsername", discord_user)

  request_time = the_pond.send_request("issueReport", json.dumps(report))
  return web.json_response({"request_time": request_time})


@routes.post("/api/issue_report/retry")
async def retry_issue_report(request):
  the_pond = request.app[utilities.THE_POND]
  check_issue_report_available(the_pond)

  if not frogpilot_utilities.is_online(the_pond.sm["deviceState"]):
    raise web.HTTPConflict(text="offline")

  frogpilot_process_state = the_pond.sm["frogpilotProcessState"]
  if get_issue_report_status(frogpilot_process_state) != "failed":
    raise web.HTTPConflict(text="no_failed_report")

  request_time = the_pond.send_request("issueReport", frogpilot_process_state.issueReport)
  return web.json_response({"request_time": request_time})


@routes.get("/api/tmux")
async def get_tmux_pane(request):
  the_pond = request.app[utilities.THE_POND]
  pane = await utilities.run_process(["tmux", "capture-pane", "-p", "-J", "-S", "-"])

  capture_id = hashlib.sha256(pane).hexdigest()
  since = request.query.get("since")
  if since == capture_id:
    return web.Response(status=204)

  lines = pane.decode(errors="replace").split("\n")
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

  if "key" in body:
    key = body["key"]
    if not isinstance(key, str) or key not in TMUX_KEYS:
      raise web.HTTPBadRequest(text="invalid_key")

    await utilities.run_process(["tmux", "send-keys", key])
    return web.Response(status=204)

  text = body.get("text")
  if not isinstance(text, str) or not 0 < len(text) <= MAX_TMUX_TEXT_LENGTH or not text.isprintable():
    raise web.HTTPBadRequest(text="invalid_text")

  await utilities.run_process(["tmux", "send-keys", "-l", "--", text])
  await utilities.run_process(["tmux", "send-keys", "Enter"])
  return web.Response(status=204)


@routes.delete("/api/tmux_logs")
async def delete_tmux_logs(request):
  for path in frogpilot_variables.TMUX_LOGS_PATH.glob(f"*{TMUX_LOG_EXTENSION}"):
    path.unlink()
  return web.Response(status=204)


@routes.delete("/api/tmux_logs/{name}")
async def delete_tmux_log(request):
  utilities.get_file(frogpilot_variables.TMUX_LOGS_PATH, request.match_info["name"]).unlink()
  return web.Response(status=204)


@routes.get("/api/tmux_logs")
async def get_tmux_logs(request):
  return web.json_response(utilities.list_files(frogpilot_variables.TMUX_LOGS_PATH, f"*{TMUX_LOG_EXTENSION}"))


@routes.get("/api/tmux_logs/{name}")
async def get_tmux_log(request):
  return get_log(request, frogpilot_variables.TMUX_LOGS_PATH)


@routes.post("/api/tmux_logs")
async def capture_tmux_log(request):
  output = await utilities.run_process(["tmux", "capture-pane", "-p", "-J", "-S", "-"])

  path = frogpilot_variables.TMUX_LOGS_PATH / f"tmux_log_{datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}{TMUX_LOG_EXTENSION}"
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_bytes(output)
  return web.json_response({"name": path.name})


@routes.put("/api/tmux_logs/{name}")
async def rename_tmux_log(request):
  path = utilities.get_file(frogpilot_variables.TMUX_LOGS_PATH, request.match_info["name"])

  name = (await utilities.read_json(request)).get("name")
  if not isinstance(name, str) or len(name) > MAX_NAME_LENGTH or not utilities.is_valid_name(name):
    raise web.HTTPBadRequest(text="invalid_name")

  new_path = path.with_name(f"{name}{TMUX_LOG_EXTENSION}")
  if new_path.exists():
    raise web.HTTPConflict(text="name_taken")

  path.rename(new_path)
  return web.Response(status=204)
