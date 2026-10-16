#!/usr/bin/env python3
from openpilot.common.constants import CV
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.car.cruise import V_CRUISE_MAX
from openpilot.selfdrive.selfdrived.events import FROGPILOT_EVENT_NAME, FrogPilotAudibleAlert, FrogPilotEventName
from openpilot.selfdrive.selfdrived.selfdrived import LONGITUDINAL_PERSONALITY_MAP, State
from openpilot.selfdrive.selfdrived.state import ACTIVE_STATES

from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.controls.lib.weather_checker import weather_category

class FrogPilotTracking:
  def __init__(self, frogpilot_planner, frogpilot_toggles):
    self.params = frogpilot_planner.params

    self.frogpilot_events = frogpilot_planner.frogpilot_events
    self.frogpilot_weather = frogpilot_planner.frogpilot_weather

    self.frogpilot_stats = self.params.get("FrogPilotStats")
    self.frogpilot_stats.pop("CurrentMonthsKilometers", None)
    self.frogpilot_stats.pop("ResetStats", None)

    weather_api_calls = self.frogpilot_stats.pop("WeatherAPICalls", {})
    self.add("WeatherCalls", sum(weather_api_calls.values()))

    self.blinker_previously_on = False
    self.drive_added = False
    self.frog_signals_active = False
    self.previously_enabled = False

    self.distance_since_override = 0
    self.drive_time = 0
    self.frog_hop_time = 0
    self.tracked_time = 0

    self.previous_random_events = set()

    self.previous_alert = None
    self.previous_sound = FrogPilotAudibleAlert.none
    self.previous_state = State.disabled

    self.model_name = frogpilot_toggles.model_name

    signal_path = frogpilot_variables.FROG_THEME_PATH / "signals"
    signal_frames = 0
    for path in signal_path.glob("*"):
      if path.suffix.lower() in {".png", ".svg"} and "blindspot" not in path.name.lower():
        signal_frames += 1
    signal_intervals = sorted(signal_path.glob("traditional_*"))
    self.frog_hop_duration = signal_frames * int(signal_intervals[-1].name.split("_")[1]) if signal_intervals else 0

  def add(self, key, amount=1):
    self.frogpilot_stats[key] = self.frogpilot_stats.get(key, 0) + amount

  def add_to(self, group, key, amount=1):
    totals = self.frogpilot_stats.setdefault(group, {})
    totals[key] = totals.get(key, 0) + amount

  def frog_theme_active(self, component):
    return (frogpilot_variables.ACTIVE_THEME_PATH / component).resolve() == (frogpilot_variables.FROG_THEME_PATH / component).resolve()

  def save_stats(self, blocking=False):
    self.add("FrogPilotSeconds", self.tracked_time)
    self.add_to("ModelTimes", self.model_name, self.tracked_time)
    self.add("TrackedTime", self.tracked_time)
    self.tracked_time = 0

    if blocking:
      self.params.wait_for_pending_writes()
      if self.params.put("FrogPilotStats", dict(sorted(self.frogpilot_stats.items()))) != 0:
        raise RuntimeError("Unable to save FrogPilotStats")
    else:
      self.params.put_nonblocking("FrogPilotStats", dict(sorted(self.frogpilot_stats.items())))

  def update(self, now, time_validated, sm):
    v_cruise = min(sm["carState"].vCruiseCluster, V_CRUISE_MAX) * CV.KPH_TO_MS
    v_ego = max(sm["carState"].vEgo, 0)

    distance_driven = v_ego * DT_MDL
    self.previously_enabled |= sm["selfdriveState"].enabled or sm["frogpilotCarState"].alwaysOnLateralEnabled
    self.drive_time += DT_MDL
    self.tracked_time += DT_MDL

    if sm["selfdriveState"].alertType not in (self.previous_alert, ""):
      self.add_to("TotalEvents", sm["selfdriveState"].alertType.split('/')[0])
    self.previous_alert = sm["selfdriveState"].alertType

    if sm["selfdriveState"].enabled:
      self.add_to("CruiseSpeedTimes", str(round(v_cruise, 2)), DT_MDL)

    if time_validated and now.month != self.frogpilot_stats.get("Month"):
      self.frogpilot_stats.update({"CurrentMonthsMeters": 0, "Month": now.month})
    self.add("CurrentMonthsMeters", distance_driven)

    if self.frogpilot_weather.sunrise != 0 and self.frogpilot_weather.sunset != 0:
      if self.frogpilot_weather.is_daytime:
        self.add("DayTime", DT_MDL)
      else:
        self.add("NightTime", DT_MDL)

    if sm["selfdriveState"].state != self.previous_state:
      if sm["selfdriveState"].state in ACTIVE_STATES and self.previous_state not in ACTIVE_STATES:
        self.add("Engages")
        if self.frog_theme_active("sounds"):
          self.add("FrogChirps")

      elif sm["selfdriveState"].state == State.disabled and self.previous_state in ACTIVE_STATES:
        self.add("Disengages")
        if self.frog_theme_active("sounds"):
          self.add("FrogSqueaks")

      if sm["selfdriveState"].state == State.overriding:
        self.add("Overrides")

      self.previous_state = sm["selfdriveState"].state

    if sm["selfdriveState"].enabled or sm["frogpilotCarState"].alwaysOnLateralEnabled:
      self.add("EngagedTime", DT_MDL)

    if sm["selfdriveState"].experimentalMode and sm["carControl"].longActive:
      self.add("ExperimentalModeTime", DT_MDL)

    blinker_on = sm["carState"].leftBlinker or sm["carState"].rightBlinker
    if blinker_on and not self.blinker_previously_on:
      self.frog_signals_active = self.frog_theme_active("signals")
    self.blinker_previously_on = blinker_on

    if blinker_on and self.frog_signals_active and self.frog_hop_duration > 0:
      self.frog_hop_time += int(DT_MDL * 1000)
      frog_hops = self.frog_hop_time // self.frog_hop_duration

      if frog_hops > 0:
        self.add("FrogHops", frog_hops)
        self.frog_hop_time -= frog_hops * self.frog_hop_duration
    else:
      self.frog_hop_time = 0

    self.add("FrogPilotMeters", distance_driven)

    if sm["frogpilotSelfdriveState"].alertSound != self.previous_sound:
      if sm["frogpilotSelfdriveState"].alertSound == FrogPilotAudibleAlert.goat:
        self.add("GoatScreams")

      self.previous_sound = sm["frogpilotSelfdriveState"].alertSound

    self.frogpilot_stats["MaxAcceleration"] = max(self.frogpilot_events.max_acceleration, self.frogpilot_stats.get("MaxAcceleration", 0))

    if sm["carControl"].latActive:
      self.add("LateralTime", DT_MDL)
    if sm["carControl"].longActive:
      self.add("LongitudinalTime", DT_MDL)
      self.add_to("PersonalityTimes", LONGITUDINAL_PERSONALITY_MAP.get(sm["selfdriveState"].personality.raw, "Unknown").capitalize(), DT_MDL)
    elif sm["frogpilotCarState"].alwaysOnLateralEnabled:
      self.add("AOLTime", DT_MDL)

    if sm["selfdriveState"].state in (State.disabled, State.overriding):
      self.distance_since_override = 0
      self.add("OverrideTime", DT_MDL)
    else:
      self.distance_since_override += distance_driven
      self.frogpilot_stats["LongestDistanceWithoutOverride"] = max(self.distance_since_override, self.frogpilot_stats.get("LongestDistanceWithoutOverride", 0))

    current_random_events = {event for event in self.frogpilot_events.events.names if frogpilot_variables.RANDOM_EVENT_START <= event <= frogpilot_variables.RANDOM_EVENT_END or event == FrogPilotEventName.goatSteerSaturated}
    for event in current_random_events - self.previous_random_events:
      self.add_to("RandomEvents", FROGPILOT_EVENT_NAME[event])

    self.previous_random_events = current_random_events

    if sm["carState"].standstill:
      self.add("StandstillTime", DT_MDL)
      if self.frogpilot_events.stopped_for_light:
        self.add("StopLightTime", DT_MDL)

    self.add("WeatherCalls", self.frogpilot_weather.weather_calls)
    self.frogpilot_weather.weather_calls = 0

    self.add_to("WeatherTimes", weather_category(self.frogpilot_weather.weather_id), DT_MDL)

    if not self.drive_added and self.drive_time >= 60 and self.previously_enabled:
      self.add("FrogPilotDrives")
      self.drive_added = True

    if self.tracked_time >= 60 and sm["carState"].standstill:
      self.save_stats()
