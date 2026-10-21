import pytest

from pathlib import Path
from openpilot.common.params import Params

from openpilot.frogpilot.common import frogpilot_backups, frogpilot_functions, frogpilot_paths


@pytest.mark.parametrize("unset_stores", [("data", "cache"), ("data",), ("cache",)])
def test_migrate_unset_secret(monkeypatch, tmp_path, unset_stores):
  monkeypatch.setattr(frogpilot_paths, "PARAMS_BACKUP_PATH", tmp_path / "backup")
  monkeypatch.setattr(frogpilot_paths, "TOGGLE_BACKUPS", tmp_path / "toggle_backups")

  stores = {"data": Params(str(tmp_path / "data")), "cache": Params(str(tmp_path / "cache"))}
  for name in unset_stores:
    Path(stores[name].get_param_path("SecOCKeys")).write_bytes(b"0")

  frogpilot_functions.migrate_params(stores["data"], stores["cache"])
  frogpilot_functions.sync_params_cache(stores["data"], stores["cache"])
  frogpilot_backups.backup_toggles(stores["data"], boot_run=True)

  for param_store in stores.values():
    assert param_store.get("SecOCKeys", return_default=True) == []
