#!/usr/bin/env python3
import os

from pathlib import Path

from cereal import CEREAL_PATH
from openpilot.system.hardware import HARDWARE, PC

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities

SCHEMA_FILES = ("car.capnp", "custom.capnp", "include/c++.capnp", "legacy.capnp", "log.capnp")


def frogpilot_boot_functions(build_metadata):
  save_log_schema(build_metadata.openpilot.git_commit)


def install_frogpilot():
  paths = [
  ]
  for path in paths:
    path.mkdir(parents=True, exist_ok=True)

  update_boot_logo(frogpilot_paths.FROGPILOT_BOOT_LOGO_PATH)


def save_log_schema(git_commit):
  schema_path = frogpilot_paths.TELEMETRY_SCHEMAS_PATH / git_commit

  if not schema_path.is_dir():
    staging_path = frogpilot_paths.TELEMETRY_SCHEMAS_PATH / f".{git_commit}"

    for name in SCHEMA_FILES:
      (staging_path / name).parent.mkdir(parents=True, exist_ok=True)
      with open(staging_path / name, "wb") as f:
        f.write(Path(CEREAL_PATH, name).read_bytes())
        f.flush()
        os.fsync(f.fileno())

    staging_path.rename(schema_path)


def sync_params_cache(params, params_cache):
  for key in params.all_keys():
    current_value = params.get(key)
    cached_value = params_cache.get(key)

    if current_value is None:
      if cached_value is not None:
        params.put(key, cached_value)
    elif current_value != cached_value:
      params_cache.put(key, current_value)


def uninstall_frogpilot():
  update_boot_logo(frogpilot_paths.STOCK_BOOT_LOGO_PATH)

  HARDWARE.uninstall()


def update_boot_logo(target_logo):
  if PC:
    return

  if frogpilot_paths.BOOT_LOGO_PATH.read_bytes() != target_logo.read_bytes():
    frogpilot_utilities.run_cmd_on_writable_mount("/", ["sudo", "cp", target_logo, frogpilot_paths.BOOT_LOGO_PATH], "Failed to replace boot logo")
