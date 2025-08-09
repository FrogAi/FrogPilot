#!/usr/bin/env python3
# PFEIFER - SLC - Modified by FrogAi for FrogPilot
import calendar
import json
import math
import numpy as np
import requests

from concurrent.futures import ThreadPoolExecutor
from datetime import datetime

import openpilot.system.sentry as sentry

from openpilot.common.conversions import Conversions as CV
from openpilot.common.realtime import DT_MDL
from openpilot.common.time import system_time_valid

from openpilot.frogpilot.common.frogpilot_utilities import calculate_bearing_offset, calculate_distance_to_point, is_url_pingable
from openpilot.frogpilot.common.frogpilot_variables import params, params_cache, params_memory

FREE_MAPBOX_REQUESTS = 100_000

class SpeedLimitController:
  def __init__(self):
    self.executor = ThreadPoolExecutor(max_workers=1)

    self.calling_mapbox = False
    self.override_slc = False

    self.denied_target = 0
    self.map_speed_limit = 0
    self.mapbox_limit = 0
    self.next_speed_limit = 0
    self.overridden_speed = 0
    self.segment_distance = 0
    self.speed_limit_changed_timer = 0
    self.target = 0
    self.unconfirmed_speed_limit = 0

    self.previous_source = "None"
    self.source = "None"

    self.mapbox_requests = json.loads(params.get("MapBoxRequests") or "{}")
    self.mapbox_requests.setdefault("month", datetime.now().month)
    self.mapbox_requests.setdefault("total_requests", 0)
    self.mapbox_requests.setdefault("max_requests", FREE_MAPBOX_REQUESTS - (28 * 100))

    self.mapbox_host = "https://api.mapbox.com"
    self.mapbox_token = params_cache.get("MapboxSecretKey", encoding="utf8")

    self.previous_target = params.get_float("PreviousSpeedLimit")

  @property
  def experimental_mode(self):
    return self.frogpilot_toggles.slc_fallback_experimental_mode and self.target == 0

  @property
  def offset(self):
    if self.frogpilot_toggles.is_metric:
      offset_map = [
        (0, 8.1, self.frogpilot_toggles.speed_limit_offset1),      # 0–29 km/h
        (8.1, 13.6, self.frogpilot_toggles.speed_limit_offset2),   # 30–49
        (13.6, 16.4, self.frogpilot_toggles.speed_limit_offset3),  # 50–59
        (16.4, 21.9, self.frogpilot_toggles.speed_limit_offset4),  # 60–79
        (21.9, 27.5, self.frogpilot_toggles.speed_limit_offset5),  # 80–99
        (27.5, 33.1, self.frogpilot_toggles.speed_limit_offset6),  # 100–119
        (33.1, 38.9, self.frogpilot_toggles.speed_limit_offset7),  # 120–140
      ]
    else:
      offset_map = [
        (0, 11.2, self.frogpilot_toggles.speed_limit_offset1),     # 0–24 mph
        (11.2, 15.2, self.frogpilot_toggles.speed_limit_offset2),  # 25–34
        (15.2, 19.6, self.frogpilot_toggles.speed_limit_offset3),  # 35–44
        (19.6, 24.1, self.frogpilot_toggles.speed_limit_offset4),  # 45–54
        (24.1, 28.6, self.frogpilot_toggles.speed_limit_offset5),  # 55–64
        (28.6, 33.1, self.frogpilot_toggles.speed_limit_offset6),  # 65–74
        (33.1, 44.2, self.frogpilot_toggles.speed_limit_offset7),  # 75–99
      ]
    return next((offset for low, high, offset in offset_map if low < self.target < high), 0)

  def get_mapbox_speed_limit(self, gps_position, v_ego):
    try:
      if not gps_position or not self.mapbox_token:
        self.mapbox_limit = 0
        self.segment_distance = 0
        return

      if v_ego == 0:
        return

      if self.segment_distance > 0:
        self.segment_distance -= v_ego * DT_MDL
        return

      if self.calling_mapbox:
        self.segment_distance = v_ego
        return

      def make_request():
        try:
          self.calling_mapbox = True

          successful = False

          if not is_url_pingable(self.mapbox_host):
            self.segment_distance = 1000
            return None

          if system_time_valid():
            current_month = datetime.now().month
            if current_month != self.mapbox_requests.get("month"):
              self.mapbox_requests.update({
                "month": current_month,
                "total_requests": 0,
                "max_requests": FREE_MAPBOX_REQUESTS - calendar.monthrange(datetime.now().year, current_month)[1] * 100,
              })

          self.mapbox_requests["total_requests"] += 1
          params.put_nonblocking("MapBoxRequests", json.dumps(self.mapbox_requests))

          current_bearing = gps_position.get("bearing")
          current_latitude = gps_position.get("latitude")
          current_longitude = gps_position.get("longitude")

          future_latitude, future_longitude = calculate_bearing_offset(current_latitude, current_longitude, current_bearing, v_ego)

          url = f"{self.mapbox_host}/matching/v5/mapbox/driving/{current_longitude},{current_latitude};{future_longitude},{future_latitude}.json"
          mapbox_params = {
            "access_token": self.mapbox_token,
            "annotations": "maxspeed,distance",
            "geometries": "geojson",
            "overview": "full",
          }

          response = requests.get(url, params=mapbox_params, timeout=10)
          response.raise_for_status()

          successful = True

          return response.json()
        except requests.ConnectionError:
          print("ConnectionError while fetching Mapbox data...")
        except requests.HTTPError:
          print("HTTPError while fetching Mapbox data...")
        except requests.RequestException:
          print("RequestException while fetching Mapbox data...")
        except requests.Timeout:
          print("Timeout while fetching Mapbox data...")
        except Exception as error:
          sentry.capture_exception(error)
          print(f"Unexpected error in Mapbox request: {error}")
        finally:
          self.calling_mapbox = False

          if not successful:
            self.mapbox_limit = 0
            self.segment_distance = v_ego

            return None

      def complete_request(future):
        try:
          data = future.result()
          if data:
            if (matchings := data.get("matchings")) and matchings[0].get("legs"):
              annotations = matchings[0]["legs"][0].get("annotation", {})
              if (speed_data := annotations.get("maxspeed")) and (speed_limit_kph := speed_data[0].get("speed")):
                self.mapbox_limit = speed_limit_kph * CV.KPH_TO_MS
                self.segment_distance = annotations.get("distance", [v_ego])[0]
                return
        except Exception as error:
          sentry.capture_exception(error)
          print(f"Mapbox Callback Error: {error}")

          self.mapbox_limit = 0
          self.segment_distance = v_ego

      future = self.executor.submit(make_request)
      future.add_done_callback(complete_request)
    except Exception as error:
      sentry.capture_exception(error)
      print(f"Unexpected error in get_mapbox_speed_limit: {error}")

      self.calling_mapbox = False

      self.mapbox_limit = 0
      self.segment_distance = v_ego

  def handle_limit_change(self, desired_source, desired_target, sm):
    self.speed_limit_changed_timer += DT_MDL

    speed_limit_accepted = (sm["frogpilotCarState"].accelPressed and sm["carControl"].enabled) or params_memory.get_bool("SpeedLimitAccepted")
    speed_limit_denied = sm["frogpilotCarState"].decelPressed or (self.speed_limit_changed_timer >= 30)

    if speed_limit_accepted:
      self.overridden_speed = 0

      self.source = desired_source
      self.target = desired_target

      params_memory.remove("SpeedLimitAccepted")

    elif speed_limit_denied:
      self.denied_target = desired_target

      self.previous_source = desired_source
      self.previous_target = desired_target

    elif desired_target < self.target and not self.frogpilot_toggles.speed_limit_confirmation_lower:
      self.source = desired_source
      self.target = desired_target

    elif desired_target > self.target and not self.frogpilot_toggles.speed_limit_confirmation_higher:
      self.source = desired_source
      self.target = desired_target

    else:
      self.source = "None"
      self.unconfirmed_speed_limit = desired_target

    if self.target != self.previous_target and self.target > 0 and not speed_limit_denied:
      self.denied_target = 0

      self.previous_source = self.source
      self.previous_target = self.target

      params.put_float_nonblocking("PreviousSpeedLimit", self.target)

  def update_limits(self, dashboard_speed_limit, gps_position, navigation_speed_limit, v_cruise, v_cruise_cluster, v_ego, sm):
    self.update_map_speed_limit(gps_position, v_ego)

    limits = {
      "Dashboard": dashboard_speed_limit,
      "Map Data": self.map_speed_limit,
      "Navigation": navigation_speed_limit
    }
    filtered_limits = {source: limit for source, limit in limits.items() if limit}

    if self.frogpilot_toggles.speed_limit_priority_highest:
      desired_source = max(filtered_limits, key=filtered_limits.get, default="None")
      desired_target = filtered_limits.get(desired_source, 0)

    elif self.frogpilot_toggles.speed_limit_priority_lowest:
      desired_source = min(filtered_limits, key=filtered_limits.get, default="None")
      desired_target = filtered_limits.get(desired_source, 0)

    elif filtered_limits:
      for priority in [
        self.frogpilot_toggles.speed_limit_priority1,
        self.frogpilot_toggles.speed_limit_priority2,
        self.frogpilot_toggles.speed_limit_priority3
      ]:
        if priority in filtered_limits:
          desired_source = priority
          desired_target = filtered_limits[desired_source]
          break
      else:
        desired_source = "None"
        desired_target = 0

    else:
      desired_source = "None"
      desired_target = 0

    if desired_target == 0:
      if self.mapbox_requests["total_requests"] < self.mapbox_requests["max_requests"] and self.frogpilot_toggles.slc_mapbox_filler:
        self.get_mapbox_speed_limit(gps_position, v_ego)

        if self.mapbox_limit:
          desired_source = "Mapbox"
          desired_target = self.mapbox_limit
          print(f"Mapbox limit found: {desired_target}")

      if desired_target == 0:
        if self.denied_target != self.previous_target > 0 and self.frogpilot_toggles.slc_fallback_previous_speed_limit:
          desired_source = self.previous_source
          desired_target = self.previous_target

          self.target = desired_target

        elif sm["controlsState"].enabled and self.frogpilot_toggles.slc_fallback_set_speed:
          desired_source = "None"
          desired_target = v_cruise_cluster
    else:
      self.mapbox_limit = 0
      self.segment_distance = 0

    if abs(desired_target - self.previous_target) >= 1:
      self.handle_limit_change(desired_source, desired_target, sm)
    elif desired_source != self.source and abs(desired_target - self.target) < 1:
      self.source = desired_source
    else:
      self.speed_limit_changed_timer = 0
      self.unconfirmed_speed_limit = 0

  def update_override(self, v_cruise, v_cruise_cluster, v_ego, sm):
    self.override_slc = self.overridden_speed > self.target + self.offset > 0
    self.override_slc |= sm["carState"].gasPressed and v_ego > self.target + self.offset > 0
    self.override_slc &= sm["controlsState"].enabled

    if self.override_slc:
      if self.frogpilot_toggles.speed_limit_controller_override_manual:
        if sm["carState"].gasPressed:
          self.overridden_speed = max(sm["carState"].vEgoCluster, self.overridden_speed)
        self.overridden_speed = float(np.clip(self.overridden_speed, self.target + self.offset, v_cruise_cluster))
      elif self.frogpilot_toggles.speed_limit_controller_override_set_speed:
        self.overridden_speed = v_cruise_cluster

      self.source = "None"
    else:
      self.overridden_speed = 0

  def update_map_speed_limit(self, gps_position, v_ego):
    if not gps_position:
      return

    self.map_speed_limit = params_memory.get_float("MapSpeedLimit")

    next_map_speed_limit = json.loads(params_memory.get("NextMapSpeedLimit") or "{}")
    self.next_speed_limit = next_map_speed_limit.get("speedlimit", 0)

    if self.next_speed_limit:
      current_latitude = gps_position.get("latitude")
      current_longitude = gps_position.get("longitude")

      next_latitude = next_map_speed_limit.get("latitude")
      next_longitude = next_map_speed_limit.get("longitude")

      distance_to_upcoming = calculate_distance_to_point(current_latitude * CV.DEG_TO_RAD, current_longitude * CV.DEG_TO_RAD, next_latitude * CV.DEG_TO_RAD, next_longitude * CV.DEG_TO_RAD)

      if self.map_speed_limit < self.next_speed_limit:
        max_lookahead = self.frogpilot_toggles.map_speed_lookahead_higher * v_ego
      elif self.map_speed_limit > self.next_speed_limit:
        max_lookahead = self.frogpilot_toggles.map_speed_lookahead_lower * v_ego
      else:
        max_lookahead = 0

      if distance_to_upcoming < max_lookahead:
        self.map_speed_limit = self.next_speed_limit
