#!/usr/bin/env python3
import requests

from concurrent.futures import ThreadPoolExecutor

from openpilot.common.params import Params

from openpilot.frogpilot.common.frogpilot_api import FrogPilotAPIError
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
  def __init__(self, frogpilot_api):
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

    self.api_25_key = None
    self.last_position = None

    self.frogpilot_api = frogpilot_api

    self.executor = ThreadPoolExecutor(max_workers=1)

    self.session = requests.Session()

  def close(self):
    self.executor.submit(self.session.close)
    self.executor.shutdown(wait=False)

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

    if self.requesting or timestamp < self.next_retry:
      return

    moved = self.last_position and calculate_distance_to_point(*self.last_position, *position) > CACHE_DISTANCE
    if timestamp < self.next_request and not moved:
      return

    api_key = self.params.get("WeatherToken")

    self.requesting = True

    self.next_retry = timestamp + RETRY_INTERVAL

    def complete_request(future):
      self.requesting = False

      try:
        data = future.result()
      except (FrogPilotAPIError, IndexError, KeyError, TypeError, ValueError, requests.RequestException):
        return

      if not isinstance(data, dict) or data.get("api_version") not in ("2.5", "3.0"):
        return
      if not all(type(data.get(key)) is int for key in ("sunrise", "sunset", "weather_id")):
        return

      using_personal_key = data.get("using_personal_key", bool(api_key))
      if not isinstance(using_personal_key, bool):
        return

      if data.get("api_version") == "2.5":
        self.api_25_calls += 1
        self.api_25_key = api_key
      else:
        self.api_3_calls += 1

      self.last_position = position

      self.next_request = timestamp + (PERSONAL_KEY_UPDATE_INTERVAL if using_personal_key else DEFAULT_UPDATE_INTERVAL)

      self.sunrise = data.get("sunrise", 0)
      self.sunset = data.get("sunset", 0)

      self.weather_id = data.get("weather_id", 0)

      self.update_offsets(frogpilot_toggles)

    def make_request():
      if self.api_25_key != api_key:
        self.api_25_key = None

      if not api_key:
        return self.frogpilot_api.post_json("/v1/weather", {"latitude": position[0], "longitude": position[1]}, session=self.session, timeout=30)

      query = {"appid": api_key, "exclude": "alerts,daily,hourly,minutely", "lat": position[0], "lon": position[1]}

      api_version = "2.5"
      if self.api_25_key != api_key:
        with self.session.get("https://api.openweathermap.org/data/3.0/onecall", params=query, timeout=30, allow_redirects=False) as response:
          if response.status_code not in (401, 403):
            response.raise_for_status()
            data = response.json()["current"]

            api_version = "3.0"
            sun_data = data

      if api_version == "2.5":
        query.pop("exclude")

        with self.session.get("https://api.openweathermap.org/data/2.5/weather", params=query, timeout=30, allow_redirects=False) as response:
          response.raise_for_status()
          data = response.json()

        sun_data = data["sys"]

      return {
        "api_version": api_version,
        "sunrise": sun_data.get("sunrise", 0),
        "sunset": sun_data.get("sunset", 0),
        "weather_id": data["weather"][0]["id"],
      }

    future = self.executor.submit(make_request)
    future.add_done_callback(complete_request)
