#!/usr/bin/env python3
import os
import requests
import time

from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone

from openpilot.frogpilot.common.frogpilot_utilities import is_url_pingable

API_KEY = os.environ.get("WEATHER_TOKEN", "")
CHECK_INTERVAL = 5 * 60
MAX_RETRIES = 3
RETRY_DELAY = 60

class WeatherChecker:
  def __init__(self):
    self.is_daytime = False
    self.updating_weather = False

    self.increase_following_distance = 0
    self.increase_stopped_distance = 0
    self.reduce_acceleration = 0
    self.reduce_lateral_acceleration = 0
    self.weather_id = 0

    self.last_updated = None

    self.session = requests.Session()
    self.session.headers.update({"Accept-Language": "en"})
    self.session.headers.update({"User-Agent": "frogpilot-weather-checker/1.0 (https://github.com/FrogAi/FrogPilot)"})

    self.executor = ThreadPoolExecutor(max_workers=1)

  def update_weather(self, gps_position, now, frogpilot_toggles):
    if not API_KEY:
      self.weather_id = 0
      return

    if self.updating_weather:
      return

    if self.last_updated and (now - self.last_updated).total_seconds() < CHECK_INTERVAL:
      return

    self.updating_weather = True

    def complete_request(future):
      data = future.result()

      current_time = datetime.now(timezone.utc)

      self.updating_weather = False

      self.last_updated = current_time

      if not data:
        self.weather_id = 0
        return

      weather = data.get("weather", [{}])[0]
      sys = data.get("sys", {})

      sunrise = sys.get("sunrise", 0)
      sunset = sys.get("sunset", 0)

      if sunrise and sunset:
        self.is_daytime = sunrise <= int(current_time.timestamp()) < sunset
      else:
        self.is_daytime = False

      self.weather_id = weather.get("id", 0)

    def make_request():
      if not is_url_pingable("https://api.openweathermap.org"):
        return None

      params = {
        "lat": gps_position["latitude"],
        "lon": gps_position["longitude"],
        "appid": API_KEY,
        "units": "metric",
      }

      for attempt in range(1, MAX_RETRIES + 1):
        try:
          response = self.session.get("https://api.openweathermap.org/data/2.5/weather", params=params, timeout=10)
          if response.status_code == 429:
            if attempt < MAX_RETRIES:
              retry_after = response.headers.get("Retry-After")
              if retry_after:
                delay = float(retry_after)
              else:
                delay = RETRY_DELAY

              time.sleep(delay)
              continue
            else:
              return None

          response.raise_for_status()
          return response.json()
        except Exception:
          if attempt < MAX_RETRIES:
            time.sleep(RETRY_DELAY)
            continue
          return None

    future = self.executor.submit(make_request)
    future.add_done_callback(complete_request)

    if 300 <= self.weather_id <= 321 or 500 <= self.weather_id <= 504:
      self.increase_following_distance = frogpilot_toggles.increase_following_distance_rain
      self.increase_stopped_distance = frogpilot_toggles.increase_stopped_distance_rain
      self.reduce_acceleration = frogpilot_toggles.reduce_acceleration_rain
      self.reduce_lateral_acceleration = frogpilot_toggles.reduce_lateral_acceleration_rain
    elif 200 <= self.weather_id <= 232 or self.weather_id == 511 or 520 <= self.weather_id <= 531:
      self.increase_following_distance = frogpilot_toggles.increase_following_distance_rain_storm
      self.increase_stopped_distance = frogpilot_toggles.increase_stopped_distance_rain_storm
      self.reduce_acceleration = frogpilot_toggles.reduce_acceleration_rain_storm
      self.reduce_lateral_acceleration = frogpilot_toggles.reduce_lateral_acceleration_rain_storm
    elif 600 <= self.weather_id <= 622:
      self.increase_following_distance = frogpilot_toggles.increase_following_distance_snow
      self.increase_stopped_distance = frogpilot_toggles.increase_stopped_distance_snow
      self.reduce_acceleration = frogpilot_toggles.reduce_acceleration_snow
      self.reduce_lateral_acceleration = frogpilot_toggles.reduce_lateral_acceleration_snow
    elif 701 <= self.weather_id <= 762:
      self.increase_following_distance = frogpilot_toggles.increase_following_distance_low_visibility
      self.increase_stopped_distance = frogpilot_toggles.increase_stopped_distance_low_visibility
      self.reduce_acceleration = frogpilot_toggles.reduce_acceleration_low_visibility
      self.reduce_lateral_acceleration = frogpilot_toggles.reduce_lateral_acceleration_low_visibility
    else:
      self.increase_following_distance = 0
      self.increase_stopped_distance = 0
      self.reduce_acceleration = 0
      self.reduce_lateral_acceleration = 0
