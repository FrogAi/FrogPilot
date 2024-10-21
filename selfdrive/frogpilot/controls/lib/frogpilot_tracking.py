from openpilot.common.params import Params
from openpilot.common.realtime import DT_MDL

class FrogPilotTracking:
  def __init__(self):
    self.params_tracking = Params("/persist/tracking")

    self.total_drives = self.params_tracking.get_int("FrogPilotDrives")
    self.total_kilometers = self.params_tracking.get_float("FrogPilotKilometers")
    self.total_minutes = self.params_tracking.get_float("FrogPilotMinutes")

    self.drive_added = False
    self.enabled = False

    self.drive_distance = 0
    self.drive_time = 0

  def update(self, carState, controlsState, frogpilotCarControl):
    self.enabled |= controlsState.enabled or frogpilotCarControl.alwaysOnLateralActive

    self.drive_distance += carState.vEgo * DT_MDL
    self.drive_time += DT_MDL

    if self.drive_time > 60 and carState.standstill and self.enabled:
      self.total_kilometers += self.drive_distance / 1000
      self.params_tracking.put_float_nonblocking("FrogPilotKilometers", self.total_kilometers)
      self.drive_distance = 0

      self.total_minutes += self.drive_time / 60
      self.params_tracking.put_float_nonblocking("FrogPilotMinutes", self.total_minutes)
      self.drive_time = 0

      if not self.drive_added:
        self.total_drives += 1
        self.params_tracking.put_int_nonblocking("FrogPilotDrives", self.total_drives)
        self.drive_added = True
