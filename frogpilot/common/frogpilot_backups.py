#!/usr/bin/env python3
import datetime
import shutil
import tarfile
import zstandard as zstd

from pathlib import Path

from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params

from openpilot.frogpilot.common.frogpilot_utilities import delete_file
from openpilot.frogpilot.common.frogpilot_variables import EXCLUDED_KEYS, FROGPILOT_BACKUPS, TOGGLE_BACKUPS


def backup_frogpilot(build_metadata, params):
  maximum_backups = 3
  cleanup_backups(FROGPILOT_BACKUPS, maximum_backups)

  today = datetime.datetime.now().date()
  for backup in FROGPILOT_BACKUPS.glob("*_auto.tar.zst"):
    if backup.name.endswith(f"_{build_metadata.channel}_auto.tar.zst"):
      if datetime.datetime.fromtimestamp(backup.stat().st_mtime).date() == today:
        if not backup.name.startswith(f"{build_metadata.openpilot.git_commit[:6]}_"):
          delete_file(backup, report=False)

  _, _, free = shutil.disk_usage(FROGPILOT_BACKUPS)
  minimum_backup_size = params.get("MinimumBackupSize")
  if free > minimum_backup_size * maximum_backups:
    destination = FROGPILOT_BACKUPS / f"{build_metadata.openpilot.git_commit}_{build_metadata.channel}_auto"
    create_backup(Path(BASEDIR), destination, "Successfully backed up FrogPilot!", "Failed to backup FrogPilot...", params, minimum_backup_size, compressed=True)


def backup_toggles(params, boot_run=False):
  params_backup = Params("/dev/shm/params_backup", return_defaults=True)

  changes_found = False
  for key in params.all_keys():
    current_value = params.get(key)
    if current_value is None:
      continue

    if boot_run:
      params_backup.put(key, current_value)
      changes_found = True
    elif current_value != params_backup.get(key):
      params_backup.put(key, current_value)
      changes_found |= key.decode("utf-8") not in EXCLUDED_KEYS

  maximum_backups = 5
  cleanup_backups(TOGGLE_BACKUPS, maximum_backups)

  if not changes_found or boot_run:
    if not changes_found:
      print("Toggles are identical to the previous backup. Aborting...")
    return

  destination = TOGGLE_BACKUPS / f"{datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}_auto"
  create_backup(Path(params_backup.get_param_path()), destination, "Successfully backed up toggles!", "Failed to backup toggles...", params)


def cleanup_backups(directory, limit):
  directory.mkdir(parents=True, exist_ok=True)

  for backup in directory.glob("*_in_progress*"):
    delete_file(backup, report=False)

  backups = sorted(directory.glob("*_auto*"), key=lambda f: f.stat().st_mtime, reverse=True)
  for oldest_backup in backups[limit:]:
    delete_file(oldest_backup, report=False)


def create_backup(backup, destination, success_message, fail_message, params, minimum_backup_size=0, compressed=False):
  final_destination = destination.parent / f"{destination.name}.tar.zst" if compressed else destination

  if final_destination.exists():
    print("Backup already exists. Aborting...")
    return

  if compressed:
    compressed_temp = destination.parent / f"{destination.name}_in_progress.tar.zst"

    with open(compressed_temp, "wb") as f_out:
      cctx = zstd.ZstdCompressor()
      with cctx.stream_writer(f_out) as compressor:
        with tarfile.open(fileobj=compressor, mode="w") as tar:
          tar.add(backup, arcname=destination.name, recursive=False)
          for entry in sorted(backup.iterdir(), key=lambda entry: entry.name):
            if entry.name == ".overlay_consistent":
              continue
            tar.add(entry, arcname=Path(destination.name) / entry.name)

    try:
      compressed_temp.rename(final_destination)
    except FileNotFoundError:
      print(fail_message)
      return

    compressed_backup_size = final_destination.stat().st_size
    if minimum_backup_size == 0 or compressed_backup_size < minimum_backup_size:
      params.put("MinimumBackupSize", int(compressed_backup_size))
  else:
    in_progress_destination = destination.parent / f"{destination.name}_in_progress"

    shutil.copytree(backup, in_progress_destination, symlinks=True)

    in_progress_destination.rename(destination)

  print(success_message)
