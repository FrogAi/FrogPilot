import math

from aiohttp import web
from functools import cache

from openpilot.common.params import Params, ParamKeyType

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system import frogpilot_stats
from openpilot.frogpilot.system.the_pond import utilities
from openpilot.frogpilot.system.the_pond.toggle_panels import (data_settings, device_settings, frogpilot_settings, lateral_settings, longitudinal_settings,
                                                               maps_settings, model_settings, navigation_settings, sounds_settings, theme_settings,
                                                               utilities_settings, vehicle_settings, visual_settings, wheel_settings)

EPSILON = 1e-6

METRIC_CLAMPED_KEYS = ("LaneLinesWidth", "PathWidth", "RoadEdgesWidth")

NUMBER_WIDGETS = ("value", "value_buttons")
STATE_FIELDS = ("changed", "current", "default", "disabled", "display", "hidden", "locked", "manage_visible", "reboot", "sub_toggles")
WRITABLE_WIDGETS = ("button_toggle", "manage", "select", "toggle", "value", "value_buttons")

SERVED_PANELS = (data_settings, device_settings, frogpilot_settings, lateral_settings, longitudinal_settings, maps_settings, model_settings,
                 navigation_settings, sounds_settings, theme_settings, utilities_settings, vehicle_settings, visual_settings, wheel_settings)

routes = web.RouteTableDef()

def row_id(row):
  return row.get("id") or row["key"]


def level_key(row):
  return row.get("level_key") or row_id(row)


def child_paths(row):
  children = row.get("children")
  if not children:
    return []
  if isinstance(children[0], tuple):
    return list(children)
  return [children]


def is_sequence(row):
  return "steps" in row.get("picker", {})


def is_writable(row):
  if row["key"] is None or "link" in row:
    return False
  return row["widget"] in WRITABLE_WIDGETS or row.get("checkable", False) or "picker" in row


def resolve(value, context):
  if callable(value):
    return value(context)
  return value


def find_openers(rows):
  openers = {}
  for row in rows:
    for path in child_paths(row):
      openers[path] = (row, None)
    if callable(row.get("options")):
      continue
    for option in row.get("options", ()):
      if "children" in option:
        openers[option["children"]] = (row, option)
  return openers


def has_dynamic_options(options):
  return callable(options) or any("visible" in option for option in options)


def dynamic_fields(row):
  fields = list(STATE_FIELDS)

  for field in ("description", "title"):
    if callable(row[field]) or f"{field}_metric" in row:
      fields.append(field)

  if row["widget"] in NUMBER_WIDGETS:
    spec = row["value"]
    if spec["metric"] is not None or callable(spec["min"]) or callable(spec["max"]):
      fields.append("number")

  if any("guard" in action for action in row.get("actions", {}).values()):
    fields.append("actions")

  if "options" in row and has_dynamic_options(row["options"]):
    fields.append("options")

  if is_sequence(row):
    if any(has_dynamic_options(step["options"]) for step in row["picker"]["steps"]):
      fields.append("picker")
  elif "picker" in row and has_dynamic_options(row["options"]):
    fields.append("picker")
  return fields


def group_by_key(rows):
  grouped = {}
  for row in rows:
    if row["key"] is not None:
      grouped.setdefault(row["key"], []).append(row)
  return grouped


def group_by_sub_toggle(rows):
  grouped = {}
  for row in rows:
    for sub_toggle in row.get("sub_toggles", ()):
      grouped.setdefault(sub_toggle["key"], []).append(row)
  return grouped


def group_by_path(rows):
  grouped = {}
  for row in rows:
    grouped.setdefault(row["path"], []).append(row)
  return grouped


ROWS = [row for panel in SERVED_PANELS for row in panel.ROWS]

DYNAMIC_FIELDS = {row_id(row): dynamic_fields(row) for row in ROWS}
OPENERS = find_openers(ROWS)
PANEL_ROWS = group_by_path(ROWS)
ROW_IDS = {row_id(row): row for row in ROWS}
ROW_KEYS = {key for row in ROWS for key in row.get("keys", ())}
SUB_TOGGLE_ROWS = group_by_sub_toggle(ROWS)
TABLE_ROWS = group_by_key(ROWS)
WRITABLE_ROWS = group_by_key([row for row in ROWS if is_writable(row)])

KEYS = {*TABLE_ROWS, *SUB_TOGGLE_ROWS, *ROW_IDS, *ROW_KEYS}


@cache
def get_tuning_levels():
  params = Params()
  return {key.decode(): params.get_tuning_level(key) for key in params.all_keys()}


def convert_metric(key, value, metric):
  converted = utilities.convert_metric(key, value, metric)
  if key not in METRIC_CLAMPED_KEYS:
    return converted

  spec = TABLE_ROWS[key][0]["value"]
  if metric:
    spec = spec["metric"]
  return min(max(converted, spec["min"]), spec["max"])


def write_param(params, key, value):
  key_type = params.get_type(key)
  if key_type == ParamKeyType.BOOL:
    params.put_bool(key, value)
  elif key_type == ParamKeyType.INT:
    params.put(key, int(value))
  elif key_type == ParamKeyType.FLOAT:
    params.put(key, round(float(value), 6))
  else:
    params.put(key, value)


class Context:
  def __init__(self, the_pond, request=None):
    params = the_pond.params
    sm = the_pond.sm

    self.params = params
    self.request = request
    self.the_pond = the_pond
    self.frogpilot_toggles = frogpilot_variables.get_frogpilot_toggles(sm)
    self.level = params.get("TuningLevel")
    self.metric = params.get_bool("IsMetric")
    self.online = frogpilot_utilities.is_online(sm["deviceState"])
    self.started = sm["deviceState"].started

    git_branch = params.get("GitBranch")
    self.development_branch = git_branch == frogpilot_variables.DEVELOPMENT_BRANCH
    self.vetting_branch = git_branch == frogpilot_variables.VETTING_BRANCH

    self.can_disable_openpilot_long = True
    self.can_map_gears = True
    self.can_use_dsu_bypass = False
    self.can_use_pedal = False
    self.can_use_sdsu = False
    self.car_detected = False
    self.car_fingerprint = ""
    self.has_auto_tune = True
    self.has_bsm = True
    self.has_dash_speed_limits = True
    self.has_lkas_button = True
    self.has_nnff_log = True
    self.has_openpilot_longitudinal = True
    self.has_pcm_cruise = False
    self.has_pedal = False
    self.has_radar = True
    self.has_sdsu = False
    self.has_sng = False
    self.has_starting_state = True
    self.has_zss = False
    self.is_angle_car = False
    self.is_gm = True
    self.is_gm_cc_only = False
    self.is_hkg = True
    self.is_hkg_can_fd = True
    self.is_honda_nidec = False
    self.is_subaru = False
    self.is_torque_car = False
    self.is_toyota = True
    self.is_volt = True
    self.lkas_allowed_for_aol = False
    self.openpilot_longitudinal_control_disabled = False

    self.friction = 0.0
    self.lat_accel_factor = 0.0
    self.longitudinal_actuator_delay = 0.0
    self.start_accel = 0.0
    self.steer_actuator_delay = 0.0
    self.steer_kp = 0.0
    self.steer_ratio = 0.0
    self.stop_accel = 0.0
    self.stopping_decel_rate = 0.0
    self.v_ego_starting = 0.0
    self.v_ego_stopping = 0.0

    car_params = frogpilot_stats.get_car_params(params)
    frogpilot_car_params = frogpilot_stats.get_frogpilot_car_params(params)
    if car_params:
      self.read_car_params(car_params, frogpilot_car_params)

    if frogpilot_car_params:
      self.can_use_pedal = frogpilot_car_params.get("canUsePedal", False)
      self.can_use_sdsu = frogpilot_car_params.get("canUseSDSU", False)
      self.has_dash_speed_limits = frogpilot_car_params.get("hasDashboardSpeedLimit", False)
      self.openpilot_longitudinal_control_disabled = frogpilot_car_params.get("openpilotLongitudinalControlDisabled", False)

    self.cookies = []
    self.notice = None
    self.reasons = {}

  def read_car_params(self, car_params, frogpilot_car_params):
    frogpilot_toggles = self.frogpilot_toggles
    torque = car_params.get("lateralTuning", {}).get("torque")
    safety_configs = car_params.get("safetyConfigs", [])
    car_make = car_params["brand"]

    self.car_detected = True
    self.car_fingerprint = car_params["carFingerprint"]
    self.is_torque_car = torque is not None

    self.can_disable_openpilot_long = frogpilot_toggles.can_disable_openpilot_long
    self.can_map_gears = frogpilot_toggles.can_map_gears
    self.can_use_dsu_bypass = frogpilot_toggles.can_use_dsu_bypass
    self.has_auto_tune = frogpilot_toggles.has_auto_tune
    self.has_bsm = car_params["enableBsm"]
    self.has_dash_speed_limits = False
    self.has_lkas_button = frogpilot_toggles.has_lkas_button
    self.has_nnff_log = frogpilot_toggles.has_nnff
    self.has_openpilot_longitudinal = frogpilot_utilities.has_openpilot_longitudinal(self.params, car_params, frogpilot_car_params)
    self.has_pcm_cruise = car_params["pcmCruise"]
    self.has_pedal = car_params["enableGasInterceptorDEPRECATED"]
    self.has_radar = not car_params["radarUnavailable"]
    self.has_sdsu = frogpilot_toggles.has_sdsu
    self.has_sng = car_params["autoResumeSng"]
    self.has_starting_state = car_params["startingState"]
    self.has_zss = frogpilot_toggles.has_zss
    self.is_angle_car = car_params["steerControlType"] == "angle"
    self.is_gm = car_make == "gm"
    self.is_gm_cc_only = frogpilot_toggles.is_gm_cc_only
    self.is_hkg = car_make == "hyundai"
    self.is_hkg_can_fd = self.is_hkg and len(safety_configs) > 0 and safety_configs[-1]["safetyModel"] == "hyundaiCanfd"
    self.is_honda_nidec = frogpilot_toggles.is_honda_nidec
    self.is_subaru = car_make == "subaru"
    self.is_toyota = car_make == "toyota"
    self.is_volt = car_params["carFingerprint"] == "CHEVROLET_VOLT"
    self.lkas_allowed_for_aol = frogpilot_toggles.lkas_allowed_for_aol

    if self.is_torque_car:
      self.friction = torque["friction"]
      self.lat_accel_factor = torque["latAccelFactor"]
      self.steer_kp = 1.0

    self.longitudinal_actuator_delay = car_params["longitudinalActuatorDelay"]
    self.start_accel = car_params["startAccel"]
    self.steer_actuator_delay = car_params["steerActuatorDelay"]
    self.steer_ratio = car_params["steerRatio"]
    self.stop_accel = car_params["stopAccel"]
    self.stopping_decel_rate = car_params["stoppingDecelRate"]
    self.v_ego_starting = car_params["vEgoStarting"]
    self.v_ego_stopping = car_params["vEgoStopping"]

  def tuning_level(self, key):
    return get_tuning_levels().get(key, 0)

  def branch_reason(self, available):
    if available:
      return None
    return {"kind": "branch"}

  def car_reason(self, available):
    if available:
      return None
    return {"kind": "car"}

  def level_reason(self, key):
    level = self.tuning_level(key)
    if self.level >= level:
      return None
    return {"kind": "level", "level": level}

  def set_reason(self, key):
    if self.params.get(key):
      return None
    return {"key": key, "kind": "set", "title": self.text(TABLE_ROWS[key][0], "title")}

  def setting_reason(self, satisfied, key, wanted=True):
    if satisfied:
      return None
    return {"key": key, "kind": "setting", "title": self.text(TABLE_ROWS[key][0], "title"), "wanted": wanted}

  def panel_reason(self, path):
    reasons = [self.row_reason(row) for row in PANEL_ROWS[path]]
    if None in reasons:
      return None

    levels = [reason["level"] for reason in reasons if reason["kind"] == "level"]
    if levels:
      return {"kind": "level", "level": min(levels)}
    return reasons[0]

  def enabled_reason(self, row):
    if "enabled" in row:
      return row["enabled"](self)
    return None

  def option_reason(self, option):
    if "visible" in option:
      return option["visible"](self)
    return None

  def row_reason(self, row):
    identifier = row_id(row)
    if identifier not in self.reasons:
      if "pair" in row:
        self.reasons[identifier] = self.row_reason(ROW_IDS[row["pair"]])
      elif "visible" in row:
        self.reasons[identifier] = row["visible"](self)
      elif row["widget"] == "manage":
        self.reasons[identifier] = self.panel_reason(row["children"])
      else:
        self.reasons[identifier] = self.level_reason(level_key(row))
    return self.reasons[identifier]

  def lock_reason(self, row):
    reason = self.row_reason(row) or self.enabled_reason(row)
    path = row["path"]
    while reason is None and path in OPENERS:
      opener, option = OPENERS[path]
      if option is not None:
        reason = self.option_reason(option)
      else:
        reason = self.row_reason(opener)
        if reason is None and opener["widget"] == "manage":
          reason = self.setting_reason(self.params.get_bool(opener["key"]), opener["key"])
      path = opener["path"]
    return reason

  def current(self, row):
    if "current" in row:
      return row["current"](self)
    return self.params.get(row["key"])

  def options(self, row):
    return resolve(row.get("options", ()), self)

  def text(self, row, field):
    if self.metric and f"{field}_metric" in row:
      return resolve(row[f"{field}_metric"], self)
    return resolve(row[field], self)

  def number(self, row):
    spec = row["value"]
    if self.metric and spec["metric"] is not None:
      spec = spec["metric"]

    return {**spec, "max": resolve(spec["max"], self), "metric": None, "min": resolve(spec["min"], self)}

  def default(self, row):
    if "default" in row:
      return row["default"](self)

    value = self.params.get_default_value(row["key"])
    if self.metric and row["key"] in frogpilot_variables.METRIC_CONVERSIONS:
      return convert_metric(row["key"], value, True)
    return value


def displayed_value(value, spec):
  factor = 10 ** spec["decimals"]
  return min(max(utilities.round_half_away(value * factor) / factor, spec["min"]), spec["max"])


def is_changed(current, default, spec):
  if current is None or default is None:
    return False
  if spec is not None:
    factor = 10 ** spec["decimals"]
    return utilities.round_half_away(current * factor) != utilities.round_half_away(default * factor)
  return current != default


def on_grid(value, anchor, step):
  steps = (value - anchor) / step
  return abs(steps - round(steps)) < EPSILON


def is_valid_number(value, spec, current, default, integer):
  if isinstance(value, bool) or not isinstance(value, (float, int)) or not math.isfinite(value):
    return False
  if integer and value != int(value):
    return False

  minimum = spec["min"]
  maximum = spec["max"]
  if value < minimum - EPSILON or value > maximum + EPSILON:
    return False
  if any(math.isclose(value, bound, abs_tol=EPSILON) for bound in (minimum, maximum, default)):
    return True

  factor = 10 ** spec["decimals"]
  if abs(value * factor - round(value * factor)) > EPSILON:
    return False

  anchors = (minimum, maximum, default, displayed_value(current, spec))
  return any(on_grid(value, anchor, spec["step"]) for anchor in anchors)


def is_offered(value, options, context):
  return any(type(value) is type(option["value"]) and value == option["value"] and context.option_reason(option) is None for option in options)


def picker_steps(row, context):
  picker = row["picker"]
  if "steps" in picker:
    return picker["steps"]
  return ({"options": context.options(row), "title": picker["title"]},)


def is_valid_sequence(row, picks, context):
  picker = row["picker"]
  if not isinstance(picks, list) or not picks or len(picks) > len(picker["steps"]):
    return False

  for index, pick in enumerate(picks):
    options = [option for option in picker["steps"][index]["options"] if option["value"] not in picks[:index]]
    if [option["value"] for option in options if context.option_reason(option) is None] == [picker["fill"]]:
      return False
    if not is_offered(pick, options, context):
      return False
    if pick in picker["stops"] and index < len(picks) - 1:
      return False
  return True


def is_valid_value(row, value, context):
  if is_sequence(row):
    return is_valid_sequence(row, value, context)
  if row["widget"] in NUMBER_WIDGETS:
    integer = context.params.get_type(row["key"]) == ParamKeyType.INT
    return is_valid_number(value, context.number(row), context.params.get(row["key"]), context.default(row), integer)
  if "options" in row:
    return is_offered(value, context.options(row), context)
  return isinstance(value, bool)


def write_sequence(row, picks, params):
  for index, key in enumerate(row["keys"]):
    if index < len(picks):
      params.put(key, picks[index])
    else:
      params.put(key, row["picker"]["fill"])


def is_sub_toggle_unlocked(owner, context):
  if context.lock_reason(owner) is not None:
    return False
  return owner["widget"] != "button_toggle" or context.params.get_bool(owner["key"])


def serialize_labels(labels):
  return [[value, label] for value, label in labels.items()]


def serialize_options(options, context):
  serialized = []
  for option in options:
    serialized.append({
      "children": option.get("children"),
      "confirm": option.get("confirm"),
      "confirm_button": option.get("confirm_button"),
      "label": option["label"],
      "locked": context.option_reason(option),
      "value": option["value"],
    })
  return serialized


def serialize_picker(row, context):
  picker = row["picker"]
  steps = [{"options": serialize_options(step["options"], context), "title": step["title"]} for step in picker_steps(row, context)]
  return {"fill": picker.get("fill"), "steps": steps, "stops": list(picker.get("stops", ()))}


def serialize_row(row, context, reboot_required):
  key = row["key"]
  writable = is_writable(row)
  data = {
    "children": child_paths(row),
    "description": context.text(row, "description"),
    "hidden": context.row_reason(row),
    "id": row_id(row),
    "key": key,
    "level": context.tuning_level(level_key(row)),
    "locked": context.lock_reason(row),
    "order": row["order"],
    "path": row["path"],
    "reboot": row_id(row) in reboot_required,
    "title": context.text(row, "title"),
    "widget": row["widget"],
    "writable": writable,
  }

  if "actions" in row:
    data["actions"] = []
    for label, action in row["actions"].items():
      blocked = action["guard"](context) if "guard" in action else None
      data["actions"].append({"blocked": blocked, "confirm": action.get("confirm"), "label": label, "messages": action.get("messages", {}),
                              "restart": action.get("restart", False), "status": action.get("status")})

  if "buttons" in row:
    data["buttons"] = row["buttons"]

  if "checkable" in row:
    data["checkable"] = row["checkable"]

  if "confirm_enable" in row:
    data["confirm_enable"] = row["confirm_enable"]

  if "display" in row:
    data["display"] = row["display"](context)

  if "enabled" in row:
    data["disabled"] = context.enabled_reason(row)

  if "exclusive" in row:
    data["exclusive"] = row["exclusive"]

  if "keys" in row:
    data["keys"] = row["keys"]

  if "link" in row:
    data["link"] = row["link"]

  if row["widget"] == "manage":
    data["manage_visible"] = context.panel_reason(row["children"]) is None

  if "note" in row:
    data["note"] = row["note"]

  if "options" in row:
    data["options"] = serialize_options(context.options(row), context)

  if "pair" in row:
    data["pair"] = row["pair"]

  if "picker" in row:
    data["picker"] = serialize_picker(row, context)

  if "reset_confirm" in row:
    data["reset_confirm"] = row["reset_confirm"]

  if "warning" in row:
    data["warning"] = row["warning"]

  spec = None
  if row["widget"] in NUMBER_WIDGETS:
    spec = context.number(row)
    data["number"] = {**spec, "labels": serialize_labels(spec["labels"])}

  current = None
  if key is not None or "current" in row:
    current = context.current(row)
    if key not in frogpilot_variables.SECRET_KEYS:
      data["current"] = current

  if key is not None:
    default = context.default(row)
    data["changed"] = writable and is_valid_value(row, default, context) and is_changed(current, default, spec)
    data["default"] = default

  if "sub_toggles" in row:
    data["sub_toggles"] = []
    for sub_toggle in row["sub_toggles"]:
      current = context.params.get(sub_toggle["key"])
      default = context.params.get_default_value(sub_toggle["key"])
      data["sub_toggles"].append({
        "changed": current != default,
        "current": current,
        "default": default,
        "key": sub_toggle["key"],
        "label": sub_toggle["label"],
      })
  return data


def serialize_state(row, context, reboot_required):
  data = serialize_row(row, context, reboot_required)
  return {field: data[field] for field in DYNAMIC_FIELDS[row_id(row)] if field in data}


def page_state(context):
  confirmed = context.params.get_bool("TuningLevelConfirmed")
  first_open = None
  if not confirmed:
    level, message = frogpilot_settings.first_open(context.params)
    first_open = {"level": level, "message": message}

  return {
    "confirmed": confirmed,
    "first_open": first_open,
    "level": context.level,
    "metric": context.metric,
    "started": context.started,
  }


def get_payload(the_pond):
  context = Context(the_pond)
  return {**page_state(context), "rows": [serialize_row(row, context, the_pond.reboot_required) for row in ROWS]}


def get_state(the_pond):
  context = Context(the_pond)
  return {**page_state(context), "state": {row_id(row): serialize_state(row, context, the_pond.reboot_required) for row in ROWS}}


@routes.get("/api/toggles")
async def get_toggles(request):
  return web.json_response(get_payload(request.app[utilities.THE_POND]))


@routes.get("/api/toggles/state")
async def get_toggles_state(request):
  return web.json_response(get_state(request.app[utilities.THE_POND]))


@routes.post("/api/toggles/metric")
async def set_metric(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params

  metric = (await utilities.read_json(request)).get("metric")
  if not isinstance(metric, bool):
    raise web.HTTPBadRequest(text="invalid_value")

  if metric != params.get_bool("IsMetric"):
    params.put_bool("IsMetric", metric)
    for key in frogpilot_variables.METRIC_CONVERSIONS:
      write_param(params, key, convert_metric(key, params.get(key), metric))
    the_pond.send_request("updateToggles")
  return web.json_response(get_state(the_pond))


@routes.post("/api/toggles/tuning_level")
async def confirm_tuning_level(request):
  the_pond = request.app[utilities.THE_POND]
  params = the_pond.params
  if params.get_bool("TuningLevelConfirmed"):
    raise web.HTTPConflict(text="already_confirmed")

  level, _ = frogpilot_settings.first_open(params)
  params.put_bool("TuningLevelConfirmed", True)
  params.put("TuningLevel", level)

  the_pond.send_request("updateToggles")
  return web.json_response(get_state(the_pond))


@routes.post("/api/toggles/{id}/actions/{label}")
async def run_action(request):
  the_pond = request.app[utilities.THE_POND]
  row = ROW_IDS.get(request.match_info["id"])
  label = request.match_info["label"]
  if row is None or label not in row.get("actions", {}):
    raise web.HTTPNotFound(text="unknown_action")

  body = await utilities.read_json(request)
  context = Context(the_pond, request)
  if context.lock_reason(row) is not None:
    raise web.HTTPConflict(text="hidden")

  action = row["actions"][label]
  if "guard" in action:
    error = action["guard"](context)
    if error is not None:
      raise web.HTTPConflict(text=error)

  succeeded = await action["run"](context, body)
  the_pond.send_request("updateToggles")
  return web.json_response({**get_state(the_pond), "succeeded": succeeded})


@routes.put("/api/toggles/{key}")
async def set_toggle(request):
  the_pond = request.app[utilities.THE_POND]
  key = request.match_info["key"]
  value = (await utilities.read_json(request)).get("value")

  rows = WRITABLE_ROWS.get(key, [])
  owners = SUB_TOGGLE_ROWS.get(key, [])
  if not rows and not owners:
    raise web.HTTPNotFound(text="unknown_setting")

  context = Context(the_pond, request)
  row = next((row for row in rows if context.lock_reason(row) is None), None)
  owner = next((owner for owner in owners if is_sub_toggle_unlocked(owner, context)), None)
  if row is None and owner is None:
    raise web.HTTPConflict(text="hidden")

  if row is not None and not is_valid_value(row, value, context):
    raise web.HTTPBadRequest(text="invalid_value")
  if row is None and not isinstance(value, bool):
    raise web.HTTPBadRequest(text="invalid_value")
  if row is None and owner.get("exclusive") and not value:
    raise web.HTTPBadRequest(text="invalid_value")

  if row is not None:
    old_value = context.current(row)
  else:
    old_value = context.params.get(key)

  if row is not None and "write" in row:
    row["write"](context, value)
  elif row is not None and is_sequence(row):
    write_sequence(row, value, context.params)
  else:
    write_param(context.params, key, value)

  if row is not None:
    changed_row = row
    on_change = row.get("on_change")
  else:
    changed_row = owner
    sub_toggle = next(sub_toggle for sub_toggle in owner["sub_toggles"] if sub_toggle["key"] == key)
    on_change = sub_toggle.get("on_change")
    if owner.get("exclusive"):
      for other in owner["sub_toggles"]:
        if other is not sub_toggle:
          context.params.put_bool(other["key"], False)

  reboot = on_change is not None and value != old_value and on_change(context, old_value, value)
  if reboot:
    the_pond.reboot_required.add(row_id(changed_row))

  the_pond.send_request("updateToggles")
  response = web.json_response({**get_state(the_pond), "notice": context.notice, "reboot": reboot})
  for name, cookie, options in context.cookies:
    response.set_cookie(name, cookie, **options)
  return response
