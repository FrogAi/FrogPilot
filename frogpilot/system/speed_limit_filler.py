#!/usr/bin/env python3
import csv
import json
import math
import requests
import time

from collections import deque

from cereal import log, messaging

from openpilot.common.conversions import Conversions as CV
from openpilot.frogpilot.common.frogpilot_utilities import calculate_distance_to_point, is_url_pingable
from openpilot.frogpilot.common.frogpilot_variables import EARTH_RADIUS, params, params_memory

MAX_SPEED_LIMITS = 1_000_000
SPEED_LIMIT_RECHECK_INTERVAL = 7 * 24 * 60 * 60

MAX_BEARING_DIFFERENCE = 30
METERS_PER_DEGREE = math.radians(1) * EARTH_RADIUS

OSM_QUERY_RADIUS = 500
OSM_SEARCH_RADIUS = 35
OSM_WAY_ID_BATCH_SIZE = 1_000

OSM_HIGHWAY_TYPES = "living_street|motorway|motorway_link|primary|secondary|tertiary|primary_link|secondary_link|tertiary_link|residential|service|trunk|trunk_link|unclassified"


def calculate_way_distance(speed_limit, way):
  latitude = speed_limit["latitude"]
  longitude = speed_limit["longitude"]
  bearing = speed_limit["bearing"]
  tags = way["tags"]

  longitude_scale = math.cos(math.radians(latitude))
  nodes = [((node["lon"] - longitude) * METERS_PER_DEGREE * longitude_scale, (node["lat"] - latitude) * METERS_PER_DEGREE) for node in way.get("geometry") or []]
  oneway = tags.get("oneway", "")
  bidirectional = not (
    oneway in ("-1", "1", "true", "yes")
    or (oneway not in ("0", "false", "no") and tags.get("highway") in ("motorway", "motorway_link", "trunk_link"))
    or tags.get("junction") in ("roundabout", "circular")
  )

  closest_distance = None
  for (ax, ay), (bx, by) in zip(nodes, nodes[1:]):
    delta_x = bx - ax
    delta_y = by - ay

    length_squared = delta_x * delta_x + delta_y * delta_y
    segment_bearing = (math.degrees(math.atan2(delta_x, delta_y)) + 360) % 360
    if oneway == "-1":
      segment_bearing = (segment_bearing + 180) % 360

    bearing_difference = abs((bearing - segment_bearing + 180) % 360 - 180)
    if bidirectional:
      bearing_difference = min(bearing_difference, 180 - bearing_difference)

    if not length_squared or bearing_difference > MAX_BEARING_DIFFERENCE:
      continue

    segment_fraction = min(1, max(0, -(ax * delta_x + ay * delta_y) / length_squared))
    distance = math.hypot(ax + segment_fraction * delta_x, ay + segment_fraction * delta_y)

    if closest_distance is None or distance < closest_distance:
      closest_distance = distance

  return closest_distance


def filtered_speed_limit_record(osm_way_id=0, speed_limit=0, last_checked=0):
  return {
    "last_checked": last_checked,
    "osm_way_id": osm_way_id,
    "speed_limit": speed_limit,
  }


def find_matching_way(speed_limit, ways):
  closest_way = None
  for way in ways:
    tags = way["tags"]
    if speed_limit["road_name"] not in (tags.get("name"), tags.get("ref")):
      continue

    distance = calculate_way_distance(speed_limit, way)
    if distance is None or distance > OSM_SEARCH_RADIUS:
      continue

    if closest_way is None or distance < closest_way[0]:
      closest_way = distance, way

  return closest_way[1] if closest_way else None


def find_osm_way(speed_limit, session, overpass_cache=None):
  query_radius = OSM_QUERY_RADIUS if overpass_cache is not None else OSM_SEARCH_RADIUS
  if overpass_cache is not None:
    cached_queries = overpass_cache.setdefault(speed_limit["road_name"], [])
    for latitude, longitude, elements in cached_queries:
      if calculate_distance_to_point(latitude, longitude, speed_limit["latitude"], speed_limit["longitude"]) <= query_radius - OSM_SEARCH_RADIUS:
        return find_matching_way(speed_limit, elements)

  road_name = json.dumps(speed_limit["road_name"])
  way_query = f"way(around:{query_radius},{speed_limit['latitude']},{speed_limit['longitude']})[highway~\"^({OSM_HIGHWAY_TYPES})$\"]"
  query = f"[out:json][timeout:10];({way_query}[name={road_name}];{way_query}[ref={road_name}];);out tags geom;"

  response = overpass_response(query, session)
  if response is None:
    return None

  try:
    data = response.json()
  except ValueError:
    return None

  if data.get("remark"):
    return None

  elements = data.get("elements", [])
  if overpass_cache is not None:
    cached_queries.append((speed_limit["latitude"], speed_limit["longitude"], elements))
  return find_matching_way(speed_limit, elements)


def load_records(raw, keys):
  try:
    records = json.loads(raw or "[]")
  except (TypeError, ValueError):
    return []
  if not isinstance(records, list):
    return []
  return [record for record in records if isinstance(record, dict) and keys.issubset(record)]


def maxspeed_matches(speed_limit, maxspeed):
  maxspeed = (maxspeed or "").split()
  if not maxspeed:
    return False

  if not maxspeed[0].isdecimal():
    return True

  conversion = CV.MS_TO_MPH if maxspeed[-1] == "mph" else CV.MS_TO_KPH
  return round(speed_limit * conversion) == int(maxspeed[0])


def overpass_response(query, session):
  try:
    response = session.post(
      "https://overpass-api.de/api/interpreter",
      data={"data": query},
      timeout=10,
    )
    response.raise_for_status()
    return response
  except requests.RequestException:
    return None


def remove_fixed_speed_limits(speed_limits, session, now):
  osm_way_ids = [
    osm_way_id for osm_way_id, speed_limit in speed_limits.items()
    if now - speed_limit["last_checked"] >= SPEED_LIMIT_RECHECK_INTERVAL
  ]

  for index in range(0, len(osm_way_ids), OSM_WAY_ID_BATCH_SIZE):
    osm_way_id_batch = osm_way_ids[index:index + OSM_WAY_ID_BATCH_SIZE]
    query = f"[out:csv(::id,maxspeed,'maxspeed:forward','maxspeed:backward';false)][timeout:10];way(id:{','.join(map(str, osm_way_id_batch))});out;"
    response = overpass_response(query, session)
    if response is None:
      continue

    for osm_way_id in osm_way_id_batch:
      speed_limits[osm_way_id]["last_checked"] = now

    for row in csv.reader(response.text.splitlines(), delimiter="\t"):
      if not row or not row[0].isdigit():
        continue

      maxspeeds = row[1:]
      osm_way_id = int(row[0])
      speed_limit = speed_limits.get(osm_way_id, {}).get("speed_limit")

      if speed_limit is not None and any(maxspeed_matches(speed_limit, maxspeed) for maxspeed in maxspeeds):
        del speed_limits[osm_way_id]


def speed_limit_record(bearing=0, latitude=0, longitude=0, road_name="", speed_limit=0):
  return {
    "bearing": bearing,
    "latitude": latitude,
    "longitude": longitude,
    "road_name": road_name,
    "speed_limit": speed_limit,
  }


def speed_limit_records(speed_limits_by_way_id):
  records = []
  for osm_way_id, speed_limit in speed_limits_by_way_id.items():
    if speed_limit["speed_limit"] is not None:
      records.append(filtered_speed_limit_record(osm_way_id, **speed_limit))
  return records


def speed_limits_match(speed_limit1, speed_limit2):
  return abs(speed_limit1 - speed_limit2) < 1


class SpeedLimitFiller:
  def __init__(self):
    self.filtered_previously = False
    self.started_previously = False

    self.sm = messaging.SubMaster(["deviceState", "frogpilotCarState", "frogpilotNavigation", "frogpilotPlan", "liveLocationKalman"], poll="liveLocationKalman")

  def filter_speed_limits(self):
    now = int(time.time())

    speed_limits = load_records(params.get("SpeedLimits"), set(speed_limit_record()))
    speed_limits_filtered = load_records(params.get("SpeedLimitsFiltered"), set(filtered_speed_limit_record()) - {"last_checked"})

    filtered_speed_limits = {
      speed_limit["osm_way_id"]: {
        "last_checked": speed_limit.get("last_checked", 0),
        "speed_limit": speed_limit["speed_limit"],
      }
      for speed_limit in speed_limits_filtered
    }

    overpass_cache = {}
    unfiltered_speed_limits = []
    with requests.Session() as session:
      session.headers.update({"User-Agent": "FrogPilot-SpeedLimitFiller/1.0"})

      remove_fixed_speed_limits(filtered_speed_limits, session, now)

      for index, speed_limit in enumerate(speed_limits):
        self.sm.update(0)

        if self.sm["deviceState"].started:
          unfiltered_speed_limits.extend(speed_limits[index:])
          break

        osm_way = find_osm_way(speed_limit, session, overpass_cache)
        if osm_way is None:
          unfiltered_speed_limits.append(speed_limit)
          continue

        source_speed_limit = speed_limit["speed_limit"]
        matching_maxspeed_exists = any(maxspeed_matches(source_speed_limit, osm_way["tags"].get(tag)) for tag in ("maxspeed", "maxspeed:backward", "maxspeed:forward"))
        if matching_maxspeed_exists:
          continue

        osm_way_id = osm_way["id"]
        existing_speed_limit = filtered_speed_limits.get(osm_way_id)
        if existing_speed_limit is None:
          filtered_speed_limits[osm_way_id] = {"last_checked": now, "speed_limit": source_speed_limit}
        elif (existing_speed_limit["speed_limit"] is not None and not speed_limits_match(existing_speed_limit["speed_limit"], source_speed_limit)):
          existing_speed_limit["speed_limit"] = None

    params.put("SpeedLimits", json.dumps(unfiltered_speed_limits))
    params.put("SpeedLimitsFiltered", json.dumps(speed_limit_records(filtered_speed_limits)))

  def log_speed_limit(self):
    source_speed_limit = next(
      (speed_limit for speed_limit in (
        self.sm["frogpilotCarState"].dashboardSpeedLimit,
        self.sm["frogpilotPlan"].slcMapboxSpeedLimit,
        self.sm["frogpilotNavigation"].navigationSpeedLimit,
      ) if speed_limit >= 1),
      0,
    )
    if source_speed_limit < 1:
      return

    location = self.sm["liveLocationKalman"]
    if not location.gpsOK or location.status != log.LiveLocationKalman.Status.valid or not location.positionGeodetic.valid:
      return

    bearing = math.degrees(location.calibratedOrientationNED.value[2])
    latitude, longitude = location.positionGeodetic.value[0], location.positionGeodetic.value[1]

    if self.logged_position is not None and calculate_distance_to_point(*self.logged_position, latitude, longitude) < 1:
      return

    road_name = params_memory.get("RoadName", encoding="utf-8")
    if not road_name:
      return

    map_speed_limit = params_memory.get_float("MapSpeedLimit")
    if map_speed_limit >= 1 and speed_limits_match(map_speed_limit, source_speed_limit):
      return

    self.new_speed_limits.append(speed_limit_record(bearing, latitude, longitude, road_name, source_speed_limit))

    self.logged_position = (latitude, longitude)

  def update(self):
    self.sm.update()

    started = self.sm["deviceState"].started

    if started and not self.started_previously:
      self.new_speed_limits = deque(maxlen=MAX_SPEED_LIMITS)

      self.logged_position = None
    elif started and self.sm.updated["liveLocationKalman"]:
      self.log_speed_limit()
    elif not started and self.started_previously:
      if self.new_speed_limits:
        speed_limits = deque(load_records(params.get("SpeedLimits"), set(speed_limit_record())), maxlen=MAX_SPEED_LIMITS)
        speed_limits.extend(self.new_speed_limits)
        params.put("SpeedLimits", json.dumps(list(speed_limits)))

      self.filtered_previously = False
    elif not started and not self.filtered_previously:
      if is_url_pingable("https://overpass-api.de/api/status"):
        self.filter_speed_limits()

        self.filtered_previously = True

    self.started_previously = started


def main():
  speed_limit_filler = SpeedLimitFiller()

  while True:
    speed_limit_filler.update()


if __name__ == "__main__":
  main()
