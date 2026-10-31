import asyncio
import json
import pytest

from openpilot.common.params import Params

from openpilot.frogpilot.system.the_pond import home, utilities

MOVE_ID = "0f8fad5b-d9cb-469f-a165-70867728950e"


class TestHome:
  @pytest.mark.parametrize("frogpilot_stats, reset_stats", [
    ({"Engages": 3}, None),
    ({"Engages": 3, "StatsMoveId": MOVE_ID}, json.dumps({"StatsMoveId": MOVE_ID}).encode()),
  ])
  def test_reset_frogpilot_stats(self, mocker, frogpilot_stats, reset_stats):
    the_pond = mocker.MagicMock(params=Params())
    the_pond.sm["deviceState"].started = False
    the_pond.params.put("FrogPilotStats", json.dumps(frogpilot_stats))

    asyncio.run(home.reset_frogpilot_stats(mocker.Mock(app={utilities.THE_POND: the_pond})))

    assert Params().get("FrogPilotStats") == reset_stats
