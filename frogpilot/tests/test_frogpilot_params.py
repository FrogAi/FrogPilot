import pytest

from openpilot.common.params import Params

from openpilot.frogpilot.common import frogpilot_backups, frogpilot_functions, frogpilot_paths, frogpilot_variables


@pytest.mark.parametrize("unset_stores", [("data", "cache"), ("data",), ("cache",)])
def test_convert_unset_secret(monkeypatch, tmp_path, unset_stores):
  stores = {"data": Params(str(tmp_path / "data")), "cache": Params(str(tmp_path / "cache"))}

  monkeypatch.setattr(frogpilot_paths, "PARAMS_BACKUP_PATH", tmp_path / "backup")
  monkeypatch.setattr(frogpilot_paths, "TOGGLE_BACKUPS", tmp_path / "toggle_backups")
  monkeypatch.setattr(frogpilot_variables, "params", stores["data"])

  for name in unset_stores:
    stores[name].put("SecOCKeys", "0")

  frogpilot_functions.convert_params(stores["cache"])
  frogpilot_backups.backup_toggles(stores["cache"], boot_run=True)

  for param_store in stores.values():
    assert param_store.get("SecOCKeys") is None


def test_restore_skips_unset_secret(monkeypatch, tmp_path):
  params = Params(str(tmp_path / "data"))
  params.put("CameraView", "0")
  params.put("SecOCKeys", "[]")

  monkeypatch.setattr(frogpilot_variables, "params", params)
  monkeypatch.setattr(frogpilot_variables, "params_cache", Params(str(tmp_path / "cache")))

  source = tmp_path / "backup"
  source.mkdir()
  (source / "CameraView").write_bytes(b"2")
  (source / "SecOCKeys").write_bytes(b"0")

  frogpilot_functions.restore_settings(source)

  assert params.get("CameraView") == b"2"
  assert params.get("SecOCKeys") == b"[]"
