import json
import random
import requests

from cereal import car, custom

from openpilot.frogpilot.common.frogpilot_utilities import clean_model_name, get_frogpilot_api_info, is_url_pingable
from openpilot.frogpilot.common.frogpilot_variables import FROGPILOT_API, get_frogpilot_toggles, params

BASE_URL = "https://nominatim.openstreetmap.org"
GITHUB_API_URL = "https://api.github.com/repos/FrogAi/FrogPilot/commits"
MINIMUM_POPULATION = 100_000

TRACKED_BRANCHES = ["FrogPilot", "FrogPilot-Staging", "FrogPilot-Testing"]

def get_branch_commits():
  commits = []

  with requests.Session() as session:
    session.headers.update({"Accept": "application/vnd.github.v3+json", "User-Agent": "frogpilot-stats/1.0"})

    for branch in TRACKED_BRANCHES:
      try:
        response = session.get(f"{GITHUB_API_URL}/{branch}", timeout=10)
        response.raise_for_status()

        sha = response.json().get("sha")
        if sha:
          commits.append({"branch": branch, "commit": sha})
      except requests.exceptions.RequestException as exception:
        print(f"Failed to get commit for {branch}: {exception}")

  return commits

def get_city_center(latitude, longitude):
  if latitude == 0 and longitude == 0:
    return (0.0, 0.0, "N/A", "N/A", "N/A")

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

def send_stats():
  if not is_url_pingable(FROGPILOT_API):
    return

  try:
    frogpilot_toggles = get_frogpilot_toggles()

    if frogpilot_toggles.car_make == "mock":
      return

    api_token, build_metadata, device_type, dongle_id = get_frogpilot_api_info()

    car_params = "{}"
    msg_bytes = params.get("CarParamsPersistent")
    if msg_bytes:
      with car.CarParams.from_bytes(msg_bytes) as CP:
        cp_dict = CP.to_dict()
        cp_dict.pop("carFw", None)
        cp_dict.pop("carVin", None)
        car_params = json.dumps(cp_dict)

    frogpilot_car_params = "{}"
    frogpilot_msg_bytes = params.get("FrogPilotCarParamsPersistent")
    if frogpilot_msg_bytes:
      with custom.FrogPilotCarParams.from_bytes(frogpilot_msg_bytes) as FPCP:
        fpcp_dict = FPCP.to_dict()
        fpcp_dict.pop("carFw", None)
        fpcp_dict.pop("carVin", None)
        frogpilot_car_params = json.dumps(fpcp_dict)

    frogpilot_stats = json.loads(params.get("FrogPilotStats") or "{}")

    location = json.loads(params.get("LastGPSPosition") or "{}")
    original_latitude = location.get("latitude", 0.0)
    original_longitude = location.get("longitude", 0.0)
    latitude, longitude, city, state, country = get_city_center(original_latitude, original_longitude)

    payload = {
      "api_token": api_token,
      "branch_commits": get_branch_commits(),
      "build_metadata": build_metadata,
      "model_scores": [],
      "user_stats": {
        "calibrated_lateral_acceleration": params.get_float("CalibratedLateralAcceleration"),
        "calibration_progress": params.get_float("CalibrationProgress"),
        "car_params": car_params,
        "city": city,
        "country": country,
        "device": device_type,
        "frogpilot_car_params": frogpilot_car_params,
        "frogpilot_dongle_id": dongle_id,
        "frogpilot_stats": json.dumps(frogpilot_stats),
        "latitude": latitude,
        "longitude": longitude,
        "state": state,
        "toggles": json.dumps(frogpilot_toggles.__dict__),
        "using_default_model": params.get("Model", encoding="utf-8").endswith("_default"),
      },
    }

    model_scores = json.loads(params.get("ModelDrivesAndScores") or "{}")
    for model_name, data in sorted(model_scores.items()):
      drives = data.get("Drives", 0)
      score = data.get("Score", 0)

      if drives > 0:
        payload["model_scores"].append({
          "model_name": clean_model_name(model_name),
          "drives": int(drives),
          "score": int(score),
        })

    response = requests.post(f"{FROGPILOT_API}/stats", json=payload, headers={"Content-Type": "application/json", "User-Agent": "frogpilot-api/1.0"}, timeout=30)
    response.raise_for_status()
    print("Successfully sent FrogPilot stats!")

  except Exception as exception:
    print(f"Failed to send FrogPilot stats: {exception}")
