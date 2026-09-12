#!/usr/bin/env python3
from concurrent.futures import ThreadPoolExecutor

from openpilot.common.params import Params

from openpilot.frogpilot.common import frogpilot_api
from openpilot.frogpilot.common.frogpilot_utilities import calculate_distance_to_point

CACHE_DISTANCE = 25_000

DEFAULT_UPDATE_INTERVAL = 15 * 60
PERSONAL_KEY_UPDATE_INTERVAL = 60

RETRY_INTERVAL = 60

# Reference: https://openweathermap.org/weather-conditions
WEATHER_CATEGORIES = {
  "RAIN": {
    "ranges": [(300, 321), (500, 504)],
    "suffix": "rain",
  },
  "RAIN_STORM": {
    "ranges": [(200, 232), (511, 511), (520, 531), (771, 771), (781, 781)],
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

WEATHER_OFFSETS = (
  "increase_following_distance",
  "increase_stopped_distance",
  "reduce_acceleration",
  "reduce_lateral_acceleration",
)


def weather_category(weather_id):
  return next((category["suffix"] for category in WEATHER_CATEGORIES.values() if any(start <= weather_id <= end for start, end in category["ranges"])), "unknown")


class WeatherChecker:
  def __init__(self):
    self.params = Params()

    self.is_daytime = False
    self.requesting = False

    self.api_25_calls = 0
    self.api_3_calls = 0
    self.increase_following_distance = 0
    self.increase_stopped_distance = 0
    self.next_request = 0
    self.next_retry = 0
    self.reduce_acceleration = 0
    self.reduce_lateral_acceleration = 0
    self.sunrise = 0
    self.sunset = 0
    self.weather_id = 0

    self.last_position = None

    self.executor = ThreadPoolExecutor(max_workers=1)

  def update_offsets(self, frogpilot_toggles):
    category = weather_category(self.weather_id)
    for offset in WEATHER_OFFSETS:
      value = getattr(frogpilot_toggles, f"{offset}_{category}") if category not in ("clear", "unknown") else 0
      setattr(self, offset, value)

  def invalidate(self):
    self.next_request = 0
    self.weather_id = 0

  def update_weather(self, gps_position, now, frogpilot_toggles):
    timestamp = now.timestamp()

    position = (gps_position["latitude"], gps_position["longitude"])

    self.is_daytime = self.sunrise <= timestamp < self.sunset

    self.update_offsets(frogpilot_toggles)

    moved = self.last_position and calculate_distance_to_point(*self.last_position, *position) > CACHE_DISTANCE
    if self.requesting or timestamp < self.next_retry or (timestamp < self.next_request and not moved):
      return

    api_key = self.params.get("WeatherToken", encoding="utf-8")
    payload = {"latitude": position[0], "longitude": position[1]}
    if api_key:
      payload["api_key"] = api_key

    self.requesting = True

    self.next_retry = timestamp + RETRY_INTERVAL

    def complete_request(future):
      self.requesting = False

      response = future.result()
      if response is None or not 200 <= response.status_code < 300:
        return

      try:
        data = response.json()
      except ValueError:
        return

      if data.get("api_version") == "2.5":
        self.api_25_calls += 1
      else:
        self.api_3_calls += 1

      self.last_position = position

      self.next_request = timestamp + (PERSONAL_KEY_UPDATE_INTERVAL if data.get("using_personal_key", bool(api_key)) else DEFAULT_UPDATE_INTERVAL)

      self.sunrise = data.get("sunrise", 0)
      self.sunset = data.get("sunset", 0)

      self.weather_id = data.get("weather_id", 0)

      self.update_offsets(frogpilot_toggles)

    future = self.executor.submit(frogpilot_api.post, "/v1/weather", json=payload, timeout=30)
    future.add_done_callback(complete_request)
