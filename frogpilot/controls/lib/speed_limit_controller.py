#!/usr/bin/env python3
# PFEIFER - SLC - Modified by FrogAi for FrogPilot
import requests

from concurrent.futures import ThreadPoolExecutor

from openpilot.common.constants import CV
from openpilot.common.realtime import DT_MDL
from openpilot.common.swaglog import cloudlog

MAPBOX_MONTHLY_REQUEST_LIMIT = 100_000
MAPBOX_REQUEST_INTERVAL = 5
MAPBOX_RETRY_DELAY = 10
MAP_MATCH_MAX_AGE_NS = 2_000_000_000

SPEED_LIMIT_CONFIRMATION_TIMEOUT = 30

OFFSET_MAP_IMPERIAL = [
  (25, "speed_limit_offset1"),   # 0-24 mph
  (35, "speed_limit_offset2"),   # 25-34
  (45, "speed_limit_offset3"),   # 35-44
  (55, "speed_limit_offset4"),   # 45-54
  (65, "speed_limit_offset5"),   # 55-64
  (75, "speed_limit_offset6"),   # 65-74
  (100, "speed_limit_offset7"),  # 75-99
]

OFFSET_MAP_METRIC = [
  (30, "speed_limit_offset1"),   # 0-29 km/h
  (50, "speed_limit_offset2"),   # 30-49
  (60, "speed_limit_offset3"),   # 50-59
  (80, "speed_limit_offset4"),   # 60-79
  (100, "speed_limit_offset5"),  # 80-99
  (120, "speed_limit_offset6"),  # 100-119
  (141, "speed_limit_offset7"),  # 120-140
]

class SpeedLimitController:
  def __init__(self, FrogPilotVCruise):
    self.frogpilot_planner = FrogPilotVCruise.frogpilot_planner

    self.mapbox_is_forward = False
    self.mapbox_request_is_forward = False

    self.denied_target = 0
    self.mapbox_next_retry = 0
    self.mapbox_request_way_id = 0
    self.mapbox_speed_limit = 0
    self.mapbox_way_id = 0
    self.map_speed_limit = 0
    self.next_speed_limit = 0
    self.overridden_speed = 0
    self.speed_limit_changed_timer = 0
    self.target = 0
    self.unconfirmed_speed_limit = 0

    self.mapbox_future = None
    self.mapbox_position = None

    self.previous_source = "None"
    self.source = "None"

    self.mapbox_access_token = self.frogpilot_planner.params.get("MapboxPublicKey")
    self.mapbox_requests = self.frogpilot_planner.params.get("MapBoxRequests") or {}

    self.speed_limits = {
      (limit["segment_id"], limit["is_forward"]): limit["speed_limit"]
      for limit in (self.frogpilot_planner.params.get("SpeedLimits") or []) if isinstance(limit, dict) and set(limit) == {"is_forward", "segment_id", "source", "speed_limit", "tile"}
    }

    self.mapbox_executor = ThreadPoolExecutor(max_workers=1)
    self.mapbox_session = requests.Session()

    self.previous_target = self.frogpilot_planner.params.get("PreviousSpeedLimit")

  def close(self):
    self.mapbox_executor.shutdown()
    self.mapbox_session.close()

  def invalidate_mapbox(self):
    self.mapbox_is_forward = False

    self.mapbox_speed_limit = 0
    self.mapbox_way_id = 0

    self.mapbox_position = None

  def reset(self):
    self.denied_target = 0
    self.map_speed_limit = 0
    self.next_speed_limit = 0
    self.overridden_speed = 0
    self.speed_limit_changed_timer = 0
    self.target = 0
    self.unconfirmed_speed_limit = 0

    self.source = "None"

    self.invalidate_mapbox()

  @property
  def experimental_mode(self):
    return self.target == 0 and self.frogpilot_toggles.slc_fallback_experimental_mode

  @property
  def offset(self):
    if self.frogpilot_toggles.is_metric:
      offset_map = OFFSET_MAP_METRIC
      displayed_speed_limit = round(self.target * CV.MS_TO_KPH)
    else:
      offset_map = OFFSET_MAP_IMPERIAL
      displayed_speed_limit = round(self.target * CV.MS_TO_MPH)

    return next((getattr(self.frogpilot_toggles, offset) for upper_bound, offset in offset_map if 0 < displayed_speed_limit < upper_bound), 0)

  def handle_limit_change(self, desired_source, desired_target, sm):
    if desired_source == "None" or self.target == 0:
      confirmation_required = False
    elif desired_target < self.target:
      confirmation_required = self.frogpilot_toggles.speed_limit_confirmation_lower
    else:
      confirmation_required = self.frogpilot_toggles.speed_limit_confirmation_higher

    speed_limit_accepted = self.frogpilot_planner.params_memory.get_bool("SpeedLimitAccepted")
    if speed_limit_accepted:
      self.frogpilot_planner.params_memory.remove("SpeedLimitAccepted")
    speed_limit_accepted |= sm["frogpilotCarState"].accelPressed and sm["carControl"].longActive

    if not confirmation_required:
      self.denied_target = 0

      self.source = desired_source
      self.target = desired_target

      if desired_source != "None":
        self.speed_limit_changed_timer = DT_MDL
      else:
        self.speed_limit_changed_timer = 0

      self.unconfirmed_speed_limit = 0
      return

    if abs(desired_target - self.unconfirmed_speed_limit) >= 1:
      self.source = "None"

      self.speed_limit_changed_timer = DT_MDL

      self.unconfirmed_speed_limit = desired_target
      return

    self.speed_limit_changed_timer += DT_MDL

    if speed_limit_accepted:
      self.denied_target = 0
      self.overridden_speed = 0

      self.source = desired_source
      self.target = desired_target

      self.speed_limit_changed_timer = 0
      self.unconfirmed_speed_limit = 0

    elif sm["frogpilotCarState"].decelPressed or self.speed_limit_changed_timer >= SPEED_LIMIT_CONFIRMATION_TIMEOUT:
      self.denied_target = desired_target

      self.speed_limit_changed_timer = 0
      self.unconfirmed_speed_limit = 0

  def update_limits(self, gps_position, now, time_validated, v_ego, sm):
    if not self.frogpilot_toggles.speed_limit_controller:
      self.overridden_speed = 0

    map_match = {}
    if gps_position:
      map_match = self.frogpilot_planner.params_memory.get("MapMatchedWay") or {}
      map_match_age = gps_position["location_mono_time"] - map_match.get("location_mono_time", 0)

      if not map_match.get("way_id") or not isinstance(map_match.get("is_forward"), bool) or not 0 <= map_match_age <= MAP_MATCH_MAX_AGE_NS:
        map_match = {}

    self.update_map_speed_limit(map_match, v_ego)

    limits = {
      "Dashboard": sm["frogpilotCarState"].dashboardSpeedLimit,
      "Map Data": self.map_speed_limit,
    }
    limits = {source: limit for source, limit in limits.items() if limit >= 1}

    if self.frogpilot_toggles.speed_limit_priority_highest:
      desired_source = max(limits, key=limits.get, default="None")
    elif self.frogpilot_toggles.speed_limit_priority_lowest:
      desired_source = min(limits, key=limits.get, default="None")
    else:
      priorities = (
        self.frogpilot_toggles.speed_limit_priority1,
        self.frogpilot_toggles.speed_limit_priority2,
      )
      desired_source = next((source for source in priorities if source in limits), "None")

    desired_target = limits.get(desired_source, 0)

    self.update_mapbox_speed_limit(gps_position, map_match, now, time_validated, v_ego, desired_target)

    if desired_target == 0:
      previous_limit_available = self.previous_target > 0 and self.denied_target != self.previous_target

      if self.mapbox_speed_limit >= 1:
        desired_source, desired_target = "Mapbox", self.mapbox_speed_limit
      elif self.frogpilot_toggles.slc_fallback_previous_speed_limit and previous_limit_available:
        desired_source, desired_target = self.previous_source, self.previous_target

        if self.unconfirmed_speed_limit or self.denied_target:
          self.source = desired_source
          self.target = desired_target

          self.speed_limit_changed_timer = 0
          return

    if desired_target == 0:
      self.target = 0

      self.speed_limit_changed_timer = 0

      self.source = "None"
      return

    if self.unconfirmed_speed_limit or self.denied_target:
      if self.target == 0:
        self.target = self.previous_target
      self.source = "None"

    if abs(desired_target - self.target) < 1:
      self.denied_target = 0
      self.unconfirmed_speed_limit = 0

      self.speed_limit_changed_timer = 0

      self.source = desired_source
      self.target = desired_target
    elif abs(desired_target - self.denied_target) < 1:
      self.unconfirmed_speed_limit = 0

      self.speed_limit_changed_timer = 0
      return
    else:
      self.handle_limit_change(desired_source, desired_target, sm)

    if self.source != "None" and self.target > 0:
      self.previous_source = self.source

      if self.target != self.previous_target:
        self.previous_target = self.target

        self.frogpilot_planner.params.put_nonblocking("PreviousSpeedLimit", self.target)

  def update_mapbox_speed_limit(self, gps_position, map_match, now, time_validated, v_ego, desired_target):
    mapbox_enabled = self.frogpilot_toggles.slc_mapbox_filler or self.frogpilot_toggles.speed_limit_filler
    if not mapbox_enabled or not self.mapbox_access_token or not gps_position or not time_validated:
      self.invalidate_mapbox()
      return

    if self.mapbox_future is not None and self.mapbox_future.done():
      future = self.mapbox_future
      self.mapbox_future = None

      try:
        with future.result() as response:
          if response.status_code in (401, 403):
            self.mapbox_access_token = None
          elif response.status_code == 429:
            self.mapbox_next_retry = int(response.headers.get("X-Rate-Limit-Reset", 0))

          response.raise_for_status()
          if self.mapbox_position is not None:
            payload = response.json()
            tracepoint = payload["tracepoints"][-1]
            matching = payload["matchings"][tracepoint["matchings_index"]]
            maxspeed = matching["legs"][tracepoint["waypoint_index"] - 1]["annotation"]["maxspeed"][-1]

            if maxspeed.get("none") or maxspeed.get("unknown"):
              self.mapbox_speed_limit = 0
            elif maxspeed.get("unit") == "km/h":
              self.mapbox_speed_limit = maxspeed["speed"] * CV.KPH_TO_MS
            elif maxspeed.get("unit") == "mph":
              self.mapbox_speed_limit = maxspeed["speed"] * CV.MPH_TO_MS
            else:
              self.mapbox_speed_limit = 0

            self.mapbox_is_forward = self.mapbox_request_is_forward

            self.mapbox_way_id = self.mapbox_request_way_id
      except (AttributeError, KeyError, IndexError, TypeError, ValueError, requests.RequestException):
        self.mapbox_is_forward = False

        self.mapbox_speed_limit = 0
        self.mapbox_way_id = 0

        self.mapbox_next_retry = max(self.mapbox_next_retry, now.timestamp() + MAPBOX_RETRY_DELAY)

      if not self.mapbox_access_token:
        cloudlog.warning("Mapbox request rejected: authorization failed")
        self.mapbox_position = None
        return

    is_forward = map_match.get("is_forward", False)
    way_id = map_match.get("way_id", 0)

    if not way_id and not (self.frogpilot_toggles.slc_mapbox_filler and desired_target == 0):
      self.invalidate_mapbox()
      return

    position = (gps_position["longitude"], gps_position["latitude"], int(now.timestamp()))

    if way_id and way_id == self.mapbox_way_id and is_forward == self.mapbox_is_forward:
      if position[2] - self.mapbox_position[2] >= MAPBOX_REQUEST_INTERVAL:
        self.mapbox_position = position
      return

    if desired_target != 0 and not self.frogpilot_toggles.speed_limit_filler:
      self.invalidate_mapbox()
      return

    if way_id or self.mapbox_way_id:
      if self.mapbox_way_id and position[2] - self.mapbox_position[2] > MAPBOX_REQUEST_INTERVAL:
        self.mapbox_position = None

      self.mapbox_is_forward = False

      self.mapbox_speed_limit = 0
      self.mapbox_way_id = 0

    if self.mapbox_future is not None:
      return

    if v_ego < 1 or now.timestamp() < self.mapbox_next_retry:
      self.invalidate_mapbox()
      return

    if self.mapbox_position is None:
      self.mapbox_position = position
      return

    if position[2] - self.mapbox_position[2] < MAPBOX_REQUEST_INTERVAL:
      return

    month = now.month
    year = now.year

    if self.mapbox_requests.get("month") == month and self.mapbox_requests.get("year", year) == year:
      request_count = self.mapbox_requests.get("total_requests", 0)
    else:
      request_count = 0

    if request_count >= MAPBOX_MONTHLY_REQUEST_LIMIT:
      if "year" not in self.mapbox_requests:
        self.mapbox_requests["year"] = year
        self.frogpilot_planner.params.put_nonblocking("MapBoxRequests", self.mapbox_requests)

      self.invalidate_mapbox()
      return

    trace = (self.mapbox_position, position)

    self.mapbox_future = self.mapbox_executor.submit(
      self.mapbox_session.get,
      f"https://api.mapbox.com/matching/v5/mapbox/driving/{';'.join(f'{longitude:.6f},{latitude:.6f}' for longitude, latitude, _ in trace)}.json",
      params={
        "access_token": self.mapbox_access_token,
        "annotations": "maxspeed",
        "overview": "full",
        "radiuses": "10;10",
        "timestamps": ";".join(str(timestamp) for _, _, timestamp in trace),
      },
      timeout=10,
    )
    self.mapbox_request_is_forward = is_forward

    self.mapbox_position = position
    self.mapbox_request_way_id = way_id
    self.mapbox_requests = {"month": month, "total_requests": request_count + 1, "year": year}

    self.frogpilot_planner.params.put_nonblocking("MapBoxRequests", self.mapbox_requests)

  def update_map_speed_limit(self, map_match, v_ego):
    if not map_match:
      self.map_speed_limit = 0
      self.next_speed_limit = 0
      return

    self.map_speed_limit = self.speed_limits.get((map_match["way_id"], map_match["is_forward"]), map_match["speed_limit"])

    self.next_speed_limit = 0

    for upcoming_speed_limit in map_match["next_speed_limits"]:
      speed_limit = self.speed_limits.get((upcoming_speed_limit["way_id"], upcoming_speed_limit["is_forward"]), upcoming_speed_limit["speed_limit"])

      if speed_limit <= 0 or speed_limit == self.map_speed_limit:
        continue

      self.next_speed_limit = speed_limit

      if self.next_speed_limit > self.map_speed_limit:
        lookahead = self.frogpilot_toggles.map_speed_lookahead_higher
      else:
        lookahead = self.frogpilot_toggles.map_speed_lookahead_lower

      if upcoming_speed_limit["distance"] < lookahead * v_ego:
        self.map_speed_limit = self.next_speed_limit
      break

  def update_override(self, v_cruise_cluster, v_ego_cluster, sm):
    speed_limit = self.target + self.offset
    gas_override = sm["carState"].gasPressed and v_ego_cluster > speed_limit

    if not sm["selfdriveState"].enabled or speed_limit <= 0 or (not gas_override and self.overridden_speed <= speed_limit):
      self.overridden_speed = 0
      return

    if self.frogpilot_toggles.speed_limit_controller_override_manual:
      if gas_override:
        self.overridden_speed = max(self.overridden_speed, v_ego_cluster)
      self.overridden_speed = min(self.overridden_speed, v_cruise_cluster)
    elif self.frogpilot_toggles.speed_limit_controller_override_set_speed:
      self.overridden_speed = v_cruise_cluster
    else:
      self.overridden_speed = 0

    self.source = "None"
