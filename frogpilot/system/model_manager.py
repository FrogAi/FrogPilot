#!/usr/bin/env python3
import os
import random
import requests
import tarfile
import threading

from openpilot.frogpilot.common import frogpilot_download_utilities, frogpilot_paths, frogpilot_utilities, frogpilot_variables

ARCHIVE_SIZE_FILE = "archive_size"

def model_url(repo_url, file_name):
  return f"{repo_url}/Models/{frogpilot_paths.MODELS_FOLDER}/{file_name}"


class ModelManager:
  def __init__(self, params):
    self.params = params

    self.download_lock = threading.Lock()
    self.download_state = frogpilot_download_utilities.DownloadState()

    self.downloading_automatically = False
    self.models_updated = False

    self.session = requests.Session()

  def download_model(self, model_id, repo_url):
    archive_path = frogpilot_paths.MODELS_PATH / f"{model_id}.tar"
    if frogpilot_download_utilities.download_file(archive_path, self.download_state, self.session, model_url(repo_url, f"{model_id}.tar")) is None:
      return False

    extract_path = frogpilot_paths.MODELS_PATH / f"{model_id}.tmp"
    frogpilot_utilities.delete_path(extract_path)
    with tarfile.open(archive_path) as archive:
      archive.extractall(extract_path, filter="data")
    (extract_path / ARCHIVE_SIZE_FILE).write_text(str(archive_path.stat().st_size))
    archive_path.unlink()

    os.sync()
    extract_path.rename(frogpilot_paths.MODELS_PATH / model_id)
    self.models_updated = True
    return True

  def download_models(self, model_ids, finished_message):
    for model_id in model_ids:
      if (frogpilot_paths.MODELS_PATH / model_id).is_dir():
        continue

      for repo_url in frogpilot_variables.RESOURCE_REPOSITORY_URLS:
        if self.download_model(model_id, repo_url):
          break

        if self.download_state.cancelled:
          return

      else:
        self.download_state.progress = "Download failed..."
        return

    self.download_state.progress = finished_message

  def downloaded_models(self):
    return [model["id"] for model in frogpilot_variables.get_models() if (frogpilot_paths.MODELS_PATH / model["id"]).is_dir()]

  def missing_models(self):
    return [model["id"] for model in frogpilot_variables.get_models() if not (frogpilot_paths.MODELS_PATH / model["id"]).is_dir()]

  def randomize_model(self, new_drive):
    blacklisted_models = self.params.get("BlacklistedModels").split(",")
    randomized_model = self.params.get("RandomizedModel")

    selectable_models = [model_id for model_id in self.downloaded_models() + [frogpilot_variables.DEFAULT_MODEL["id"]] if model_id not in blacklisted_models]
    if not selectable_models:
      selectable_models = [frogpilot_variables.DEFAULT_MODEL["id"]]

    if randomized_model in selectable_models:
      if not new_drive:
        return

      if len(selectable_models) > 1:
        selectable_models.remove(randomized_model)

    self.params.put("RandomizedModel", random.choice(selectable_models))

  def remove_outdated_models(self, repo_url):
    for model_id in self.downloaded_models():
      remote_size = frogpilot_download_utilities.get_remote_file_size(self.session, model_url(repo_url, f"{model_id}.tar"))
      if remote_size == 0:
        continue

      size_path = frogpilot_paths.MODELS_PATH / model_id / ARCHIVE_SIZE_FILE
      if not size_path.is_file():
        size_path.write_text(str(remote_size))
      elif int(size_path.read_text()) != remote_size:
        outdated_path = frogpilot_paths.MODELS_PATH / f"{model_id}.tmp"
        frogpilot_utilities.delete_path(outdated_path)
        (frogpilot_paths.MODELS_PATH / model_id).rename(outdated_path)
        frogpilot_utilities.delete_path(outdated_path)
        self.models_updated = True

  def update_models(self, automatically_download):
    for repo_url in frogpilot_variables.RESOURCE_REPOSITORY_URLS:
      try:
        response = self.session.get(model_url(repo_url, "models.json"), timeout=10)
        response.raise_for_status()
        remote_models = response.json()
      except requests.exceptions.RequestException:
        continue

      break

    else:
      return

    models = []
    for model in remote_models:
      if model["id"] == frogpilot_variables.DEFAULT_MODEL["id"]:
        continue

      models.append({"id": str(model["id"]), "lat_smooth_seconds": float(model["lat_smooth_seconds"]), "name": str(model["name"])})

    if models != frogpilot_variables.get_models():
      frogpilot_paths.MODELS_PATH.mkdir(parents=True, exist_ok=True)
      frogpilot_utilities.update_json_file(frogpilot_paths.MODELS_LIST_PATH, models)
      self.models_updated = True

    model_ids = {model["id"] for model in models}
    for path in frogpilot_paths.MODELS_PATH.iterdir():
      if path != frogpilot_paths.MODELS_LIST_PATH and path.name not in model_ids:
        frogpilot_utilities.delete_path(path)
        self.models_updated = True

    if automatically_download:
      self.remove_outdated_models(repo_url)

    missing_models = self.missing_models()
    if missing_models and automatically_download:
      self.downloading_automatically = True
      try:
        self.download_state.cancelled = False
        self.download_state.progress = "Downloading..."
        self.download_models(missing_models, "All models downloaded!")
      finally:
        self.downloading_automatically = False
