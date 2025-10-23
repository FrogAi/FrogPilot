import json
import os
import random
import requests

from collections import Counter
from datetime import datetime, timezone
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

from cereal import car
from openpilot.common.conversions import Conversions as CV
from openpilot.system.hardware import HARDWARE
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.common.frogpilot_utilities import clean_model_name
from openpilot.frogpilot.common.frogpilot_variables import get_frogpilot_toggles, params

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

  except Exception:
    print(f"Falling back to (0, 0) for {latitude}, {longitude}")
    return float(0.0), float(0.0), "N/A", "N/A", "N/A"

def update_branch_commits(now):
  points = []
  for branch in ["FrogPilot", "FrogPilot-Staging", "FrogPilot-Testing"]:
    try:
      response = requests.get(f"https://api.github.com/repos/FrogAi/FrogPilot/commits/{branch}")
      response.raise_for_status()
      sha = response.json()["sha"]
      points.append(Point("branch_commits").field("commit", sha).tag("branch", branch).time(now))
    except Exception as e:
      print(f"Failed to fetch commit for {branch}: {e}")
  return points

def send_stats():
  try:
    build_metadata = get_build_metadata()
    frogpilot_toggles = get_frogpilot_toggles()

    if frogpilot_toggles.car_make == "mock":
      return

    bucket = os.environ.get("STATS_BUCKET", "")
    org_ID = os.environ.get("STATS_ORG_ID", "")
    token = os.environ.get("STATS_TOKEN", "")
    url = os.environ.get("STATS_URL", "")

    car_params = "{}"
    msg_bytes = params.get("CarParamsPersistent")
    if msg_bytes:
      with car.CarParams.from_bytes(msg_bytes) as CP:
        cp_dict = CP.to_dict()
        cp_dict.pop("carFw", None)
        car_params = json.dumps(cp_dict)

    dongle_id = params.get("FrogPilotDongleId", encoding="utf-8")
    frogpilot_stats = json.loads(params.get("FrogPilotStats") or "{}")

    location = json.loads(params.get("LastGPSPosition") or "{}")
    original_latitude = location.get("latitude", 0.0)
    original_longitude = location.get("longitude", 0.0)
    latitude, longitude, city, state, country = get_city_center(original_latitude, original_longitude)

    now = datetime.now(timezone.utc)

    theme_attributes = sorted(["color_scheme", "distance_icons", "icon_pack", "signal_icons", "sound_pack"])
    theme_counts = Counter(getattr(frogpilot_toggles, attribute).replace("-animated", "") for attribute in theme_attributes)
    winners = [theme for theme, count in theme_counts.items() if count == max(theme_counts.values(), default=0)]
    if len(winners) > 1 and "stock" in winners:
      winners.remove("stock")
    selected_theme = random.choice(winners).replace("-user_created", "").replace("_", " ") if winners else "stock"

    user_point = (
      Point("user_stats")
      .field("calibrated_lateral_acceleration", params.get_float("CalibratedLateralAcceleration"))
      .field("calibration_progress", params.get_float("CalibrationProgress"))
      .field("car_params", car_params)
      .field("city", city)
      .field("commit", build_metadata.openpilot.git_commit)
      .field("country", country)
      .field("device", HARDWARE.get_device_type())
      .field("event", 1)
      .field("latitude", latitude)
      .field("longitude", longitude)
      .field("state", state)
      .field("stats", json.dumps(frogpilot_stats))
      .field("theme", selected_theme.title())
      .field("toggles", json.dumps(frogpilot_toggles.__dict__))
      .field("tuning_level", params.get_int("TuningLevel") + 1 if params.get_bool("TuningLevelConfirmed") else 0)
      .field("using_default_model", params.get("Model", encoding="utf-8").endswith("_default"))
      .tag("branch", build_metadata.channel)
      .tag("dongle_id", dongle_id)
      .time(now)
    )

    model_scores = json.loads(params.get("ModelDrivesAndScores") or "{}")
    model_points = []
    for model_name, data in sorted(model_scores.items()):
      drives = data.get("Drives", 0)
      score = data.get("Score", 0)

      if drives > 0:
        point = (
          Point("model_scores")
          .field("drives", int(drives))
          .field("score", int(score))
          .tag("dongle_id", dongle_id)
          .tag("model_name", clean_model_name(model_name))
          .time(now)
        )
        model_points.append(point)

    all_points = model_points + [user_point] + update_branch_commits(now)

    client = InfluxDBClient(org=org_ID, token=token, url=url)
    client.write_api(write_options=SYNCHRONOUS).write(bucket=bucket, org=org_ID, record=all_points)
    print("Successfully sent FrogPilot stats!")

  except Exception as exception:
    print(f"Failed to send FrogPilot stats: {exception}")
