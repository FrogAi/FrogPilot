#!/usr/bin/env python3
import requests
import time

from concurrent.futures import ThreadPoolExecutor

from openpilot.common.params import Params

from openpilot.frogpilot.common import frogpilot_api, frogpilot_utilities

CACHE_DISTANCE = 25_000
CACHE_MAX_AGE = 20 * 60
UPDATE_DISTANCE = 20_000

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


def valid_weather(data):
  return isinstance(data, dict) and all(type(data.get(key)) is int for key in ("sunrise", "sunset", "weather_id"))


def weather_category(weather_id):
  return next(
    (category["suffix"] for category in WEATHER_CATEGORIES.values() if any(start <= weather_id <= end for start, end in category["ranges"])),
    "unknown",
  )


class WeatherChecker:
  def __init__(self, api):
    self.params = Params()

    self.is_daytime = False

    self.increase_following_distance = 0
    self.increase_stopped_distance = 0
    self.next_request = 0
    self.next_retry = 0
    self.reduce_acceleration = 0
    self.reduce_lateral_acceleration = 0
    self.sunrise = 0
    self.sunset = 0
    self.weather_calls = 0
    self.weather_id = 0

    self.future = None
    self.last_position = None
    self.last_request_time = None
    self.request_position = None
    self.request_time = None

    self.frogpilot_api = api

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
    self.last_position = None
    self.last_request_time = None
    self.next_request = 0
    self.weather_id = 0

  def update_weather(self, gps_position, now, frogpilot_toggles):
    timestamp = time.monotonic()

    position = (gps_position["latitude"], gps_position["longitude"])

    if self.future is not None and self.future.done():
      self.complete_request()

    distance = 0
    if self.last_position is not None:
      distance = frogpilot_utilities.calculate_distance_to_point(*self.last_position, *position)
      if distance >= CACHE_DISTANCE or timestamp - self.last_request_time >= CACHE_MAX_AGE:
        self.invalidate()

    self.is_daytime = self.sunrise <= now.timestamp() < self.sunset

    self.update_offsets(frogpilot_toggles)

    if self.future is not None or timestamp < self.next_retry:
      return

    moved = distance >= UPDATE_DISTANCE
    if timestamp < self.next_request and not moved:
      return

    api_key = self.params.get("WeatherToken")

    self.request_position = position
    self.request_time = timestamp

    def make_request():
      if api_key:
        query = {"appid": api_key, "lat": position[0], "lon": position[1]}
        try:
          with self.session.get("https://api.openweathermap.org/data/4.0/onecall/current", params=query, timeout=30, allow_redirects=False) as response:
            if 200 <= response.status_code < 300:
              data = response.json()["data"][0]
              if not isinstance(data, dict):
                raise ValueError("Invalid personal weather data")

              weather = {
                "sunrise": data.get("sunrise", 0),
                "sunset": data.get("sunset", 0),
                "weather_id": data["weather"][0]["id"],
              }
              if valid_weather(weather):
                return weather, PERSONAL_KEY_UPDATE_INTERVAL
        except (IndexError, KeyError, TypeError, ValueError, requests.RequestException):
          pass

      return self.frogpilot_api.post_json("/v1/weather", {"latitude": position[0], "longitude": position[1]}, session=self.session, timeout=30), DEFAULT_UPDATE_INTERVAL

    self.future = self.executor.submit(make_request)

  def complete_request(self):
    future = self.future
    self.future = None

    self.next_retry = time.monotonic() + RETRY_INTERVAL

    try:
      data, interval = future.result()
    except (frogpilot_api.FrogPilotAPIError, IndexError, KeyError, TypeError, ValueError, requests.RequestException):
      return

    if not valid_weather(data):
      return

    self.weather_calls += 1

    self.last_position = self.request_position
    self.last_request_time = self.request_time
    self.next_request = self.request_time + interval

    self.sunrise = data["sunrise"]
    self.sunset = data["sunset"]
    self.weather_id = data["weather_id"]
