#!/usr/bin/env python3
import datetime
import hashlib
import json
import os
import requests
import xattr

from openpilot.common.realtime import DT_MDL

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.common.frogpilot_api import FrogPilotAPIError

ENDPOINT = "/v1/speed-limits"
IMAGES_ENDPOINT = "/v1/speed-limit-images"

MAX_SHARED_SPEED_LIMIT = 40
MAX_SHARED_SPEED_LIMITS = 25000

class SpeedLimitFiller:
  def __init__(self):
    self.changed = False
    self.conditional = False
    self.parked = False

    self.map_speed_limit = 0

    self.distances = {}
    self.finished_ways = {}
    self.first_seen = {}

    self.way = None

    self.thread_manager = frogpilot_utilities.ThreadManager()

    data = frogpilot_utilities.load_json_file(frogpilot_paths.SPEED_LIMIT_FILLER_PATH)
    self.confirmations = {(entry["way_id"], entry["is_forward"]): entry for entry in data.get("confirmations", [])}
    self.speed_limits = {(entry["way_id"], entry["is_forward"]): entry for entry in data.get("speed_limits", [])}

  def finish_way(self):
    if self.conditional:
      self.remove_speed_limit(self.way)
      return

    if not self.distances:
      return

    speed_limit = max(self.distances, key=self.distances.get)

    entry = {
      "distance": round(self.distances[speed_limit]),
      "is_forward": self.way[1],
      "observed": datetime.datetime.now(datetime.UTC).date().isoformat(),
      "speed_limit": speed_limit,
      "way_id": self.way[0],
      **self.first_seen[speed_limit],
    }

    self.distances = {}
    self.first_seen = {}

    if abs(speed_limit - self.map_speed_limit) < 1:
      confirmation = {key: entry[key] for key in ("is_forward", "observed", "speed_limit", "way_id")}
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
    if replaced or previous.get("observed") != entry["observed"]:
      self.speed_limits.pop(self.way, None)
      self.speed_limits[self.way] = entry
      self.changed = True

      if len(self.speed_limits) > MAX_SHARED_SPEED_LIMITS:
        self.remove_speed_limit(next(iter(self.speed_limits)))

    if self.way in self.finished_ways:
      replaced |= self.finished_ways[self.way][1]

    self.finished_ways[self.way] = (speed_limit, replaced)

  def get_speed_limit(self, map_match):
    entry = self.speed_limits.get((map_match.wayId, map_match.isForward))
    if entry is None:
      return map_match.speedLimit

    return entry["speed_limit"]

  def remove_speed_limit(self, way):
    if self.speed_limits.pop(way, None) is not None:
      self.changed = True

    self.finished_ways[way] = (None, True)

  def save(self):
    self.finish_way()

    self.thread_manager.join_thread("write_speed_limits")

    if self.changed or self.finished_ways:
      self.start_write()

      self.thread_manager.join_thread("write_speed_limits")

    for path in frogpilot_paths.SPEED_LIMIT_FILLER_PENDING_IMAGES_PATH.glob("*"):
      path.unlink(missing_ok=True)

  def start_write(self):
    if self.changed:
      data = {"confirmations": list(self.confirmations.values()), "speed_limits": list(self.speed_limits.values())}
    else:
      data = None

    self.thread_manager.run_in_thread(write_speed_limits, (data, self.finished_ways))

    self.changed = False
    self.finished_ways = {}

  def update(self, gps_position, v_ego, sm):
    if sm["frogpilotCarState"].parked and not self.parked:
      self.finish_way()

    self.parked = sm["frogpilotCarState"].parked

    if (self.changed or self.finished_ways) and not self.thread_manager.is_thread_alive("write_speed_limits"):
      self.start_write()

    if not frogpilot_utilities.is_mapd_data_valid(gps_position is not None, sm):
      return

    map_match = sm["mapdOut"]

    way = (map_match.wayId, map_match.isForward)
    if way != self.way:
      self.finish_way()

      self.conditional = map_match.conditionalSpeedLimit != ""
      self.way = way

      if self.conditional:
        self.remove_speed_limit(way)

    if self.conditional:
      return

    self.map_speed_limit = map_match.speedLimit

    if sm.alive["frogpilotSignReading"]:
      camera_speed_limit = sm["frogpilotSignReading"].speedLimit
    else:
      camera_speed_limit = 0

    if camera_speed_limit >= 1:
      self.distances[camera_speed_limit] = self.distances.get(camera_speed_limit, 0) + v_ego * DT_MDL
      self.first_seen.setdefault(camera_speed_limit, gps_position)

def upload_speed_limits(params, frogpilot_api):
  if not frogpilot_paths.SPEED_LIMIT_FILLER_PATH.is_file():
    return

  data = frogpilot_utilities.load_json_file(frogpilot_paths.SPEED_LIMIT_FILLER_PATH)

  entries = []
  for entry in data.get("speed_limits", []):
    if "observed" in entry and entry["speed_limit"] <= MAX_SHARED_SPEED_LIMIT:
      entries.append(entry)

  speed_limits = []
  for entry in sorted(entries, key=lambda entry: (entry["way_id"], entry["is_forward"])):
    speed_limits.append({
      "bearing": entry["bearing"],
      "distance": entry["distance"],
      "is_forward": entry["is_forward"],
      "latitude": entry["latitude"],
      "longitude": entry["longitude"],
      "observed": entry["observed"],
      "segment_id": entry["way_id"],
      "source": "Camera",
      "speed_limit": entry["speed_limit"],
    })

  entries = []
  for entry in data.get("confirmations", []):
    if entry["speed_limit"] <= MAX_SHARED_SPEED_LIMIT:
      entries.append(entry)

  confirmations = []
  for entry in sorted(entries, key=lambda entry: (entry["way_id"], entry["is_forward"])):
    confirmations.append({"is_forward": entry["is_forward"], "observed": entry["observed"], "segment_id": entry["way_id"], "speed_limit": entry["speed_limit"]})

  body = json.dumps({"confirmations": confirmations, "schema_version": 1, "speed_limits": speed_limits}, separators=(",", ":")).encode()
  body_hash = hashlib.sha256(body).hexdigest()

  if body_hash != params.get("SpeedLimitFillerUploadedHash"):
    response = frogpilot_api.post_gzip(ENDPOINT, body, timeout=30)
    if not frogpilot_api.succeeded(response):
      return

    params.put("SpeedLimitFillerUploadedHash", body_hash)

  shared_speed_limits = {(entry["segment_id"], entry["is_forward"]): entry["speed_limit"] for entry in speed_limits}

  for image_path in sorted(frogpilot_paths.SPEED_LIMIT_FILLER_IMAGES_PATH.glob("*.jpg")):
    if params.get_bool("IsOnroad"):
      return

    try:
      uploaded = xattr.getxattr(image_path, frogpilot_variables.UPLOAD_ATTR_NAME) == frogpilot_variables.UPLOAD_DONE
    except OSError:
      uploaded = False

    if uploaded:
      continue

    way_id, is_forward = image_path.stem.split("_")
    way = (int(way_id), is_forward == "1")

    if way not in shared_speed_limits:
      continue

    capture = frogpilot_utilities.load_json_file(image_path.with_suffix(".json"))
    if not capture:
      continue

    with open(image_path, "rb") as image:
      image_data = image.read()

      descriptor = {
        "bearing": capture["bearing"],
        "captured_at": capture["captured_at"],
        "is_forward": way[1],
        "latitude": capture["latitude"],
        "longitude": capture["longitude"],
        "segment_id": way[0],
        "sha256": hashlib.sha256(image_data).hexdigest(),
        "size_bytes": len(image_data),
        "speed_limit": shared_speed_limits[way],
      }
      response = frogpilot_api.post(IMAGES_ENDPOINT, json=descriptor, timeout=30)
      if not frogpilot_api.succeeded(response):
        return

      upload = response.json().get("upload")
      if upload is not None:
        try:
          frogpilot_api.put_upload(upload, image_data, image_path.name, "speed_limit_image")
        except (FrogPilotAPIError, requests.exceptions.RequestException):
          return

      xattr.setxattr(image, frogpilot_variables.UPLOAD_ATTR_NAME, frogpilot_variables.UPLOAD_DONE)

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
