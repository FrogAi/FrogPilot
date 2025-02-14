#!/usr/bin/env python3
import json
import math
import requests

from collections import deque

import openpilot.system.sentry as sentry

from cereal import log, messaging

from openpilot.selfdrive.frogpilot.frogpilot_utilities import calculate_distance_to_point, is_url_pingable
from openpilot.selfdrive.frogpilot.frogpilot_variables import params, params_memory

GPS_SEARCH_RANGE = 0.0001
MAX_ENTRIES = 10_000_000

OVERPASS_API_URL = "http://overpass-api.de/api/interpreter"

def add_entry(dataset, entry):
  dataset.append(entry)

class MapSpeedLogger:
  def __init__(self):
    self.speed_limits_checked = False
    self.started_previously = False

    self.previous_coords = None

    self.dataset = deque(json.loads(params.get("SpeedLimits") or "[]"), maxlen=MAX_ENTRIES)
    self.filtered_dataset = deque(json.loads(params.get("SpeedLimitsFiltered") or "[]"), maxlen=MAX_ENTRIES)

    self.sm = messaging.SubMaster(["deviceState", "frogpilotCarState", "frogpilotNavigation", "liveLocationKalman"])

  def log_speed_limit(self):
    self.sm.update()

    if not self.sm["deviceState"].started and self.started_previously:
      params.put("SpeedLimits", json.dumps(list(self.dataset)))
      self.speed_limits_checked = False
      self.previous_coords = None

    self.started_previously = self.sm["deviceState"].started

    if not self.sm.updated["liveLocationKalman"]:
      return

    localizer_valid = (self.sm['liveLocationKalman'].status == log.LiveLocationKalman.Status.valid) and self.sm['liveLocationKalman'].positionGeodetic.valid
    if not (self.sm['liveLocationKalman'].gpsOK and localizer_valid):
      self.previous_coords = None
      return

    if params_memory.get_float("MapSpeedLimit") != 0:
      self.previous_coords = None
      return

    current_latitude = self.sm["liveLocationKalman"].positionGeodetic.value[0]
    current_longitude = self.sm["liveLocationKalman"].positionGeodetic.value[1]

    if self.previous_coords != None:
      start_latitude, start_longitude = map(math.radians, [self.previous_coords["latitude"], self.previous_coords["longitude"]])
      end_latitude, end_longitude = map(math.radians, [current_latitude, current_longitude])
      distance = calculate_distance_to_point(start_latitude, start_longitude, end_latitude, end_longitude)

      if distance < 1:
        return
    else:
      self.previous_coords = {"latitude": current_latitude, "longitude": current_longitude}
      return

    dashboard_speed = self.sm["frogpilotCarState"].dashboardSpeedLimit
    navigation_speed = self.sm["frogpilotNavigation"].navigationSpeedLimit

    if dashboard_speed:
      add_entry(self.dataset, {
        "start_coordinates": self.previous_coords,
        "end_coordinates": {"latitude": current_latitude, "longitude": current_longitude},
        "speed_limit": dashboard_speed,
        "source": "Dashboard"
      })

    elif navigation_speed:
      add_entry(self.dataset, {
        "start_coordinates": self.previous_coords,
        "end_coordinates": {"latitude": current_latitude, "longitude": current_longitude},
        "speed_limit": navigation_speed,
        "source": "NOO"
      })

    self.previous_coords = {"latitude": current_latitude, "longitude": current_longitude}

  def fetch_segments_from_overpass(self, start_coords, end_coords):
    road_types = "(motorway|motorway_link|primary|primary_link|residential|secondary|secondary_link|tertiary|tertiary_link|trunk|trunk_link)"

    min_lat = min(start_coords.get("latitude"), end_coords.get("latitude")) - GPS_SEARCH_RANGE
    max_lat = max(start_coords.get("latitude"), end_coords.get("latitude")) + GPS_SEARCH_RANGE
    min_lon = min(start_coords.get("longitude"), end_coords.get("longitude")) - GPS_SEARCH_RANGE
    max_lon = max(start_coords.get("longitude"), end_coords.get("longitude")) + GPS_SEARCH_RANGE

    for attempt in range(10):
      query = (
        f"[out:json]; "
        f"way({min_lat},{min_lon},{max_lat},{max_lon})"
        f"[highway~'{road_types}']; "
        f"out body; >; out skel qt;"
      )

      try:
        response = requests.get(OVERPASS_API_URL, params={"data": query}, timeout=10)
        response.raise_for_status()

        data = response.json()
        ways = [element for element in data.get("elements", []) if element.get("type") == "way"]

        if ways:
          segments = []
          for segment in ways:
            segment_id = segment.get("id")
            maxspeed = segment.get("tags", {}).get("maxspeed")

            try:
              speed_limit = int(maxspeed.split()[0]) if maxspeed else None
            except (ValueError, AttributeError):
              speed_limit = None
            segments.append((segment_id, speed_limit))
          return segments

      except Exception as e:
        print(f"Unexpected error: {e}")
        return None

      min_lat -= GPS_SEARCH_RANGE
      max_lat += GPS_SEARCH_RANGE
      min_lon -= GPS_SEARCH_RANGE
      max_lon += GPS_SEARCH_RANGE

    return None

  def fetch_speed_limit_for_segment_id(self, segment_id):
    query = f"[out:json]; way({segment_id}); out body;"

    try:
      response = requests.get(OVERPASS_API_URL, params={"data": query}, timeout=10)
      response.raise_for_status()

      data = response.json()
      ways = [element for element in data.get("elements", []) if element.get("type") == "way"]
      maxspeed = ways[0].get("tags", {}).get("maxspeed")

      try:
        speed_limit = int(maxspeed.split()[0]) if maxspeed else None
      except (ValueError, AttributeError):
        speed_limit = None

      return speed_limit
    except Exception as e:
      print(f"Unexpected error while fetching speed limit for segment {segment_id}: {e}")
      return None

  def update_speed_limits(self):
    if not self.dataset:
      return

    filtered_cleaned = deque(maxlen=MAX_ENTRIES)
    for entry in self.filtered_dataset:
      self.sm.update()

      if self.sm["deviceState"].started:
        return

      segment_id = entry.get("segment_id")
      if segment_id:
        overpass_speed = self.fetch_speed_limit_for_segment_id(segment_id)
        if overpass_speed is not None:
          continue
        filtered_cleaned.append(entry)

    self.filtered_dataset = filtered_cleaned

    existing_segment_ids = {entry["segment_id"] for entry in self.filtered_dataset} if self.filtered_dataset else set()
    total_entries = len(self.dataset)

    for count, entry in enumerate(list(self.dataset), start=1):
      print(f"Processing entry {count}/{total_entries}")

      self.sm.update()

      if self.sm["deviceState"].started:
        break

      #self.dataset.remove(entry)

      start_coords = entry.get("start_coordinates")
      end_coords = entry.get("end_coordinates")
      if not start_coords or not end_coords:
        print("Skipping entry due to missing coordinates")
        continue

      result = self.fetch_segments_from_overpass(start_coords, end_coords)
      if result is not None:
        for segment_id, speed_limit in result:
          if not segment_id:
            print("Skipping entry because no segment ID was found")
            continue
          if speed_limit:
            print("Skipping entry because a speed limit was already present")
            continue
          if segment_id in existing_segment_ids:
            print(f"Skipping entry because segment ID {segment_id} is already in filtered dataset")
            continue

          print(f"Found segment ID: {segment_id} with speed limit: {speed_limit}")

          add_entry(self.filtered_dataset, {
            "segment_id": segment_id,
            "source": entry.get("source"),
            "speed_limit": entry.get("speed_limit"),
          })

          existing_segment_ids.add(segment_id)
      else:
        print("Skipping entry because no result was found")

      if count % 100 == 0:
        params.put("SpeedLimits", json.dumps(list(self.dataset)))
        params.put("SpeedLimitsFiltered", json.dumps(list(deque(sorted(self.filtered_dataset, key=lambda entry: entry["segment_id"]), maxlen=MAX_ENTRIES))))

    params.put("SpeedLimits", json.dumps(list(self.dataset)))
    params.put("SpeedLimitsFiltered", json.dumps(list(deque(sorted(self.filtered_dataset, key=lambda entry: entry["segment_id"]), maxlen=MAX_ENTRIES))))

    self.speed_limits_checked = True

def main():
  logger = MapSpeedLogger()

  while True:
    try:
      if not logger.speed_limits_checked and is_url_pingable("http://overpass-api.de"):
        logger.update_speed_limits()

      logger.log_speed_limit()
    except Exception as error:
      print(f"Error in speed_limit_filler: {error}")
      sentry.capture_exception(error)

if __name__ == "__main__":
  main()
