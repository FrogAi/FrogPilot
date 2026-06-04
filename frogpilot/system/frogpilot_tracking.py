#!/usr/bin/env python3
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.car.cruise import V_CRUISE_MAX
from openpilot.selfdrive.selfdrived.events import FROGPILOT_EVENT_NAME
from openpilot.selfdrive.selfdrived.selfdrived import LONGITUDINAL_PERSONALITY_MAP, State
from openpilot.selfdrive.selfdrived.state import ACTIVE_STATES
from openpilot.selfdrive.ui.soundd import FrogPilotAudibleAlert

from openpilot.frogpilot.common import frogpilot_utilities
from openpilot.frogpilot.controls.lib.frogpilot_events import RANDOM_EVENT_END, RANDOM_EVENT_START
from openpilot.frogpilot.controls.lib.weather_checker import WEATHER_CATEGORIES


CRUISE_SPEED_BUCKET_KPH = 5


def increment_bucket(stats, key, bucket, value=1):
  buckets = stats.get(key, {})
  buckets[bucket] = buckets.get(bucket, 0) + value
  stats[key] = buckets


def increment_stat(stats, key, value):
  stats[key] = stats.get(key, 0) + value


def speed_bucket_kph(speed):
  bucket = int(speed // CRUISE_SPEED_BUCKET_KPH) * CRUISE_SPEED_BUCKET_KPH
  return f"{bucket}-{bucket + CRUISE_SPEED_BUCKET_KPH}kph"


class FrogPilotTracking:
  def __init__(self, frogpilot_planner, frogpilot_toggles):
    self.params = frogpilot_planner.params

    self.frogpilot_events = frogpilot_planner.frogpilot_events
    self.frogpilot_weather = frogpilot_planner.frogpilot_weather

    self.frogpilot_stats = self.params.get("FrogPilotStats")
    self.frogpilot_stats.pop("ResetStats", None)

    self.drive_added = False
    self.previous_active = False
    self.previous_standstill = False
    self.previous_stoplight = False
    self.previously_enabled = False

    self.distance_since_override = 0
    self.drive_distance = 0
    self.drive_time = 0
    self.tracked_time = 0

    self.previous_random_events = set()

    self.previous_alert = None
    self.previous_sound = FrogPilotAudibleAlert.none
    self.previous_state = State.disabled

    self.model_name = frogpilot_utilities.clean_model_name(frogpilot_toggles.model_name)

  def update(self, now, time_validated, sm, frogpilot_toggles):
    v_cruise_kph = min(max(sm["carState"].vCruiseCluster, sm["carState"].vCruise), V_CRUISE_MAX)
    v_ego = max(sm["carState"].vEgo, 0)

    distance_driven = v_ego * DT_MDL

    engagement_active = sm["selfdriveState"].state in ACTIVE_STATES or sm["frogpilotCarState"].alwaysOnLateralEnabled

    self.previously_enabled |= engagement_active

    self.drive_distance += distance_driven
    self.drive_time += DT_MDL

    self.tracked_time += DT_MDL

    if time_validated:
      current_month = now.month
      if current_month != self.frogpilot_stats.get("Month"):
        self.frogpilot_stats["Month"] = current_month
        self.frogpilot_stats["CurrentMonthsMeters"] = 0

    increment_stat(self.frogpilot_stats, "CurrentMonthsMeters", distance_driven)

    self.frogpilot_stats["MaxAcceleration"] = max(self.frogpilot_events.max_acceleration, self.frogpilot_stats.get("MaxAcceleration", 0))

    current_alert = sm["selfdriveState"].alertType
    if current_alert and current_alert != self.previous_alert:
      alert_name = current_alert.partition("/")[0]

      total_events = self.frogpilot_stats.get("TotalEvents", {})
      total_events[alert_name] = total_events.get(alert_name, 0) + 1

      self.frogpilot_stats["TotalEvents"] = total_events
    self.previous_alert = current_alert

    if sm["selfdriveState"].enabled:
      increment_bucket(self.frogpilot_stats, "CruiseSpeedTimes", speed_bucket_kph(v_cruise_kph), DT_MDL)

    if engagement_active != self.previous_active:
      if engagement_active:
        increment_stat(self.frogpilot_stats, "Engages", 1)
        if frogpilot_toggles.sound_pack == "frog":
          increment_stat(self.frogpilot_stats, "FrogChirps", 1)
      else:
        increment_stat(self.frogpilot_stats, "Disengages", 1)
        if frogpilot_toggles.sound_pack == "frog":
          increment_stat(self.frogpilot_stats, "FrogSqueaks", 1)

      self.previous_active = engagement_active

    if sm["selfdriveState"].state != self.previous_state:
      if sm["selfdriveState"].state == State.overriding:
        increment_stat(self.frogpilot_stats, "Overrides", 1)

      self.previous_state = sm["selfdriveState"].state

    if engagement_active and sm["selfdriveState"].experimentalMode:
      increment_stat(self.frogpilot_stats, "ExperimentalModeMeters", distance_driven)
      increment_stat(self.frogpilot_stats, "ExperimentalModeTime", DT_MDL)

    increment_stat(self.frogpilot_stats, "FrogPilotMeters", distance_driven)

    if sm["frogpilotSelfdriveState"].alertSound != self.previous_sound:
      if sm["frogpilotSelfdriveState"].alertSound == FrogPilotAudibleAlert.goat:
        increment_stat(self.frogpilot_stats, "GoatScreams", 1)

      self.previous_sound = sm["frogpilotSelfdriveState"].alertSound

    if sm["carControl"].latActive:
      increment_stat(self.frogpilot_stats, "LateralMeters", distance_driven)
      increment_stat(self.frogpilot_stats, "LateralTime", DT_MDL)
    if sm["carControl"].longActive:
      increment_stat(self.frogpilot_stats, "LongitudinalMeters", distance_driven)
      increment_stat(self.frogpilot_stats, "LongitudinalTime", DT_MDL)

      personality_name = LONGITUDINAL_PERSONALITY_MAP.get(sm["selfdriveState"].personality, "Unknown").capitalize()
      increment_bucket(self.frogpilot_stats, "PersonalityTimes", personality_name, DT_MDL)
    elif sm["frogpilotCarState"].alwaysOnLateralEnabled:
      increment_stat(self.frogpilot_stats, "AOLMeters", distance_driven)
      increment_stat(self.frogpilot_stats, "AOLTime", DT_MDL)

    if sm["selfdriveState"].state in (State.disabled, State.overriding):
      self.distance_since_override = 0
      increment_stat(self.frogpilot_stats, "OverrideTime", DT_MDL)
    else:
      self.distance_since_override += distance_driven
      self.frogpilot_stats["LongestDistanceWithoutOverride"] = max(self.distance_since_override, self.frogpilot_stats.get("LongestDistanceWithoutOverride", 0))

    current_random_events = {event for event in self.frogpilot_events.events.names if RANDOM_EVENT_START <= event <= RANDOM_EVENT_END}
    for event in current_random_events - self.previous_random_events:
      increment_bucket(self.frogpilot_stats, "RandomEvents", FROGPILOT_EVENT_NAME[event])
    self.previous_random_events = current_random_events

    if sm["carState"].standstill:
      increment_stat(self.frogpilot_stats, "StandstillTime", DT_MDL)
      if not self.previous_standstill:
        increment_stat(self.frogpilot_stats, "StandstillEvents", 1)

      if self.frogpilot_events.stopped_for_light:
        increment_stat(self.frogpilot_stats, "StopLightTime", DT_MDL)
        if not self.previous_stoplight:
          increment_stat(self.frogpilot_stats, "StopLightStops", 1)
      self.previous_stoplight = self.frogpilot_events.stopped_for_light
    else:
      self.previous_stoplight = False
    self.previous_standstill = sm["carState"].standstill

    if self.frogpilot_weather.api_25_calls:
      increment_bucket(self.frogpilot_stats, "WeatherAPICalls", "2.5", self.frogpilot_weather.api_25_calls)

      self.frogpilot_weather.api_25_calls = 0
    if self.frogpilot_weather.api_3_calls:
      increment_bucket(self.frogpilot_stats, "WeatherAPICalls", "3.0", self.frogpilot_weather.api_3_calls)

      self.frogpilot_weather.api_3_calls = 0

    if self.frogpilot_weather.sunrise != 0 and self.frogpilot_weather.sunset != 0:
      if self.frogpilot_weather.is_daytime:
        increment_stat(self.frogpilot_stats, "DayMeters", distance_driven)
        increment_stat(self.frogpilot_stats, "DayTime", DT_MDL)
      else:
        increment_stat(self.frogpilot_stats, "NightMeters", distance_driven)
        increment_stat(self.frogpilot_stats, "NightTime", DT_MDL)

    if self.frogpilot_weather.sunrise != 0 and self.frogpilot_weather.sunset != 0:
      suffix = "unknown"
      for category in WEATHER_CATEGORIES.values():
        if any(start <= self.frogpilot_weather.weather_id <= end for start, end in category["ranges"]):
          suffix = category["suffix"]
          break

      increment_bucket(self.frogpilot_stats, "WeatherTimes", suffix, DT_MDL)

    if self.tracked_time >= 60 and sm["carState"].standstill and self.previously_enabled:
      self.write_stats()

  def write_stats(self):
    increment_bucket(self.frogpilot_stats, "ModelTimes", self.model_name, self.tracked_time)

    increment_stat(self.frogpilot_stats, "FrogPilotSeconds", self.tracked_time)
    increment_stat(self.frogpilot_stats, "TrackedTime", self.tracked_time)

    self.tracked_time = 0

    if not self.drive_added:
      increment_stat(self.frogpilot_stats, "FrogPilotDrives", 1)

      self.drive_added = True

    self.frogpilot_stats["LongestDriveDistance"] = max(self.drive_distance, self.frogpilot_stats.get("LongestDriveDistance", 0))
    self.frogpilot_stats["LongestDriveDuration"] = max(self.drive_time, self.frogpilot_stats.get("LongestDriveDuration", 0))

    self.params.put("FrogPilotStats", dict(sorted(self.frogpilot_stats.items())))
