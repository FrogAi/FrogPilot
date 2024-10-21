import os
import random

import openpilot.system.sentry as sentry

from openpilot.common.conversions import Conversions as CV
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.controlsd import Desire
from openpilot.selfdrive.controls.lib.events import EventName, Events

from openpilot.selfdrive.frogpilot.assets.theme_manager import update_wheel_image
from openpilot.selfdrive.frogpilot.frogpilot_variables import CRUISING_SPEED, params, params_memory

class FrogPilotEvents:
  def __init__(self, FrogPilotPlanner):
    self.events = Events()
    self.frogpilot_planner = FrogPilotPlanner

    self.accel30_played = False
    self.accel35_played = False
    self.accel40_played = False
    self.always_on_lateral_active_previously = False
    self.dejaVu_played = False
    self.fcw_played = False
    self.firefox_played = False
    self.goat_played = False
    self.holiday_theme_played = False
    self.no_entry_alert_played = False
    self.openpilot_crashed_played = False
    self.previous_traffic_mode = False
    self.random_event_played = False
    self.stopped_for_light = False
    self.this_is_fine_played = False
    self.vCruise69_played = False
    self.youveGotMail_played = False

    self.frame = 0
    self.max_acceleration = 0
    self.random_event_timer = 0
    self.tracking_lead_distance = 0

  def update(self, carState, controlsState, frogpilotCarControl, frogpilotCarState, lead_distance, modelData, v_lead, frogpilot_toggles):
    self.events.clear()

    if self.random_event_played:
      self.random_event_timer += DT_MDL
      if self.random_event_timer >= 4:
        update_wheel_image(frogpilot_toggles.wheel_image, frogpilot_toggles.current_holiday_theme, False)
        params_memory.put_bool("UpdateWheelImage", True)
        self.random_event_played = False
        self.random_event_timer = 0

    if self.frogpilot_planner.frogpilot_vcruise.forcing_stop:
      self.events.add(EventName.forcingStop)

    if frogpilot_toggles.green_light_alert and not self.frogpilot_planner.tracking_lead and carState.standstill:
      if not self.frogpilot_planner.model_stopped and self.stopped_for_light:
        self.events.add(EventName.greenLight)
      self.stopped_for_light = self.frogpilot_planner.cem.stop_light_detected
    else:
      self.stopped_for_light = False

    if not self.holiday_theme_played and frogpilot_toggles.current_holiday_theme is not None and self.frame >= 10:
      self.events.add(EventName.holidayActive)
      self.holiday_theme_played = True

    if frogpilot_toggles.lead_departing_alert and self.frogpilot_planner.tracking_lead and carState.standstill:
      if self.tracking_lead_distance == 0:
        self.tracking_lead_distance = lead_distance

      lead_departing = lead_distance - self.tracking_lead_distance > 1
      lead_departing &= v_lead > 1

      if lead_departing:
        self.events.add(EventName.leadDeparting)
    else:
      self.tracking_lead_distance = 0

    if not self.openpilot_crashed_played and os.path.isfile(os.path.join(sentry.CRASHES_DIR, 'error.txt')):
      if frogpilot_toggles.random_events:
        self.events.add(EventName.openpilotCrashedRandomEvent)
      else:
        self.events.add(EventName.openpilotCrashed)
      self.openpilot_crashed_played = True

    if not self.random_event_played and frogpilot_toggles.random_events:
      acceleration = carState.aEgo

      if not carState.gasPressed:
        self.max_acceleration = max(acceleration, self.max_acceleration)
      else:
        self.max_acceleration = 0

      if not self.accel30_played and 3.5 > self.max_acceleration >= 3.0 and acceleration < 1.5:
        self.events.add(EventName.accel30)
        update_wheel_image("weeb_wheel")
        params_memory.put_bool("UpdateWheelImage", True)
        self.accel30_played = True
        self.random_event_played = True
        self.max_acceleration = 0

      elif not self.accel35_played and 4.0 > self.max_acceleration >= 3.5 and acceleration < 1.5:
        self.events.add(EventName.accel35)
        update_wheel_image("tree_fiddy")
        params_memory.put_bool("UpdateWheelImage", True)
        self.accel35_played = True
        self.random_event_played = True
        self.max_acceleration = 0

      elif not self.accel40_played and self.max_acceleration >= 4.0 and acceleration < 1.5:
        self.events.add(EventName.accel40)
        update_wheel_image("great_scott")
        params_memory.put_bool("UpdateWheelImage", True)
        self.accel40_played = True
        self.random_event_played = True
        self.max_acceleration = 0

      if not self.dejaVu_played and carState.vEgo > CRUISING_SPEED * 2:
        if carState.vEgo > (1 / self.frogpilot_planner.road_curvature)**0.75 * 2 > CRUISING_SPEED * 2 and abs(carState.steeringAngleDeg) > 30:
          self.events.add(EventName.dejaVuCurve)
          self.dejaVu_played = True
          self.random_event_played = True

      if not self.no_entry_alert_played and frogpilotCarControl.noEntryEventTriggered:
        self.events.add(EventName.hal9000)
        self.no_entry_alert_played = True
        self.random_event_played = True

      if frogpilotCarControl.steerSaturatedEventTriggered:
        event_choices = []
        if not self.firefox_played:
          event_choices.append("firefoxSteerSaturated")
        if not self.goat_played:
          event_choices.append("goatSteerSaturated")
        if not self.this_is_fine_played:
          event_choices.append("thisIsFineSteerSaturated")

        if self.frame % 100 == 0 and event_choices:
          event_choice = random.choice(event_choices)
          if event_choice == "firefoxSteerSaturated":
            self.events.add(EventName.firefoxSteerSaturated)
            update_wheel_image("firefox")
            params_memory.put_bool("UpdateWheelImage", True)
            self.firefox_played = True
          elif event_choice == "goatSteerSaturated":
            self.events.add(EventName.goatSteerSaturated)
            update_wheel_image("goat")
            params_memory.put_bool("UpdateWheelImage", True)
            self.goat_played = True
          elif event_choice == "thisIsFineSteerSaturated":
            self.events.add(EventName.thisIsFineSteerSaturated)
            update_wheel_image("this_is_fine")
            params_memory.put_bool("UpdateWheelImage", True)
            self.this_is_fine_played = True
          self.random_event_played = True

      if not self.vCruise69_played and 70 > max(controlsState.vCruise, controlsState.vCruiseCluster) * (1 if frogpilot_toggles.is_metric else CV.KPH_TO_MPH) >= 69:
        self.events.add(EventName.vCruise69)
        self.vCruise69_played = True
        self.random_event_played = True

      if not self.fcw_played and frogpilotCarControl.fcwEventTriggered:
        self.events.add(EventName.yourFrogTriedToKillMe)
        self.fcw_played = True
        self.random_event_played = True

      if not self.youveGotMail_played and frogpilotCarControl.alwaysOnLateralActive and not self.always_on_lateral_active_previously:
        if random.random() < 0.01 and carState.vEgo > CRUISING_SPEED:
          self.events.add(EventName.youveGotMail)
          self.youveGotMail_played = True
          self.random_event_played = True
      self.always_on_lateral_active_previously = frogpilotCarControl.alwaysOnLateralActive

    if frogpilot_toggles.speed_limit_changed_alert and self.frogpilot_planner.frogpilot_vcruise.speed_limit_changed:
      self.events.add(EventName.speedLimitChanged)

    if self.frame == 4 and params.get("NNFFModelName", encoding='utf-8') is not None:
      self.events.add(EventName.torqueNNLoad)

    if frogpilotCarState.trafficModeActive != self.previous_traffic_mode:
      if self.previous_traffic_mode:
        self.events.add(EventName.trafficModeInactive)
      else:
        self.events.add(EventName.trafficModeActive)
      self.previous_traffic_mode = frogpilotCarState.trafficModeActive

    if modelData.meta.turnDirection == Desire.turnLeft:
      self.events.add(EventName.turningLeft)
    elif modelData.meta.turnDirection == Desire.turnRight:
      self.events.add(EventName.turningRight)

    self.frame = round(self.frame + DT_MDL, 2)
