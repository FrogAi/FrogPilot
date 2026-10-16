#!/usr/bin/env python3
import datetime
import os
import shutil
import tarfile
import zstandard as zstd

from pathlib import Path

from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables


def backup_frogpilot(build_metadata, params):
  maximum_backups = 3
  cleanup_backups(frogpilot_variables.FROGPILOT_BACKUPS, maximum_backups)

  today = datetime.datetime.now().date()
  for backup in frogpilot_variables.FROGPILOT_BACKUPS.glob(f"*_{build_metadata.channel}{frogpilot_variables.AUTOMATIC_BACKUP_SUFFIX}.tar.zst"):
    made_today = datetime.datetime.fromtimestamp(backup.stat().st_mtime).date() == today
    same_commit = backup.name.startswith(f"{build_metadata.openpilot.git_commit}_")
    if made_today and not same_commit:
      frogpilot_utilities.delete_path(backup, report=False)

  _, _, free = shutil.disk_usage(frogpilot_variables.FROGPILOT_BACKUPS)
  minimum_backup_size = params.get("MinimumBackupSize")
  if free <= minimum_backup_size * maximum_backups:
    return

  backup_name = f"{build_metadata.openpilot.git_commit}_{build_metadata.channel}{frogpilot_variables.AUTOMATIC_BACKUP_SUFFIX}"
  destination = frogpilot_variables.FROGPILOT_BACKUPS / f"{backup_name}.tar.zst"
  if destination.exists():
    print("Backup already exists. Aborting...")
    return

  in_progress_destination = frogpilot_variables.FROGPILOT_BACKUPS / f"{backup_name}{frogpilot_variables.IN_PROGRESS_BACKUP_SUFFIX}.tar.zst"
  with open(in_progress_destination, "wb") as backup_file:
    compressor = zstd.ZstdCompressor()
    with compressor.stream_writer(backup_file) as stream:
      with tarfile.open(fileobj=stream, mode="w") as tar:
        tar.add(BASEDIR, arcname=backup_name, recursive=False)
        for entry in sorted(Path(BASEDIR).iterdir(), key=lambda entry: entry.name):
          if entry.name == ".overlay_consistent":
            continue
          tar.add(entry, arcname=Path(backup_name) / entry.name)

  try:
    in_progress_destination.rename(destination)
  except FileNotFoundError:
    print("Failed to backup FrogPilot...")
    return

  compressed_backup_size = destination.stat().st_size
  if minimum_backup_size == 0 or compressed_backup_size < minimum_backup_size:
    params.put("MinimumBackupSize", int(compressed_backup_size))

  remove_oldest_backups(frogpilot_variables.FROGPILOT_BACKUPS, maximum_backups)

  print("Successfully backed up FrogPilot!")


def backup_toggles(params, boot_run=False):
  params_backup = Params("/dev/shm/params_backup", return_defaults=True)

  changes_found = False
  for key in frogpilot_variables.SETTINGS_BACKUP_KEYS:
    current_value = params.get(key)
    if current_value is None:
      continue

    if boot_run or current_value != params_backup.get(key):
      params_backup.put(key, current_value)
      changes_found = True

  maximum_backups = 5
  cleanup_backups(frogpilot_variables.TOGGLE_BACKUPS, maximum_backups)

  if boot_run:
    return

  if not changes_found:
    print("Toggles are identical to the previous backup. Aborting...")
    return

  create_settings_backup(params_backup, f"{datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S_%f')}{frogpilot_variables.AUTOMATIC_BACKUP_SUFFIX}")

  remove_oldest_backups(frogpilot_variables.TOGGLE_BACKUPS, maximum_backups)

  print("Successfully backed up toggles!")


def cleanup_backups(directory, limit):
  directory.mkdir(parents=True, exist_ok=True)

  for backup in directory.glob(f"*{frogpilot_variables.IN_PROGRESS_BACKUP_SUFFIX}*"):
    frogpilot_utilities.delete_path(backup, report=False)

  remove_oldest_backups(directory, limit)


def create_settings_backup(params, name):
  destination = frogpilot_variables.TOGGLE_BACKUPS / name
  in_progress_destination = frogpilot_variables.TOGGLE_BACKUPS / f"{name}{frogpilot_variables.IN_PROGRESS_BACKUP_SUFFIX}"

  shutil.rmtree(in_progress_destination, ignore_errors=True)
  shutil.copytree(params.get_param_path(), in_progress_destination, symlinks=True,
                  ignore=lambda directory, entries: [entry for entry in entries if entry not in frogpilot_variables.SETTINGS_BACKUP_KEYS])
  in_progress_destination.rename(destination)


def remove_oldest_backups(directory, limit):
  backups = sorted(directory.glob(f"*{frogpilot_variables.AUTOMATIC_BACKUP_SUFFIX}*"), key=lambda backup: backup.stat().st_mtime, reverse=True)
  for oldest_backup in backups[limit:]:
    frogpilot_utilities.delete_path(oldest_backup, report=False)


def restore_settings_backup(params, backup_path):
  params_path = Path(params.get_param_path())
  for path in backup_path.iterdir():
    key = path.name
    if not path.is_file() or key not in frogpilot_variables.SETTINGS_BACKUP_KEYS:
      continue

    temporary_path = params_path / f".{key}.restore"
    shutil.copy2(path, temporary_path)
    os.replace(temporary_path, params_path / key)

    if path.stat().st_size == 0:
      params.remove(key)
