#!/usr/bin/env python3
from openpilot.common.constants import CV
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.car.cruise import V_CRUISE_MAX
from openpilot.selfdrive.selfdrived.selfdrived import EventName, FrogPilotEventName, State
from openpilot.selfdrive.ui.soundd import FrogPilotAudibleAlert

from openpilot.frogpilot.common.frogpilot_utilities import clean_model_name

RANDOM_EVENTS = {
  FrogPilotEventName.accel30: "accel30",
  FrogPilotEventName.accel35: "accel35",
  FrogPilotEventName.accel40: "accel40",
  FrogPilotEventName.dejaVuCurve: "dejaVuCurve",
  FrogPilotEventName.firefoxSteerSaturated: "firefoxSteerSaturated",
  FrogPilotEventName.hal9000: "hal9000",
  FrogPilotEventName.openpilotCrashedRandomEvent: "openpilotCrashedRandomEvent",
  FrogPilotEventName.thisIsFineSteerSaturated: "thisIsFineSteerSaturated",
  FrogPilotEventName.toBeContinued: "toBeContinued",
  FrogPilotEventName.vCruise69: "vCruise69",
  FrogPilotEventName.yourFrogTriedToKillMe: "yourFrogTriedToKillMe",
  FrogPilotEventName.youveGotMail: "youveGotMail",
}

class FrogPilotTracking:
  def __init__(self, frogpilot_planner, params, frogpilot_toggles):
    self.frogpilot_events = frogpilot_planner.frogpilot_events

    self.frogpilot_stats = params.get("FrogPilotStats")

    self.drive_added = False
    self.enabled = False

    self.distance_since_override = 0
    self.tracked_time = 0

    self.previous_events = set()
    self.previous_random_events = set()

    self.previous_sound = FrogPilotAudibleAlert.none
    self.previous_state = State.disabled

    self.model_name = clean_model_name(frogpilot_toggles.model_name)

  def update(self, now, time_validated, params, sm, frogpilot_toggles):
    v_cruise = min(sm["carState"].vCruiseCluster, V_CRUISE_MAX) * CV.KPH_TO_MS
    v_ego = max(sm["carState"].vEgo, 0)

    distance_driven = v_ego * DT_MDL
    self.enabled |= sm["selfdriveState"].enabled or sm["frogpilotCarState"].alwaysOnLateralEnabled
    self.tracked_time += DT_MDL

    current_events = {event for event in self.frogpilot_events.event_names}
    if len(current_events) > 0:
      new_events = current_events - self.previous_events

      if new_events:
        if (EventName.fcw in self.frogpilot_events.event_names or EventName.stockAeb in self.frogpilot_events.event_names):
          self.frogpilot_stats["AEBEvents"] = self.frogpilot_stats.get("AEBEvents", 0) + 1

    self.previous_events = current_events

    if sm["selfdriveState"].enabled:
      key = str(round(v_cruise, 2))
      total_cruise_speed_times = self.frogpilot_stats.get("CruiseSpeedTimes", {})
      total_cruise_speed_times[key] = total_cruise_speed_times.get(key, 0) + DT_MDL
      self.frogpilot_stats["CruiseSpeedTimes"] = total_cruise_speed_times

    self.frogpilot_stats["CurrentMonthsKilometers"] = self.frogpilot_stats.get("CurrentMonthsKilometers", 0) + (distance_driven / 1000)

    if sm["selfdriveState"].state != self.previous_state:
      if sm["selfdriveState"].state == State.disabled:
        self.frogpilot_stats["Disengages"] = self.frogpilot_stats.get("Disengages", 0) + 1

        if frogpilot_toggles.sound_pack == "frog":
          self.frogpilot_stats["FrogSqueaks"] = self.frogpilot_stats.get("FrogSqueaks", 0) + 1
      elif sm["selfdriveState"].state == State.enabled:
        self.frogpilot_stats["Engages"] = self.frogpilot_stats.get("Engages", 0) + 1

        if frogpilot_toggles.sound_pack == "frog":
          self.frogpilot_stats["FrogChirps"] = self.frogpilot_stats.get("FrogChirps", 0) + 1
      elif sm["selfdriveState"].state == State.overriding:
        self.frogpilot_stats["Overrides"] = self.frogpilot_stats.get("Overrides", 0) + 1

      self.previous_state = sm["selfdriveState"].state

    if sm["selfdriveState"].experimentalMode:
      self.frogpilot_stats["ExperimentalModeTime"] = self.frogpilot_stats.get("ExperimentalModeTime", 0) + DT_MDL

    self.frogpilot_stats["FrogPilotMeters"] = self.frogpilot_stats.get("FrogPilotMeters", 0) + distance_driven

    if sm["frogpilotSelfdriveState"].alertSound != self.previous_sound:
      if sm["frogpilotSelfdriveState"].alertSound == FrogPilotAudibleAlert.goat:
        self.frogpilot_stats["GoatScreams"] = self.frogpilot_stats.get("GoatScreams", 0) + 1

      self.previous_sound = sm["frogpilotSelfdriveState"].alertSound

    self.frogpilot_stats["HighestAcceleration"] = max(self.frogpilot_events.max_acceleration, self.frogpilot_stats.get("HighestAcceleration", 0))

    if sm["carControl"].latActive:
      self.frogpilot_stats["LateralTime"] = self.frogpilot_stats.get("LateralTime", 0) + DT_MDL
    if sm["carControl"].longActive:
      self.frogpilot_stats["LongitudinalTime"] = self.frogpilot_stats.get("LongitudinalTime", 0) + DT_MDL
    elif sm["frogpilotCarState"].alwaysOnLateralEnabled:
      self.frogpilot_stats["AOLTime"] = self.frogpilot_stats.get("AOLTime", 0) + DT_MDL

    if sm["selfdriveState"].state in (State.disabled, State.overriding):
      self.distance_since_override = 0
      self.frogpilot_stats["OverrideTime"] = self.frogpilot_stats.get("OverrideTime", 0) + DT_MDL
    else:
      self.distance_since_override += v_ego * DT_MDL
      self.frogpilot_stats["LongestDistanceWithoutOverride"] = max(self.distance_since_override, self.frogpilot_stats.get("LongestDistanceWithoutOverride", 0))

    current_random_events = {event for event in self.frogpilot_events.events.names if event in RANDOM_EVENTS}
    if len(current_random_events) > 0:
      new_events = current_random_events - self.previous_random_events

      if new_events:
        total_random_events = self.frogpilot_stats.get("RandomEvents", {})
        for event in new_events:
          event_name = RANDOM_EVENTS[event]
          total_random_events[event_name] = total_random_events.get(event_name, 0) + 1
        self.frogpilot_stats["RandomEvents"] = total_random_events

    self.previous_random_events = current_random_events

    if sm["carState"].standstill:
      self.frogpilot_stats["StandstillTime"] = self.frogpilot_stats.get("StandstillTime", 0) + DT_MDL
      if self.frogpilot_events.stopped_for_light:
        self.frogpilot_stats["StopLightTime"] = self.frogpilot_stats.get("StopLightTime", 0) + DT_MDL

    if self.tracked_time > 60 and sm["carState"].standstill and self.enabled:
      if time_validated:
        current_month = now.month
        if current_month != self.frogpilot_stats.get("Month"):
          self.frogpilot_stats.update({
            "CurrentMonthsKilometers": 0,
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

      params.put_nonblocking("FrogPilotStats", self.frogpilot_stats)
