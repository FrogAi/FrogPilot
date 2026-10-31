import types

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities
from openpilot.frogpilot.frogpilot_process import FrogPilotRequests


def test_requests_update_without_pending_requests():
  frogpilot_requests = FrogPilotRequests(model_manager=None, theme_manager=None)
  sm = {"deviceState": types.SimpleNamespace(networkMetered=False)}

  assert frogpilot_requests.update(False, sm, None, None) == (False, False)


def test_thread_exception_does_not_write_error_file(tmp_path, monkeypatch):
  monkeypatch.setattr(frogpilot_paths, "ERROR_LOGS_PATH", tmp_path)
  monkeypatch.setattr(frogpilot_paths, "ERROR_FILE_PATH", tmp_path / "error.txt")

  def failing_job():
    raise KeyError("reader_compatibility")

  frogpilot_utilities.run_thread_with_lock("update_checks", failing_job)
  frogpilot_utilities.running_threads["update_checks"].join()

  assert not (tmp_path / "error.txt").exists()
  assert len(list(tmp_path.iterdir())) == 1
