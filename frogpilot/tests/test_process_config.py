import pytest
import types

from cereal import car
from openpilot.common.params import Params
from openpilot.system.manager.process_config import managed_processes


@pytest.mark.parametrize("name", ["logmessaged", "statsd"])
@pytest.mark.parametrize("started", [False, True])
def test_log_receivers_follow_no_logging(name, started):
  CP = car.CarParams.new_message()

  for no_logging in (False, True):
    frogpilot_toggles = types.SimpleNamespace(no_logging=no_logging)
    assert managed_processes[name].should_run(started, Params(), CP, frogpilot_toggles) == (not no_logging)
