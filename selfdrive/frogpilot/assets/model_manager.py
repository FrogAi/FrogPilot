#!/usr/bin/env python3
import json
import re
import requests
import shutil
import time
import urllib.parse
import urllib.request

from pathlib import Path

from openpilot.selfdrive.frogpilot.assets.download_functions import GITLAB_URL, download_file, get_repository_url, handle_error, handle_request_error, verify_download
from openpilot.selfdrive.frogpilot.frogpilot_utilities import delete_file
from openpilot.selfdrive.frogpilot.frogpilot_variables import DEFAULT_CLASSIC_MODEL, DEFAULT_MODEL, DEFAULT_TINYGRAD_MODEL, MODELS_PATH, params, params_memory

VERSION = "v13"

CANCEL_DOWNLOAD_PARAM = "CancelModelDownload"
DOWNLOAD_PROGRESS_PARAM = "ModelDownloadProgress"
MODEL_DOWNLOAD_PARAM = "ModelToDownload"

class ModelManager:
  def __init__(self):
    self.available_models = (params.get("AvailableModels", encoding="utf-8") or "").split(",")

    self.downloading_model = False

  @staticmethod
  def fetch_models(url):
    try:
      with urllib.request.urlopen(url, timeout=10) as response:
        return json.loads(response.read().decode("utf-8"))["models"]
    except Exception as error:
      handle_request_error(error, None, None, None, None)
      return []

  @staticmethod
  def fetch_all_model_sizes(repo_url):
    project_path = "FrogAi/FrogPilot-Resources"
    branch = "Models"

    if "github" in repo_url:
      api_url = f"https://api.github.com/repos/{project_path}/contents?ref={branch}"
    elif "gitlab" in repo_url:
      api_url = f"https://gitlab.com/api/v4/projects/{urllib.parse.quote_plus(project_path)}/repository/tree?ref={branch}"
    else:
      return {}

    try:
      response = requests.get(api_url)
      response.raise_for_status()
      model_files = [file for file in response.json() if "." in file["name"]]

      if "gitlab" in repo_url:
        model_sizes = {}
        for file in model_files:
          file_path = file["path"]
          metadata_url = f"https://gitlab.com/api/v4/projects/{urllib.parse.quote_plus(project_path)}/repository/files/{urllib.parse.quote_plus(file_path)}/raw?ref={branch}"
          metadata_response = requests.head(metadata_url)
          metadata_response.raise_for_status()
          model_sizes[file["name"].rsplit(".", 1)[0]] = int(metadata_response.headers.get("content-length", 0))
        return model_sizes
      else:
        return {file["name"].rsplit(".", 1)[0]: file["size"] for file in model_files if "size" in file}

    except Exception as error:
      handle_request_error(f"Failed to fetch model sizes from {'GitHub' if 'github' in repo_url else 'GitLab'}: {error}", None, None, None, None)
      return {}

  def handle_verification_failure(self, model, model_path, file_extension):
    print(f"Verification failed for model {model}. Retrying from GitLab...")
    model_url = f"{GITLAB_URL}/Models/{model}.{file_extension}"
    download_file(CANCEL_DOWNLOAD_PARAM, model_path, DOWNLOAD_PROGRESS_PARAM, model_url, MODEL_DOWNLOAD_PARAM, params_memory)

    if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
      handle_error(None, "Download cancelled...", "Download cancelled...", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM, params_memory)
      self.downloading_model = False
      return

    if verify_download(model_path, model_url):
      print(f"Model {model} downloaded and verified successfully!")
      params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
      params_memory.remove(MODEL_DOWNLOAD_PARAM)
      self.downloading_model = False
    else:
      handle_error(model_path, "Verification failed...", "GitLab verification failed", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM, params_memory)
      self.downloading_model = False

  def download_model(self, model_to_download):
    self.downloading_model = True

    repo_url = get_repository_url()
    if not repo_url:
      handle_error(None, "GitHub and GitLab are offline...", "Repository unavailable", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM, params_memory)
      self.downloading_model = False
      return

    file_extension = "thneed" if self.model_versions[self.available_models.index(model_to_download)] in {"v1", "v2", "v3", "v4", "v5", "v6"} else "pkl"
    model_path = MODELS_PATH / f"{model_to_download}.{file_extension}"
    model_url = f"{repo_url}/Models/{model_to_download}.{file_extension}"
    print(f"Downloading model: {model_to_download}")
    download_file(CANCEL_DOWNLOAD_PARAM, model_path, DOWNLOAD_PROGRESS_PARAM, model_url, MODEL_DOWNLOAD_PARAM, params_memory)

    if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
      handle_error(None, "Download cancelled...", "Download cancelled...", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM, params_memory)
      self.downloading_model = False
      return

    if verify_download(model_path, model_url):
      print(f"Model {model_to_download} downloaded and verified successfully!")
      params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
      params_memory.remove(MODEL_DOWNLOAD_PARAM)
      self.downloading_model = False
    else:
      self.handle_verification_failure(model_to_download, model_path, file_extension)

  @staticmethod
  def copy_default_model():
    classic_default_model_path = MODELS_PATH / f"{DEFAULT_CLASSIC_MODEL}.thneed"
    source_path = Path(__file__).parents[2] / "classic_modeld/models/supercombo.thneed"
    if source_path.is_file() and not classic_default_model_path.is_file():
      shutil.copyfile(source_path, classic_default_model_path)
      print(f"Copied the classic default model from {source_path} to {classic_default_model_path}")

    default_model_path = MODELS_PATH / f"{DEFAULT_MODEL}.thneed"
    source_path = Path(__file__).parents[2] / "modeld/models/supercombo.thneed"
    if source_path.is_file() and not default_model_path.is_file():
      shutil.copyfile(source_path, default_model_path)
      print(f"Copied the default model from {source_path} to {default_model_path}")

    tingrad_default_model_path = MODELS_PATH / f"{DEFAULT_TINYGRAD_MODEL}.pkl"
    source_path = Path(__file__).parents[2] / "tinygrad_modeld/models/supercombo_tinygrad.pkl"
    if source_path.is_file() and not tingrad_default_model_path.is_file():
      shutil.copyfile(source_path, tingrad_default_model_path)
      print(f"Copied the default tinygrad model from {source_path} to {tingrad_default_model_path}")

  def check_models(self, boot_run, repo_url):
    available_models = set(self.available_models) - {DEFAULT_MODEL, DEFAULT_CLASSIC_MODEL}
    downloaded_models = {path.stem for path in MODELS_PATH.iterdir() if path.is_file()} - {DEFAULT_MODEL, DEFAULT_CLASSIC_MODEL}

    outdated_models = downloaded_models - available_models
    for model in outdated_models:
      for model_file in MODELS_PATH.glob(f"{model}.*"):
        print(f"Removing outdated model: {model}")
        delete_file(model_file)

    for tmp_file in MODELS_PATH.glob("tmp*"):
      if tmp_file.is_file():
        delete_file(tmp_file)

    automatically_update_models = not boot_run and params.get_bool("AutomaticallyUpdateModels")
    if not automatically_update_models:
      return

    model_sizes = self.fetch_all_model_sizes(repo_url)
    if not model_sizes:
      print("No model size data available. Skipping model checks")
      return

    for model in available_models:
      model_files = list(MODELS_PATH.glob(f"{model}.*"))
      expected_size = model_sizes.get(model)

      if expected_size is None:
        print(f"Size data for {model} not available.")
        continue

      for model_file in model_files:
        if model_file.is_file():
          local_size = model_file.stat().st_size
          if local_size == expected_size:
            continue
          print(f"Model {model} is outdated. Deleting {model_file}...")
          delete_file(model_file)

    self.download_all_models()

  def update_model_params(self, model_info, repo_url):
    self.available_models = [model["id"] for model in model_info]
    self.model_versions = [model["version"] for model in model_info]

    params.put("AvailableModels", ",".join(self.available_models))
    params.put("AvailableModelNames", ",".join([model["name"] for model in model_info]))
    params.put("ExperimentalModels", ",".join([model["id"] for model in model_info if model.get("experimental", False)]))
    params.put("ModelVersions", ",".join(self.model_versions))
    print("Models list updated successfully")

  def update_models(self, boot_run=False):
    if self.downloading_model:
      return

    repo_url = get_repository_url()
    if repo_url is None:
      print("GitHub and GitLab are offline...")
      return

    model_info = self.fetch_models(f"{repo_url}/Versions/model_names_{VERSION}.json")
    if model_info:
      self.update_model_params(model_info, repo_url)
      self.check_models(boot_run, repo_url)

  def queue_model_download(self, model, model_name=None):
    while params_memory.get(MODEL_DOWNLOAD_PARAM, encoding="utf-8"):
      time.sleep(1)

    params_memory.put(MODEL_DOWNLOAD_PARAM, model)
    if model_name:
      params_memory.put(DOWNLOAD_PROGRESS_PARAM, f"Downloading \"{model_name}\"...")

  def download_all_models(self):
    repo_url = get_repository_url()
    if not repo_url:
      handle_error(None, "GitHub and GitLab are offline...", "Repository unavailable", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM, params_memory)
      return

    model_info = self.fetch_models(f"{repo_url}/Versions/model_names_{VERSION}.json")
    if model_info:
      available_models = [model["id"] for model in model_info]
      available_model_names = [re.sub(r"[🗺️👀📡]", "", model["name"]).strip() for model in model_info]
      for model_id, model_name in zip(available_models, available_model_names):
        model_files = list(MODELS_PATH.glob(f"{model_id}.*"))
        if not model_files:
          print(f"Model {model_id} does not exist. Preparing to download...")

          if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
            handle_error(None, "Download cancelled...", "Download cancelled...", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM, params_memory)
            return

          self.queue_model_download(model_id, model_name)

      while not all(any(file.is_file() for file in MODELS_PATH.glob(f"{model}.*")) for model in available_models):
        if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
          handle_error(None, "Download cancelled...", "Download cancelled...", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM, params_memory)
          return
        time.sleep(1)

      params_memory.put(DOWNLOAD_PROGRESS_PARAM, "All models downloaded!")
    else:
      handle_error(None, "Unable to fetch models...", "Model list unavailable", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM, params_memory)
      return
