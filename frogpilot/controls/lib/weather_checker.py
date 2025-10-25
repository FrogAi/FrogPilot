#!/usr/bin/env python3
import os
import requests
import time

from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone

from openpilot.common.conversions import Conversions as CV
from openpilot.common.params import Params

from openpilot.frogpilot.common.frogpilot_utilities import calculate_distance_to_point, is_url_pingable

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
    self.updating_weather = False

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

    user_api_key = Params().get("WeatherToken", encoding="utf-8")
    self.api_key = user_api_key or os.environ.get("WEATHER_TOKEN", "")

    if user_api_key:
      self.check_interval = 60
    else:
      self.check_interval = 15 * 60

    self.session = requests.Session()
    self.session.headers.update({"Accept-Language": "en"})
    self.session.headers.update({"User-Agent": "frogpilot-weather-checker/1.0 (https://github.com/FrogAi/FrogPilot)"})

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
    if not self.api_key:
      self.weather_id = 0
      return

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

    if self.updating_weather:
      return

    if self.last_updated and (now - self.last_updated).total_seconds() < self.check_interval:
      if self.hourly_forecast:
        current_forecast = min(self.hourly_forecast, key=lambda f: abs(f["dt"] - now.timestamp()))
        self.weather_id = current_forecast.get("weather", [{}])[0].get("id", 0)
        self.update_offsets(frogpilot_toggles)
      return

    self.updating_weather = True

    def complete_request(future):
      data = future.result()
      self.updating_weather = False
      self.last_updated = datetime.now(timezone.utc)

      if data:
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
      if not is_url_pingable("https://api.openweathermap.org"):
        self.weather_id = WEATHER_CATEGORIES["CLEAR"]["ranges"][0][0]
        return None

      params = {
        "lat": gps_position["latitude"],
        "lon": gps_position["longitude"],
        "appid": self.api_key,
        "units": "metric",
        "exclude": "alerts,minutely,daily",
      }

      for attempt in range(1, MAX_RETRIES + 1):
        try:
          response = self.session.get("https://api.openweathermap.org/data/3.0/onecall", params=params, timeout=10)
          if response.status_code == 429:
            fallback_params = params.copy()
            fallback_params.pop("exclude", None)
            fallback_response = self.session.get("https://api.openweathermap.org/data/2.5/weather", params=fallback_params, timeout=10)
            fallback_response.raise_for_status()
            return fallback_response.json()

          response.raise_for_status()
          return response.json()
        except Exception:
          if attempt < MAX_RETRIES:
            time.sleep(RETRY_DELAY)
          continue
      return None

    future = self.executor.submit(make_request)
    future.add_done_callback(complete_request)
