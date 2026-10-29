#!/usr/bin/env python3
import random

import numpy as np

from openpilot.common.constants import ACCELERATION_DUE_TO_GRAVITY, CV
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.desire_helper import TurnDirection
from openpilot.selfdrive.selfdrived.events import ET, EVENT_NAME, FROGPILOT_EVENT_NAME, EventName, FrogPilotEventName, Events

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_variables

DEJA_VU_G_FORCE = 0.75
GREEN_LIGHT_LOOKAHEAD = 6
GREEN_LIGHT_TIME = 0.35
RANDOM_EVENTS_CHANCE = 0.01
RANDOM_EVENTS_LENGTH = 5

FCW_EVENTS = [FrogPilotEventName.toBeContinued, FrogPilotEventName.yourFrogTriedToKillMe]
STEER_SATURATED_EVENTS = [FrogPilotEventName.firefoxSteerSaturated, FrogPilotEventName.goatSteerSaturated, FrogPilotEventName.thisIsFineSteerSaturated]

class FrogPilotEvents:
  def __init__(self, FrogPilotPlanner, ThemeManager):
    self.frogpilot_planner = FrogPilotPlanner
    self.theme_manager = ThemeManager

    self.events = Events(frogpilot=True)

    self.always_on_lateral_allowed_previously = False
    self.previous_traffic_mode = False
    self.random_event_playing = False
    self.startup_seen = False
    self.stopped_for_light = False

    self.green_light_timer = 0
    self.max_acceleration = 0
    self.random_event_timer = 0
    self.tracked_lead_distance = 0

    self.played_events = set()

    self.screen_recorder_events = []

    self.wheel_image_update_count = 0

  def add_wheel_event(self, event):
    self.events.add(event)

    self.theme_manager.update_wheel_image(FROGPILOT_EVENT_NAME[event], random_event=True)
    self.wheel_image_update_count += 1

  def unplayed(self, events):
    return [event for event in events if FROGPILOT_EVENT_NAME[event] not in self.played_events]

  def update(self, long_control_active, sm, frogpilot_toggles):
    current_alert = sm["selfdriveState"].alertType
    current_frogpilot_alert = sm["frogpilotSelfdriveState"].alertType

    alerts_empty = all(sm[state].alertText1 == "" and sm[state].alertText2 == "" for state in ["selfdriveState", "frogpilotSelfdriveState"])

    self.events.clear()

    acceleration = sm["carControl"].actuators.accel

    if long_control_active:
      self.max_acceleration = max(acceleration, self.max_acceleration)
    else:
      self.max_acceleration = 0

    if self.frogpilot_planner.frogpilot_vcruise.forcing_stop:
      self.events.add(FrogPilotEventName.forcingStop)

    if not self.frogpilot_planner.lead_relevant and sm["carState"].standstill and sm["carState"].gearShifter not in frogpilot_variables.NON_DRIVING_GEARS:
      model_moving = np.interp(GREEN_LIGHT_LOOKAHEAD, sm["modelV2"].velocity.t, sm["modelV2"].velocity.x) >= frogpilot_variables.CRUISING_SPEED
      if model_moving:
        self.green_light_timer += DT_MDL
      else:
        self.green_light_timer = 0

      if self.green_light_timer >= GREEN_LIGHT_TIME and self.stopped_for_light and frogpilot_toggles.green_light_alert:
        self.events.add(FrogPilotEventName.greenLight)

      self.stopped_for_light |= self.frogpilot_planner.frogpilot_cem.stop_light_detected
    else:
      self.green_light_timer = 0
      self.stopped_for_light = False

    if "holidayActive" not in self.played_events and self.startup_seen and alerts_empty and len(self.events) == 0 and frogpilot_toggles.current_holiday_theme != "stock":
      self.events.add(FrogPilotEventName.holidayActive)

    if not sm["carState"].standstill:
      self.tracked_lead_distance = self.frogpilot_planner.lead_one.dRel if self.frogpilot_planner.lead_one.status else 0
    elif self.frogpilot_planner.lead_one.status and self.tracked_lead_distance != 0 and sm["carState"].gearShifter not in frogpilot_variables.NON_DRIVING_GEARS:
      lead_departing = self.frogpilot_planner.lead_one.dRel - self.tracked_lead_distance >= 1
      lead_departing &= self.frogpilot_planner.lead_one.vLead >= frogpilot_variables.LEAD_DEPARTURE_SPEED

      if lead_departing and frogpilot_toggles.lead_departing_alert:
        self.events.add(FrogPilotEventName.leadDeparting)
    else:
      self.tracked_lead_distance = 0

    if "nnffLoaded" not in self.played_events and self.startup_seen and alerts_empty and len(self.events) == 0 and self.frogpilot_planner.params.get("NNFFModelName") is not None and frogpilot_toggles.nnff:
      self.events.add(FrogPilotEventName.nnffLoaded)

    if self.random_event_playing:
      self.random_event_timer += DT_MDL

      if self.random_event_timer >= RANDOM_EVENTS_LENGTH:
        if self.theme_manager.restore_wheel_image():
          self.wheel_image_update_count += 1

        self.random_event_playing = False
        self.random_event_timer = 0

    if not self.random_event_playing and frogpilot_toggles.random_events:
      if "accel30" not in self.played_events and 3.5 > self.max_acceleration >= 3.0 and acceleration < 1.5:
        self.add_wheel_event(FrogPilotEventName.accel30)

        self.max_acceleration = 0

      elif "accel35" not in self.played_events and 4.0 > self.max_acceleration >= 3.5 and acceleration < 1.5:
        self.add_wheel_event(FrogPilotEventName.accel35)

        self.max_acceleration = 0

      elif "accel40" not in self.played_events and self.max_acceleration >= 4.0 and acceleration < 1.5:
        self.add_wheel_event(FrogPilotEventName.accel40)

        self.max_acceleration = 0

      if "dejaVuCurve" not in self.played_events and sm["carState"].vEgo > frogpilot_variables.CRUISING_SPEED:
        if abs(self.frogpilot_planner.lateral_acceleration) >= DEJA_VU_G_FORCE * ACCELERATION_DUE_TO_GRAVITY:
          self.events.add(FrogPilotEventName.dejaVuCurve)

      if "hal9000" not in self.played_events and (ET.NO_ENTRY in current_alert or ET.NO_ENTRY in current_frogpilot_alert):
        self.events.add(FrogPilotEventName.hal9000)

      if f"{EVENT_NAME[EventName.steerSaturated]}/" in current_alert or f"{FROGPILOT_EVENT_NAME[FrogPilotEventName.goatSteerSaturated]}/" in current_frogpilot_alert:
        event_choices = self.unplayed(STEER_SATURATED_EVENTS)

        if event_choices and random.random() < RANDOM_EVENTS_CHANCE * DT_MDL:
          event_choice = random.choice(event_choices)

          self.add_wheel_event(event_choice)

          self.random_event_playing |= event_choice == FrogPilotEventName.goatSteerSaturated

      if "vCruise69" not in self.played_events and round(max(sm["carState"].vCruise, sm["carState"].vCruiseCluster) * (1 if frogpilot_toggles.is_metric else CV.KPH_TO_MPH)) == 69:
        self.events.add(FrogPilotEventName.vCruise69)

      if f"{EVENT_NAME[EventName.fcw]}/" in current_alert or f"{EVENT_NAME[EventName.stockAeb]}/" in current_alert:
        event_choices = self.unplayed(FCW_EVENTS)

        if event_choices:
          self.events.add(random.choice(event_choices))

      if "youveGotMail" not in self.played_events and sm["frogpilotCarState"].alwaysOnLateralAllowed and not self.always_on_lateral_allowed_previously and frogpilot_toggles.always_on_lateral:
        if random.random() < RANDOM_EVENTS_CHANCE:
          self.events.add(FrogPilotEventName.youveGotMail)

      self.always_on_lateral_allowed_previously = sm["frogpilotCarState"].alwaysOnLateralAllowed
      self.random_event_playing |= any(frogpilot_variables.RANDOM_EVENT_START <= event <= frogpilot_variables.RANDOM_EVENT_END for event in self.events.names)

    if frogpilot_paths.ERROR_LOG_PATH.is_file():
      if frogpilot_toggles.random_events:
        self.events.add(FrogPilotEventName.openpilotCrashedRandomEvent)
      else:
        self.events.add(FrogPilotEventName.openpilotCrashed)

    if sm["frogpilotCarState"].pedalInterceptorNoBrake:
      if sm["carControl"].enabled:
        self.events.add(FrogPilotEventName.pedalInterceptorNoBrake)
      else:
        self.events.add(FrogPilotEventName.pedalInterceptorNoBrakeNoEntry)

    if self.frogpilot_planner.frogpilot_vcruise.slc.speed_limit_changed_timer == DT_MDL and frogpilot_toggles.speed_limit_changed_alert:
      self.events.add(FrogPilotEventName.speedLimitChanged)

    self.startup_seen |= sm["frogpilotSelfdriveState"].alertText1 == frogpilot_toggles.startup_alert_top and sm["frogpilotSelfdriveState"].alertText2 == frogpilot_toggles.startup_alert_bottom

    if sm["frogpilotCarState"].trafficModeEnabled != self.previous_traffic_mode:
      if self.previous_traffic_mode:
        self.events.add(FrogPilotEventName.trafficModeInactive)
      else:
        self.events.add(FrogPilotEventName.trafficModeActive)

      self.previous_traffic_mode = sm["frogpilotCarState"].trafficModeEnabled

    if sm["frogpilotModelV2"].turnDirection == TurnDirection.turnLeft:
      self.events.add(FrogPilotEventName.turningLeft)
    elif sm["frogpilotModelV2"].turnDirection == TurnDirection.turnRight:
      self.events.add(FrogPilotEventName.turningRight)

    if self.screen_recorder_events and alerts_empty and len(self.events) == 0:
      self.events.add(self.screen_recorder_events.pop(0))

    self.played_events.update(FROGPILOT_EVENT_NAME[event] for event in self.events.names)
