import json
import os
import re
import requests
import shutil
import subprocess
import time
import urllib.request

from openpilot.common.basedir import BASEDIR

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import MODELS_PATH, delete_file, is_url_pingable

VERSION = "v4"

GITHUB_REPOSITORY_URL = "https://raw.githubusercontent.com/FrogAi/FrogPilot-Resources/"
GITLAB_REPOSITORY_URL = "https://gitlab.com/FrogAi/FrogPilot-Resources/-/raw/"

DEFAULT_MODEL = "north-dakota-v2"
DEFAULT_MODEL_NAME = "North Dakota V2 (Default)"

def get_repository_url():
  if is_url_pingable("https://github.com"):
    return GITHUB_REPOSITORY_URL
  if is_url_pingable("https://gitlab.com"):
    return GITLAB_REPOSITORY_URL
  return None

def get_remote_file_size(url):
  try:
    response = requests.head(url, timeout=5)
    response.raise_for_status()
    return int(response.headers.get('Content-Length', 0))
  except requests.RequestException as e:
    print(f"Error fetching file size: {e}")
    return None

def process_model_name(model_name):
  model_cleaned = re.sub(r'[🗺️👀📡]', '', model_name).strip()
  score_param = re.sub(r'[^a-zA-Z0-9()-]', '', model_cleaned).replace(' ', '').strip().replace('(Default)', '').replace('-', '')
  cleaned_name = ''.join(score_param.split())
  print(f'Processed Model Name: {cleaned_name}')
  return cleaned_name

def handle_error(destination, error_message, error, params_memory):
  print(f"Error occurred: {error}")
  params_memory.put("ModelDownloadProgress", error_message)
  params_memory.remove("DownloadAllModels")
  params_memory.remove("ModelToDownload")
  delete_file(destination)

def verify_download(file_path, model_url):
  if not os.path.exists(file_path):
    return False

  remote_file_size = get_remote_file_size(model_url)
  if remote_file_size is None:
    return False

  return remote_file_size == os.path.getsize(file_path)

def download_file(destination, url, params_memory):
  try:
    with requests.get(url, stream=True, timeout=5) as r:
      r.raise_for_status()
      total_size = get_remote_file_size(url)
      downloaded_size = 0

      with open(destination, 'wb') as f:
        for chunk in r.iter_content(chunk_size=8192):
          if params_memory.get_bool("CancelModelDownload"):
            handle_error(destination, "Download cancelled...", "Download cancelled...", params_memory)
            return
          if chunk:
            f.write(chunk)
            downloaded_size += len(chunk)
            progress = (downloaded_size / total_size) * 100
            if progress != 100:
              params_memory.put("ModelDownloadProgress", f"{progress:.0f}%")
            else:
              params_memory.put("ModelDownloadProgress", "Verifying authenticity...")

  except requests.HTTPError as http_error:
    handle_error(destination, f"Failed: Server error ({http_error.response.status_code})", http_error, params_memory)
  except requests.ConnectionError as connection_error:
    handle_error(destination, "Failed: Connection dropped...", connection_error, params_memory)
  except requests.Timeout as timeout_error:
    handle_error(destination, "Failed: Download timed out...", timeout_error, params_memory)
  except requests.RequestException as request_error:
    handle_error(destination, "Failed: Network request error. Check connection.", request_error, params_memory)
  except Exception as e:
    handle_error(destination, "Failed: Unexpected error.", e, params_memory)

def handle_existing_model(model, params_memory):
  print(f"Model {model} already exists, skipping download...")
  params_memory.put("ModelDownloadProgress", "Model already exists...")
  params_memory.remove("ModelToDownload")

def handle_verification_failure(model, model_path, model_url, params_memory):
  if params_memory.get_bool("CancelModelDownload"):
    handle_error(model_path, "Download cancelled...", "Download cancelled...", params_memory)
    return

  handle_error(model_path, "Issue connecting to Github, trying Gitlab", f"Model {model} verification failed. Redownloading from Gitlab...", params_memory)
  second_model_url = f"{GITLAB_REPOSITORY_URL}Models/{model}.thneed"
  download_file(model_path, second_model_url, params_memory)

  if verify_download(model_path, second_model_url):
    print(f"Model {model} redownloaded and verified successfully from Gitlab.")
  else:
    print(f"Model {model} redownload verification failed from Gitlab.")

def download_model(model_to_download, params_memory):
  model_path = os.path.join(MODELS_PATH, f"{model_to_download}.thneed")
  if os.path.exists(model_path):
    handle_existing_model(model_to_download, params_memory)
    return

  repo_url = get_repository_url()
  if repo_url is None:
    handle_error(model_path, "Github and Gitlab are offline...", "Github and Gitlab are offline...", params_memory)
    return

  model_url = f"{repo_url}Models/{model_to_download}.thneed"
  download_file(model_path, model_url, params_memory)

  if verify_download(model_path, model_url):
    print(f"Model {model_to_download} downloaded and verified successfully!")
    params_memory.put("ModelDownloadProgress", "Downloaded!")
    params_memory.remove("ModelToDownload")
  else:
    handle_verification_failure(model_to_download, model_path, model_url, params_memory)

def fetch_models(url):
  try:
    with urllib.request.urlopen(url) as response:
      return json.loads(response.read().decode('utf-8'))['models']
  except Exception as e:
    print(f"Failed to update models list. Error: {e}")
    return None

def are_all_models_downloaded(available_models, available_model_names, repo_url, params, params_memory):
  automatically_update_models = params.get_bool("AutomaticallyUpdateModels")
  all_models_downloaded = True

  for model in available_models:
    model_path = os.path.join(MODELS_PATH, f"{model}.thneed")
    model_url = f"{repo_url}Models/{model}.thneed"

    if os.path.exists(model_path):
      if automatically_update_models:
        remote_file_size = get_remote_file_size(model_url)
        try:
          local_file_size = os.path.getsize(model_path)
        except FileNotFoundError:
          print(f"File not found: {model_path}. It may have been moved or deleted.")
          local_file_size = 0

        if remote_file_size is not None and remote_file_size != local_file_size:
          print(f"Model {model} is outdated. Local size: {local_file_size}, Remote size: {remote_file_size}. Re-downloading...")
          delete_file(model_path)
          part_model_param = process_model_name(available_model_names[available_models.index(model)])
          params.remove(part_model_param + "CalibrationParams")
          params.remove(part_model_param + "LiveTorqueParameters")
          while params_memory.get("ModelToDownload", encoding='utf-8') is not None:
            time.sleep(1)
          params_memory.put("ModelToDownload", model)
          all_models_downloaded = False
    else:
      if automatically_update_models:
        while params_memory.get("ModelToDownload", encoding='utf-8') is not None:
          time.sleep(1)
        print(f"Model {model} is missing. Re-downloading...")
        params_memory.put("ModelToDownload", model)
        part_model_param = process_model_name(available_model_names[available_models.index(model)])
        params.remove(part_model_param + "CalibrationParams")
        params.remove(part_model_param + "LiveTorqueParameters")
      all_models_downloaded = False

  return all_models_downloaded

def update_model_params(model_info, repo_url, params, params_memory):
  available_models = []
  available_model_names = []
  experimental_models = []
  navigation_models = []
  radarless_models = []

  for model in model_info:
    available_models.append(model['id'])
    available_model_names.append(model['name'])
    if model.get("experimental", False):
      experimental_models.append(model['id'])
    if "🗺️" in model['name']:
      navigation_models.append(model['id'])
    if "📡" not in model['name']:
      radarless_models.append(model['id'])

  params.put_nonblocking("AvailableModels", ','.join(available_models))
  params.put_nonblocking("AvailableModelsNames", ','.join(available_model_names))
  params.put_nonblocking("ExperimentalModels", ','.join(experimental_models))
  params.put_nonblocking("NavigationModels", ','.join(navigation_models))
  params.put_nonblocking("RadarlessModels", ','.join(radarless_models))
  print("Models list updated successfully.")

  if available_models is not None:
    params.put_bool_nonblocking("ModelsDownloaded", are_all_models_downloaded(available_models, available_model_names, repo_url, params, params_memory))

def validate_models(params):
  current_model = params.get("Model", encoding='utf-8')
  current_model_name = params.get("ModelName", encoding='utf-8')
  if "(Default)" in current_model_name and current_model_name != DEFAULT_MODEL_NAME:
    params.put_nonblocking("ModelName", current_model_name.replace(" (Default)", ""))

  available_models = params.get("AvailableModels", encoding='utf-8')
  if available_models is None:
    return

  for model_file in os.listdir(MODELS_PATH):
    if model_file.endswith('.thneed') and model_file[:-7] not in available_models.split(','):
      if model_file == current_model:
        params.put_nonblocking("Model", DEFAULT_MODEL)
        params.put_nonblocking("ModelName", DEFAULT_MODEL_NAME)
      delete_file(os.path.join(MODELS_PATH, model_file))
      print(f"Deleted model file: {model_file}")

def copy_default_model():
  default_model_path = os.path.join(MODELS_PATH, f"{DEFAULT_MODEL}.thneed")
  if not os.path.exists(default_model_path):
    source_path = os.path.join(BASEDIR, "selfdrive/modeld/models/supercombo.thneed")
    if os.path.exists(source_path):
      shutil.copyfile(source_path, default_model_path)
      print(f"Copied default model from {source_path} to {default_model_path}")
    else:
      print(f"Source default model not found at {source_path}. Exiting...")

def update_models(params, params_memory, boot_run=True):
  try:
    if boot_run:
      copy_default_model()
      validate_models(params)

    repo_url = get_repository_url()
    if repo_url is None:
      return

    model_info = fetch_models(f"{repo_url}Versions/model_names_{VERSION}.json")
    if model_info is None:
      return

    update_model_params(model_info, repo_url, params, params_memory)
  except subprocess.CalledProcessError as e:
    print(f"Failed to update models. Error: {e}")

def download_all_models(params, params_memory):
  copy_default_model()

  repo_url = get_repository_url()
  if repo_url is None:
    handle_error(None, "Github and Gitlab are offline...", "Github and Gitlab are offline...", params_memory)
    return

  model_info = fetch_models(f"{repo_url}Versions/model_names_{VERSION}.json")
  if model_info is None:
    handle_error(None, "Unable to update model list...", "Unable to update model list...", params_memory)
    return

  update_model_params(model_info, repo_url, params, params_memory)

  available_models = params.get("AvailableModels", encoding='utf-8').split(',')
  available_model_names = params.get("AvailableModelsNames", encoding='utf-8').split(',')

  for model in available_models:
    if params_memory.get_bool("CancelModelDownload"):
      handle_error(None, "Download cancelled...", "Download cancelled...", params_memory)
      return
    model_path = os.path.join(MODELS_PATH, f"{model}.thneed")
    if not os.path.exists(model_path):
      model_index = available_models.index(model)
      model_name = available_model_names[model_index]
      cleaned_model_name = re.sub(r'[🗺️👀📡]', '', model_name).strip()
      print(f"Downloading model: {cleaned_model_name}")
      params_memory.put("ModelToDownload", model)
      params_memory.put("ModelDownloadProgress", f"Downloading {cleaned_model_name}...")
      while params_memory.get("ModelToDownload", encoding='utf-8') is not None:
        time.sleep(1)

  all_downloaded = False
  while not all_downloaded:
    if params_memory.get_bool("CancelModelDownload"):
      handle_error(None, "Download cancelled...", "Download cancelled...", params_memory)
      return
    all_downloaded = all([os.path.exists(os.path.join(MODELS_PATH, f"{model}.thneed")) for model in available_models])
    time.sleep(1)

  params_memory.put("ModelDownloadProgress", "All models downloaded!")
  params_memory.remove("DownloadAllModels")
  params.put_bool_nonblocking("ModelsDownloaded", True)
