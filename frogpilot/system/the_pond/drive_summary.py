#!/usr/bin/env python3
import capnp
import json
import sys
import zstandard

from cereal import log
from openpilot.system.loggerd.config import SEGMENT_LENGTH

from openpilot.frogpilot.common import frogpilot_variables

CE_STATUS_REASONS = {
  frogpilot_variables.CE_STATUS["CURVATURE"]: "curve",
  frogpilot_variables.CE_STATUS["LEAD"]: "lead",
  frogpilot_variables.CE_STATUS["SIGNAL"]: "signal",
  frogpilot_variables.CE_STATUS["SPEED"]: "speed",
  frogpilot_variables.CE_STATUS["SPEED_LIMIT"]: "speed_limit",
  frogpilot_variables.CE_STATUS["STOP_LIGHT"]: "stop_light",
  frogpilot_variables.CE_STATUS["USER_DISABLED"]: "user_disabled",
  frogpilot_variables.CE_STATUS["USER_OVERRIDDEN"]: "user_overridden",
}
EVENT_GAP = 10.0
HARD_BRAKE_ACCEL = -3.0
MAX_GAP = 1.0
MAX_PATH_POINTS = 1000
ROAD_SETTLE_TIME = 5.0
SETTLE_TIME = 1.0

class SettledValue:
  def __init__(self, hold, ignored=None):
    self.hold = hold
    self.ignored = ignored

    self.candidate = None
    self.current = None

    self.since = 0.0

  def update(self, value, time):
    if value != self.candidate:
      self.candidate = value
      self.since = time
      return False

    if value in (self.current, self.ignored) or time - self.since < self.hold:
      return False

    self.current = value
    return True


def get_drive_time(position, qlog_start, log_mono_time):
  return round(position * SEGMENT_LENGTH + (log_mono_time - qlog_start) * 1e-9, 1)


def get_model_name(params):
  model_id = params.get("RandomizedModel" if params.get("ModelRandomizer") == b"1" else "DrivingModel", b"").decode()
  if not model_id:
    return frogpilot_variables.DEFAULT_MODEL["name"]

  names = {model["id"]: model["name"] for model in [frogpilot_variables.DEFAULT_MODEL, *frogpilot_variables.get_models()]}
  return names.get(model_id) or params.get("DrivingModelName", b"").decode() or model_id


def add_event(events, last_seen, kind, time, reason=None):
  previous = last_seen.get((kind, reason))
  last_seen[(kind, reason)] = time
  if previous is None or time - previous >= EVENT_GAP:
    events.append({"kind": kind, "reason": reason, "time": time})


def read_events(qlog):
  with open(qlog, "rb") as file:
    data = zstandard.ZstdDecompressor().stream_reader(file).read()

  try:
    yield from log.Event.read_multiple_bytes(data)
  except capnp.KjException:
    return


def summarize(qlogs):
  distance = 0.0
  engaged_time = 0.0

  enabled = False
  previous_time = None
  v_ego = 0.0

  alert = ""
  disable_reason = None
  disengagement = None
  engaged = []
  events = []
  last_seen = {}

  ce_status = 0
  csc_controlling = False
  lane_changing = False
  standstill = False

  gear = SettledValue(SETTLE_TIME, "unknown")
  personality = SettledValue(SETTLE_TIME)
  road = SettledValue(ROAD_SETTLE_TIME, "")
  speed_limit = SettledValue(ROAD_SETTLE_TIME, 0)

  gps_service = None
  model = None
  path = []

  for position, qlog in qlogs:
    qlog_start = None
    for msg in read_events(qlog):
      which = msg.which()
      if qlog_start is None and which != "initData":
        qlog_start = msg.logMonoTime

      if which == "initData":
        if model is None:
          model = get_model_name({entry.key: entry.value for entry in msg.initData.params.entries})
      elif which == "carState":
        car_state = msg.carState
        if previous_time is not None:
          dt = (msg.logMonoTime - previous_time) * 1e-9
          if dt < MAX_GAP:
            distance += v_ego * dt
            if enabled:
              engaged_time += dt

        previous_time = msg.logMonoTime
        v_ego = car_state.vEgo

        time = get_drive_time(position, qlog_start, msg.logMonoTime)
        if car_state.standstill and enabled and not standstill:
          add_event(events, last_seen, "stop", time)
        standstill = car_state.standstill

        if car_state.aEgo < HARD_BRAKE_ACCEL:
          add_event(events, last_seen, "hard_brake", time)

        previous_gear = gear.current
        if gear.update(str(car_state.gearShifter), time) and previous_gear is not None:
          events.append({"kind": "gear", "reason": gear.current, "time": gear.since})
      elif which == "selfdriveState":
        time = get_drive_time(position, qlog_start, msg.logMonoTime)
        was_enabled = enabled
        enabled = msg.selfdriveState.enabled

        disengagement = None
        if enabled or was_enabled:
          if not was_enabled:
            disable_reason = None
            engaged.append([time, time])
          engaged[-1][1] = time

          if not enabled:
            disengagement = {"kind": "disengaged", "reason": disable_reason, "time": time}
            events.append(disengagement)

        warning = ""
        if msg.selfdriveState.alertStatus != log.SelfdriveState.AlertStatus.normal:
          warning = f"{msg.selfdriveState.alertText1} {msg.selfdriveState.alertText2}".strip()
        if warning and warning != alert:
          add_event(events, last_seen, "alert", time, warning)
        alert = warning

        previous_personality = personality.current
        if personality.update(str(msg.selfdriveState.personality), time) and previous_personality is not None:
          events.append({"kind": "personality", "reason": personality.current, "time": personality.since})
      elif which == "onroadEvents":
        reasons = [str(event.name) for event in msg.onroadEvents if event.userDisable or event.immediateDisable or event.softDisable]
        if reasons:
          disable_reason = reasons[0]
          if disengagement is not None and disengagement["reason"] is None:
            disengagement["reason"] = disable_reason
      elif which == "userBookmark":
        events.append({"kind": "bookmark", "time": get_drive_time(position, qlog_start, msg.logMonoTime)})
      elif which == "frogpilotPlan":
        frogpilot_plan = msg.frogpilotPlan
        time = get_drive_time(position, qlog_start, msg.logMonoTime)
        if frogpilot_plan.ceStatus != ce_status and frogpilot_plan.ceStatus in CE_STATUS_REASONS:
          add_event(events, last_seen, "experimental", time, CE_STATUS_REASONS[frogpilot_plan.ceStatus])
        ce_status = frogpilot_plan.ceStatus

        if frogpilot_plan.cscControllingSpeed and not csc_controlling:
          add_event(events, last_seen, "curve", time)
        csc_controlling = frogpilot_plan.cscControllingSpeed

        if speed_limit.update(round(frogpilot_plan.slcSpeedLimit, 2), time):
          events.append({"kind": "speed_limit", "reason": speed_limit.current, "time": speed_limit.since})
      elif which == "mapdOut":
        if road.update(msg.mapdOut.roadName, get_drive_time(position, qlog_start, msg.logMonoTime)):
          events.append({"kind": "road", "reason": road.current, "time": road.since})
      elif which == "drivingModelData":
        meta = msg.drivingModelData.meta
        starting = meta.laneChangeState == "laneChangeStarting"
        if starting and not lane_changing:
          add_event(events, last_seen, "lane_change", get_drive_time(position, qlog_start, msg.logMonoTime), str(meta.laneChangeDirection))
        lane_changing = starting
      elif which in ("gpsLocation", "gpsLocationExternal"):
        if gps_service is None:
          gps_service = which

        gps = getattr(msg, which)
        if which == gps_service and gps.hasFix:
          path.append([round(gps.latitude, 6), round(gps.longitude, 6)])

  return {
    "distance": round(distance),
    "engaged": engaged,
    "engaged_time": round(engaged_time),
    "events": sorted(events, key=lambda event: event["time"]),
    "model": model,
    "path": path[::len(path) // MAX_PATH_POINTS + 1],
  }


if __name__ == "__main__":
  print(json.dumps(summarize(json.loads(sys.argv[1]))))
