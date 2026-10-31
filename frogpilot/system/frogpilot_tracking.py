#!/usr/bin/env python3
import json

from cereal import custom, log
from openpilot.common.conversions import Conversions as CV
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.drive_helpers import ACTIVE_STATES, V_CRUISE_MAX
from openpilot.selfdrive.controls.lib.events import FROGPILOT_EVENT_NAME

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.controls.lib.weather_checker import weather_category

State = log.ControlsState.OpenpilotState
FrogPilotEventName = custom.FrogPilotCarEvent.EventName
FrogPilotAudibleAlert = custom.FrogPilotCarControl.HUDControl.AudibleAlert

class FrogPilotTracking:
  def __init__(self, frogpilot_planner, frogpilot_toggles):
    self.frogpilot_events = frogpilot_planner.frogpilot_events
    self.frogpilot_weather = frogpilot_planner.frogpilot_weather

    self.frogpilot_stats = json.loads(frogpilot_variables.params.get("FrogPilotStats") or "{}")
    self.frogpilot_stats.pop("CurrentMonthsKilometers", None)
    self.frogpilot_stats.pop("ResetStats", None)

    weather_api_calls = self.frogpilot_stats.pop("WeatherAPICalls", {})
    self.frogpilot_stats["WeatherCalls"] = self.frogpilot_stats.get("WeatherCalls", 0) + sum(weather_api_calls.values())

    self.blinker_previously_on = False
    self.drive_added = False
    self.frog_signals_active = False
    self.previously_enabled = False

    self.distance_since_override = 0
    self.drive_time = 0
    self.frog_hop_time = 0
    self.tracked_time = 0

    self.previous_random_events = set()

    self.personality_map = {key: key.capitalize() for key in log.LongitudinalPersonality.schema.enumerants.keys()}

    self.previous_alert = None
    self.previous_sound = FrogPilotAudibleAlert.none
    self.previous_state = State.disabled

    self.model_name = frogpilot_utilities.clean_model_name(frogpilot_toggles.model_name)

    self.frog_hop_duration = 0

    signal_path = frogpilot_paths.FROG_THEME_PATH / "signals"
    signal_files = [*signal_path.glob("*.png"), *signal_path.glob("*.svg")]
    signal_frames = sum("blindspot" not in path.name.lower() for path in signal_files)
    signal_intervals = sorted(signal_path.glob("traditional_*"))
    if signal_intervals:
      self.frog_hop_duration = signal_frames * int(signal_intervals[-1].name.split('_')[1])

  def frog_theme_active(self, component):
    return (frogpilot_paths.ACTIVE_THEME_PATH / component).resolve() == (frogpilot_paths.FROG_THEME_PATH / component).resolve()

  def save_stats(self, blocking=False):
    self.frogpilot_stats["FrogPilotSeconds"] = self.frogpilot_stats.get("FrogPilotSeconds", 0) + self.tracked_time

    total_model_times = self.frogpilot_stats.get("ModelTimes", {})
    total_model_times[self.model_name] = total_model_times.get(self.model_name, 0) + self.tracked_time
    self.frogpilot_stats["ModelTimes"] = total_model_times

    self.frogpilot_stats["TrackedTime"] = self.frogpilot_stats.get("TrackedTime", 0) + self.tracked_time
    self.tracked_time = 0

    if blocking:
      frogpilot_variables.params.wait_for_pending_writes()
      if frogpilot_variables.params.put("FrogPilotStats", json.dumps(self.frogpilot_stats)) != 0:
        raise RuntimeError("Unable to save FrogPilotStats")
    else:
      frogpilot_variables.params.put_nonblocking("FrogPilotStats", json.dumps(self.frogpilot_stats))

  def update(self, now, time_validated, sm):
    v_cruise = min(sm["controlsState"].vCruiseCluster, V_CRUISE_MAX) * CV.KPH_TO_MS
    v_ego = max(sm["carState"].vEgo, 0)

    distance_driven = v_ego * DT_MDL

    self.previously_enabled |= sm["controlsState"].enabled or sm["frogpilotCarState"].alwaysOnLateralEnabled

    self.drive_time += DT_MDL
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

    if time_validated and now.month != self.frogpilot_stats.get("Month"):
      self.frogpilot_stats.update({
        "CurrentMonthsMeters": 0,
        "Month": now.month
      })
    self.frogpilot_stats["CurrentMonthsMeters"] = self.frogpilot_stats.get("CurrentMonthsMeters", 0) + distance_driven

    if self.frogpilot_weather.weather_id != 0 and self.frogpilot_weather.sunrise != 0 and self.frogpilot_weather.sunset != 0:
      if self.frogpilot_weather.is_daytime:
        self.frogpilot_stats["DayTime"] = self.frogpilot_stats.get("DayTime", 0) + DT_MDL
      else:
        self.frogpilot_stats["NightTime"] = self.frogpilot_stats.get("NightTime", 0) + DT_MDL

    if sm["controlsState"].state != self.previous_state:
      if sm["controlsState"].state in ACTIVE_STATES and self.previous_state not in ACTIVE_STATES:
        self.frogpilot_stats["Engages"] = self.frogpilot_stats.get("Engages", 0) + 1
        if self.frog_theme_active("sounds"):
          self.frogpilot_stats["FrogChirps"] = self.frogpilot_stats.get("FrogChirps", 0) + 1

      elif sm["controlsState"].state == State.disabled and self.previous_state in ACTIVE_STATES:
        self.frogpilot_stats["Disengages"] = self.frogpilot_stats.get("Disengages", 0) + 1
        if self.frog_theme_active("sounds"):
          self.frogpilot_stats["FrogSqueaks"] = self.frogpilot_stats.get("FrogSqueaks", 0) + 1

      if sm["controlsState"].state == State.overriding and self.previous_state != State.overriding:
        self.frogpilot_stats["Overrides"] = self.frogpilot_stats.get("Overrides", 0) + 1

      self.previous_state = sm["controlsState"].state

    if sm["controlsState"].experimentalMode and sm["carControl"].longActive:
      self.frogpilot_stats["ExperimentalModeTime"] = self.frogpilot_stats.get("ExperimentalModeTime", 0) + DT_MDL

    blinker_on = sm["carState"].leftBlinker or sm["carState"].rightBlinker
    if blinker_on and not self.blinker_previously_on:
      self.frog_signals_active = self.frog_theme_active("signals")
    self.blinker_previously_on = blinker_on

    if blinker_on and self.frog_signals_active and self.frog_hop_duration > 0:
      self.frog_hop_time += int(DT_MDL * 1000)
      frog_hops = self.frog_hop_time // self.frog_hop_duration

      if frog_hops > 0:
        self.frogpilot_stats["FrogHops"] = self.frogpilot_stats.get("FrogHops", 0) + frog_hops
        self.frog_hop_time -= frog_hops * self.frog_hop_duration
    else:
      self.frog_hop_time = 0

    self.frogpilot_stats["FrogPilotMeters"] = self.frogpilot_stats.get("FrogPilotMeters", 0) + distance_driven

    if sm["frogpilotControlsState"].alertSound != self.previous_sound:
      if sm["frogpilotControlsState"].alertSound == FrogPilotAudibleAlert.goat:
        self.frogpilot_stats["GoatScreams"] = self.frogpilot_stats.get("GoatScreams", 0) + 1

      self.previous_sound = sm["frogpilotControlsState"].alertSound

    self.frogpilot_stats["MaxAcceleration"] = max(self.frogpilot_events.max_acceleration, self.frogpilot_stats.get("MaxAcceleration", 0))

    if sm["carControl"].latActive or sm["carControl"].longActive:
      self.frogpilot_stats["EngagedTime"] = self.frogpilot_stats.get("EngagedTime", 0) + DT_MDL

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

    current_random_events = {event for event in self.frogpilot_events.events.names
                             if frogpilot_variables.RANDOM_EVENT_START <= event <= frogpilot_variables.RANDOM_EVENT_END
                             or event == FrogPilotEventName.goatSteerSaturated}
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

    self.frogpilot_stats["WeatherCalls"] = self.frogpilot_stats.get("WeatherCalls", 0) + self.frogpilot_weather.weather_calls
    self.frogpilot_weather.weather_calls = 0

    weather_times = self.frogpilot_stats.get("WeatherTimes", {})
    category = weather_category(self.frogpilot_weather.weather_id)
    weather_times[category] = weather_times.get(category, 0) + DT_MDL
    self.frogpilot_stats["WeatherTimes"] = weather_times

    if not self.drive_added and self.drive_time >= 60 and self.previously_enabled:
      self.frogpilot_stats["FrogPilotDrives"] = self.frogpilot_stats.get("FrogPilotDrives", 0) + 1
      self.drive_added = True

    if self.tracked_time >= 60 and sm["carState"].standstill:
      self.save_stats()
