import pytest

from cereal import log
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
