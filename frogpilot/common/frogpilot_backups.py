#!/usr/bin/env python3
import datetime
import shutil
import tarfile
import zstandard as zstd

from pathlib import Path

from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables


def backup_frogpilot(build_metadata):
  maximum_backups = 3
  cleanup_backups(frogpilot_paths.FROGPILOT_BACKUPS, maximum_backups)

  _, _, free = shutil.disk_usage(frogpilot_paths.FROGPILOT_BACKUPS)
  minimum_backup_size = frogpilot_variables.params.get_int("MinimumBackupSize")

  if free <= minimum_backup_size * maximum_backups:
    return

  backup_name = f"{build_metadata.channel}_{build_metadata.openpilot.git_commit_date[12:-16]}{frogpilot_variables.AUTOMATIC_BACKUP_SUFFIX}"
  destination = frogpilot_paths.FROGPILOT_BACKUPS / f"{backup_name}.tar.zst"

  if destination.exists():
    print("Backup already exists. Aborting...")
    return

  in_progress_destination = frogpilot_paths.FROGPILOT_BACKUPS / f"{backup_name}{frogpilot_variables.IN_PROGRESS_BACKUP_SUFFIX}.tar.zst"

  try:
    with open(in_progress_destination, "wb") as backup_file:
      compressor = zstd.ZstdCompressor()
      with compressor.stream_writer(backup_file) as stream:
        with tarfile.open(fileobj=stream, mode="w") as tar:
          tar.add(BASEDIR, arcname=backup_name, recursive=False)

          for entry in sorted(Path(BASEDIR).iterdir(), key=lambda entry: entry.name):
            if entry.name == ".overlay_consistent":
              continue

            tar.add(entry, arcname=Path(backup_name) / entry.name)

    in_progress_destination.rename(destination)
  except FileNotFoundError:
    print("Failed to backup FrogPilot...")
    return
  finally:
    in_progress_destination.unlink(missing_ok=True)

  compressed_backup_size = destination.stat().st_size

  if minimum_backup_size == 0 or compressed_backup_size < minimum_backup_size:
    frogpilot_variables.params.put_int("MinimumBackupSize", compressed_backup_size)

  remove_oldest_backups(frogpilot_paths.FROGPILOT_BACKUPS, maximum_backups)

  print("Successfully backed up FrogPilot!")


def backup_toggles(params_cache, boot_run=False):
  for key, _, _, _ in frogpilot_variables.frogpilot_default_params:
    current_value = frogpilot_variables.params.get(key)

    if current_value is not None and current_value != params_cache.get(key):
      params_cache.put(key, current_value)

  params_backup = Params(str(frogpilot_paths.PARAMS_BACKUP_PATH))

  changes_found = False
  for key in frogpilot_variables.SETTINGS_BACKUP_KEYS:
    current_value = frogpilot_variables.params.get(key)

    if boot_run or current_value != params_backup.get(key):
      if current_value is None:
        params_backup.remove(key)
      else:
        params_backup.put(key, current_value)

      changes_found = True

  maximum_backups = 5
  cleanup_backups(frogpilot_paths.TOGGLE_BACKUPS, maximum_backups)

  if boot_run:
    return

  if not changes_found:
    print("Toggles are identical to the previous backup. Aborting...")
    return

  create_settings_backup(params_backup, f"{datetime.datetime.now().strftime('%Y-%m-%d_%I-%M%p').lower()}{frogpilot_variables.AUTOMATIC_BACKUP_SUFFIX}")

  remove_oldest_backups(frogpilot_paths.TOGGLE_BACKUPS, maximum_backups)

  print("Successfully backed up toggles!")


def cleanup_backups(directory, limit):
  directory.mkdir(parents=True, exist_ok=True)

  for backup in directory.glob(f"*{frogpilot_variables.IN_PROGRESS_BACKUP_SUFFIX}*"):
    frogpilot_utilities.delete_path(backup, report=False)

  remove_oldest_backups(directory, limit)


def create_settings_backup(params, name):
  destination = frogpilot_paths.TOGGLE_BACKUPS / name
  in_progress_destination = frogpilot_paths.TOGGLE_BACKUPS / f"{name}{frogpilot_variables.IN_PROGRESS_BACKUP_SUFFIX}"

  shutil.rmtree(in_progress_destination, ignore_errors=True)
  shutil.copytree(params.get_param_path(), in_progress_destination, symlinks=True,
                  ignore=lambda directory, entries: [entry for entry in entries if entry not in frogpilot_variables.SETTINGS_BACKUP_KEYS])

  if destination.exists():
    frogpilot_utilities.delete_path(destination, report=False)

  in_progress_destination.rename(destination)


def normalize_settings(settings):
  normalized_settings = {}

  for key, value in settings.items():
    current_key = frogpilot_variables.RENAMED_KEYS.get(key, key)

    if current_key not in frogpilot_variables.SETTINGS_BACKUP_KEYS and current_key != "CEModelStopTime":
      continue

    if key == current_key or current_key not in settings:
      normalized_settings[current_key] = value

  return normalized_settings


def read_settings_backup(source):
  files = {path.name: path for path in source.iterdir() if path.is_file()}
  settings = normalize_settings(files)

  return {key: path.read_bytes() for key, path in settings.items()}


def remove_oldest_backups(directory, limit):
  backups = sorted(directory.glob(f"*{frogpilot_variables.AUTOMATIC_BACKUP_SUFFIX}*"), key=lambda backup: backup.stat().st_mtime, reverse=True)

  for oldest_backup in backups[limit:]:
    frogpilot_utilities.delete_path(oldest_backup, report=False)
