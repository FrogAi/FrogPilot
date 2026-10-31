import pytest

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.updated import update_helpers


class FakeParams:
  def __init__(self):
    self.values = {"IsOnroad": b"0", "LastUpdateTime": b"0", "UpdateAvailable": b"0", "UpdateFailedCount": b"0",
                   "UpdaterFetchAvailable": b"0", "UpdaterState": b"idle"}

  def get(self, key, encoding=None):
    value = self.values.get(key)
    return value.decode() if encoding is not None and value is not None else value

  def get_bool(self, key):
    return self.values.get(key) == b"1"

  def put(self, key, value):
    self.values[key] = value


class FakeUpdated:
  def __init__(self, params, fetch_fails=False, starts_drive=False):
    self.params = params
    self.fetch_fails = fetch_fails
    self.starts_drive = starts_drive

    self.failed_count = 0
    self.iterations = 0
    self.ready = False
    self.remote_ahead = True
    self.writes = []

  def signal(self, cmd, *args, **kwargs):
    fetch = cmd[1] == "-SIGHUP"
    failed = fetch and self.fetch_fails
    self.iterations += 1

    self.writes += [
      ("UpdateFailedCount", str(self.failed_count).encode()),
      ("UpdaterState", b"checking..."),
    ]
    if fetch:
      self.writes += [("UpdaterState", b"downloading..."), ("UpdateAvailable", b"0")]
      if self.starts_drive:
        self.writes.append(("IsOnroad", b"1"))
      if not failed:
        self.remote_ahead = False
        self.ready = True

    self.failed_count = self.failed_count + 1 if failed else 0
    self.writes += [
      ("UpdateFailedCount", str(self.failed_count).encode()),
      ("UpdaterFetchAvailable", b"1" if self.remote_ahead else b"0"),
    ]
    if not failed:
      self.writes.append(("LastUpdateTime", str(self.iterations).encode()))
    self.writes.append(("UpdateAvailable", b"1" if self.ready and not failed else b"0"))
    self.writes.append(("UpdaterState", b"idle"))
    return ""

  def step(self):
    if self.writes:
      self.params.put(*self.writes.pop(0))
    elif self.params.get_bool("IsOnroad"):
      self.params.put("IsOnroad", b"0")


@pytest.fixture
def updater(monkeypatch):
  def run(fetch_fails=False, starts_drive=False):
    params = FakeParams()
    updated = FakeUpdated(params, fetch_fails, starts_drive)
    reboots = []
    sleeps = []

    def sleep(seconds):
      sleeps.append(seconds)
      assert len(sleeps) < 1000, "update thread never finished"
      updated.step()

    monkeypatch.setattr(frogpilot_variables, "params", params)
    monkeypatch.setattr(frogpilot_utilities, "run_cmd", updated.signal)
    monkeypatch.setattr(update_helpers.time, "sleep", sleep)
    monkeypatch.setattr(update_helpers.HARDWARE, "reboot", lambda: reboots.append(params.get_bool("IsOnroad")))

    update_helpers.update_openpilot()
    return reboots

  return run


def test_update_reboots_once_parked(updater):
  assert updater() == [False]


def test_update_started_before_a_drive_waits_for_offroad_to_reboot(updater):
  assert updater(starts_drive=True) == [False]


def test_failed_download_ends_without_reboot(updater):
  assert updater(fetch_fails=True) == []
