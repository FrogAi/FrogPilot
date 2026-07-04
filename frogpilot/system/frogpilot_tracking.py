#!/usr/bin/env python3
import json

from cereal import log
from openpilot.common.conversions import Conversions as CV
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.controlsd import ACTIVE_STATES, FrogPilotEventName, State
from openpilot.selfdrive.controls.lib.drive_helpers import V_CRUISE_MAX
from openpilot.selfdrive.controls.lib.events import FROGPILOT_EVENT_NAME
from openpilot.selfdrive.ui.soundd import FrogPilotAudibleAlert

from openpilot.frogpilot.common.frogpilot_utilities import clean_model_name
from openpilot.frogpilot.common.frogpilot_variables import params
from openpilot.frogpilot.controls.lib.weather_checker import WEATHER_CATEGORIES

RANDOM_EVENT_START = FrogPilotEventName.accel30
RANDOM_EVENT_END = FrogPilotEventName.youveGotMail

class FrogPilotTracking:
  def __init__(self, frogpilot_planner, frogpilot_toggles):
    self.frogpilot_events = frogpilot_planner.frogpilot_events
    self.frogpilot_weather = frogpilot_planner.frogpilot_weather

    self.frogpilot_stats = json.loads(params.get("FrogPilotStats") or "{}")
    self.frogpilot_stats.setdefault("AOLTime", self.frogpilot_stats.get("TotalAOLTime", 0))
    self.frogpilot_stats.setdefault("LateralTime", self.frogpilot_stats.get("TotalLateralTime", 0))
    self.frogpilot_stats.setdefault("LongitudinalTime", self.frogpilot_stats.get("TotalLongitudinalTime", 0))
    self.frogpilot_stats.setdefault("TrackedTime", self.frogpilot_stats.get("TotalTrackedTime", 0))

    self.frogpilot_stats = {
      key: (
        {sub_key: sub_value for sub_key, sub_value in value.items() if sub_key.lower() != "unknown"}
        if isinstance(value, dict) else value
      )
      for key, value in self.frogpilot_stats.items()
      if not key.startswith("Total") and key.lower() != "unknown"
    }

    if "ResetStats" not in self.frogpilot_stats:
      self.frogpilot_stats["Disengages"] = 0
      self.frogpilot_stats["Engages"] = 0
      self.frogpilot_stats["FrogChirps"] = 0
      self.frogpilot_stats["FrogSqueaks"] = 0
      self.frogpilot_stats["Overrides"] = 0
      self.frogpilot_stats["ResetStats"] = True

    self.frogpilot_stats.pop("CurrentMonthsKilometers", None)

    params.put("FrogPilotStats", json.dumps(self.frogpilot_stats))

    self.drive_added = False
    self.previously_enabled = False

    self.distance_since_override = 0
    self.tracked_time = 0

    self.previous_random_events = set()

    self.personality_map = {key: key.capitalize() for key in log.LongitudinalPersonality.schema.enumerants.keys()}

    self.previous_alert = None
    self.previous_sound = FrogPilotAudibleAlert.none
    self.previous_state = State.disabled

    self.model_name = clean_model_name(frogpilot_toggles.model_name)

  def update(self, now, time_validated, sm, frogpilot_toggles):
    v_cruise = min(sm["controlsState"].vCruiseCluster, V_CRUISE_MAX) * CV.KPH_TO_MS
    v_ego = max(sm["carState"].vEgo, 0)

    distance_driven = v_ego * DT_MDL
    self.previously_enabled |= sm["controlsState"].enabled or sm["frogpilotCarState"].alwaysOnLateralEnabled
    self.tracked_time += DT_MDL

    if sm["controlsState"].alertType not in (self.previous_alert, ""):
      alert_name = sm["controlsState"].alertType.split('/')[0]
      total_events = self.frogpilot_stats.get("TotalEvents", {})
      total_events[alert_name] = total_events.get(alert_name, 0) + 1
      self.frogpilot_stats["TotalEvents"] = total_events
    self.previous_alert = sm["controlsState"].alertType

    if sm["controlsState"].enabled:
      key = str(round(v_cruise, 2))
      total_cruise_speed_times = self.frogpilot_stats.get("CruiseSpeedTimes", {})
      total_cruise_speed_times[key] = total_cruise_speed_times.get(key, 0) + DT_MDL
      self.frogpilot_stats["CruiseSpeedTimes"] = total_cruise_speed_times

    self.frogpilot_stats["CurrentMonthsMeters"] = self.frogpilot_stats.get("CurrentMonthsMeters", 0) + distance_driven

    if self.frogpilot_weather.sunrise != 0 and self.frogpilot_weather.sunset != 0:
      if self.frogpilot_weather.is_daytime:
        self.frogpilot_stats["DayTime"] = self.frogpilot_stats.get("DayTime", 0) + DT_MDL
      else:
        self.frogpilot_stats["NightTime"] = self.frogpilot_stats.get("NightTime", 0) + DT_MDL

    if sm["controlsState"].state != self.previous_state:
      if sm["controlsState"].state in ACTIVE_STATES and self.previous_state not in ACTIVE_STATES:
        self.frogpilot_stats["Engages"] = self.frogpilot_stats.get("Engages", 0) + 1
        if frogpilot_toggles.sound_pack == "frog":
          self.frogpilot_stats["FrogChirps"] = self.frogpilot_stats.get("FrogChirps", 0) + 1

      elif sm["controlsState"].state == State.disabled and self.previous_state in ACTIVE_STATES:
        self.frogpilot_stats["Disengages"] = self.frogpilot_stats.get("Disengages", 0) + 1
        if frogpilot_toggles.sound_pack == "frog":
          self.frogpilot_stats["FrogSqueaks"] = self.frogpilot_stats.get("FrogSqueaks", 0) + 1

      if sm["controlsState"].state == State.overriding and self.previous_state != State.overriding:
        self.frogpilot_stats["Overrides"] = self.frogpilot_stats.get("Overrides", 0) + 1

      self.previous_state = sm["controlsState"].state

    if sm["controlsState"].experimentalMode:
      self.frogpilot_stats["ExperimentalModeTime"] = self.frogpilot_stats.get("ExperimentalModeTime", 0) + DT_MDL

    self.frogpilot_stats["FrogPilotMeters"] = self.frogpilot_stats.get("FrogPilotMeters", 0) + distance_driven

    if sm["frogpilotControlsState"].alertSound != self.previous_sound:
      if sm["frogpilotControlsState"].alertSound == FrogPilotAudibleAlert.goat:
        self.frogpilot_stats["GoatScreams"] = self.frogpilot_stats.get("GoatScreams", 0) + 1

      self.previous_sound = sm["frogpilotControlsState"].alertSound

    self.frogpilot_stats["MaxAcceleration"] = max(self.frogpilot_events.max_acceleration, self.frogpilot_stats.get("MaxAcceleration", 0))

    if sm["carControl"].latActive:
      self.frogpilot_stats["LateralTime"] = self.frogpilot_stats.get("LateralTime", 0) + DT_MDL
    if sm["carControl"].longActive:
      self.frogpilot_stats["LongitudinalTime"] = self.frogpilot_stats.get("LongitudinalTime", 0) + DT_MDL

      personality_name = self.personality_map.get(str(sm["controlsState"].personality), "unknown")
      total_personality_times = self.frogpilot_stats.get("PersonalityTimes", {})
      total_personality_times[personality_name] = total_personality_times.get(personality_name, 0) + DT_MDL
      self.frogpilot_stats["PersonalityTimes"] = total_personality_times
    elif sm["frogpilotCarState"].alwaysOnLateralEnabled:
      self.frogpilot_stats["AOLTime"] = self.frogpilot_stats.get("AOLTime", 0) + DT_MDL

    if sm["controlsState"].state in (State.disabled, State.overriding):
      self.distance_since_override = 0
      self.frogpilot_stats["OverrideTime"] = self.frogpilot_stats.get("OverrideTime", 0) + DT_MDL
    else:
      self.distance_since_override += distance_driven
      self.frogpilot_stats["LongestDistanceWithoutOverride"] = max(self.distance_since_override, self.frogpilot_stats.get("LongestDistanceWithoutOverride", 0))

    current_random_events = {event for event in self.frogpilot_events.events.names if RANDOM_EVENT_START <= event <= RANDOM_EVENT_END}
    if len(current_random_events) > 0:
      new_events = current_random_events - self.previous_random_events
      if new_events:
        total_random_events = self.frogpilot_stats.get("RandomEvents", {})
        for event in new_events:
          event_name = FROGPILOT_EVENT_NAME[event]
          total_random_events[event_name] = total_random_events.get(event_name, 0) + 1
        self.frogpilot_stats["RandomEvents"] = total_random_events

    self.previous_random_events = current_random_events

    if sm["carState"].standstill:
      self.frogpilot_stats["StandstillTime"] = self.frogpilot_stats.get("StandstillTime", 0) + DT_MDL
      if self.frogpilot_events.stopped_for_light:
        self.frogpilot_stats["StopLightTime"] = self.frogpilot_stats.get("StopLightTime", 0) + DT_MDL

    weather_api_calls = self.frogpilot_stats.get("WeatherAPICalls", {})
    weather_api_calls["2.5"] = weather_api_calls.get("2.5", 0) + self.frogpilot_weather.api_25_calls
    weather_api_calls["3.0"] = weather_api_calls.get("3.0", 0) + self.frogpilot_weather.api_3_calls
    self.frogpilot_stats["WeatherAPICalls"] = weather_api_calls

    self.frogpilot_weather.api_25_calls = 0
    self.frogpilot_weather.api_3_calls = 0

    suffix = "unknown"
    for category in WEATHER_CATEGORIES.values():
      if any(start <= self.frogpilot_weather.weather_id <= end for start, end in category["ranges"]):
        suffix = category["suffix"]
        break

    weather_times = self.frogpilot_stats.get("WeatherTimes", {})
    weather_times[suffix] = weather_times.get(suffix, 0) + DT_MDL
    self.frogpilot_stats["WeatherTimes"] = weather_times

    if self.tracked_time >= 60 and sm["carState"].standstill and self.previously_enabled:
      if time_validated:
        current_month = now.month
        if current_month != self.frogpilot_stats.get("Month"):
          self.frogpilot_stats.update({
            "CurrentMonthsMeters": 0,
            "Month": current_month
          })

      self.frogpilot_stats["FrogPilotSeconds"] = self.frogpilot_stats.get("FrogPilotSeconds", 0) + self.tracked_time

      current_model = self.model_name
      total_model_times = self.frogpilot_stats.get("ModelTimes", {})
      total_model_times[current_model] = total_model_times.get(current_model, 0) + self.tracked_time
      self.frogpilot_stats["ModelTimes"] = total_model_times

      self.frogpilot_stats["TrackedTime"] = self.frogpilot_stats.get("TrackedTime", 0) + self.tracked_time

      self.tracked_time = 0

      if not self.drive_added:
        self.frogpilot_stats["FrogPilotDrives"] = self.frogpilot_stats.get("FrogPilotDrives", 0) + 1
        self.drive_added = True

      params.put_nonblocking("FrogPilotStats", json.dumps(self.frogpilot_stats))
