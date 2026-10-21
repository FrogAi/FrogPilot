import json
import pytest
import time

from openpilot.common.params import Params
from openpilot.system.loggerd.xattr_cache import setxattr

from openpilot.frogpilot.common import frogpilot_api, frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system import frogpilot_telemetry


@pytest.fixture
def telemetry(mocker, tmp_path):
  mocker.patch.object(frogpilot_paths, "LOG_ROOTS", ())
  mocker.patch.object(frogpilot_paths, "SPEED_LIMIT_CAPTURES_PATH", tmp_path / "speed_limit_captures")
  mocker.patch.object(frogpilot_paths, "TELEMETRY_SCHEMAS_PATH", tmp_path / "telemetry_schemas")

  mocker.patch.object(frogpilot_api.FrogPilotAPI, "regenerate_token", return_value=None)
  mocker.patch.object(frogpilot_telemetry, "get_key_pair", return_value=("RSA", "private key", "public key"))
  mocker.patch.object(frogpilot_telemetry.FrogPilotTelemetry, "can_upload", return_value=True)
  mocker.patch.object(frogpilot_utilities, "is_FrogsGoMoo", return_value=False)
  mocker.patch.object(frogpilot_variables, "get_frogpilot_toggles", return_value=mocker.Mock(telemetry_sharing=True))

  params = Params()
  params.put("FrogPilotApiToken", "A" * 43)
  params.put("GitCommit", "0123456789abcdef0123456789abcdef01234567")

  return frogpilot_telemetry.FrogPilotTelemetry()


def respond(mocker, status_code):
  return mocker.patch("requests.Session.request", return_value=mocker.Mock(status_code=status_code))


def save_capture(name):
  capture = frogpilot_paths.SPEED_LIMIT_CAPTURES_PATH / f"{name}.hevc"
  capture.parent.mkdir(exist_ok=True, parents=True)
  capture.write_bytes(b"clip")
  capture.with_suffix(".json").write_text(json.dumps({"trigger": "lkas"}))

  setxattr(capture, frogpilot_variables.UPLOAD_ATTR_NAME, frogpilot_variables.UPLOAD_PENDING)

  return capture


class TestFrogPilotTelemetry:
  @pytest.mark.parametrize("status_code", [400, 404, 413])
  def test_refused_capture(self, telemetry, mocker, status_code):
    refused = save_capture("1")
    waiting = save_capture("2")

    request = respond(mocker, status_code)

    telemetry.update()

    assert request.call_count == 1
    assert not refused.exists()
    assert not refused.with_suffix(".json").exists()
    assert waiting.exists()
    assert telemetry.retry_at > time.monotonic()

  @pytest.mark.parametrize("status_code", [401, 408, 409, 429, 500, 503])
  def test_retried_capture(self, telemetry, mocker, status_code):
    capture = save_capture("1")

    request = respond(mocker, status_code)

    telemetry.update()

    assert request.call_count == 1
    assert capture.exists()
    assert capture.with_suffix(".json").exists()
    assert telemetry.retry_at > time.monotonic()
