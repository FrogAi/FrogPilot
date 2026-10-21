import gzip
import json
import pytest
import requests
import threading
import types
import zlib

from openpilot.common.params import Params

from openpilot.frogpilot.common import frogpilot_api, frogpilot_paths
from openpilot.frogpilot.system import frogpilot_stats

MOVE_ID = "0f8fad5b-d9cb-469f-a165-70867728950e"
PREVIOUS_MOVE_ID = "7c9e6679-7425-40de-944b-e07fc1f90ae7"

TOGGLES = types.SimpleNamespace(car_make="toyota", frogpilot_telemetry=True, model="")


class RecordingParams(Params):
  def __init__(self, *args, **kwargs):
    self.saved_stats = []

  def put(self, key, dat):
    if key == "FrogPilotStats":
      self.saved_stats.append(dat)

    return super().put(key, dat)


@pytest.fixture
def api(mocker):
  mocker.patch.object(frogpilot_api.FrogPilotAPI, "regenerate_token", return_value="B" * 43)

  params = Params(return_defaults=True)
  params.put("FrogPilotApiToken", "A" * 43)
  params.put_bool("FrogPilotTelemetryConfirmed", True)

  return frogpilot_api.FrogPilotAPI(params)


def respond(mocker, *answers):
  responses = []
  for answer in answers:
    if isinstance(answer, dict):
      response = mocker.Mock(status_code=200)
      response.json.return_value = answer
    else:
      response = mocker.Mock(status_code=answer)

    responses.append(response)

  return mocker.patch("requests.request", side_effect=responses)


def sent(request):
  bodies = []
  for call in request.call_args_list:
    path = call.args[1].removeprefix(frogpilot_api.FROGPILOT_API)

    if path == "/v1/stats":
      decompressor = zlib.decompressobj(wbits=-15, zdict=frogpilot_paths.STATS_DICTIONARY_PATH.read_bytes())
      bodies.append((path, json.loads(decompressor.decompress(call.kwargs["data"]))))
    else:
      bodies.append((path, json.loads(gzip.decompress(call.kwargs["data"]))))

  return bodies


class TestMergeStats:
  def test_counted_keys(self):
    device_stats = {"FrogHops": 2, "FrogPilotDrives": 1, "FrogPilotMeters": 1.5, "FrogPilotSeconds": 0.1}
    added_stats = {"FrogHops": 3, "FrogPilotDrives": 2, "FrogPilotMeters": 2.25, "FrogPilotSeconds": 0.2, "GoatScreams": 1}

    frogpilot_stats.merge_stats(device_stats, added_stats)

    assert device_stats == {"FrogHops": 5, "FrogPilotDrives": 3, "FrogPilotMeters": 3.75, "FrogPilotSeconds": 0.1 + 0.2, "GoatScreams": 1}

  @pytest.mark.parametrize("month, current_months_meters", [(5, 30), (6, 10)])
  def test_current_months_meters(self, month, current_months_meters):
    device_stats = {"CurrentMonthsMeters": 10, "Month": 5}

    frogpilot_stats.merge_stats(device_stats, {"CurrentMonthsMeters": 20, "Month": month})

    assert device_stats == {"CurrentMonthsMeters": current_months_meters, "Month": 5}

  def test_kept_values(self):
    device_stats = {"Enabled": True, "Name": "device"}

    frogpilot_stats.merge_stats(device_stats, {"Added": False, "Enabled": False, "Name": "other", "StatsMoveId": MOVE_ID})

    assert device_stats == {"Enabled": True, "Name": "device"}

  def test_maximums(self):
    device_stats = {"HighestSpeed": 30, "LongestDistanceWithoutOverride": 9, "MaxAcceleration": 1.5}
    added_stats = {"HighestSpeed": 40, "LongestDistanceWithoutOverride": 4, "LongestDrive": 5, "MaxAcceleration": 2.5, "MaxLateral": 2}

    frogpilot_stats.merge_stats(device_stats, added_stats)

    assert device_stats == {"HighestSpeed": 40, "LongestDistanceWithoutOverride": 9, "LongestDrive": 5, "MaxAcceleration": 2.5, "MaxLateral": 2}

  @pytest.mark.parametrize("device_stats, merged_stats", [
    ({}, {"CurrentMonthsMeters": 20, "Month": 6}),
    ({"CurrentMonthsMeters": 10}, {"CurrentMonthsMeters": 20, "Month": 6}),
    ({"Month": 5}, {"Month": 5}),
  ])
  def test_month(self, device_stats, merged_stats):
    frogpilot_stats.merge_stats(device_stats, {"CurrentMonthsMeters": 20, "Month": 6})

    assert device_stats == merged_stats

  def test_numbers(self):
    device_stats = {"Engages": 3}

    frogpilot_stats.merge_stats(device_stats, {"Engages": 4, "Overrides": 2})

    assert device_stats == {"Engages": 7, "Overrides": 2}

  def test_objects(self):
    device_stats = {"ModelTimes": {"a": 1, "b": 2}}

    frogpilot_stats.merge_stats(device_stats, {"ModelTimes": {"b": 3, "c": 4}, "WeatherTimes": {"rain": 5}})

    assert device_stats == {"ModelTimes": {"a": 1, "b": 5, "c": 4}, "WeatherTimes": {"rain": 5}}


class TestStatsMove:
  def test_add(self, api, mocker):
    api.params.put("FrogPilotStats", {"Engages": 3, "StatsMoveId": PREVIOUS_MOVE_ID})
    api.params.put("StatsMoveAckedId", PREVIOUS_MOVE_ID)

    request = respond(mocker, {"action": "add", "id": MOVE_ID, "stats": {"Engages": 4}}, 204, 204)

    frogpilot_stats.send_stats(api.params, TOGGLES, api, threading.Lock())

    assert sent(request)[:2] == [("/v1/stats-move", {}), ("/v1/stats-move", {"id": MOVE_ID})]
    assert sent(request)[2][1]["user_stats"]["frogpilot_stats"] == {"Engages": 7}
    assert api.params.get("FrogPilotStats") == {"Engages": 7, "StatsMoveId": MOVE_ID}

  def test_acknowledged_move(self, api, mocker):
    api.params.put("FrogPilotStats", {"Engages": 3})

    request = respond(mocker, {"action": "wipe", "id": MOVE_ID}, {"action": "send", "id": PREVIOUS_MOVE_ID}, 503, 204)

    frogpilot_stats.send_stats(api.params, TOGGLES, api, threading.Lock())

    assert [path for path, _ in sent(request)] == ["/v1/stats-move"] * 3 + ["/v1/stats"]

  @pytest.mark.parametrize("action", ["add", "send", "wipe"])
  def test_applied_id(self, api, mocker, action):
    api.params.put("FrogPilotStats", {"Engages": 3, "StatsMoveId": MOVE_ID})

    request = respond(mocker, {"action": action, "id": MOVE_ID, "stats": {"Engages": 4}}, 204, 204)

    stats_lock = threading.Lock()
    stats_lock.acquire()

    frogpilot_stats.send_stats(api.params, TOGGLES, api, stats_lock)

    assert sent(request)[:2] == [("/v1/stats-move", {}), ("/v1/stats-move", {"id": MOVE_ID})]
    assert sent(request)[2][0] == "/v1/stats"
    assert api.params.get("FrogPilotStats") == {"Engages": 3, "StatsMoveId": MOVE_ID}

  @pytest.mark.parametrize("instruction, saved_stats", [
    ({"action": "add", "id": MOVE_ID, "stats": {"Engages": 4}}, {"Engages": 7, "StatsMoveId": MOVE_ID}),
    ({"action": "wipe", "id": MOVE_ID}, {"StatsMoveId": MOVE_ID}),
  ])
  def test_atomic_write(self, api, mocker, instruction, saved_stats):
    api.params.put("FrogPilotStats", {"Engages": 3})
    api.params = RecordingParams(return_defaults=True)

    respond(mocker, instruction, 503)

    frogpilot_stats.send_stats(api.params, TOGGLES, api, threading.Lock())

    assert api.params.saved_stats == [saved_stats]

  @pytest.mark.parametrize("action", ["add", "send", "wipe"])
  def test_drive_in_progress(self, api, mocker, action):
    api.params.put("FrogPilotStats", {"Engages": 3})

    request = respond(mocker, {"action": action, "id": MOVE_ID, "stats": {"Engages": 4}}, 204)

    stats_lock = threading.Lock()
    stats_lock.acquire()

    frogpilot_stats.send_stats(api.params, TOGGLES, api, stats_lock)

    assert sent(request)[0] == ("/v1/stats-move", {})
    assert [path for path, _ in sent(request)] == ["/v1/stats-move", "/v1/stats"]
    assert api.params.get("FrogPilotStats") == {"Engages": 3}

  def test_no_response(self, api, mocker):
    api.params.put("FrogPilotStats", {"Engages": 3})

    request = mocker.patch("requests.request", side_effect=requests.exceptions.ConnectionError)

    frogpilot_stats.send_stats(api.params, TOGGLES, api, threading.Lock())

    assert request.call_count == 2
    assert api.params.get("FrogPilotStats") == {"Engages": 3}

  def test_nothing_to_do(self, api, mocker):
    api.params.put("FrogPilotStats", {"Engages": 3})

    request = respond(mocker, 204, 204)

    frogpilot_stats.send_stats(api.params, TOGGLES, api, threading.Lock())

    bodies = sent(request)
    assert [path for path, _ in bodies] == ["/v1/stats-move", "/v1/stats"]
    assert bodies[0][1] == {}
    assert bodies[1][1]["user_stats"]["frogpilot_stats"] == {"Engages": 3}

  def test_pending_move(self, api, mocker):
    api.params.put("FrogPilotStats", {"Engages": 3})

    request = respond(mocker, {"action": "wipe", "id": MOVE_ID}, 503, {"action": "wipe", "id": MOVE_ID}, 503, 503, 204, 204)

    frogpilot_stats.send_stats(api.params, TOGGLES, api, threading.Lock())
    frogpilot_stats.send_stats(api.params, TOGGLES, api, threading.Lock())
    frogpilot_stats.send_stats(api.params, TOGGLES, api, threading.Lock())

    assert [path for path, _ in sent(request)] == ["/v1/stats-move"] * 5

    frogpilot_stats.send_stats(api.params, TOGGLES, api, threading.Lock())

    assert [path for path, _ in sent(request)] == ["/v1/stats-move"] * 6 + ["/v1/stats"]
    assert sent(request)[6][1]["user_stats"]["frogpilot_stats"] == {}

  @pytest.mark.parametrize("status_code, attempts", [(400, 1), (401, 2), (405, 1), (413, 1), (503, 1)])
  def test_refused_check_in(self, api, mocker, status_code, attempts):
    api.params.put("FrogPilotStats", {"Engages": 3})

    request = respond(mocker, *[status_code] * attempts, 204)

    frogpilot_stats.send_stats(api.params, TOGGLES, api, threading.Lock())

    assert sent(request)[:attempts] == [("/v1/stats-move", {})] * attempts
    assert [path for path, _ in sent(request)][attempts:] == ["/v1/stats"]
    assert api.params.get("FrogPilotStats") == {"Engages": 3}

  def test_round_trip_limit(self, api, mocker):
    api.params.put("FrogPilotStats", {"Engages": 3})

    request = respond(mocker, *[{"action": "send", "id": MOVE_ID}] * 4, 204)

    frogpilot_stats.send_stats(api.params, TOGGLES, api, threading.Lock())

    assert [path for path, _ in sent(request)] == ["/v1/stats-move"] * 4 + ["/v1/stats"]

  def test_send(self, api, mocker):
    api.params.put("FrogPilotStats", {"Engages": 3, "StatsMoveId": PREVIOUS_MOVE_ID})
    api.params.put("StatsMoveAckedId", PREVIOUS_MOVE_ID)

    request = respond(mocker, {"action": "send", "id": MOVE_ID}, {"action": "wipe", "id": MOVE_ID}, 204, 204)

    frogpilot_stats.send_stats(api.params, TOGGLES, api, threading.Lock())

    assert sent(request)[:3] == [
      ("/v1/stats-move", {}),
      ("/v1/stats-move", {"id": MOVE_ID, "stats": {"Engages": 3}}),
      ("/v1/stats-move", {"id": MOVE_ID}),
    ]
    assert sent(request)[3][1]["user_stats"]["frogpilot_stats"] == {}
    assert api.params.get("FrogPilotStats") == {"StatsMoveId": MOVE_ID}
