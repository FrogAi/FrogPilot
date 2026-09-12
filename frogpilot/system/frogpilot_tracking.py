#!/usr/bin/env python3
from openpilot.common.constants import CV
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.car.cruise import V_CRUISE_MAX
from openpilot.selfdrive.selfdrived.events import FROGPILOT_EVENT_NAME
from openpilot.selfdrive.selfdrived.selfdrived import LONGITUDINAL_PERSONALITY_MAP, State
from openpilot.selfdrive.selfdrived.state import ACTIVE_STATES
from openpilot.selfdrive.ui.soundd import FrogPilotAudibleAlert

from openpilot.frogpilot.common.frogpilot_utilities import clean_model_name
from openpilot.frogpilot.common.frogpilot_variables import THEME_SAVE_PATH
from openpilot.frogpilot.controls.lib.frogpilot_events import RANDOM_EVENT_END, RANDOM_EVENT_START
from openpilot.frogpilot.controls.lib.weather_checker import weather_category

class FrogPilotTracking:
  def __init__(self, frogpilot_planner, frogpilot_toggles):
    self.params = frogpilot_planner.params

    self.frogpilot_events = frogpilot_planner.frogpilot_events
    self.frogpilot_weather = frogpilot_planner.frogpilot_weather

    self.frogpilot_stats = self.params.get("FrogPilotStats")
    self.frogpilot_stats.pop("CurrentMonthsKilometers", None)
    self.frogpilot_stats.pop("ResetStats", None)

    self.drive_added = False
    self.previously_enabled = False

    self.distance_since_override = 0
    self.drive_time = 0
    self.frog_hop_time = 0
    self.tracked_time = 0

    self.previous_random_events = set()

    self.previous_alert = None
    self.previous_sound = FrogPilotAudibleAlert.none
    self.previous_state = State.disabled

    self.model_name = clean_model_name(frogpilot_toggles.model_name)

    self.frog_hop_duration = 0
    signal_path = THEME_SAVE_PATH / "theme_packs/frog/signals"
    signal_frames = sum("blindspot" not in path.name.lower() for path in signal_path.glob("*.png"))
    signal_intervals = sorted(signal_path.glob("traditional_*"))
    if signal_intervals:
      self.frog_hop_duration = signal_frames * int(signal_intervals[-1].name.split("_")[1])

  def save_stats(self):
    self.frogpilot_stats["FrogPilotSeconds"] = self.frogpilot_stats.get("FrogPilotSeconds", 0) + self.tracked_time

    total_model_times = self.frogpilot_stats.get("ModelTimes", {})
    total_model_times[self.model_name] = total_model_times.get(self.model_name, 0) + self.tracked_time
    self.frogpilot_stats["ModelTimes"] = total_model_times

    self.frogpilot_stats["TrackedTime"] = self.frogpilot_stats.get("TrackedTime", 0) + self.tracked_time
    self.tracked_time = 0

    self.params.put_nonblocking("FrogPilotStats", dict(sorted(self.frogpilot_stats.items())))

  def update(self, now, time_validated, sm, frogpilot_toggles):
    v_cruise = min(sm["carState"].vCruiseCluster, V_CRUISE_MAX) * CV.KPH_TO_MS
    v_ego = max(sm["carState"].vEgo, 0)

    distance_driven = v_ego * DT_MDL
    self.previously_enabled |= sm["selfdriveState"].enabled or sm["frogpilotCarState"].alwaysOnLateralEnabled
    self.drive_time += DT_MDL
    self.tracked_time += DT_MDL

    if sm["selfdriveState"].alertType not in (self.previous_alert, ""):
      alert_name = sm["selfdriveState"].alertType.split('/')[0]
      total_events = self.frogpilot_stats.get("TotalEvents", {})
      total_events[alert_name] = total_events.get(alert_name, 0) + 1
      self.frogpilot_stats["TotalEvents"] = total_events
    self.previous_alert = sm["selfdriveState"].alertType

    if sm["selfdriveState"].enabled:
      key = str(round(v_cruise, 2))
      total_cruise_speed_times = self.frogpilot_stats.get("CruiseSpeedTimes", {})
      total_cruise_speed_times[key] = total_cruise_speed_times.get(key, 0) + DT_MDL
      self.frogpilot_stats["CruiseSpeedTimes"] = total_cruise_speed_times

    if time_validated and now.month != self.frogpilot_stats.get("Month"):
      self.frogpilot_stats.update({"CurrentMonthsMeters": 0, "Month": now.month})
    self.frogpilot_stats["CurrentMonthsMeters"] = self.frogpilot_stats.get("CurrentMonthsMeters", 0) + distance_driven

    if self.frogpilot_weather.sunrise != 0 and self.frogpilot_weather.sunset != 0:
      if self.frogpilot_weather.is_daytime:
        self.frogpilot_stats["DayTime"] = self.frogpilot_stats.get("DayTime", 0) + DT_MDL
      else:
        self.frogpilot_stats["NightTime"] = self.frogpilot_stats.get("NightTime", 0) + DT_MDL

    if sm["selfdriveState"].state != self.previous_state:
      if sm["selfdriveState"].state in ACTIVE_STATES and self.previous_state not in ACTIVE_STATES:
        self.frogpilot_stats["Engages"] = self.frogpilot_stats.get("Engages", 0) + 1
        if frogpilot_toggles.sound_pack == "frog":
          self.frogpilot_stats["FrogChirps"] = self.frogpilot_stats.get("FrogChirps", 0) + 1

      elif sm["selfdriveState"].state == State.disabled and self.previous_state in ACTIVE_STATES:
        self.frogpilot_stats["Disengages"] = self.frogpilot_stats.get("Disengages", 0) + 1
        if frogpilot_toggles.sound_pack == "frog":
          self.frogpilot_stats["FrogSqueaks"] = self.frogpilot_stats.get("FrogSqueaks", 0) + 1

      if sm["selfdriveState"].state == State.overriding and self.previous_state != State.overriding:
        self.frogpilot_stats["Overrides"] = self.frogpilot_stats.get("Overrides", 0) + 1

      self.previous_state = sm["selfdriveState"].state

    if sm["selfdriveState"].experimentalMode:
      self.frogpilot_stats["ExperimentalModeTime"] = self.frogpilot_stats.get("ExperimentalModeTime", 0) + DT_MDL

    if frogpilot_toggles.signal_icons == "frog" and self.frog_hop_duration > 0:
      if sm["carState"].leftBlinker or sm["carState"].rightBlinker:
        self.frog_hop_time += int(DT_MDL * 1000)
        frog_hops = self.frog_hop_time // self.frog_hop_duration
        if frog_hops > 0:
          self.frogpilot_stats["FrogHops"] = self.frogpilot_stats.get("FrogHops", 0) + frog_hops
          self.frog_hop_time -= frog_hops * self.frog_hop_duration
    else:
      self.frog_hop_time = 0

    self.frogpilot_stats["FrogPilotMeters"] = self.frogpilot_stats.get("FrogPilotMeters", 0) + distance_driven

    if sm["frogpilotSelfdriveState"].alertSound != self.previous_sound:
      if sm["frogpilotSelfdriveState"].alertSound == FrogPilotAudibleAlert.goat:
        self.frogpilot_stats["GoatScreams"] = self.frogpilot_stats.get("GoatScreams", 0) + 1

      self.previous_sound = sm["frogpilotSelfdriveState"].alertSound

    self.frogpilot_stats["MaxAcceleration"] = max(self.frogpilot_events.max_acceleration, self.frogpilot_stats.get("MaxAcceleration", 0))

    if sm["carControl"].latActive:
      self.frogpilot_stats["LateralTime"] = self.frogpilot_stats.get("LateralTime", 0) + DT_MDL
    if sm["carControl"].longActive:
      self.frogpilot_stats["LongitudinalTime"] = self.frogpilot_stats.get("LongitudinalTime", 0) + DT_MDL

      personality_name = LONGITUDINAL_PERSONALITY_MAP.get(sm["selfdriveState"].personality, "Unknown").capitalize()
      total_personality_times = self.frogpilot_stats.get("PersonalityTimes", {})
      total_personality_times[personality_name] = total_personality_times.get(personality_name, 0) + DT_MDL
      self.frogpilot_stats["PersonalityTimes"] = total_personality_times
    elif sm["frogpilotCarState"].alwaysOnLateralEnabled:
      self.frogpilot_stats["AOLTime"] = self.frogpilot_stats.get("AOLTime", 0) + DT_MDL

    if sm["selfdriveState"].state in (State.disabled, State.overriding):
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

    weather_times = self.frogpilot_stats.get("WeatherTimes", {})
    category = weather_category(self.frogpilot_weather.weather_id)
    weather_times[category] = weather_times.get(category, 0) + DT_MDL
    self.frogpilot_stats["WeatherTimes"] = weather_times

    if not self.drive_added and self.drive_time >= 60 and self.previously_enabled:
      self.frogpilot_stats["FrogPilotDrives"] = self.frogpilot_stats.get("FrogPilotDrives", 0) + 1
      self.drive_added = True

    if self.tracked_time >= 60 and sm["carState"].standstill:
      self.save_stats()
