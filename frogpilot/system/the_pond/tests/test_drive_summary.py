import pytest
import zstandard

from cereal import log

from openpilot.frogpilot.common import frogpilot_functions, frogpilot_paths, frogpilot_utilities
from openpilot.frogpilot.system.the_pond import drive_summary

DATA_SECTION_START = 16


@pytest.fixture
def schemas_path(mocker, tmp_path):
  mocker.patch.object(frogpilot_paths, "TELEMETRY_SCHEMAS_PATH", tmp_path / "telemetry_schemas")

  return tmp_path / "telemetry_schemas"


def car_state(log_mono_time, v_ego):
  return log.Event.new_message(carState={"vEgo": v_ego}, logMonoTime=log_mono_time).to_bytes()


def init_data(git_commit, schema=log):
  return schema.Event.new_message(initData={"gitCommit": git_commit}).to_bytes()


def save_qlog(path, events):
  qlog = path / "qlog.zst"
  qlog.write_bytes(zstandard.ZstdCompressor().compress(b"".join(events)))

  return str(qlog)


def unknown_event(log_mono_time):
  event = bytearray(log.Event.new_message(logMonoTime=log_mono_time).to_bytes())
  schema = log.Event.schema.node.struct
  discriminant = DATA_SECTION_START + schema.discriminantOffset * 2
  event[discriminant:discriminant + 2] = schema.discriminantCount.to_bytes(2, "little")

  return bytes(event)


class TestDriveSummary:
  def test_empty_qlog(self, schemas_path, tmp_path):
    qlog = save_qlog(tmp_path, [])

    summary = drive_summary.summarize([[0, qlog]])

    assert summary["distance"] == 0

  def test_saved_schema(self, schemas_path, tmp_path):
    frogpilot_functions.save_log_schema("other")
    moved_field = "selfdriveStateMoved @130 :SelfdriveState;\n    selfdriveState @150 :SelfdriveState;"
    schema_path = schemas_path / "other" / "log.capnp"
    schema_path.write_text(schema_path.read_text().replace("selfdriveState @130 :SelfdriveState;", moved_field))
    other_log = frogpilot_utilities.load_log_schema("other")

    events = [init_data("other", other_log)]
    for log_mono_time in (1000000000, 1100000000):
      events.append(other_log.Event.new_message(logMonoTime=log_mono_time, selfdriveState={"enabled": True}).to_bytes())

    qlog = save_qlog(tmp_path, events)

    summary = drive_summary.summarize([[0, qlog]])

    assert summary["engaged"] == [[0.0, 0.1]]

  def test_unknown_event(self, schemas_path, tmp_path):
    qlog = save_qlog(tmp_path, [init_data("unsaved"), car_state(1000000000, 10.0), unknown_event(1050000000), car_state(1100000000, 10.0)])

    summary = drive_summary.summarize([[0, qlog]])

    assert summary["distance"] == 1
