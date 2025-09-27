import json
import os
import random
import requests
import subprocess
import sys

from collections import Counter
from datetime import datetime, timezone

from openpilot.common.conversions import Conversions as CV
from openpilot.system.hardware import HARDWARE
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.common.frogpilot_utilities import run_cmd
from openpilot.frogpilot.common.frogpilot_variables import get_frogpilot_toggles, params, params_tracking

BASE_URL = "https://nominatim.openstreetmap.org"
MINIMUM_POPULATION = 100_000

def get_city_center(latitude, longitude):
  try:
    with requests.Session() as session:
      session.headers.update({"Accept-Language": "en"})
      session.headers.update({"User-Agent": "frogpilot-city-center-checker/1.0 (https://github.com/FrogAi/FrogPilot)"})

      response = session.get(f"{BASE_URL}/reverse", params={"addressdetails": 1, "extratags": 0, "format": "jsonv2", "lat": latitude, "lon": longitude, "namedetails": 0, "zoom": 14}, timeout=10)
      response.raise_for_status()
      data = response.json() or {}

      address = data.get("address") or {}
      city_name = address.get("city") or address.get("hamlet") or address.get("town") or address.get("village")
      country_code = (address.get("country_code") or "").lower()
      country_name = address.get("country") or "N/A"
      state_name = address.get("province") or address.get("region") or address.get("state") or address.get("state_district") or "N/A"

      if city_name:
        response = session.get(f"{BASE_URL}/search", params={"addressdetails": 1, "extratags": 1, "format": "jsonv2", "limit": 1, "q": f"{city_name}, {state_name}, {country_name}"}, timeout=10)
        response.raise_for_status()
        data = response.json() or []

        if data:
          tags = data[0]
          population = (tags.get("extratags") or {}).get("population")

          population_value = None
          if population is not None:
            try:
              population_value = int(str(population).replace(",", "").split(";")[0].strip())
            except Exception:
              population_value = None

          if population_value is not None and population_value >= MINIMUM_POPULATION:
            latitude_value = float(tags["lat"])
            longitude_value = float(tags["lon"])

            resolved_address = tags.get("address") or {}
            city_label = resolved_address.get("city") or resolved_address.get("town") or city_name

            return latitude_value, longitude_value, city_label, state_name, country_name

      query = f"{state_name} state capital" if country_code == "us" else f"capital of {state_name}, {country_name}"
      response = session.get(f"{BASE_URL}/search", params={"addressdetails": 1, "extratags": 1, "format": "jsonv2", "limit": 5, "q": query}, timeout=10)
      response.raise_for_status()
      candidates = response.json() or []

      chosen_candidate = None
      for candidate in candidates:
        address = candidate.get("address") or {}
        capital = (candidate.get("extratags") or {}).get("capital")
        country = address.get("country")
        state = address.get("province") or address.get("region") or address.get("state") or address.get("state_district")

        if (state == state_name or state_name == "N/A") and country == country_name and (capital in ("administrative", "state", "yes") or address.get("city") or address.get("town")):
          chosen_candidate = candidate
          break

      if not chosen_candidate and candidates:
        chosen_candidate = candidates[0]

      if chosen_candidate:
        latitude_value = float(chosen_candidate["lat"])
        longitude_value = float(chosen_candidate["lon"])

        chosen_address = chosen_candidate.get("address") or {}
        city_label = chosen_address.get("city") or chosen_address.get("town") or (chosen_candidate.get("display_name") or "").split(",")[0]

        return latitude_value, longitude_value, city_label, state_name, country_name

      print(f"Falling back to (0, 0) for {latitude}, {longitude}")
      return float(0.0), float(0.0), "N/A", "N/A", "N/A"

  except Exception as exception:
    print(f"Falling back to (0, 0) for {latitude}, {longitude}")
    return float(0.0), float(0.0), "N/A", "N/A", "N/A"

def install_influxdb_client():
  try:
    import influxdb_client
    import influxdb_client.client.write_api
  except ModuleNotFoundError:
    print("influxdb-client not found. Attempting installation...")
    stock_mount_options = subprocess.run(["findmnt", "-no", "OPTIONS", "/"], capture_output=True, text=True, check=True).stdout.strip()

    run_cmd(["sudo", "mount", "-o", "remount,rw", "/"], "Successfully remounted / as read-write", "Failed to remount / as read-write", report=False)
    run_cmd(["sudo", sys.executable, "-m", "pip", "install", "influxdb-client"], "Successfully installed influxdb-client", "Failed to install influxdb-client", report=False)
    run_cmd(["sudo", "mount", "-o", f"remount,{stock_mount_options}", "/"], "Successfully restored stock mount options", "Failed to restore stock mount options", report=False)

def is_up_to_date(build_metadata):
  remote_commit = subprocess.check_output(["git", "ls-remote", "origin", build_metadata.channel], text=True, stderr=subprocess.DEVNULL).strip()

  if remote_commit:
    return build_metadata.openpilot.git_commit == remote_commit.split()[0]

  return True

def send_stats():
  try:
    build_metadata = get_build_metadata()
    frogpilot_toggles = get_frogpilot_toggles()

    if frogpilot_toggles.frogs_go_moo:
      return

    if frogpilot_toggles.car_make == "mock":
      return

    install_influxdb_client()

    from influxdb_client import InfluxDBClient, Point
    from influxdb_client.client.write_api import SYNCHRONOUS

    bucket = os.environ.get("STATS_BUCKET", "")
    org_ID = os.environ.get("STATS_ORG_ID", "")
    token = os.environ.get("STATS_TOKEN", "")
    url = os.environ.get("STATS_URL", "")

    frogpilot_stats = json.loads(params.get("FrogPilotStats") or "{}")

    location = json.loads(params.get("LastGPSPosition") or "{}")
    if not (location.get("latitude") and location.get("longitude")):
      return
    original_latitude = location.get("latitude")
    original_longitude = location.get("longitude")
    latitude, longitude, city, state, country = get_city_center(original_latitude, original_longitude)

    theme_sources = [
      frogpilot_toggles.icon_pack.replace("-animated", ""),
      frogpilot_toggles.color_scheme,
      frogpilot_toggles.distance_icons.replace("-animated", ""),
      frogpilot_toggles.signal_icons.replace("-animated", ""),
      frogpilot_toggles.sound_pack
    ]

    theme_counter = Counter(theme_sources)
    most_common = theme_counter.most_common()
    max_count = most_common[0][1]

    selected_theme = random.choice([item for item, count in most_common if count == max_count]).replace("-user_created", "").replace("_", " ")

    point = (Point("user_stats")
      .field("car_make", "GM" if frogpilot_toggles.car_make == "gm" else frogpilot_toggles.car_make.title())
      .field("car_model", frogpilot_toggles.car_model)
      .field("city", city)
      .field("country", country)
      .field("device", HARDWARE.get_device_type())
      .field("driving_model", frogpilot_toggles.model_name.replace("🗺️", "").replace("📡", "").replace("👀", "").replace("(Default)", "").strip())
      .field("event", 1)
      .field("frogpilot_drives", params_tracking.get_int("FrogPilotDrives"))
      .field("frogpilot_hours", params_tracking.get_int("FrogPilotMinutes") / 60)
      .field("frogpilot_miles", params_tracking.get_int("FrogPilotKilometers") * CV.KPH_TO_MPH)
      .field("goat_scream", frogpilot_toggles.goat_scream_alert)
      .field("has_cc_long", frogpilot_toggles.has_cc_long)
      .field("has_openpilot_longitudinal", frogpilot_toggles.openpilot_longitudinal)
      .field("has_pedal", frogpilot_toggles.has_pedal)
      .field("has_sdsu", frogpilot_toggles.has_sdsu)
      .field("has_zss", frogpilot_toggles.has_zss)
      .field("latitude", latitude)
      .field("longitude", longitude)
      .field("rainbow_path", frogpilot_toggles.rainbow_path)
      .field("random_events", frogpilot_toggles.random_events)
      .field("state", state)
      .field("theme", selected_theme.title())
      .field("total_aol_seconds", float(frogpilot_stats.get("TotalAOLTime", 0)))
      .field("total_lateral_seconds", float(frogpilot_stats.get("TotalLateralTime", 0)))
      .field("total_longitudinal_seconds", float(frogpilot_stats.get("TotalLongitudinalTime", 0)))
      .field("total_tracked_seconds", float(frogpilot_stats.get("TotalTrackedTime", 0)))
      .field("tuning_level", params.get_int("TuningLevel") + 1 if params.get_bool("TuningLevelConfirmed") else 0)
      .field("up_to_date", is_up_to_date(build_metadata))
      .field("using_stock_acc", not (frogpilot_toggles.has_cc_long or frogpilot_toggles.openpilot_longitudinal))

      .tag("branch", build_metadata.channel)
      .tag("dongle_id", params.get("FrogPilotDongleId", encoding="utf-8"))

      .time(datetime.now(timezone.utc))
    )

    InfluxDBClient(org=org_ID, token=token, url=url).write_api(write_options=SYNCHRONOUS).write(bucket=bucket, org=org_ID, record=point)
    print("Successfully sent FrogPilot stats!")
  except Exception as exception:
    print(f"Failed to send FrogPilot stats: {exception}")
