import datetime
import pytest
import threading

from cereal import messaging
from openpilot.common.params import Params

from openpilot.frogpilot import frogpilot_process

MOVE_ID = "0f8fad5b-d9cb-469f-a165-70867728950e"


class Stop(Exception):
  pass


def run_frogpilot_thread(mocker, started_states):
  events = []
  locks = []

  new_lock = threading.Lock

  def make_lock():
    locks.append(new_lock())

    return locks[-1]

  def run_in_thread(target, args=(), report=True):
    if target is frogpilot_process.send_stats:
      events.append(("send_stats", args[3].locked()))

  def make_tracking(*args):
    events.append(("load_stats", any(lock.locked() for lock in locks)))

    tracking = mocker.MagicMock()
    tracking.save_stats.side_effect = lambda blocking=False: events.append(("save_stats", any(lock.locked() for lock in locks)))

    return tracking

  states = iter(started_states)

  def update():
    try:
      sm.started = next(states)
    except StopIteration:
      raise Stop from None

  sm = mocker.MagicMock(started=False)
  sm.update.side_effect = update
  sm.__getitem__.side_effect = lambda name: mocker.MagicMock(forceOnroadClearedCount=0, started=sm.started)
  sm.updated.__getitem__.return_value = False

  mocker.patch.object(frogpilot_process.frogpilot_api, "FrogPilotAPI")
  mocker.patch.object(frogpilot_process.frogpilot_utilities, "ThreadManager", return_value=mocker.MagicMock(run_in_thread=run_in_thread))
  mocker.patch.object(frogpilot_process.frogpilot_utilities, "is_unmetered_network", return_value=False)
  mocker.patch.object(frogpilot_process.frogpilot_variables, "FrogPilotVariables")
  mocker.patch.object(frogpilot_process.messaging, "PubMaster")
  mocker.patch.object(frogpilot_process.messaging, "SubMaster", return_value=sm)
  mocker.patch.object(frogpilot_process.messaging, "new_message")
  mocker.patch.object(frogpilot_process.threading, "Lock", side_effect=make_lock)
  mocker.patch.object(frogpilot_process, "FrogPilotPlanner", return_value=mocker.MagicMock(last_gps_position=None))
  mocker.patch.object(frogpilot_process, "FrogPilotRequests", return_value=mocker.MagicMock(**{"update.return_value": (False, False)}))
  mocker.patch.object(frogpilot_process, "FrogPilotTracking", side_effect=make_tracking)
  mocker.patch.object(frogpilot_process, "ModelManager")
  mocker.patch.object(frogpilot_process, "SpeedLimitReaderModels")
  mocker.patch.object(frogpilot_process, "ThemeManager")
  mocker.patch.object(frogpilot_process, "system_time_valid", return_value=True)
  mocker.patch.object(frogpilot_process, "transition_onroad")
  mocker.patch.object(frogpilot_process, "update_toggles")

  with pytest.raises(Stop):
    frogpilot_process.frogpilot_thread()

  return events


class TestFrogPilotProcess:
  def test_offroad_boot(self, mocker):
    events = run_frogpilot_thread(mocker, [False, False, True, True, False])

    assert events == [("send_stats", False), ("load_stats", True), ("save_stats", True), ("send_stats", False)]

  def test_onroad_boot(self, mocker):
    events = run_frogpilot_thread(mocker, [True, True, False])

    assert events == [("load_stats", True), ("send_stats", True), ("save_stats", True), ("send_stats", False)]

  def test_reset_stats(self, mocker):
    params = Params(return_defaults=True)
    params.put("FrogPilotStats", {"Engages": 3, "StatsMoveId": MOVE_ID})

    msg = messaging.new_message("frogpilotUIRequest")
    msg.frogpilotUIRequest.resetStats = None

    mocker.patch.object(frogpilot_process.messaging, "drain_sock", side_effect=[[msg.as_reader()], []])
    mocker.patch.object(frogpilot_process.messaging, "sub_sock")

    frogpilot_requests = frogpilot_process.FrogPilotRequests(threading.Event(), mocker.MagicMock(), mocker.MagicMock(), mocker.MagicMock())
    frogpilot_requests.update(datetime.datetime.now(datetime.UTC), False, mocker.MagicMock(), params, mocker.MagicMock(), mocker.MagicMock())

    assert params.get("FrogPilotStats") == {"StatsMoveId": MOVE_ID}
