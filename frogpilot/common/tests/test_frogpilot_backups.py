from openpilot.common.params import Params

from openpilot.frogpilot.common import frogpilot_backups


def test_restore_skips_unset_secret(tmp_path):
  params = Params(str(tmp_path / "data"))
  params.put("CameraView", 0)
  params.put("SecOCKeys", [])

  backup_path = tmp_path / "backup"
  backup_path.mkdir()
  (backup_path / "CameraView").write_bytes(b"2")
  (backup_path / "SecOCKeys").write_bytes(b"0")

  frogpilot_backups.restore_settings_backup(params, backup_path)

  assert params.get("CameraView") == 2
  assert params.get("SecOCKeys") == []
