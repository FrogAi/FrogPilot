import time
import types

from cereal import car, custom, log
from openpilot.selfdrive.ui.ui_state import UIStatus

from openpilot.frogpilot.ui.mici.frogpilot_screen import FrogPilotScreen


class SubMaster(dict):
  def __init__(self):
    super().__init__(carState=car.CarState.new_message(), frogpilotSelfdriveState=custom.FrogPilotSelfdriveState.new_message(),
                     selfdriveState=log.SelfdriveState.new_message())
    self.recv_frame = {"selfdriveState": 1}
    self.recv_time = {"selfdriveState": time.monotonic()}


def get_wakes(statuses):
  screen = FrogPilotScreen()
  ui_state = types.SimpleNamespace(frogpilot_toggles={"standby_mode": True}, sm=SubMaster(), started=True, started_frame=0, status=None)

  wakes = []
  for status in statuses:
    ui_state.status = status
    screen.update(ui_state)
    wakes.append(screen.keep_awake)
  return wakes


def test_override_does_not_wake():
  assert get_wakes([UIStatus.ENGAGED, UIStatus.OVERRIDE, UIStatus.ENGAGED]) == [True, False, False]


def test_disengage_during_override_wakes():
  assert get_wakes([UIStatus.ENGAGED, UIStatus.OVERRIDE, UIStatus.DISENGAGED]) == [True, False, True]
