import os
import pytest
import signal
import time

from cereal import car
from openpilot.common.params import Params
import openpilot.system.manager.manager as manager
from openpilot.system.manager.process import NativeProcess, ensure_running, join_process
from openpilot.system.manager.process_config import managed_processes, procs
from openpilot.system.hardware import HARDWARE

os.environ['FAKEUPLOAD'] = "1"

MAX_STARTUP_TIME = 3
BLACKLIST_PROCS = ['manage_athenad', 'pandad', 'pigeond']


class TestManager:
  def setup_method(self):
    HARDWARE.set_power_save(False)

    # ensure clean CarParams
    params = Params()
    params.clear_all()

  def teardown_method(self):
    manager.manager_cleanup()

  def test_manager_prepare(self):
    os.environ['PREPAREONLY'] = '1'
    manager.main()

  def test_duplicate_procs(self):
    assert len(procs) == len(managed_processes), "Duplicate process names"

  def test_blacklisted_procs(self):
    # TODO: ensure there are blacklisted procs until we have a dedicated test
    assert len(BLACKLIST_PROCS), "No blacklisted procs to test not_run"

  def test_set_params_with_default_value(self):
    params = Params()
    params.clear_all()

    os.environ['PREPAREONLY'] = '1'
    manager.main()
    for k in params.all_keys():
      default_value = params.get_default_value(k)
      if default_value is not None:
        assert params.get(k) == default_value
    assert params.get("OpenpilotEnabledToggle")
    assert params.get("RouteCount") == 0

  def test_restart_if_crash_flags(self):
    # the always-run logging pipeline must self-heal when it crashes:
    # a dead loggerd blocks engagement (processNotRunning) and a dead
    # logmessaged silently loses all logs for the rest of the drive
    for name in ("loggerd", "encoderd", "logmessaged"):
      assert managed_processes[name].restart_if_crash, f"{name} must be flagged restart_if_crash"

  def test_restart_if_crash(self):
    def always_run(started, params, CP, frogpilot_toggles):
      return True

    flagged = NativeProcess("test_flagged", ".", ["sleep", "60"], always_run, sigkill=True, restart_if_crash=True)
    unflagged = NativeProcess("test_unflagged", ".", ["sleep", "60"], always_run, sigkill=True)
    test_procs = [flagged, unflagged]
    try:
      ensure_running(test_procs, started=False, params=Params(), CP=None, frogpilot_toggles=None)
      for p in test_procs:
        assert p.proc.is_alive()
      first_pids = {p.name: p.proc.pid for p in test_procs}

      # simulate a self-crash
      for p in test_procs:
        os.kill(p.proc.pid, signal.SIGKILL)
        join_process(p.proc, 5)
        assert not p.proc.is_alive()

      ensure_running(test_procs, started=False, params=Params(), CP=None, frogpilot_toggles=None)

      # a flagged process is reaped and restarted
      assert flagged.proc is not None and flagged.proc.is_alive()
      assert flagged.proc.pid != first_pids["test_flagged"]
      # an unflagged process keeps the opt-in semantics: it stays dead
      assert unflagged.proc is not None and not unflagged.proc.is_alive()
      assert unflagged.proc.pid == first_pids["test_unflagged"]
    finally:
      for p in test_procs:
        p.stop(retry=False, block=True, sig=signal.SIGKILL)

  @pytest.mark.skip("this test is flaky the way it's currently written, should be moved to test_onroad")
  def test_clean_exit(self, subtests):
    """
      Ensure all processes exit cleanly when stopped.
    """
    HARDWARE.set_power_save(False)
    manager.manager_init()

    CP = car.CarParams.new_message()
    procs = ensure_running(managed_processes.values(), True, Params(), CP, not_run=BLACKLIST_PROCS)

    time.sleep(10)

    for p in procs:
      with subtests.test(proc=p.name):
        state = p.get_process_state_msg()
        assert state.running, f"{p.name} not running"
        exit_code = p.stop(retry=False)

        assert p.name not in BLACKLIST_PROCS, f"{p.name} was started"

        assert exit_code is not None, f"{p.name} failed to exit"

        # TODO: interrupted blocking read exits with 1 in cereal. use a more unique return code
        exit_codes = [0, 1]
        if p.sigkill:
          exit_codes = [-signal.SIGKILL]
        assert exit_code in exit_codes, f"{p.name} died with {exit_code}"
