import pytest
import types

from cereal import log
from openpilot.system.loggerd.xattr_cache import getxattr

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_variables
from openpilot.frogpilot.system import frogpilot_telemetry

GM_GPS_ADDRESS = 0x32A
STEERING_ADDRESS = 0x1E1


def build_events():
  return [
    log.Event.new_message(gpsLocation={"latitude": 33.4}),
    log.Event.new_message(controlsState={"alertText1": "Startup", "alertText2": "Message"}),
    log.Event.new_message(can=[{"address": GM_GPS_ADDRESS, "dat": b"location", "src": 0}, {"address": STEERING_ADDRESS, "dat": b"steering", "src": 0}]),
  ]


def filter_events(events, default_car_make):
  data = b"".join(event.to_bytes() for event in events)
  filtered_data, _ = frogpilot_telemetry.filter_log(data, log, default_car_make)

  filtered_bytes = bytes(filtered_data)
  return filtered_bytes, {event.which(): event for event in log.Event.read_multiple_bytes(filtered_bytes)}


@pytest.mark.parametrize("car_make, addresses", [("gm", [STEERING_ADDRESS]), ("toyota", [GM_GPS_ADDRESS, STEERING_ADDRESS])])
def test_filter_log(car_make, addresses):
  events = build_events() + [log.Event.new_message(carParams={"carName": car_make})]

  filtered_bytes, filtered_events = filter_events(events, None)

  assert "gpsLocation" not in filtered_events
  assert filtered_events["controlsState"].controlsState.alertText1 == ""
  assert filtered_events["controlsState"].controlsState.alertText2 == ""
  assert [frame.address for frame in filtered_events["can"].can] == addresses
  assert (b"location" in filtered_bytes) == (GM_GPS_ADDRESS in addresses)


def test_filter_log_without_car_params_uses_default_car_make():
  filtered_bytes, filtered_events = filter_events(build_events(), "gm")

  assert [frame.address for frame in filtered_events["can"].can] == [STEERING_ADDRESS]
  assert b"location" not in filtered_bytes


def test_log_with_unreadable_schema_is_not_retried(tmp_path, monkeypatch):
  log_path = tmp_path / "rlog"
  log_path.write_bytes(log.Event.new_message(initData={"gitCommit": "previous"}).to_bytes())

  schema_path = tmp_path / "schemas" / "previous"
  schema_path.mkdir(parents=True)
  (schema_path / "log.capnp").write_bytes(b"")
  monkeypatch.setattr(frogpilot_paths, "TELEMETRY_SCHEMAS_PATH", tmp_path / "schemas")

  reports = []
  monkeypatch.setattr(frogpilot_telemetry.sentry, "capture_exception", lambda error, crash_log: reports.append(error))

  telemetry = frogpilot_telemetry.FrogPilotTelemetry.__new__(frogpilot_telemetry.FrogPilotTelemetry)
  telemetry.can_upload = lambda: True
  telemetry.frogpilot_api = types.SimpleNamespace(get_token=lambda: "token")
  telemetry.git_commit = "current"
  telemetry.mark_shared_logs = lambda: None
  telemetry.pending_logs = lambda: iter([(log_path, "drive")])
  telemetry.private_key = None
  telemetry.retry_at = 0
  telemetry.schemas = {}
  telemetry.update()

  assert len(reports) == 1
  assert getxattr(str(log_path), frogpilot_variables.UPLOAD_ATTR_NAME) == b"drive"
