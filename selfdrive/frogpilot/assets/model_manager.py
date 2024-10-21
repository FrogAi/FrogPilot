import json
import os
import re
import requests
import shutil
import time
import urllib.parse
import urllib.request

from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params

from openpilot.selfdrive.frogpilot.assets.download_functions import GITHUB_URL, GITLAB_URL, download_file, get_repository_url, handle_error, handle_request_error, verify_download
from openpilot.selfdrive.frogpilot.frogpilot_utilities import delete_file
from openpilot.selfdrive.frogpilot.frogpilot_variables import DEFAULT_MODEL, DEFAULT_MODEL_NAME, DEFAULT_CLASSIC_MODEL, DEFAULT_CLASSIC_MODEL_NAME, MODELS_PATH

VERSION = "v10"

class ModelManager:
  def __init__(self):
    self.params = Params()
    self.params_memory = Params("/dev/shm/params")

    self.cancel_download_param = "CancelModelDownload"
    self.download_param = "ModelToDownload"
    self.download_progress_param = "ModelDownloadProgress"

  @staticmethod
  def fetch_models(url):
    try:
      with urllib.request.urlopen(url, timeout=10) as response:
        return json.loads(response.read().decode('utf-8'))['models']
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
      thneed_files = [file for file in response.json() if file['name'].endswith('.thneed')]

      if "gitlab" in repo_url:
        model_sizes = {}
        for file in thneed_files:
          file_path = file['path']
          metadata_url = f"https://gitlab.com/api/v4/projects/{urllib.parse.quote_plus(project_path)}/repository/files/{urllib.parse.quote_plus(file_path)}/raw?ref={branch}"
          metadata_response = requests.head(metadata_url)
          metadata_response.raise_for_status()
          model_sizes[file['name'].replace('.thneed', '')] = int(metadata_response.headers.get('content-length', 0))
        return model_sizes
      else:
        return {file['name'].replace('.thneed', ''): file['size'] for file in thneed_files if 'size' in file}
    except Exception as e:
      handle_request_error(f"Failed to fetch model sizes from {'GitHub' if 'github' in repo_url else 'GitLab'}: {e}", None, None, None, None)
      return {}

  @staticmethod
  def copy_default_model():
    classic_default_model_path = os.path.join(MODELS_PATH, f"{DEFAULT_CLASSIC_MODEL}.thneed")
    source_path = os.path.join(BASEDIR, "selfdrive", "classic_modeld", "models", "supercombo.thneed")

    if os.path.isfile(source_path):
      shutil.copyfile(source_path, classic_default_model_path)
      print(f"Copied the classic default model from {source_path} to {classic_default_model_path}")

    default_model_path = os.path.join(MODELS_PATH, f"{DEFAULT_MODEL}.thneed")
    source_path = os.path.join(BASEDIR, "selfdrive", "modeld", "models", "supercombo.thneed")

    if os.path.isfile(source_path):
      shutil.copyfile(source_path, default_model_path)
      print(f"Copied the default model from {source_path} to {default_model_path}")

  def handle_verification_failure(self, model, model_path, temp_model_path):
    if self.params_memory.get_bool(self.cancel_download_param):
      return

    print(f"Verification failed for model {model}. Retrying from GitLab...")
    model_url = f"{GITLAB_URL}Models/{model}.thneed"
    download_file(self.cancel_download_param, model_path, temp_model_path, self.download_progress_param, model_url, self.download_param, self.params_memory)

    if verify_download(model_path, temp_model_path, model_url):
      print(f"Model {model} redownloaded and verified successfully from GitLab")
    else:
      handle_error(model_path, "GitLab verification failed", "Verification failed", self.download_param, self.download_progress_param, self.params_memory)

  def download_model(self, model_to_download):
    model_path = os.path.join(MODELS_PATH, f"{model_to_download}.thneed")
    temp_model_path = f"{os.path.splitext(model_path)[0]}_temp.thneed"
    if os.path.isfile(model_path):
      handle_error(model_path, "Model already exists...", "Model already exists...", self.download_param, self.download_progress_param, self.params_memory)
      return

    repo_url = get_repository_url()
    if not repo_url:
      handle_error(temp_model_path, "GitHub and GitLab are offline...", "Repository unavailable", self.download_param, self.download_progress_param, self.params_memory)
      return

    model_url = f"{repo_url}Models/{model_to_download}.thneed"
    print(f"Downloading model: {model_to_download}")
    download_file(self.cancel_download_param, model_path, temp_model_path, self.download_progress_param, model_url, self.download_param, self.params_memory)

    if verify_download(model_path, temp_model_path, model_url):
      print(f"Model {model_to_download} downloaded and verified successfully!")
      self.params_memory.put(self.download_progress_param, "Downloaded!")
      self.params_memory.remove(self.download_param)
    else:
      self.handle_verification_failure(model_to_download, model_path, temp_model_path)

  def queue_model_download(self, model, model_name=None):
    while self.params_memory.get(self.download_param, encoding='utf-8'):
      time.sleep(1)

    self.params_memory.put(self.download_param, model)
    if model_name:
      self.params_memory.put(self.download_progress_param, f"Downloading {model_name}...")

  def update_model_params(self, model_info, repo_url):
    available_models = []
    for model in model_info:
      available_models.append(model['id'])

    self.params.put_nonblocking("AvailableModels", ','.join(available_models))
    self.params.put_nonblocking("AvailableModelsNames", ','.join([model['name'] for model in model_info]))
    self.params.put_nonblocking("ClassicModels", ','.join([model['id'] for model in model_info if model.get("classic_model", False)]))
    self.params.put_nonblocking("ExperimentalModels", ','.join([model['id'] for model in model_info if model.get("experimental", False)]))
    self.params.put_nonblocking("NavigationModels", ','.join([model['id'] for model in model_info if "🗺️" in model['name']]))
    self.params.put_nonblocking("RadarlessModels", ','.join([model['id'] for model in model_info if "📡" not in model['name']]))
    print("Models list updated successfully")

    if available_models:
      models_downloaded = self.are_all_models_downloaded(available_models, repo_url)
      self.params.put_bool_nonblocking("ModelsDownloaded", models_downloaded)

  def are_all_models_downloaded(self, available_models, repo_url):
    available_models = set(available_models) - {DEFAULT_MODEL, DEFAULT_CLASSIC_MODEL}

    automatically_update_models = self.params.get_bool("AutomaticallyUpdateModels")
    all_models_downloaded = True

    model_sizes = self.fetch_all_model_sizes(repo_url)
    download_queue = []

    for model in available_models:
      model_path = os.path.join(MODELS_PATH, f"{model}.thneed")
      expected_size = model_sizes.get(model)

      if expected_size is None:
        print(f"Size data for {model} not available.")
        continue

      if os.path.isfile(model_path):
        local_size = os.path.getsize(model_path)
        if automatically_update_models and local_size != expected_size:
          print(f"Model {model} is outdated. Re-downloading...")
          delete_file(model_path)
          download_queue.append(model)
          all_models_downloaded = False
      else:
        if automatically_update_models:
          print(f"Model {model} isn't downloaded. Downloading...")
          download_queue.append(model)
        all_models_downloaded = False

    for model in download_queue:
      self.queue_model_download(model)

    return all_models_downloaded

  def validate_models(self):
    current_model = self.params.get("Model", encoding='utf-8')
    current_model_name = self.params.get("ModelName", encoding='utf-8')

    if "(Default)" in current_model_name and current_model_name != DEFAULT_CLASSIC_MODEL_NAME:
      self.params.put_nonblocking("ModelName", current_model_name.replace(" (Default)", ""))

    available_models = self.params.get("AvailableModels", encoding='utf-8')
    if not available_models:
      return

    current_model_path = os.path.join(MODELS_PATH, f"{current_model}.thneed")
    if not os.path.isfile(current_model_path):
      print(f"Model {current_model} is not downloaded. Downloading...")
      self.download_model(current_model)

    for model_file in os.listdir(MODELS_PATH):
      model_name = model_file.replace(".thneed", "")
      if model_name not in available_models.split(','):
        if model_name == current_model:
          self.params.put_nonblocking("Model", DEFAULT_CLASSIC_MODEL)
          self.params.put_nonblocking("ModelName", DEFAULT_CLASSIC_MODEL_NAME)
        delete_file(os.path.join(MODELS_PATH, model_file))
        print(f"Deleted model file: {model_file} - Reason: Model is not in the list of available models")

  def update_models(self, boot_run=False):
    if boot_run:
      self.copy_default_model()

    repo_url = get_repository_url()
    if repo_url is None:
      print("GitHub and GitLab are offline...")
      return

    model_info = self.fetch_models(f"{repo_url}Versions/model_names_{VERSION}.json")
    if model_info:
      self.update_model_params(model_info, repo_url)

    if boot_run:
      self.validate_models()

  def download_all_models(self):
    repo_url = get_repository_url()
    if not repo_url:
      handle_error(None, "GitHub and GitLab are offline...", "Repository unavailable", self.download_param, self.download_progress_param, self.params_memory)
      return

    model_info = self.fetch_models(f"{repo_url}Versions/model_names_{VERSION}.json")
    if not model_info:
      handle_error(None, "Unable to update model list...", "Model list unavailable", self.download_param, self.download_progress_param, self.params_memory)
      return

    available_models = self.params.get("AvailableModels", encoding='utf-8')
    if not available_models:
      handle_error(None, "There's no model to download...", "There's no model to download...", self.download_param, self.download_progress_param, self.params_memory)
      return

    available_models = available_models.split(',')
    available_model_names = self.params.get("AvailableModelsNames", encoding='utf-8').split(',')

    for model in available_models:
      if self.params_memory.get_bool(self.cancel_download_param):
        return

      if not os.path.isfile(os.path.join(MODELS_PATH, f"{model}.thneed")):
        model_index = available_models.index(model)
        model_name = available_model_names[model_index]
        cleaned_model_name = re.sub(r'[🗺️👀📡]', '', model_name).strip()
        print(f"Downloading model: {cleaned_model_name}")
        self.queue_model_download(model, cleaned_model_name)

        while self.params_memory.get(self.download_param, encoding='utf-8'):
          time.sleep(1)

    while not all(os.path.isfile(os.path.join(MODELS_PATH, f"{model}.thneed")) for model in available_models):
      time.sleep(1)

    self.params_memory.put(self.download_progress_param, "All models downloaded!")
    self.params_memory.remove("DownloadAllModels")
    self.params.put_bool_nonblocking("ModelsDownloaded", True)
