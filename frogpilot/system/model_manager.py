#!/usr/bin/env python3
import requests
import tarfile
import threading

from openpilot.frogpilot.common import frogpilot_download_utilities, frogpilot_utilities, frogpilot_variables

class ModelManager:
  def __init__(self, params):
    self.params = params

    self.download_lock = threading.Lock()
    self.download_state = frogpilot_download_utilities.DownloadState()

    self.downloading_automatically = False
    self.models_updated = False

    self.session = requests.Session()

  def download_model(self, model_id, repo_url):
    archive_path = frogpilot_variables.MODELS_PATH / f"{model_id}.tar"
    url = f"{repo_url}/Models/{frogpilot_variables.MODELS_FOLDER}/{model_id}.tar"
    if frogpilot_download_utilities.download_file(archive_path, self.download_state, self.session, url) is None:
      return False

    extract_path = frogpilot_variables.MODELS_PATH / f"{model_id}.tmp"
    frogpilot_utilities.delete_file(extract_path)
    with tarfile.open(archive_path) as archive:
      archive.extractall(extract_path, filter="data")
    archive_path.unlink()

    extract_path.rename(frogpilot_variables.MODELS_PATH / model_id)
    self.models_updated = True
    return True

  def download_models(self, model_ids, finished_message):
    for model_id in model_ids:
      if (frogpilot_variables.MODELS_PATH / model_id).is_dir():
        continue

      for repo_url in [frogpilot_download_utilities.GITHUB_URL, frogpilot_download_utilities.GITLAB_URL]:
        self.download_state.progress = "Downloading..."

        if self.download_model(model_id, repo_url):
          break

        if self.download_state.cancelled:
          self.download_state.progress = "Download cancelled..."
          return

      else:
        self.download_state.progress = "Download failed..."
        return

    self.download_state.progress = finished_message

  def downloaded_models(self):
    return [model["id"] for model in frogpilot_variables.get_models() if (frogpilot_variables.MODELS_PATH / model["id"]).is_dir()]

  def missing_models(self):
    return [model["id"] for model in frogpilot_variables.get_models() if not (frogpilot_variables.MODELS_PATH / model["id"]).is_dir()]

  def update_models(self, automatically_download):
    for repo_url in [frogpilot_download_utilities.GITHUB_URL, frogpilot_download_utilities.GITLAB_URL]:
      try:
        response = self.session.get(f"{repo_url}/Models/{frogpilot_variables.MODELS_FOLDER}/models.json", timeout=10)
        response.raise_for_status()
        downloaded_models = response.json()
      except requests.exceptions.RequestException:
        continue

      break

    else:
      self.download_state.progress = "Download failed..."
      return

    models = []
    for model in downloaded_models:
      if model["id"] == frogpilot_variables.DEFAULT_MODEL["id"]:
        continue

      models.append({"id": str(model["id"]), "lat_smooth_seconds": float(model["lat_smooth_seconds"]), "name": str(model["name"])})

    if models != frogpilot_variables.get_models():
      frogpilot_variables.MODELS_PATH.mkdir(parents=True, exist_ok=True)
      frogpilot_utilities.update_json_file(frogpilot_variables.MODELS_LIST_PATH, models)
      self.models_updated = True

    model_ids = {model["id"] for model in models}
    for path in frogpilot_variables.MODELS_PATH.iterdir():
      if path != frogpilot_variables.MODELS_LIST_PATH and path.name not in model_ids:
        frogpilot_utilities.delete_file(path)
        self.models_updated = True

    missing_models = self.missing_models()
    if missing_models and automatically_download:
      self.downloading_automatically = True
      try:
        self.download_state.cancelled = False
        self.download_state.progress = "Downloading..."
        self.download_models(missing_models, "All models downloaded!")
      finally:
        self.downloading_automatically = False
