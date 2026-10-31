#!/usr/bin/env python3
import capnp
import datetime
import gzip
import hashlib
import json
import math
import os
import requests

from openpilot.common.realtime import DT_MDL

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.common.frogpilot_api import FrogPilotAPIError

ENDPOINT = "/v1/speed-limits"
IMAGES_ENDPOINT = "/v1/speed-limit-images"

MAX_CONDITIONAL_TILES = 2

MAX_SHARED_SPEED_LIMIT = 40
MAX_SHARED_SPEED_LIMITS = 25000

OFFLINE_MAPS = capnp.load(str(frogpilot_paths.OFFLINE_MAPS_SCHEMA_PATH))

class SpeedLimitFiller:
  def __init__(self):
    self.changed = False
    self.parked = False

    self.map_speed_limit = 0

    self.distances = {}
    self.finished_ways = {}
    self.first_seen = {}

    self.conditional_ways = {}

    self.way = None
    self.way_tile = None

    data = frogpilot_utilities.load_json_file(frogpilot_paths.SPEED_LIMIT_FILLER_PATH)
    self.confirmations = {(entry["way_id"], entry["is_forward"]): entry for entry in data.get("confirmations", [])}
    self.speed_limits = {(entry["way_id"], entry["is_forward"]): entry for entry in data.get("speed_limits", [])}

  def finish_way(self):
    conditional_ways = self.conditional_ways.get(self.way_tile)

    if conditional_ways is not None and self.way in conditional_ways:
      self.distances = {}
      self.first_seen = {}

      self.remove_speed_limit(self.way)
      return

    if not self.distances or conditional_ways is None:
      self.distances = {}
      self.first_seen = {}
      return

    speed_limit = max(self.distances, key=self.distances.get)

    entry = {
      "distance": round(self.distances[speed_limit]),
      "is_forward": self.way[1],
      "speed_limit": speed_limit,
      "way_id": self.way[0],
    }

    if speed_limit in self.first_seen:
      entry["month"] = datetime.datetime.now(datetime.UTC).strftime("%Y-%m")
      entry.update(self.first_seen[speed_limit])

    self.distances = {}
    self.first_seen = {}

    if abs(speed_limit - self.map_speed_limit) < 1:
      if "month" in entry:
        confirmation = {key: entry[key] for key in ("is_forward", "month", "speed_limit", "way_id")}
        if self.confirmations.get(self.way) != confirmation:
          self.confirmations.pop(self.way, None)
          self.confirmations[self.way] = confirmation
          self.changed = True

          if len(self.confirmations) > MAX_SHARED_SPEED_LIMITS:
            self.confirmations.pop(next(iter(self.confirmations)))

      self.remove_speed_limit(self.way)
      return

    previous = self.speed_limits.get(self.way, {})

    replaced = previous.get("speed_limit") != speed_limit
    if replaced or previous.get("month") != entry.get("month") or previous.get("distance", 0) < entry["distance"]:
      self.speed_limits.pop(self.way, None)
      self.speed_limits[self.way] = entry
      self.changed = True

      if len(self.speed_limits) > MAX_SHARED_SPEED_LIMITS:
        self.remove_speed_limit(next(iter(self.speed_limits)))

    if self.way in self.finished_ways:
      replaced |= self.finished_ways[self.way][1]

    if "month" in entry:
      self.finished_ways[self.way] = (speed_limit, replaced)
    else:
      self.finished_ways[self.way] = (None, replaced)

  def get_speed_limit(self, way, map_speed_limit):
    entry = self.speed_limits.get(way)
    if entry is None:
      return map_speed_limit

    return entry["speed_limit"]

  def load_conditional_ways(self, tile):
    latitude, longitude = tile

    path = frogpilot_paths.MAPS_PATH / f"{math.floor(latitude / 2) * 2}/{math.floor(longitude / 2) * 2}"
    path /= f"{latitude:.6f}_{longitude:.6f}_{latitude + 0.25:.6f}_{longitude + 0.25:.6f}"

    conditional_ways = set()
    if path.is_file():
      offline_maps = OFFLINE_MAPS.Offline.from_bytes_packed(path.read_bytes())

      for way in offline_maps.ways:
        if way.maxSpeedConditional or way.maxSpeedForwardConditional:
          conditional_ways.add((way.id, not way.osmNodeOrderReversed))

        if way.maxSpeedConditional or way.maxSpeedBackwardConditional:
          conditional_ways.add((way.id, way.osmNodeOrderReversed))

    self.conditional_ways[tile] = conditional_ways

  def remove_speed_limit(self, way):
    if self.speed_limits.pop(way, None) is not None:
      self.changed = True

    self.finished_ways[way] = (None, True)

  def save(self):
    self.finish_way()

    frogpilot_utilities.join_thread("write_speed_limits")

    if self.changed or self.finished_ways:
      self.start_write()

      frogpilot_utilities.join_thread("write_speed_limits")

    for path in frogpilot_paths.SPEED_LIMIT_FILLER_PENDING_IMAGES_PATH.glob("*"):
      path.unlink(missing_ok=True)

  def start_write(self):
    if self.changed:
      data = {"confirmations": list(self.confirmations.values()), "speed_limits": list(self.speed_limits.values())}
    else:
      data = None

    frogpilot_utilities.run_thread_with_lock("write_speed_limits", write_speed_limits, (data, self.finished_ways))

    self.changed = False
    self.finished_ways = {}

  def update(self, gps_position, map_match, v_ego, sm, frogpilot_toggles):
    if sm["frogpilotCarState"].parked and not self.parked:
      self.finish_way()

    self.parked = sm["frogpilotCarState"].parked

    if (self.changed or self.finished_ways) and not frogpilot_utilities.is_thread_alive("write_speed_limits"):
      self.start_write()

    if not map_match:
      return

    tile = (math.floor(gps_position["latitude"] * 4) / 4, math.floor(gps_position["longitude"] * 4) / 4)
    if tile not in self.conditional_ways and not frogpilot_utilities.is_thread_alive("load_conditional_ways"):
      self.conditional_ways[tile] = None
      if len(self.conditional_ways) > MAX_CONDITIONAL_TILES:
        self.conditional_ways.pop(next(iter(self.conditional_ways)))

      frogpilot_utilities.run_thread_with_lock("load_conditional_ways", self.load_conditional_ways, (tile,))

    way = (map_match["way_id"], map_match["is_forward"])
    if way != self.way:
      self.finish_way()

      self.way = way
      self.way_tile = tile

    conditional_ways = self.conditional_ways.get(self.way_tile)
    if conditional_ways is not None and way in conditional_ways:
      if way in self.speed_limits:
        self.remove_speed_limit(way)
      return

    self.map_speed_limit = map_match["speed_limit"]

    if sm.alive["frogpilotSignReading"]:
      camera_speed_limit = sm["frogpilotSignReading"].speedLimit
    else:
      camera_speed_limit = 0

    if camera_speed_limit >= 1:
      self.distances[camera_speed_limit] = self.distances.get(camera_speed_limit, 0) + v_ego * DT_MDL

      if frogpilot_toggles.frogpilot_telemetry and not frogpilot_toggles.uploads_disabled:
        self.first_seen.setdefault(camera_speed_limit, {
          "bearing": gps_position["bearing"] % 360,
          "latitude": gps_position["latitude"],
          "longitude": gps_position["longitude"],
        })

def upload_speed_limits(params, frogpilot_api):
  if not frogpilot_paths.SPEED_LIMIT_FILLER_PATH.is_file():
    return

  data = frogpilot_utilities.load_json_file(frogpilot_paths.SPEED_LIMIT_FILLER_PATH)

  entries = []
  for entry in data.get("speed_limits", []):
    if "month" in entry and entry["speed_limit"] <= MAX_SHARED_SPEED_LIMIT:
      entries.append(entry)

  speed_limits = []
  for entry in sorted(entries, key=lambda entry: (entry["way_id"], entry["is_forward"])):
    speed_limits.append({
      "bearing": entry["bearing"],
      "distance": entry["distance"],
      "is_forward": entry["is_forward"],
      "latitude": entry["latitude"],
      "longitude": entry["longitude"],
      "observed": f"{entry['month']}-01",
      "segment_id": entry["way_id"],
      "source": "Camera",
      "speed_limit": entry["speed_limit"],
    })

  entries = []
  for entry in data.get("confirmations", []):
    if "month" in entry and entry["speed_limit"] <= MAX_SHARED_SPEED_LIMIT:
      entries.append(entry)

  confirmations = []
  for entry in sorted(entries, key=lambda entry: (entry["way_id"], entry["is_forward"])):
    confirmations.append({
      "is_forward": entry["is_forward"],
      "observed": f"{entry['month']}-01",
      "segment_id": entry["way_id"],
      "speed_limit": entry["speed_limit"],
    })

  body = json.dumps({"confirmations": confirmations, "schema_version": 1, "speed_limits": speed_limits}, separators=(",", ":")).encode()
  body_hash = hashlib.sha256(body).hexdigest()

  if body_hash != params.get("SpeedLimitFillerUploadedHash", encoding="utf-8"):
    response = frogpilot_api.post(ENDPOINT, data=gzip.compress(body, compresslevel=9, mtime=0), headers={
      "Content-Encoding": "gzip",
      "Content-Type": "application/json",
    }, timeout=30)
    if response is None or not 200 <= response.status_code < 300:
      return

    params.put("SpeedLimitFillerUploadedHash", body_hash)

  shared_speed_limits = {(entry["segment_id"], entry["is_forward"]): entry["speed_limit"] for entry in speed_limits}

  for image_path in sorted(frogpilot_paths.SPEED_LIMIT_FILLER_IMAGES_PATH.glob("*.jpg")):
    if params.get_bool("IsOnroad"):
      return

    try:
      uploaded = os.getxattr(image_path, frogpilot_variables.UPLOAD_ATTR_NAME) == frogpilot_variables.UPLOAD_DONE
    except OSError:
      uploaded = False

    if uploaded:
      continue

    way_id, is_forward = image_path.stem.split("_")
    way = (int(way_id), is_forward == "1")

    if way not in shared_speed_limits:
      continue

    capture = frogpilot_utilities.load_json_file(image_path.with_suffix(".json"))
    if "month" not in capture:
      continue

    month_start = datetime.datetime.strptime(capture["month"], "%Y-%m").replace(tzinfo=datetime.UTC)

    with open(image_path, "rb") as image:
      image_data = image.read()

      descriptor = {
        "bearing": capture["bearing"],
        "captured_at": int(month_start.timestamp() * 1000),
        "is_forward": way[1],
        "latitude": capture["latitude"],
        "longitude": capture["longitude"],
        "segment_id": way[0],
        "sha256": hashlib.sha256(image_data).hexdigest(),
        "size_bytes": len(image_data),
        "speed_limit": shared_speed_limits[way],
      }
      response = frogpilot_api.post(IMAGES_ENDPOINT, json=descriptor, timeout=30)
      if response is None or not 200 <= response.status_code < 300:
        return

      upload = response.json().get("upload")
      if upload is not None:
        try:
          frogpilot_api.put_upload(upload, image_data, image_path.name, "speed_limit_image")
        except (FrogPilotAPIError, requests.exceptions.RequestException):
          return

      os.setxattr(image.fileno(), frogpilot_variables.UPLOAD_ATTR_NAME, frogpilot_variables.UPLOAD_DONE)

def write_speed_limits(data, finished_ways):
  if data is not None:
    frogpilot_utilities.update_json_file(frogpilot_paths.SPEED_LIMIT_FILLER_PATH, data)

  for (way_id, is_forward), (speed_limit, replaced) in finished_ways.items():
    image_path = frogpilot_paths.SPEED_LIMIT_FILLER_IMAGES_PATH / f"{way_id}_{int(is_forward)}.jpg"

    if speed_limit is None:
      image_path.unlink(missing_ok=True)
      image_path.with_suffix(".json").unlink(missing_ok=True)
    else:
      pending_path = frogpilot_paths.SPEED_LIMIT_FILLER_PENDING_IMAGES_PATH / f"{way_id}_{int(is_forward)}_{round(speed_limit * 100)}.jpg"
      if pending_path.is_file() and pending_path.with_suffix(".json").is_file():
        os.replace(pending_path.with_suffix(".json"), image_path.with_suffix(".json"))
        os.replace(pending_path, image_path)
      elif replaced:
        image_path.unlink(missing_ok=True)
        image_path.with_suffix(".json").unlink(missing_ok=True)

    for path in frogpilot_paths.SPEED_LIMIT_FILLER_PENDING_IMAGES_PATH.glob(f"{way_id}_{int(is_forward)}_*"):
      if path.suffix != ".tmp":
        path.unlink()
