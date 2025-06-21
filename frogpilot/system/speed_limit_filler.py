#!/usr/bin/env python3
import json
import math
import requests
import time

from collections import OrderedDict, deque
from datetime import datetime, timedelta, timezone

import openpilot.system.sentry as sentry

from cereal import log, messaging

from openpilot.frogpilot.common.frogpilot_utilities import calculate_bearing_offset, calculate_distance_to_point, calculate_lane_width
from openpilot.frogpilot.common.frogpilot_variables import params, params_memory

MAX_ENTRIES = 1_000_000
MAX_OVERPASS_REQUESTS = 10_000
VETTING_INTERVAL = 7

OVERPASS_API_URL = "https://overpass-api.de/api/interpreter"

def cleanup_dataset(dataset):
  cleaned_dataset = OrderedDict()

  for entry in dataset:
    entry_copy = entry.copy()
    entry_copy.pop("last_vetted", None)

    entry_data = json.dumps(entry_copy, sort_keys=True)
    cleaned_dataset[entry_data] = entry

  return deque(cleaned_dataset.values(), maxlen=MAX_ENTRIES)

class MapSpeedLogger:
  def __init__(self):
    self.started_previously = False

    self.total_queries_to_process = 0
    self.total_requests = 0

    self.previous_coords = None

    self.dataset_additions = cleanup_dataset(json.loads("[]"))

    self.overpass_requests = json.loads(params.get("OverpassRequests") or "{}")
    self.overpass_requests.setdefault("day", datetime.now().day)
    self.overpass_requests.setdefault("total_requests", 0)
    self.overpass_requests.setdefault("max_requests", MAX_OVERPASS_REQUESTS)

    self.sm = messaging.SubMaster(["deviceState", "frogpilotCarState", "frogpilotNavigation", "frogpilotPlan", "liveLocationKalman", "modelV2"])

  def can_make_overpass_request(self):
    if datetime.now().day != self.overpass_requests["day"]:
      self.overpass_requests.update({"day": datetime.now().day, "total_requests": 0})
    return self.overpass_requests["total_requests"] < self.overpass_requests["max_requests"]

  def record_overpass_request(self):
    self.total_requests += 1
    params_memory.put("UpdateSpeedLimitsStatus", f"{self.total_requests} / {self.total_queries_to_process}")

  def log_speed_limit(self):
    self.sm.update()

    if not self.sm["deviceState"].started and self.started_previously:
      self.dataset_additions = cleanup_dataset(self.dataset_additions)
      if self.dataset_additions:
        existing_dataset = cleanup_dataset(json.loads(params.get("SpeedLimits") or "[]"))

        for entry in self.dataset_additions:
          existing_dataset.append(entry)

        params.put("SpeedLimits", json.dumps(list(existing_dataset)))

        self.dataset_additions.clear()

      self.previous_coords = None

    self.started_previously = self.sm["deviceState"].started

    if not self.sm.updated["liveLocationKalman"]:
      return

    localizer_valid = self.sm["liveLocationKalman"].status == log.LiveLocationKalman.Status.valid and self.sm["liveLocationKalman"].positionGeodetic.valid
    if not (self.sm["liveLocationKalman"].gpsOK and localizer_valid):
      self.previous_coords = None
      return

    if params_memory.get_float("MapSpeedLimit") != 0:
      self.previous_coords = None
      return

    current_bearing = math.degrees(self.sm["liveLocationKalman"].calibratedOrientationNED.value[2])
    current_latitude = self.sm["liveLocationKalman"].positionGeodetic.value[0]
    current_longitude = self.sm["liveLocationKalman"].positionGeodetic.value[1]

    if self.previous_coords is not None:
      start_latitude = math.radians(self.previous_coords["latitude"])
      start_longitude = math.radians(self.previous_coords["longitude"])

      end_latitude = math.radians(current_latitude)
      end_longitude = math.radians(current_longitude)

      distance = calculate_distance_to_point(start_latitude, start_longitude, end_latitude, end_longitude)
      if distance < 1:
        return
    else:
      self.previous_coords = {"latitude": current_latitude, "longitude": current_longitude}
      return

    dashboard_speed = self.sm["frogpilotCarState"].dashboardSpeedLimit
    mapbox_speed = self.sm["frogpilotPlan"].slcMapboxSpeedLimit
    navigation_speed = self.sm["frogpilotNavigation"].navigationSpeedLimit

    road_name = params_memory.get("RoadName", encoding="utf-8")
    road_width = calculate_lane_width(self.sm["modelV2"].roadEdges[0], self.sm["modelV2"].roadEdges[1])

    if navigation_speed and road_name and road_width:
      self.dataset_additions.append({
        "start_coordinates": self.previous_coords,
        "end_coordinates": {"latitude": current_latitude, "longitude": current_longitude},
        "bearing": current_bearing,
        "speed_limit": navigation_speed,
        "source": "NOO",
        "road_name": road_name,
        "road_width": road_width
      })

    elif mapbox_speed and road_name and road_width:
      self.dataset_additions.append({
        "start_coordinates": self.previous_coords,
        "end_coordinates": {"latitude": current_latitude, "longitude": current_longitude},
        "bearing": current_bearing,
        "speed_limit": mapbox_speed,
        "source": "Mapbox",
        "road_name": road_name,
        "road_width": road_width
      })

    elif dashboard_speed and road_name and road_width:
      self.dataset_additions.append({
        "start_coordinates": self.previous_coords,
        "end_coordinates": {"latitude": current_latitude, "longitude": current_longitude},
        "bearing": current_bearing,
        "speed_limit": dashboard_speed,
        "source": "Dashboard",
        "road_name": road_name,
        "road_width": road_width
      })

    self.previous_coords = {"latitude": current_latitude, "longitude": current_longitude}

  def fetch_segments_from_overpass(self, start_coords, end_coords, bearing, road_width):
    road_types = "(motorway|motorway_link|primary|primary_link|residential|secondary|secondary_link|tertiary|tertiary_link|trunk|trunk_link)"

    start_latitude = start_coords["latitude"]
    start_longitude = start_coords["longitude"]

    start_left = calculate_bearing_offset(start_latitude, start_longitude, bearing - 90, road_width)
    start_right = calculate_bearing_offset(start_latitude, start_longitude, bearing + 90, road_width)

    points = [(start_latitude, start_longitude), start_left, start_right]

    min_latitude = min(pt[0] for pt in points)
    max_latitude = max(pt[0] for pt in points)
    min_longitude = min(pt[1] for pt in points)
    max_longitude = max(pt[1] for pt in points)

    query = (
      f"[out:json]; "
      f"way({min_latitude},{min_longitude},{max_latitude},{max_longitude})"
      f"[highway~'{road_types}']; out body; >; out body;"
    )

    for api_url in [OVERPASS_API_URL]:
      try:
        response = requests.get(api_url, params={"data": query}, timeout=10)
        if response.status_code == 429:
          print("Rate limited. Retrying after 15 seconds...")
          time.sleep(15)
          continue

        response.raise_for_status()
        self.record_overpass_request()

        data = response.json()
        node_elements = {element["id"]: element for element in data.get("elements", []) if element.get("type") == "node"}
        ways = [element for element in data.get("elements", []) if element.get("type") == "way"]

        if ways:
          segments = []
          for way in ways:
            node_ids = way.get("nodes", [])

            coords = [(node_elements[node]["lat"], node_elements[node]["lon"]) for node in node_ids if node in node_elements]
            if coords:
              latitudes = [pt[0] for pt in coords]
              longitudes = [pt[1] for pt in coords]

              segment_coordinates = {
                "min_latitude": min(latitudes),
                "max_latitude": max(latitudes),
                "min_longitude": min(longitudes),
                "max_longitude": max(longitudes),
              }
            else:
              segment_coordinates = None

            segments.append({
              "maxspeed": way.get("tags", {}).get("maxspeed"),
              "road_name": way.get("tags", {}).get("name"),
              "segment_id": way.get("id"),
              "segment_coordinates": segment_coordinates,
            })
          return segments
        else:
          return []

      except requests.ConnectionError:
        print(f"ConnectionError while fetching from {api_url}")
      except requests.HTTPError:
        print(f"HTTPError while fetching from {api_url}")
      except requests.RequestException:
        print(f"RequestException while fetching from {api_url}")
      except requests.Timeout:
        print(f"Timeout while fetching from {api_url}")
      except Exception as error:
        sentry.capture_exception(error)
        print(f"Unexpected error fetching from {api_url}: {error}")

    return None

  def fetch_speed_limit_for_segment_id(self, segment_id):
    query = f"[out:json]; way({segment_id}); out tags;"

    for api_url in [OVERPASS_API_URL]:
      try:
        response = requests.get(api_url, params={"data": query}, timeout=10)
        if response.status_code == 429:
          print("Rate limited. Retrying after 15 seconds...")
          time.sleep(15)
          continue

        response.raise_for_status()
        self.record_overpass_request()

        data = response.json()
        ways = [element for element in data.get("elements", []) if element.get("type") == "way"]
        return ways[0].get("tags", {}).get("maxspeed") if ways else None

      except requests.ConnectionError:
        print(f"ConnectionError while fetching from {api_url}")
      except requests.HTTPError:
        print(f"HTTPError while fetching from {api_url}")
      except requests.RequestException:
        print(f"RequestException while fetching from {api_url}")
      except requests.Timeout:
        print(f"Timeout while fetching from {api_url}")
      except Exception as error:
        sentry.capture_exception(error)
        print(f"Unexpected error fetching from {api_url}: {error}")

    return None

  def process_entry(self, entry):
    bearing = entry.get("bearing")
    end_coords = entry.get("end_coordinates")
    road_width = entry.get("road_width")
    start_coords = entry.get("start_coordinates")

    segments = self.fetch_segments_from_overpass(start_coords, end_coords, bearing, road_width)
    return entry, segments

  def process_vetted_entry(self, entry):
    if datetime.now(timezone.utc) - datetime.fromisoformat(entry.get("last_vetted")) < timedelta(days=VETTING_INTERVAL):
      return entry

    if self.fetch_speed_limit_for_segment_id(entry.get("segment_id")) is None:
      entry["last_vetted"] = datetime.now(timezone.utc).isoformat()
      return entry

  def update_speed_limits(self):
    if datetime.now().day != self.overpass_requests["day"]:
      self.overpass_requests.update({"day": datetime.now().day, "total_requests": 0})

    dataset = cleanup_dataset(json.loads(params.get("SpeedLimits") or "[]"))
    filtered_dataset = cleanup_dataset(json.loads(params.get("SpeedLimitsFiltered") or "[]"))

    vetted_needed = sum(1 for entry in filtered_dataset if datetime.now(timezone.utc) - datetime.fromisoformat(entry.get("last_vetted")) >= timedelta(days=VETTING_INTERVAL))
    self.total_queries_to_process = min(vetted_needed + len(dataset), self.overpass_requests["max_requests"] - self.overpass_requests["total_requests"])

    filtered_vetted = deque(maxlen=MAX_ENTRIES)
    for entry in filtered_dataset:
      self.sm.update()

      if self.sm["deviceState"].started:
        break

      if not self.can_make_overpass_request():
        break

      result = self.process_vetted_entry(entry)
      if result is not None:
        filtered_vetted.append(result)

    filtered_dataset = cleanup_dataset(filtered_vetted)

    if not dataset:
      self.overpass_requests["total_requests"] += self.total_requests

      params.put("OverpassRequests", json.dumps(self.overpass_requests))
      params.put("SpeedLimitsFiltered", json.dumps(list(filtered_dataset)))

      params_memory.remove("UpdateSpeedLimits")
      params_memory.remove("UpdateSpeedLimitsStatus")
      return

    existing_segment_ids = {entry["segment_id"] for entry in filtered_dataset if "segment_id" in entry}
    processed_count = 0

    for entry in list(dataset):
      self.sm.update()

      if self.sm["deviceState"].started:
        break

      if not self.can_make_overpass_request():
        break

      entry, segments = self.process_entry(entry)

      if segments is None:
        continue

      dataset.remove(entry)

      for segment in segments:
        segment_id = segment["segment_id"]
        if segment_id in existing_segment_ids:
          continue
        if segment["maxspeed"]:
          continue
        if segment["road_name"] != entry.get("road_name"):
          continue

        filtered_dataset.append({
          "segment_id": segment_id,
          "source": entry.get("source"),
          "speed_limit": entry.get("speed_limit"),
          "last_vetted": datetime.now(timezone.utc).isoformat()
        })

        existing_segment_ids.add(segment_id)

      processed_count += 1

      if processed_count % 100 == 0:
        self.overpass_requests["total_requests"] += 100

        params.put("OverpassRequests", json.dumps(self.overpass_requests))
        params.put("SpeedLimits", json.dumps(list(dataset)))
        params.put("SpeedLimitsFiltered", json.dumps(list(filtered_dataset)))

    self.overpass_requests["total_requests"] += processed_count
    self.total_requests = 0

    params.put("OverpassRequests", json.dumps(self.overpass_requests))
    params.put("SpeedLimits", json.dumps(list(dataset)))
    params.put("SpeedLimitsFiltered", json.dumps(list(filtered_dataset)))

    params_memory.remove("UpdateSpeedLimits")
    params_memory.remove("UpdateSpeedLimitsStatus")

def main():
  logger = MapSpeedLogger()

  while True:
    try:
      if not logger.sm["deviceState"].started and params_memory.get_bool("UpdateSpeedLimits"):
        logger.update_speed_limits()

      logger.log_speed_limit()
    except Exception as error:
      print(f"Error in speed_limit_filler: {error}")
      sentry.capture_exception(error)

if __name__ == "__main__":
  main()
