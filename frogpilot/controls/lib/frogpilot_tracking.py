#!/usr/bin/env python3
from openpilot.common.realtime import DT_MDL

from openpilot.frogpilot.common.frogpilot_variables import params_tracking

class FrogPilotTracking:
  def __init__(self):
    self.total_drives = params_tracking.get_int("FrogPilotDrives")
    self.total_kilometers = params_tracking.get_float("FrogPilotKilometers")
    self.total_minutes = params_tracking.get_float("FrogPilotMinutes")

    self.drive_added = False
    self.enabled = False

    self.drive_distance = 0
    self.drive_time = 0

  def update(self, sm):
    self.enabled |= sm["controlsState"].enabled or sm["frogpilotCarState"].alwaysOnLateralEnabled

    self.drive_distance += sm["carState"].vEgo * DT_MDL
    self.drive_time += DT_MDL

    if self.drive_time > 60 and sm["carState"].standstill and self.enabled:
      self.total_kilometers += self.drive_distance / 1000
      params_tracking.put_float_nonblocking("FrogPilotKilometers", self.total_kilometers)
      self.drive_distance = 0

      self.total_minutes += self.drive_time / 60
      params_tracking.put_float_nonblocking("FrogPilotMinutes", self.total_minutes)
      self.drive_time = 0

      if not self.drive_added:
        self.total_drives += 1
        params_tracking.put_int_nonblocking("FrogPilotDrives", self.total_drives)
        self.drive_added = True
