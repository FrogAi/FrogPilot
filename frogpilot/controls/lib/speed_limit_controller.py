#!/usr/bin/env python3
# PFEIFER - SLC - Modified by FrogAi for FrogPilot
import requests

from concurrent.futures import ThreadPoolExecutor

from cereal import custom

from openpilot.common.constants import CV
from openpilot.common.realtime import DT_MDL

from openpilot.frogpilot.common.frogpilot_utilities import is_mapd_data_valid

MAPBOX_MONTHLY_REQUEST_LIMIT = 100_000

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

    self.denied_target = 0
    self.mapbox_requested_way_id = 0
    self.mapbox_speed_limit = 0
    self.mapbox_way_id = 0
    self.map_speed_limit = 0
    self.next_speed_limit = 0
    self.overridden_speed = 0
    self.speed_limit_changed_timer = 0
    self.target = 0
    self.unconfirmed_speed_limit = 0

    self.mapbox_positions = []

    self.mapbox_future = None

    self.previous_source = "None"
    self.source = "None"

    self.mapbox_access_token = self.frogpilot_planner.params.get("MapboxPublicKey")
    self.mapbox_requests = self.frogpilot_planner.params.get("MapBoxRequests")
    self.previous_target = self.frogpilot_planner.params.get("PreviousSpeedLimit")

    self.mapbox_executor = ThreadPoolExecutor(max_workers=1)
    self.mapbox_session = requests.Session()

  def close(self):
    self.mapbox_executor.shutdown()
    self.mapbox_session.close()

  def reset(self):
    self.denied_target = 0
    self.mapbox_requested_way_id = 0
    self.mapbox_speed_limit = 0
    self.mapbox_way_id = 0
    self.map_speed_limit = 0
    self.next_speed_limit = 0
    self.overridden_speed = 0
    self.speed_limit_changed_timer = 0
    self.target = 0
    self.unconfirmed_speed_limit = 0

    self.mapbox_positions = []

    self.mapbox_future = None

    self.source = "None"

  def request_mapbox_speed_limit(self, coordinates, way_id):
    response = self.mapbox_session.get(
      f"https://api.mapbox.com/matching/v5/mapbox/driving/{coordinates}.json",
      params={"access_token": self.mapbox_access_token, "annotations": "maxspeed", "overview": "full"},
      timeout=10,
    )
    if response.status_code in (401, 403):
      self.mapbox_access_token = None
    response.raise_for_status()

    matchings = response.json()["matchings"]
    if not matchings:
      return 0, way_id

    maxspeed = matchings[-1]["legs"][-1]["annotation"]["maxspeed"][-1]
    if maxspeed.get("unit") == "km/h":
      return maxspeed["speed"] * CV.KPH_TO_MS, way_id
    if maxspeed.get("unit") == "mph":
      return maxspeed["speed"] * CV.MPH_TO_MS, way_id
    return 0, way_id

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

  def update_limits(self, now, time_validated, v_ego, sm):
    if not self.frogpilot_toggles.speed_limit_controller:
      self.overridden_speed = 0

    self.update_mapbox_speed_limit(now, time_validated, sm)
    self.update_map_speed_limit(v_ego, sm)

    limits = {
      source: limit for source, limit in {
        "Dashboard": sm["frogpilotCarState"].dashboardSpeedLimit,
        "Map Data": self.map_speed_limit,
      }.items() if limit >= 1
    }

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

    if desired_target == 0:
      if self.mapbox_speed_limit >= 1:
        desired_source, desired_target = "Mapbox", self.mapbox_speed_limit
      elif self.frogpilot_toggles.slc_fallback_previous_speed_limit and self.previous_target > 0 and self.denied_target != self.previous_target:
        desired_source, desired_target = self.previous_source, self.previous_target

    if desired_target == 0:
      self.denied_target = 0
      self.target = 0
      self.unconfirmed_speed_limit = 0

      self.speed_limit_changed_timer = 0

      self.source = "None"
      return

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

  def update_map_speed_limit(self, v_ego, sm):
    if not is_mapd_data_valid(sm["mapdOut"], self.frogpilot_planner.gps_valid, sm):
      self.map_speed_limit = 0
      self.next_speed_limit = 0
      return

    self.map_speed_limit = sm["mapdOut"].speedLimit
    self.next_speed_limit = sm["mapdOut"].nextSpeedLimit

    if self.next_speed_limit <= 0 or self.next_speed_limit == self.map_speed_limit:
      return

    if self.next_speed_limit > self.map_speed_limit:
      lookahead = self.frogpilot_toggles.map_speed_lookahead_higher
    else:
      lookahead = self.frogpilot_toggles.map_speed_lookahead_lower

    if sm["mapdOut"].nextSpeedLimitDistance < lookahead * v_ego:
      self.map_speed_limit = self.next_speed_limit

  def update_mapbox_speed_limit(self, now, time_validated, sm):
    mapbox_enabled = self.frogpilot_toggles.slc_mapbox_filler or self.frogpilot_toggles.speed_limit_filler
    if not (mapbox_enabled and self.mapbox_access_token and time_validated and is_mapd_data_valid(sm["mapdOut"], self.frogpilot_planner.gps_valid, sm)):
      self.mapbox_requested_way_id = 0
      self.mapbox_speed_limit = 0
      self.mapbox_way_id = 0

      self.mapbox_positions = []

      self.mapbox_future = None
      return

    gps_log_mono_time = sm.logMonoTime[self.frogpilot_planner.gps_location_service]
    gps_position = (self.frogpilot_planner.gps_position["longitude"], self.frogpilot_planner.gps_position["latitude"])

    if not self.mapbox_positions or gps_log_mono_time != self.mapbox_positions[-1][1]:
      self.mapbox_positions.append((gps_position, gps_log_mono_time))
      self.mapbox_positions = self.mapbox_positions[-2:]

    if sm["mapdOut"].waySelectionType == custom.WaySelectionType.current and sm["mapdOut"].wayId != self.mapbox_way_id:
      self.mapbox_speed_limit = 0
      self.mapbox_way_id = 0

    if self.mapbox_future is not None:
      if not self.mapbox_future.done():
        return

      if sm["mapdOut"].wayId != self.mapbox_requested_way_id:
        self.mapbox_future = None
      elif sm["mapdOut"].waySelectionType != custom.WaySelectionType.current:
        return

    if self.mapbox_future is not None:
      try:
        self.mapbox_speed_limit, self.mapbox_way_id = self.mapbox_future.result()
      except (AttributeError, KeyError, IndexError, TypeError, ValueError, requests.RequestException) as error:
        self.mapbox_speed_limit, self.mapbox_way_id = 0, 0
        if isinstance(error, requests.RequestException) and (not isinstance(error, requests.HTTPError) or error.response.status_code >= 500):
          self.mapbox_requested_way_id = 0
          self.mapbox_positions = []

      self.mapbox_future = None

    if sm["mapdOut"].waySelectionType == custom.WaySelectionType.current:
      if sm["mapdOut"].wayId in (self.mapbox_way_id, self.mapbox_requested_way_id):
        return
    elif sm["mapdOut"].waySelectionType != custom.WaySelectionType.predicted or sm["mapdOut"].wayId == self.mapbox_requested_way_id:
      return

    if len(self.mapbox_positions) < 2:
      return

    if self.mapbox_requests.get("month") == now.month and self.mapbox_requests.get("year", now.year) == now.year:
      request_count = self.mapbox_requests.get("total_requests", 0)
    else:
      request_count = 0

    if request_count >= MAPBOX_MONTHLY_REQUEST_LIMIT:
      if "year" not in self.mapbox_requests:
        self.mapbox_requests["year"] = now.year
        self.frogpilot_planner.params.put_nonblocking("MapBoxRequests", self.mapbox_requests)
      return

    self.mapbox_future = self.mapbox_executor.submit(
      self.request_mapbox_speed_limit,
      ";".join(f"{longitude:.6f},{latitude:.6f}" for (longitude, latitude), _ in self.mapbox_positions),
      sm["mapdOut"].wayId,
    )
    self.mapbox_requested_way_id = sm["mapdOut"].wayId
    self.mapbox_requests = {"month": now.month, "total_requests": request_count + 1, "year": now.year}

    self.frogpilot_planner.params.put_nonblocking("MapBoxRequests", self.mapbox_requests)

  def update_override(self, v_cruise_cluster, v_ego, v_ego_cluster, sm):
    speed_limit = self.target + self.offset
    gas_override = sm["carState"].gasPressed and v_ego > speed_limit

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
