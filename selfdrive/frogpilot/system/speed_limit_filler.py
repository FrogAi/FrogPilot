#!/usr/bin/env python3
import json
import math
import requests
import time

from collections import OrderedDict, deque
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timedelta, timezone

import openpilot.system.sentry as sentry

from cereal import log, messaging
from openpilot.common.time import system_time_valid
from openpilot.selfdrive.frogpilot.frogpilot_utilities import calculate_bearing_offset, calculate_distance_to_point, calculate_lane_width, is_url_pingable
from openpilot.selfdrive.frogpilot.frogpilot_variables import params, params_memory

BEARING_THRESHOLD = 15
DISTANCE_THRESHOLD = 100
MAX_ENTRIES = 1_000_000

OVERPASS_API_URL = "https://overpass-api.de/api/interpreter"

def add_entry(dataset, entry):
  dataset.append(entry)

def cleanup_dataset(dataset):
  cleaned_dataset = OrderedDict()

  for entry in dataset:
    entry_copy = entry.copy()
    entry_copy.pop("last_vetted", None)

    entry_data = json.dumps(entry_copy, sort_keys=True)
    cleaned_dataset[entry_data] = entry

  while len(cleaned_dataset) > MAX_ENTRIES:
    cleaned_dataset.popitem(last=False)

  return deque(cleaned_dataset.values(), maxlen=MAX_ENTRIES)

class MapSpeedLogger:
  def __init__(self):
    self.speed_limits_checked = False
    self.started_previously = False

    self.dataset_additions = cleanup_dataset(json.loads("[]"))

    self.previous_coords = None

    self.sm = messaging.SubMaster(["deviceState", "frogpilotCarState", "frogpilotNavigation", "frogpilotPlan", "liveLocationKalman", "modelV2"])

  def log_speed_limit(self):
    self.sm.update()

    if not self.sm["deviceState"].started and self.started_previously:
      self.dataset_additions = cleanup_dataset(self.dataset_additions)
      if self.dataset_additions:
        existing_dataset = cleanup_dataset(json.loads(params.get("SpeedLimits") or "[]"))

        for entry in self.dataset_additions:
          add_entry(existing_dataset, entry)

        params.put("SpeedLimits", json.dumps(list(existing_dataset)))

        self.dataset_additions.clear()

      self.speed_limits_checked = False

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
      add_entry(self.dataset_additions, {
        "start_coordinates": self.previous_coords,
        "end_coordinates": {"latitude": current_latitude, "longitude": current_longitude},
        "bearing": current_bearing,
        "speed_limit": navigation_speed,
        "source": "NOO",
        "road_name": road_name,
        "road_width": road_width
      })

    elif mapbox_speed and road_name and road_width:
      add_entry(self.dataset_additions, {
        "start_coordinates": self.previous_coords,
        "end_coordinates": {"latitude": current_latitude, "longitude": current_longitude},
        "bearing": current_bearing,
        "speed_limit": mapbox_speed,
        "source": "Mapbox",
        "road_name": road_name,
        "road_width": road_width
      })

    elif dashboard_speed and road_name and road_width:
      add_entry(self.dataset_additions, {
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

    points = [
      (start_latitude, start_longitude),
      start_left,
      start_right
    ]
    min_latitude = min(pt[0] for pt in points)
    max_latitude = max(pt[0] for pt in points)
    min_longitude = min(pt[1] for pt in points)
    max_longitude = max(pt[1] for pt in points)

    query = (
      f"[out:json]; "
      f"way({min_latitude},{min_longitude},{max_latitude},{max_longitude})[highway~'{road_types}']; "
      f"out body; >; out skel qt;"
    )

    for api_url in [OVERPASS_API_URL]:
      try:
        response = requests.get(api_url, params={"data": query}, timeout=10)
        if response.status_code == 429:
          time.sleep(5)
          continue

        response.raise_for_status()
        data = response.json()
        ways = [element for element in data.get("elements", []) if element.get("type") == "way"]

        if ways:
          return [{
            "maxspeed": way.get("tags", {}).get("maxspeed"),
            "road_name": way.get("tags", {}).get("name"),
            "segment_id": way.get("id")
          } for way in ways]
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
        response.raise_for_status()

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
    bearing = entry.get("bearing", 0)
    start_coords = entry.get("start_coordinates")
    end_coords = entry.get("end_coordinates")

    segments = self.fetch_segments_from_overpass(start_coords, end_coords, bearing, entry.get("road_width"))
    return entry, segments

  def process_vetted_entry(self, entry):
    last_vetted_date = datetime.fromisoformat(entry.get("last_vetted"))

    if datetime.now(timezone.utc) - last_vetted_date < timedelta(days=7):
      return entry
    else:
      if self.fetch_speed_limit_for_segment_id(entry.get("segment_id")) is None:
        entry["last_vetted"] = datetime.now(timezone.utc).isoformat()
        return entry

    return None

  def update_entries(self):
    dataset = cleanup_dataset(json.loads(params.get("SpeedLimits") or "[]"))
    if not dataset:
      return

    current_entry = None

    merged_dataset = []
    for entry in dataset:
      if current_entry is None:
        current_entry = entry
      else:
        if entry.get("road_name") == current_entry.get("road_name") and entry.get("speed_limit") == current_entry.get("speed_limit"):
          current_end = current_entry.get("end_coordinates")
          new_start = entry.get("start_coordinates")

          current_end_latitude = math.radians(current_end["latitude"])
          current_end_longitude = math.radians(current_end["longitude"])

          new_start_latitude = math.radians(new_start["latitude"])
          new_start_longitude = math.radians(new_start["longitude"])

          distance = calculate_distance_to_point(current_end_latitude, current_end_longitude, new_start_latitude, new_start_longitude)

          current_bearing = current_entry.get("bearing")
          new_bearing = entry.get("bearing")
          bearing_diff = abs((abs(current_bearing - new_bearing) + 180) % 360 - 180)

          if bearing_diff < BEARING_THRESHOLD and distance < DISTANCE_THRESHOLD:
            current_entry["end_coordinates"] = entry.get("end_coordinates")
          else:
            merged_dataset.append(current_entry)
            current_entry = entry
        else:
          merged_dataset.append(current_entry)
          current_entry = entry

    if current_entry is not None:
      merged_dataset.append(current_entry)

    params.put("SpeedLimits", json.dumps(list(cleanup_dataset(merged_dataset))))

  def update_speed_limits(self):
    #self.update_entries()

    while not system_time_valid():
      self.sm.update()

      if self.sm["deviceState"].started:
        return

      time.sleep(60)

    while not is_url_pingable("https://overpass-api.de"):
      self.sm.update()

      if self.sm["deviceState"].started:
        return

      time.sleep(60)

    filtered_dataset = cleanup_dataset(json.loads(params.get("SpeedLimitsFiltered") or "[]"))

    filtered_vetted = deque(maxlen=MAX_ENTRIES)

    with ThreadPoolExecutor(max_workers=10) as executor:
      futures = {}
      for entry in filtered_dataset:
        self.sm.update()

        if self.sm["deviceState"].started:
          return

        future = executor.submit(self.process_vetted_entry, entry)
        futures[future] = entry

      for future in as_completed(futures):
        result = future.result()
        if result is not None:
          filtered_vetted.append(result)

    filtered_dataset = cleanup_dataset(filtered_vetted)
    params.put("SpeedLimitsFiltered", json.dumps(list(filtered_dataset)))

    dataset = cleanup_dataset(json.loads(params.get("SpeedLimits") or "[]"))
    if not dataset:
      self.speed_limits_checked = True
      return

    existing_segment_ids = {entry["segment_id"] for entry in filtered_dataset if "segment_id" in entry}

    with ThreadPoolExecutor(max_workers=10) as executor:
      futures = {}
      for entry in dataset:
        self.sm.update()

        if self.sm["deviceState"].started:
          break

        future = executor.submit(self.process_entry, entry)
        futures[future] = entry

      for count, future in enumerate(as_completed(futures), start=1):
        print(f"Processing entry {count}/{len(futures)}")
        entry, segments = future.result()

        if segments is None:
          continue

        dataset.remove(entry)

        for segment in segments:
          segment_id = segment["segment_id"]
          if segment_id in existing_segment_ids:
            continue

          if segment["road_name"] != entry.get("road_name"):
            continue

          if segment["maxspeed"]:
            existing_segment_ids.add(segment_id)
            continue

          add_entry(filtered_dataset, {
            "segment_id": segment_id,
            "source": entry.get("source"),
            "speed_limit": entry.get("speed_limit"),
            "last_vetted": datetime.now(timezone.utc).isoformat()
          })

        if count % 100 == 0:
          dataset = cleanup_dataset(dataset)
          filtered_dataset = cleanup_dataset(filtered_dataset)

          params.put("SpeedLimits", json.dumps(list(dataset)))
          params.put("SpeedLimitsFiltered", json.dumps(list(filtered_dataset)))

    dataset = cleanup_dataset(dataset)
    filtered_dataset = cleanup_dataset(filtered_dataset)

    params.put("SpeedLimits", json.dumps(list(dataset)))
    params.put("SpeedLimitsFiltered", json.dumps(list(filtered_dataset)))

    self.speed_limits_checked = not dataset

def main():
  logger = MapSpeedLogger()

  while True:
    try:
      if False:#not logger.speed_limits_checked:
        logger.update_speed_limits()

      logger.log_speed_limit()
    except Exception as error:
      print(f"Error in speed_limit_filler: {error}")
      sentry.capture_exception(error)

if __name__ == "__main__":
  main()
