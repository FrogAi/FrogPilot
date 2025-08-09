#!/usr/bin/env python3
import json

from openpilot.common.realtime import DT_MDL

from openpilot.frogpilot.common.frogpilot_variables import params, params_tracking

class FrogPilotTracking:
  def __init__(self):
    self.frogpilot_stats = json.loads(params.get("FrogPilotStats") or "{}")

    self.total_drives = params_tracking.get_int("FrogPilotDrives")
    self.total_kilometers = params_tracking.get_float("FrogPilotKilometers")
    self.total_minutes = params_tracking.get_float("FrogPilotMinutes")

    self.drive_added = False
    self.enabled = False

    self.aol_engaged_time = 0
    self.drive_distance = 0
    self.drive_time = 0
    self.lateral_engaged_time = 0
    self.longitudinal_engaged_time = 0

    self.total_aol_engaged = self.frogpilot_stats.get("TotalAOLTime", 0)
    self.total_lateral_engaged = self.frogpilot_stats.get("TotalLateralTime", 0)
    self.total_longitudinal_engaged = self.frogpilot_stats.get("TotalLongitudinalTime", 0)
    self.total_tracked_time = self.frogpilot_stats.get("TotalTrackedTime", 0)

  def update(self, sm):
    self.enabled |= sm["controlsState"].enabled or sm["frogpilotCarState"].alwaysOnLateralEnabled

    self.drive_distance += sm["carState"].vEgo * DT_MDL
    self.drive_time += DT_MDL

    if sm["carControl"].latActive:
      self.lateral_engaged_time += DT_MDL
    if sm["carControl"].longActive:
      self.longitudinal_engaged_time += DT_MDL
    elif sm["frogpilotCarState"].alwaysOnLateralEnabled:
      self.aol_engaged_time += DT_MDL

    if self.drive_time > 60 and sm["carState"].standstill and self.enabled:
      self.total_kilometers += self.drive_distance / 1000
      params_tracking.put_float_nonblocking("FrogPilotKilometers", self.total_kilometers)
      self.drive_distance = 0

      self.total_minutes += self.drive_time / 60
      params_tracking.put_float_nonblocking("FrogPilotMinutes", self.total_minutes)

      self.total_aol_engaged += self.aol_engaged_time
      self.total_lateral_engaged += self.lateral_engaged_time
      self.total_longitudinal_engaged += self.longitudinal_engaged_time
      self.total_tracked_time += self.drive_time

      self.frogpilot_stats["TotalAOLTime"] = self.total_aol_engaged
      self.frogpilot_stats["TotalLateralTime"] = self.total_lateral_engaged
      self.frogpilot_stats["TotalLongitudinalTime"] = self.total_longitudinal_engaged
      self.frogpilot_stats["TotalTrackedTime"] = self.total_tracked_time

      params.put("FrogPilotStats", json.dumps(self.frogpilot_stats))

      self.aol_engaged_time = 0
      self.drive_time = 0
      self.lateral_engaged_time = 0
      self.longitudinal_engaged_time = 0

      if not self.drive_added:
        self.total_drives += 1
        params_tracking.put_int_nonblocking("FrogPilotDrives", self.total_drives)
        self.drive_added = True
