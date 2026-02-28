#!/usr/bin/env python3
import requests
import time

from concurrent.futures import ThreadPoolExecutor

from openpilot.common.conversions import Conversions as CV
from openpilot.common.params import Params

from openpilot.frogpilot.common.frogpilot_utilities import calculate_distance_to_point, get_frogpilot_api_info, is_url_pingable
from openpilot.frogpilot.common.frogpilot_variables import FROGPILOT_API

CACHE_DISTANCE = 25
MAX_RETRIES = 3
RETRY_DELAY = 60

# Reference: https://openweathermap.org/weather-conditions
WEATHER_CATEGORIES = {
  "RAIN": {
    "ranges": [(300, 321), (500, 504)],
    "suffix": "rain",
  },
  "RAIN_STORM": {
    "ranges": [(200, 232), (511, 511), (520, 531)],
    "suffix": "rain_storm",
  },
  "SNOW": {
    "ranges": [(600, 622)],
    "suffix": "snow",
  },
  "LOW_VISIBILITY": {
    "ranges": [(701, 762)],
    "suffix": "low_visibility",
  },
  "CLEAR": {
    "ranges": [(800, 800)],
    "suffix": "clear",
  },
}

class WeatherChecker:
  def __init__(self):
    self.is_daytime = False

    self.api_25_calls = 0
    self.api_3_calls = 0
    self.increase_following_distance = 0
    self.increase_stopped_distance = 0
    self.reduce_acceleration = 0
    self.reduce_lateral_acceleration = 0
    self.sunrise = 0
    self.sunset = 0
    self.weather_id = 0

    self.hourly_forecast = None
    self.last_gps_position = None
    self.last_updated = None
    self.requesting = False

    self.user_api_key = Params().get("WeatherToken", encoding="utf-8")

    if self.user_api_key:
      self.check_interval = 60
    else:
      self.check_interval = 15 * 60

    self.api_token, self.build_metadata, self.device_type, self.dongle_id = get_frogpilot_api_info()

    self.session = requests.Session()
    self.session.headers.update({"Accept-Language": "en", "User-Agent": "frogpilot-api/1.0"})

    self.executor = ThreadPoolExecutor(max_workers=1)

  def update_offsets(self, frogpilot_toggles):
    suffix = WEATHER_CATEGORIES["CLEAR"]["suffix"]
    for category in WEATHER_CATEGORIES.values():
      if any(start <= self.weather_id <= end for start, end in category["ranges"]):
        suffix = category["suffix"]
        break

    if suffix != WEATHER_CATEGORIES["CLEAR"]["suffix"]:
      self.increase_following_distance = getattr(frogpilot_toggles, f"increase_following_distance_{suffix}")
      self.increase_stopped_distance = getattr(frogpilot_toggles, f"increase_stopped_distance_{suffix}")
      self.reduce_acceleration = getattr(frogpilot_toggles, f"reduce_acceleration_{suffix}")
      self.reduce_lateral_acceleration = getattr(frogpilot_toggles, f"reduce_lateral_acceleration_{suffix}")
    else:
      self.increase_following_distance = 0
      self.increase_stopped_distance = 0
      self.reduce_acceleration = 0
      self.reduce_lateral_acceleration = 0

  def update_weather(self, gps_position, now, frogpilot_toggles):
    if self.last_gps_position and self.last_updated:
      distance = calculate_distance_to_point(
        self.last_gps_position["latitude"] * CV.DEG_TO_RAD,
        self.last_gps_position["longitude"] * CV.DEG_TO_RAD,
        gps_position.get("latitude") * CV.DEG_TO_RAD,
        gps_position.get("longitude") * CV.DEG_TO_RAD
      )
      if distance / 1000 > CACHE_DISTANCE:
        self.hourly_forecast = None
        self.last_updated = None

    if self.sunrise and self.sunset:
      self.is_daytime = self.sunrise <= int(now.timestamp()) < self.sunset

    if self.last_updated and (now - self.last_updated).total_seconds() < self.check_interval:
      if self.hourly_forecast:
        current_forecast = min(self.hourly_forecast, key=lambda f: abs(f["dt"] - now.timestamp()))
        self.weather_id = current_forecast.get("weather", [{}])[0].get("id", 0)
        self.update_offsets(frogpilot_toggles)
      return

    if self.requesting:
      return

    self.requesting = True

    def complete_request(future):
      self.requesting = False
      data = future.result()
      if data:
        self.last_updated = now
        self.hourly_forecast = data.get("hourly")
        self.last_gps_position = gps_position

        if "current" in data:
          source_data = data.get("current", {})
          current_data = source_data
        else:
          source_data = data
          current_data = source_data.get("sys", source_data)

        self.sunrise = current_data.get("sunrise", 0)
        self.sunset = current_data.get("sunset", 0)
        self.weather_id = source_data.get("weather", [{}])[0].get("id", 0)

      self.update_offsets(frogpilot_toggles)

    def make_request():
      if not is_url_pingable(FROGPILOT_API):
        return None

      payload = {
        "api_key": self.user_api_key,
        "api_token": self.api_token,
        "build_metadata": self.build_metadata,
        "device": self.device_type,
        "frogpilot_dongle_id": self.dongle_id,
        "lat": gps_position["latitude"],
        "lon": gps_position["longitude"],
      }

      for attempt in range(1, MAX_RETRIES + 1):
        try:
          response = self.session.post(f"{FROGPILOT_API}/weather", json=payload, headers={"Content-Type": "application/json"}, timeout=10)
          response.raise_for_status()

          data = response.json()
          if data.get("api_version") == "2.5":
            self.api_25_calls += 1
          else:
            self.api_3_calls += 1
          return data
        except Exception:
          if attempt < MAX_RETRIES:
            time.sleep(RETRY_DELAY)
          continue
      return None

    future = self.executor.submit(make_request)
    future.add_done_callback(complete_request)
