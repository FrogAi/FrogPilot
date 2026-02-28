#!/usr/bin/env python3
import json
import requests
import time

from functools import cache

from cereal import car, custom, messaging
from openpilot.common.params import ParamKeyType
from openpilot.common.realtime import Ratekeeper
from openpilot.common.time import system_time_valid

from openpilot.frogpilot.common.frogpilot_utilities import get_frogpilot_api_info, is_url_pingable
from openpilot.frogpilot.common.frogpilot_variables import EXCLUDED_KEYS, FROGPILOT_API, params, update_frogpilot_toggles
from openpilot.frogpilot.ui.layouts.settings import toggle_metadata

POND_PRESENCE_INTERVAL_ACTIVE = 60
POND_PRESENCE_INTERVAL_IDLE = 240

REMOTE_TOGGLE_CHECK_INTERVAL_ACTIVE = 10
REMOTE_TOGGLE_CHECK_INTERVAL_IDLE = 60


@cache
def get_toggle_metadata_params():
  ToggleDefinition = toggle_metadata.ToggleDefinition
  ToggleType = toggle_metadata.ToggleType
  metadata_params = set()

  def is_valid_param_key(key):
    if not key or key in EXCLUDED_KEYS:
      return False
    try:
      return params.check_key(key)
    except Exception:
      return False

  def collect_param_key(key):
    if is_valid_param_key(key):
      metadata_params.add(key)

  def collect_params(definition):
    if not isinstance(definition, ToggleDefinition):
      return
    collect_param_key(definition.param)
    if definition.toggle_type != ToggleType.BUTTON_PARAM:
      for option in definition.button_options or []:
        collect_param_key(option)

  for value in vars(toggle_metadata).values():
    if isinstance(value, ToggleDefinition):
      collect_params(value)
      continue
    if isinstance(value, tuple):
      for item in value:
        collect_params(item)

  return tuple(sorted(metadata_params))


def get_persistent_car_params():
  car_params = None
  msg_bytes = params.get("CarParamsPersistent")
  if msg_bytes:
    with car.CarParams.from_bytes(msg_bytes) as CP:
      cp_dict = CP.to_dict()
      cp_dict.pop("carFw", None)
      cp_dict.pop("carVin", None)
      car_params = json.dumps(cp_dict)

  frogpilot_car_params = None
  frogpilot_msg_bytes = params.get("FrogPilotCarParamsPersistent")
  if frogpilot_msg_bytes:
    with custom.FrogPilotCarParams.from_bytes(frogpilot_msg_bytes) as FPCP:
      fpcp_dict = FPCP.to_dict()
      fpcp_dict.pop("carFw", None)
      fpcp_dict.pop("carVin", None)
      frogpilot_car_params = json.dumps(fpcp_dict)

  return car_params, frogpilot_car_params


def check_toggles(started, sm=None, boot_run=False):
  if not params.get_bool("PondPaired"):
    return None

  if not is_url_pingable(FROGPILOT_API):
    return None

  if not boot_run and sm is not None:
    if started and not sm["frogpilotCarState"].isParked:
      return None
    if sm["deviceState"].screenBrightnessPercent == 0:
      return None

  try:
    api_token, build_metadata, device_type, dongle_id = get_frogpilot_api_info()
    if not dongle_id or not api_token:
      return None

    response = requests.get(
      f"{FROGPILOT_API}/pond/toggles/pending",
      params={"dongle_id": dongle_id, "api_token": api_token},
      headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"},
      timeout=10,
    )
    response.raise_for_status()

    data = response.json()
    pond_active = data.get("pond_active") is True

    if data.get("paired") is False:
      params.put_bool("PondPaired", False)
      print("Device was unpaired remotely")
      return False

    toggles = data.get("toggles")
    if not toggles:
      return pond_active

    for key, value in toggles.items():
      if key in EXCLUDED_KEYS:
        continue

      try:
        if not params.check_key(key):
          print(f"Skipping unknown param key: {key}")
          continue
      except Exception:
        print(f"Skipping unknown param key: {key}")
        continue

      if value is None:
        continue

      try:
        params.put(key, value)
      except Exception as exception:
        print(f"Skipping remote toggle {key}: {exception}")
        continue

    update_frogpilot_toggles()

    requests.post(
      f"{FROGPILOT_API}/pond/toggles/ack",
      json={
        "api_token": api_token,
        "build_metadata": build_metadata,
        "device": device_type,
        "frogpilot_dongle_id": dongle_id,
      },
      headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"},
      timeout=10,
    ).raise_for_status()

    print(f"Successfully applied {len(toggles)} remote toggles")
    return pond_active

  except Exception as exception:
    print(f"Failed to check remote toggles: {exception}")
    return None


def ping_pond_presence(interval, parked, started, state_changed):
  last_ping = getattr(ping_pond_presence, "_last_ping", 0.0)
  now = time.monotonic()
  if not state_changed and (now - last_ping) < interval:
    return

  try:
    api_token, build_metadata, device_type, dongle_id = get_frogpilot_api_info()
    if not dongle_id or not api_token:
      return

    payload = {
      "api_token": api_token,
      "build_metadata": build_metadata,
      "device": device_type,
      "dongle_id": dongle_id,
      "frogpilot_dongle_id": dongle_id,
      "is_onroad": bool(started),
      "is_parked": bool(parked),
    }

    response = requests.post(
      f"{FROGPILOT_API}/pond/presence/device",
      json=payload,
      headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"},
      timeout=10,
    )
    response.raise_for_status()
    ping_pond_presence._last_ping = now

  except Exception as exception:
    print(f"Failed to update Pond presence: {exception}")


def upload_toggles():
  if not is_url_pingable(FROGPILOT_API):
    return False

  try:
    api_token, build_metadata, device_type, dongle_id = get_frogpilot_api_info()
    if not dongle_id or not api_token:
      return False

    toggles = {}
    for key in get_toggle_metadata_params():
      if key in EXCLUDED_KEYS or params.get_key_flag(key) & ParamKeyType.DONT_LOG:
        continue

      value = params.get(key)
      if value is None:
        continue

      toggles[key] = value.decode("utf-8", "replace") if isinstance(value, (bytes, bytearray)) else value

    if not toggles:
      return False

    car_params, frogpilot_car_params = get_persistent_car_params()

    payload = {
      "api_token": api_token,
      "build_metadata": build_metadata,
      "device": device_type,
      "dongle_id": dongle_id,
      "frogpilot_dongle_id": dongle_id,
      "toggles": toggles,
    }
    if car_params is not None:
      payload["car_params"] = car_params
    if frogpilot_car_params is not None:
      payload["frogpilot_car_params"] = frogpilot_car_params

    requests.post(
      f"{FROGPILOT_API}/pond/toggles/sync",
      json=payload,
      headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"},
      timeout=10,
    ).raise_for_status()

    print("Successfully uploaded toggles to FrogPilot.com")
    return True

  except Exception as exception:
    print(f"Failed to upload toggles: {exception}")
    return False


def sync_thread():
  rate_keeper = Ratekeeper(1, None)

  sm = messaging.SubMaster(["deviceState", "frogpilotCarState"])

  boot_sync_complete = False
  pond_active = False
  previous_parked = False
  previous_started = False
  next_toggle_check_at = 0.0

  while True:
    sm.update(0)

    parked = sm["frogpilotCarState"].isParked
    started = sm["deviceState"].started
    state_changed = started != previous_started or parked != previous_parked

    if params.get_bool("PondPaired"):
      presence_interval = POND_PRESENCE_INTERVAL_ACTIVE if started or pond_active else POND_PRESENCE_INTERVAL_IDLE
      ping_pond_presence(presence_interval, parked, started, state_changed)

    if not boot_sync_complete and system_time_valid():
      boot_pond_active = check_toggles(False, boot_run=True)
      if boot_pond_active is not None:
        pond_active = boot_pond_active
      boot_sync_complete = True

    now = time.monotonic()
    if state_changed and parked:
      next_toggle_check_at = 0.0

    if boot_sync_complete and now >= next_toggle_check_at:
      latest_pond_active = check_toggles(started, sm)
      if latest_pond_active is not None:
        pond_active = latest_pond_active
      next_toggle_check_at = now + REMOTE_TOGGLE_CHECK_INTERVAL_ACTIVE if pond_active else REMOTE_TOGGLE_CHECK_INTERVAL_IDLE

    if params.get_bool("PondUploadPending"):
      if not params.get_bool("PondPaired"):
        params.put_bool("PondUploadPending", False)
      elif upload_toggles():
        params.put_bool("PondUploadPending", False)

    previous_parked = parked
    previous_started = started

    rate_keeper.keep_time()


def main():
  sync_thread()


if __name__ == "__main__":
  main()
