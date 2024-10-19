import json
import os
import re
import shutil
import time
import urllib.request

from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params, UnknownKeyName

from openpilot.selfdrive.frogpilot.assets.download_functions import GITHUB_URL, GITLAB_URL, download_file, get_remote_file_size, get_repository_url, handle_error, handle_request_error, verify_download
from openpilot.selfdrive.frogpilot.frogpilot_functions import MODELS_PATH
from openpilot.selfdrive.frogpilot.frogpilot_utilities import delete_file

VERSION = "v9"

DEFAULT_MODEL = "north-dakota"
DEFAULT_MODEL_NAME = "North Dakota (Default)"

def process_model_name(model_name):
  cleaned_name = re.sub(r'[🗺️👀📡]', '', model_name)
  cleaned_name = re.sub(r'[^a-zA-Z0-9()-]', '', cleaned_name)
  return cleaned_name.replace(' ', '').replace('(Default)', '').replace('-', '')

class ModelManager:
  def __init__(self):
    self.params = Params()
    self.params_memory = Params("/dev/shm/params")

    self.cancel_download_param = "CancelModelDownload"
    self.download_param = "ModelToDownload"
    self.download_progress_param = "ModelDownloadProgress"

  def handle_verification_failure(self, model, model_path):
    if self.params_memory.get_bool(self.cancel_download_param):
      return

    print(f"Verification failed for model {model}. Retrying from GitLab...")
    model_url = f"{GITLAB_URL}Models/{model}.thneed"
    download_file(self.cancel_download_param, model_path, self.download_progress_param, model_url, self.download_param, self.params_memory)

    if verify_download(model_path, model_url):
      print(f"Model {model} redownloaded and verified successfully from GitLab.")
    else:
      handle_error(model_path, "GitLab verification failed", "Verification failed", self.download_param, self.download_progress_param, self.params_memory)

  def download_model(self, model_to_download):
    model_path = os.path.join(MODELS_PATH, f"{model_to_download}.thneed")
    if os.path.isfile(model_path):
      handle_error(model_path, "Model already exists...", "Model already exists...", self.download_param, self.download_progress_param, self.params_memory)
      return

    repo_url = get_repository_url()
    if not repo_url:
      handle_error(model_path, "GitHub and GitLab are offline...", "Repository unavailable", self.download_param, self.download_progress_param, self.params_memory)
      return

    model_url = f"{repo_url}Models/{model_to_download}.thneed"
    print(f"Downloading model: {model_to_download}")
    download_file(self.cancel_download_param, model_path, self.download_progress_param, model_url, self.download_param, self.params_memory)

    if verify_download(model_path, model_url):
      print(f"Model {model_to_download} downloaded and verified successfully!")
      self.params_memory.put(self.download_progress_param, "Downloaded!")
      self.params_memory.remove(self.download_param)
    else:
      self.handle_verification_failure(model_to_download, model_path)

  def fetch_models(self, url):
    try:
      with urllib.request.urlopen(url, timeout=10) as response:
        return json.loads(response.read().decode('utf-8'))['models']
    except Exception as error:
      handle_request_error(error, None, None, None, None)
      return []

  def update_model_params(self, model_info, repo_url):
    available_models, available_model_names, experimental_models, navigation_models, radarless_models, velocity_models = [], [], [], [], [], []

    for model in model_info:
      available_models.append(model['id'])
      available_model_names.append(model['name'])

      if model.get("e2e_longitudinal", False):
        e2e_longitudinal_models.append(model['id'])
      if model.get("experimental", False):
        experimental_models.append(model['id'])
      if model.get("gas_brake", False):
        gas_brake_models.append(model['id'])
      if model.get("velocity", False):
        velocity_models.append(model['id'])
      if "🗺️" in model['name']:
        navigation_models.append(model['id'])
      if "📡" not in model['name']:
        radarless_models.append(model['id'])

    self.params.put_nonblocking("AvailableModels", ','.join(available_models))
    self.params.put_nonblocking("AvailableModelsNames", ','.join(available_model_names))
    self.params.put_nonblocking("ExperimentalModels", ','.join(experimental_models))
    self.params.put_nonblocking("NavigationModels", ','.join(navigation_models))
    self.params.put_nonblocking("RadarlessModels", ','.join(radarless_models))
    self.params.put_nonblocking("VelocityModels", ','.join(velocity_models))
    print("Models list updated successfully.")

    if available_models:
      models_downloaded = self.are_all_models_downloaded(available_models, available_model_names, repo_url)
      self.params.put_bool_nonblocking("ModelsDownloaded", models_downloaded)

  def are_all_models_downloaded(self, available_models, available_model_names, repo_url):
    automatically_update_models = self.params.get_bool("AutomaticallyUpdateModels")
    all_models_downloaded = True

    download_queue = []
    for model in available_models:
      model_path = os.path.join(MODELS_PATH, f"{model}.thneed")
      model_url = f"{repo_url}Models/{model}.thneed"

      if os.path.isfile(model_path):
        if automatically_update_models:
          verify_result = verify_download(model_path, model_url, False)
          if verify_result is None:
            all_models_downloaded = False
          elif not verify_result:
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

  def queue_model_download(self, model, model_name=None):
    while self.params_memory.get(self.download_param, encoding='utf-8'):
      time.sleep(1)

    self.params_memory.put(self.download_param, model)
    if model_name:
      self.params_memory.put(self.download_progress_param, f"Downloading {model_name}...")

  def validate_models(self):
    current_model = self.params.get("Model", encoding='utf-8')
    current_model_name = self.params.get("ModelName", encoding='utf-8')

    if "(Default)" in current_model_name and current_model_name != DEFAULT_MODEL_NAME:
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
        reason = "Model is not in the list of available models"
        if model_name == current_model:
          self.params.put_nonblocking("Model", DEFAULT_MODEL)
          self.params.put_nonblocking("ModelName", DEFAULT_MODEL_NAME)
        delete_file(os.path.join(MODELS_PATH, model_file))
        print(f"Deleted model file: {model_file} - Reason: {reason}")

  def copy_default_model(self):
    default_model_path = os.path.join(MODELS_PATH, f"{DEFAULT_MODEL}.thneed")
    source_path = os.path.join(BASEDIR, "selfdrive", "classic_modeld", "models", "supercombo.thneed")

    if os.path.isfile(source_path) and not os.path.isfile(default_model_path):
      shutil.copyfile(source_path, default_model_path)
      print(f"Copied default model from {source_path} to {default_model_path}")

    sgo_model_path = os.path.join(MODELS_PATH, "secret-good-openpilot.thneed")
    source_path = os.path.join(BASEDIR, "selfdrive", "modeld", "models", "supercombo.thneed")

    if os.path.isfile(source_path) and not os.path.isfile(sgo_model_path):
      shutil.copyfile(source_path, sgo_model_path)
      print(f"Copied 'secret-good-openpilot' model from {source_path} to {sgo_model_path}")

  def update_models(self, boot_run=False):
    if boot_run:
      self.copy_default_model()

    repo_url = get_repository_url()
    if repo_url is None:
      print("GitHub and GitLab are offline...")
      return

    if boot_run:
      self.validate_models()
    else:
      model_info = self.fetch_models(f"{repo_url}Versions/model_names_{VERSION}.json")
      if model_info:
        self.update_model_params(model_info, repo_url)

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
