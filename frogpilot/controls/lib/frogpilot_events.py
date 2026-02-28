#!/usr/bin/env python3
import random

from openpilot.common.conversions import Conversions as CV
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.desire_helper import TurnDirection
from openpilot.selfdrive.controls.lib.events import ET, EVENT_NAME, FROGPILOT_EVENT_NAME, EventName, FrogPilotEventName, Events
from openpilot.selfdrive.controls.lib.vehicle_model import ACCELERATION_DUE_TO_GRAVITY

from openpilot.frogpilot.common.frogpilot_variables import CRUISING_SPEED, NON_DRIVING_GEARS, params, params_memory

DEJA_VU_G_FORCE = 0.75
RANDOM_EVENTS_CHANCE = 0.01 * DT_MDL

class FrogPilotEvents:
  def __init__(self, FrogPilotPlanner, error_log, ThemeManager):
    self.frogpilot_planner = FrogPilotPlanner
    self.theme_manager = ThemeManager

    self.error_log = error_log

    self.events = Events(frogpilot=True)

    self.always_on_lateral_enabled_previously = False
    self.previous_traffic_mode = False
    self.random_event_playing = False
    self.startup_seen = False
    self.stopped_for_light = False

    self.max_acceleration = 0
    self.random_event_timer = 0
    self.tracked_lead_distance = 0

    self.played_events = set()

  def update(self, v_cruise, sm, frogpilot_toggles):
    current_alert = sm["controlsState"].alertType
    current_frogpilot_alert = sm["frogpilotControlsState"].alertType

    alerts_empty = all(sm[state].alertText1 == "" and sm[state].alertText2 == "" for state in ["controlsState", "frogpilotControlsState"])

    self.events.clear()

    if self.random_event_playing:
      self.random_event_timer += DT_MDL

      if self.random_event_timer >= 5:
        self.theme_manager.update_wheel_image(frogpilot_toggles.wheel_image)
        params_memory.put_bool("UpdateWheelImage", True)

        self.random_event_playing = False
        self.random_event_timer = 0

    acceleration = sm["carControl"].actuators.accel

    if not sm["carState"].gasPressed:
      self.max_acceleration = max(acceleration, self.max_acceleration)
    else:
      self.max_acceleration = 0

    if self.error_log.is_file():
      if frogpilot_toggles.random_events:
        self.events.add(FrogPilotEventName.openpilotCrashedRandomEvent)
      else:
        self.events.add(FrogPilotEventName.openpilotCrashed)

    if self.frogpilot_planner.frogpilot_vcruise.forcing_stop:
      self.events.add(FrogPilotEventName.forcingStop)

    if not self.frogpilot_planner.tracking_lead and sm["carState"].standstill and sm["carState"].gearShifter not in NON_DRIVING_GEARS:
      if not self.frogpilot_planner.model_stopped and self.stopped_for_light and frogpilot_toggles.green_light_alert:
        self.events.add(FrogPilotEventName.greenLight)

      self.stopped_for_light = self.frogpilot_planner.cem.stop_light_detected
    else:
      self.stopped_for_light = False

    if "holidayActive" not in self.played_events and self.startup_seen and alerts_empty and len(self.events) == 0 and frogpilot_toggles.current_holiday_theme != "stock":
      self.events.add(FrogPilotEventName.holidayActive)

      self.played_events.add("holidayActive")

    if self.frogpilot_planner.tracking_lead and sm["carState"].standstill and sm["carState"].gearShifter not in NON_DRIVING_GEARS and frogpilot_toggles.lead_departing_alert:
      if self.tracked_lead_distance == 0:
        self.tracked_lead_distance = self.frogpilot_planner.lead_one.dRel

      lead_departing = self.frogpilot_planner.lead_one.dRel - self.tracked_lead_distance > 1
      lead_departing &= self.frogpilot_planner.lead_one.vLead > 1

      if lead_departing:
        self.events.add(FrogPilotEventName.leadDeparting)
    else:
      self.tracked_lead_distance = 0

    if "torqueNNLoad" not in self.played_events and self.startup_seen and alerts_empty and len(self.events) == 0 and params.get("NNFFModelName", encoding="utf-8") is not None and frogpilot_toggles.nnff:
      self.events.add(FrogPilotEventName.torqueNNLoad)

      self.played_events.add("torqueNNLoad")

    if not self.random_event_playing and frogpilot_toggles.random_events:
      if "accel30" not in self.played_events and 3.5 > self.max_acceleration >= 3.0 and acceleration < 1.5:
        self.events.add(FrogPilotEventName.accel30)

        self.theme_manager.update_wheel_image("weeb_wheel", random_event=True)
        params_memory.put_bool("UpdateWheelImage", True)

        self.random_event_playing = True
        self.max_acceleration = 0
        self.played_events.add("accel30")

      elif "accel35" not in self.played_events and 4.0 > self.max_acceleration >= 3.5 and acceleration < 1.5:
        self.events.add(FrogPilotEventName.accel35)

        self.theme_manager.update_wheel_image("tree_fiddy", random_event=True)
        params_memory.put_bool("UpdateWheelImage", True)

        self.random_event_playing = True
        self.max_acceleration = 0
        self.played_events.add("accel35")

      elif "accel40" not in self.played_events and self.max_acceleration >= 4.0 and acceleration < 1.5:
        self.events.add(FrogPilotEventName.accel40)

        self.theme_manager.update_wheel_image("great_scott", random_event=True)
        params_memory.put_bool("UpdateWheelImage", True)

        self.random_event_playing = True
        self.max_acceleration = 0
        self.played_events.add("accel40")

      if "dejaVuCurve" not in self.played_events and sm["carState"].vEgo > CRUISING_SPEED:
        if self.frogpilot_planner.lateral_acceleration >= DEJA_VU_G_FORCE * ACCELERATION_DUE_TO_GRAVITY:
          self.events.add(FrogPilotEventName.dejaVuCurve)

          self.random_event_playing = True
          self.played_events.add("dejaVuCurve")

      if "hal9000" not in self.played_events and (ET.NO_ENTRY in current_alert or ET.NO_ENTRY in current_frogpilot_alert):
        self.events.add(FrogPilotEventName.hal9000)

        self.random_event_playing = True
        self.played_events.add("hal9000")

      if f"{EVENT_NAME[EventName.steerSaturated]}/" in current_alert or f"{FROGPILOT_EVENT_NAME[FrogPilotEventName.goatSteerSaturated]}/" in current_frogpilot_alert:
        event_choices = []
        if "firefoxSteerSaturated" not in self.played_events:
          event_choices.append("firefoxSteerSaturated")
        if "goatSteerSaturated" not in self.played_events:
          event_choices.append("goatSteerSaturated")
        if "thisIsFineSteerSaturated" not in self.played_events:
          event_choices.append("thisIsFineSteerSaturated")

        if event_choices and random.random() < RANDOM_EVENTS_CHANCE:
          event_choice = random.choice(event_choices)

          if event_choice == "firefoxSteerSaturated":
            self.events.add(FrogPilotEventName.firefoxSteerSaturated)

            self.theme_manager.update_wheel_image("firefox", random_event=True)
            params_memory.put_bool("UpdateWheelImage", True)
          elif event_choice == "goatSteerSaturated":
            self.events.add(FrogPilotEventName.goatSteerSaturated)

            self.theme_manager.update_wheel_image("goat", random_event=True)
            params_memory.put_bool("UpdateWheelImage", True)
          elif event_choice == "thisIsFineSteerSaturated":
            self.events.add(FrogPilotEventName.thisIsFineSteerSaturated)

            self.theme_manager.update_wheel_image("this_is_fine", random_event=True)
            params_memory.put_bool("UpdateWheelImage", True)

          self.random_event_playing = True
          self.played_events.add(event_choice)

      if "vCruise69" not in self.played_events and 70 > max(sm["controlsState"].vCruise, sm["controlsState"].vCruiseCluster) * (1 if frogpilot_toggles.is_metric else CV.KPH_TO_MPH) >= 69:
        self.events.add(FrogPilotEventName.vCruise69)

        self.random_event_playing = True
        self.played_events.add("vCruise69")

      if f"{EVENT_NAME[EventName.fcw]}/" in current_alert or f"{EVENT_NAME[EventName.stockAeb]}/" in current_alert:
        event_choices = []
        if "toBeContinued" not in self.played_events:
          event_choices.append("toBeContinued")
        if "yourFrogTriedToKillMe" not in self.played_events:
          event_choices.append("yourFrogTriedToKillMe")

        if event_choices:
          event_choice = random.choice(event_choices)
          if event_choice == "toBeContinued":
            self.events.add(FrogPilotEventName.toBeContinued)
          elif event_choice == "yourFrogTriedToKillMe":
            self.events.add(FrogPilotEventName.yourFrogTriedToKillMe)

          self.random_event_playing = True
          self.played_events.add(event_choice)

      if "youveGotMail" not in self.played_events and sm["frogpilotCarState"].alwaysOnLateralEnabled and not self.always_on_lateral_enabled_previously:
        if random.random() < RANDOM_EVENTS_CHANCE:
          self.events.add(FrogPilotEventName.youveGotMail)

          self.random_event_playing = True
          self.played_events.add("youveGotMail")

      self.always_on_lateral_enabled_previously = sm["frogpilotCarState"].alwaysOnLateralEnabled

    if self.frogpilot_planner.frogpilot_vcruise.slc.speed_limit_changed_timer == DT_MDL and frogpilot_toggles.speed_limit_changed_alert:
      self.events.add(FrogPilotEventName.speedLimitChanged)

    self.startup_seen |= sm["frogpilotControlsState"].alertText1 == frogpilot_toggles.startup_alert_top and sm["frogpilotControlsState"].alertText2 == frogpilot_toggles.startup_alert_bottom

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
