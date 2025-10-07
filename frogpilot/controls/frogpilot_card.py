#!/usr/bin/env python3
from cereal import car
from openpilot.selfdrive.selfdrived.events import EventName

from openpilot.frogpilot.common.frogpilot_variables import NON_DRIVING_GEARS

class FrogPilotCard:
  def __init__(self, CP):

  def update(self, carState, frogpilotCarState, sm, frogpilot_toggles):
    self.always_on_lateral_enabled = frogpilot_toggles.always_on_lateral_set

    if frogpilot_toggles.use_lkas_for_aol:
      self.always_on_lateral_enabled |= frogpilot_toggles.always_on_lateral_lkas or carState.cruiseState.enabled
      self.always_on_lateral_enabled &= frogpilotCarState.alwaysOnLateralAllowed
    else:
      self.always_on_lateral_enabled |= frogpilot_toggles.always_on_lateral_main or carState.cruiseState.enabled
      self.always_on_lateral_enabled &= carState.cruiseState.available

    self.always_on_lateral_enabled &= carState.gearShifter not in NON_DRIVING_GEARS
    self.always_on_lateral_enabled &= sm["frogpilotPlan"].lateralCheck
    self.always_on_lateral_enabled &= sm["liveCalibration"].calPerc >= 1
    self.always_on_lateral_enabled &= not (carState.brakePressed and carState.vEgo < frogpilot_toggles.always_on_lateral_pause_speed or carState.standstill)
    self.always_on_lateral_enabled &= not any(event.immediateDisable for events in (sm["onroadEvents"], sm["frogpilotOnroadEvents"]) for event in events if event.name != EventName.speedTooLow) or frogpilot_toggles.frogs_go_moo

    frogpilotCarState.alwaysOnLateralEnabled = self.always_on_lateral_enabled

    return frogpilotCarState
