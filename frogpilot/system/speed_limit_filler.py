#!/usr/bin/env python3
import json
import math
import requests
import time

from collections import OrderedDict, deque
from datetime import datetime, timedelta, timezone

import openpilot.system.sentry as sentry

from cereal import log, messaging

from openpilot.frogpilot.common.frogpilot_utilities import calculate_distance_to_point, calculate_lane_width, is_url_pingable
from openpilot.frogpilot.common.frogpilot_variables import params, params_memory

NetworkType = log.DeviceState.NetworkType

BOUNDING_BOX_RADIUS_DEGREE = 0.1
MAX_ENTRIES = 1_000_000
MAX_OVERPASS_DATA_BYTES = 1_073_741_824
MAX_OVERPASS_REQUESTS = 10_000
METERS_PER_DEG_LAT = 111_320
VETTING_INTERVAL_DAYS = 7

OVERPASS_API_URL = "https://overpass-api.de/api/interpreter"
OVERPASS_STATUS_URL = "https://overpass-api.de/api/status"

class MapSpeedLogger:
  def __init__(self):
    self.cached_box = None
    self.previous_coordinates = None

    self.cached_segments = {}

    self.dataset_additions = deque(maxlen=MAX_ENTRIES)

    self.overpass_requests = json.loads(params.get("OverpassRequests") or "{}")
    self.overpass_requests.setdefault("day", datetime.now(timezone.utc).day)
    self.overpass_requests.setdefault("total_bytes", 0)
    self.overpass_requests.setdefault("total_requests", 0)

    self.session = requests.Session()
    self.session.headers.update({"Accept-Language": "en"})
    self.session.headers.update({"User-Agent": "frogpilot-map-speed-logger/1.0 (https://github.com/FrogAi/FrogPilot)"})

    self.sm = messaging.SubMaster(["deviceState", "frogpilotCarState", "frogpilotNavigation", "frogpilotPlan", "liveLocationKalman", "modelV2"])

  @property
  def can_make_overpass_request(self):
    return self.overpass_requests["total_bytes"] < MAX_OVERPASS_DATA_BYTES and self.overpass_requests["total_requests"] < MAX_OVERPASS_REQUESTS

  @property
  def should_stop_processing(self):
    return self.sm["deviceState"].started or not params_memory.get_bool("UpdateSpeedLimits")

  @staticmethod
  def cleanup_dataset(dataset):
    cleaned_data = OrderedDict()

    for item in dataset:
      if "last_vetted" in item:
        required = {"incorrect_limit", "last_vetted", "segment_id", "source", "speed_limit", "start_coordinates"}
      else:
        required = {"bearing", "end_coordinates", "incorrect_limit", "road_name", "road_width", "source", "speed_limit", "start_coordinates"}

      if not required.issubset(item.keys()):
        continue

      entry_copy = item.copy()
      entry_copy.pop("last_vetted", None)

      key = json.dumps(entry_copy, sort_keys=True)
      cleaned_data[key] = item

    return deque(cleaned_data.values(), maxlen=MAX_ENTRIES)

  @staticmethod
  def meters_to_deg_lat(meters):
    return meters / METERS_PER_DEG_LAT

  @staticmethod
  def meters_to_deg_lon(meters, latitude):
    return meters / (METERS_PER_DEG_LAT * math.cos(math.radians(latitude)))

  def get_speed_limit_source(self):
    sources = [
      (self.sm["frogpilotNavigation"].navigationSpeedLimit, "NOO"),
      (self.sm["frogpilotPlan"].slcMapboxSpeedLimit, "Mapbox"),
      (self.sm["frogpilotCarState"].dashboardSpeedLimit, "Dashboard"),
    ]
    for speed_limit, source in sources:
      if speed_limit > 0:
        return speed_limit, source
    return None

  def is_in_cached_box(self, latitude, longitude):
    if self.cached_box is None:
      return False
    return self.cached_box["min_latitude"] <= latitude <= self.cached_box["max_latitude"] and \
           self.cached_box["min_longitude"] <= longitude <= self.cached_box["max_longitude"]

  def record_overpass_request(self, content_bytes):
    self.overpass_requests["total_bytes"] += content_bytes
    self.overpass_requests["total_requests"] += 1

  def reset_daily_api_limits(self):
    current_day = datetime.now(timezone.utc).day
    if current_day != self.overpass_requests["day"]:
      self.overpass_requests.update({
        "day": current_day,
        "total_requests": 0,
        "total_bytes": 0,
      })

  def update_params(self, dataset, filtered_dataset):
    params.put("OverpassRequests", json.dumps(self.overpass_requests))
    params.put("SpeedLimits", json.dumps(list(dataset)))
    params.put("SpeedLimitsFiltered", json.dumps(list(filtered_dataset)))

  def wait_for_api(self):
    while not is_url_pingable(OVERPASS_STATUS_URL):
      print("Waiting for Overpass API to be available...")
      self.sm.update()

      if self.should_stop_processing:
        return False

      time.sleep(10)
    return True

  def fetch_from_overpass(self, latitude, longitude):
    min_lat = latitude - BOUNDING_BOX_RADIUS_DEGREE
    max_lat = latitude + BOUNDING_BOX_RADIUS_DEGREE
    min_lon = longitude - BOUNDING_BOX_RADIUS_DEGREE
    max_lon = longitude + BOUNDING_BOX_RADIUS_DEGREE

    self.cached_box = {"min_latitude": min_lat, "max_latitude": max_lat, "min_longitude": min_lon, "max_longitude": max_lon}
    self.cached_segments.clear()

    query = (
      f"[out:json][timeout:90][maxsize:{MAX_OVERPASS_DATA_BYTES // 10}];"
      f"way({min_lat:.5f},{min_lon:.5f},{max_lat:.5f},{max_lon:.5f})"
      "[highway~'^(motorway|motorway_link|primary|primary_link|residential|"
      "secondary|secondary_link|tertiary|tertiary_link|trunk|trunk_link)$'];"
      "out geom qt;"
    )

    try:
      response = self.session.post(OVERPASS_API_URL, data=query, timeout=90)
      self.record_overpass_request(len(response.content))

      if response.status_code == 429:
        retry_after = int(response.headers.get("Retry-After", 10))
        print(f"Overpass API rate limit hit. Retrying in {retry_after} seconds.")

        time.sleep(retry_after)

        response = self.session.post(OVERPASS_API_URL, data=query, timeout=90)
        self.record_overpass_request(len(response.content))

      response.raise_for_status()
      return response.json().get("elements", [])
    except requests.exceptions.RequestException as exception:
      print(f"Overpass API request failed: {exception}")
      self.cached_segments.clear()
      return []

  def filter_segments_for_entry(self, entry):
    bearing_rad = math.radians(entry["bearing"])
    start_lat, start_lon = entry["start_coordinates"]["latitude"], entry["start_coordinates"]["longitude"]
    end_lat, end_lon = entry["end_coordinates"]["latitude"], entry["end_coordinates"]["longitude"]
    mid_lat = (start_lat + end_lat) / 2

    forward_buffer_lat = self.meters_to_deg_lat(entry["speed_limit"])
    forward_buffer_lon = self.meters_to_deg_lon(entry["speed_limit"], mid_lat)
    side_buffer_lat = self.meters_to_deg_lat(entry["road_width"])
    side_buffer_lon = self.meters_to_deg_lon(entry["road_width"], mid_lat)

    delta_lat_fwd = forward_buffer_lat * math.cos(bearing_rad)
    delta_lon_fwd = forward_buffer_lon * math.sin(bearing_rad)
    delta_lat_side = side_buffer_lat * math.cos(bearing_rad + math.pi / 2)
    delta_lon_side = side_buffer_lon * math.sin(bearing_rad + math.pi / 2)

    min_lat = min(start_lat, end_lat) - abs(delta_lat_fwd) - abs(delta_lat_side)
    max_lat = max(start_lat, end_lat) + abs(delta_lat_fwd) + abs(delta_lat_side)
    min_lon = min(start_lon, end_lon) - abs(delta_lon_fwd) - abs(delta_lon_side)
    max_lon = max(start_lon, end_lon) + abs(delta_lon_fwd) + abs(delta_lon_side)

    relevant_segments = []
    for segment in self.cached_segments.values():
      if not segment or "nodes" not in segment:
        continue

      latitudes = [node[0] for node in segment["nodes"]]
      longitudes = [node[1] for node in segment["nodes"]]

      if not (max(latitudes) < min_lat or min(latitudes) > max_lat or max(longitudes) < min_lon or min(longitudes) > max_lon):
        relevant_segments.append(segment)

    return relevant_segments

  def log_speed_limit(self):
    if not self.sm.updated["liveLocationKalman"]:
      return

    localizer_valid = self.sm["liveLocationKalman"].status == log.LiveLocationKalman.Status.valid and self.sm["liveLocationKalman"].positionGeodetic.valid
    if not (self.sm["liveLocationKalman"].gpsOK and localizer_valid):
      self.previous_coordinates = None
      return

    current_latitude = self.sm["liveLocationKalman"].positionGeodetic.value[0]
    current_longitude = self.sm["liveLocationKalman"].positionGeodetic.value[1]

    if self.previous_coordinates is None:
      self.previous_coordinates = {"latitude": current_latitude, "longitude": current_longitude}
      return

    current_speed_source = self.get_speed_limit_source()
    valid_sources = {source[0] for source in [current_speed_source] if source and source[0] > 0}

    map_speed = params_memory.get_float("MapSpeedLimit")
    is_incorrect_limit = bool(map_speed > 0 and valid_sources and all(abs(map_speed - source) > 1 for source in valid_sources))

    if map_speed > 0 and not is_incorrect_limit:
      self.previous_coordinates = None
      return

    road_name = params_memory.get("RoadName", encoding="utf-8")
    if not road_name or not current_speed_source:
      return

    distance = calculate_distance_to_point(
      math.radians(self.previous_coordinates["latitude"]),
      math.radians(self.previous_coordinates["longitude"]),
      math.radians(current_latitude),
      math.radians(current_longitude)
    )
    if distance < 1:
      return

    speed_limit, source = current_speed_source
    self.dataset_additions.append({
      "bearing": math.degrees(self.sm["liveLocationKalman"].calibratedOrientationNED.value[2]),
      "end_coordinates": {"latitude": current_latitude, "longitude": current_longitude},
      "incorrect_limit": is_incorrect_limit,
      "road_name": road_name,
      "road_width": calculate_lane_width(self.sm["modelV2"].laneLines[1], self.sm["modelV2"].laneLines[2]),
      "source": source,
      "speed_limit": speed_limit,
      "start_coordinates": self.previous_coordinates,
    })

    self.previous_coordinates = {"latitude": current_latitude, "longitude": current_longitude}

  def process_new_entries(self, dataset, filtered_dataset):
    existing_segment_ids = {entry["segment_id"] for entry in filtered_dataset if "segment_id" in entry}
    entries_to_process = list(dataset)
    total_entries = len(entries_to_process)

    for i, entry in enumerate(entries_to_process):
      self.sm.update()

      if self.should_stop_processing:
        break

      if not self.can_make_overpass_request:
        params_memory.put("UpdateSpeedLimitsStatus", "Hit API limit...")
        time.sleep(5)
        break

      params_memory.put("UpdateSpeedLimitsStatus", f"Processing: {i + 1} / {total_entries}")

      start_coords = entry["start_coordinates"]
      self.update_cached_segments(start_coords["latitude"], start_coords["longitude"])
      segments = self.filter_segments_for_entry(entry)

      dataset.remove(entry)

      for segment in segments:
        segment_id = segment["segment_id"]
        if segment_id in existing_segment_ids:
          continue
        if segment["maxspeed"] and not entry.get("incorrect_limit"):
          continue
        if segment["road_name"] != entry.get("road_name"):
          continue

        filtered_dataset.append({
          "incorrect_limit": entry.get("incorrect_limit"),
          "last_vetted": datetime.now(timezone.utc).isoformat(),
          "segment_id": segment_id,
          "source": entry["source"],
          "speed_limit": entry["speed_limit"],
          "start_coordinates": entry["start_coordinates"],
        })
        existing_segment_ids.add(segment_id)

      if i % 100 == 0:
        self.update_params(dataset, filtered_dataset)

  def process_speed_limits(self):
    self.reset_daily_api_limits()

    if not self.wait_for_api():
      return

    self.cached_box, self.cached_segments = None, {}

    dataset = self.cleanup_dataset(json.loads(params.get("SpeedLimits") or "[]"))
    filtered_dataset = self.cleanup_dataset(json.loads(params.get("SpeedLimitsFiltered") or "[]"))

    filtered_dataset = self.vet_entries(filtered_dataset)
    self.update_params(dataset, filtered_dataset)

    if dataset and not self.should_stop_processing:
      self.cached_box, self.cached_segments = None, {}
      params_memory.put("UpdateSpeedLimitsStatus", "Calculating...")
      self.process_new_entries(dataset, filtered_dataset)

    self.update_params(dataset, filtered_dataset)
    params_memory.put("UpdateSpeedLimitsStatus", "Completed!")
    params_memory.remove("UpdateSpeedLimits")

  def update_cached_segments(self, latitude, longitude, vetting=False):
    if not self.is_in_cached_box(latitude, longitude):
      elements = self.fetch_from_overpass(latitude, longitude)
      for way in elements:
        if way.get("type") == "way" and (segment_id := way.get("id")):
          tags = way.get("tags", {})
          if vetting:
            self.cached_segments[segment_id] = tags.get("maxspeed")
          elif "geometry" in way and (nodes := way["geometry"]):
            self.cached_segments[segment_id] = {
              "maxspeed": tags.get("maxspeed"),
              "nodes": [(node["lat"], node["lon"]) for node in nodes],
              "road_name": tags.get("name"),
              "segment_id": segment_id,
            }

  def vet_entries(self, filtered_dataset):
    dataset_list = list(filtered_dataset)
    total_to_vet = len(filtered_dataset)
    vetted_entries = deque(maxlen=MAX_ENTRIES)

    for i, entry in enumerate(dataset_list):
      self.sm.update()

      if self.should_stop_processing:
        vetted_entries.extend(dataset_list[i:])
        break

      if not self.can_make_overpass_request:
        params_memory.put("UpdateSpeedLimitsStatus", "Hit API limit...")
        time.sleep(5)
        vetted_entries.extend(dataset_list[i:])
        break

      params_memory.put("UpdateSpeedLimitsStatus", f"Vetting: {i + 1} / {total_to_vet}")

      last_vetted_time = datetime.fromisoformat(entry["last_vetted"])
      if datetime.now(timezone.utc) - last_vetted_time < timedelta(days=VETTING_INTERVAL_DAYS):
        vetted_entries.append(entry)
        continue

      start_coords = entry["start_coordinates"]
      self.update_cached_segments(start_coords["latitude"], start_coords["longitude"], vetting=True)

      current_maxspeed = self.cached_segments.get(entry["segment_id"])
      if current_maxspeed is None or (entry.get("incorrect_limit") and current_maxspeed != entry.get("speed_limit")):
        entry["last_vetted"] = datetime.now(timezone.utc).isoformat()
        vetted_entries.append(entry)

    return self.cleanup_dataset(list(vetted_entries))

def main():
  logger = MapSpeedLogger()

  previously_started = False

  while True:
    try:
      logger.sm.update()

      if logger.sm["deviceState"].started:
        logger.log_speed_limit()

        previously_started = True
      elif previously_started:
        existing_dataset = json.loads(params.get("SpeedLimits") or "[]")
        existing_dataset.extend(logger.dataset_additions)

        new_dataset = logger.cleanup_dataset(existing_dataset)
        params.put("SpeedLimits", json.dumps(list(new_dataset)))

        if logger.sm["deviceState"].networkType in (NetworkType.ethernet, NetworkType.wifi):
          params_memory.put_bool("UpdateSpeedLimits", True)

        logger.dataset_additions.clear()

        previously_started = False
      elif params_memory.get_bool("UpdateSpeedLimits"):
        logger.process_speed_limits()
      else:
        time.sleep(5)

    except Exception as exception:
      print(f"Error in speed_limit_filler: {exception}")
      sentry.capture_exception(exception)
      time.sleep(1)

if __name__ == "__main__":
  main()
