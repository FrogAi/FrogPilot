import http.client
import os
import socket
import urllib.error
import urllib.request

from openpilot.common.params import Params
from openpilot.system.version import get_short_branch

VERSION = 'v1' if get_short_branch() == "FrogPilot" else 'v2'
GITHUB_REPOSITORY_URL = 'https://raw.githubusercontent.com/FrogAi/FrogPilot-Resources/'
GITLAB_REPOSITORY_URL = 'https://gitlab.com/FrogAi/FrogPilot-Resources/-/raw/'

DEFAULT_MODEL = "wd-40"
DEFAULT_MODEL_NAME = "WD40 (Default)"
MODELS_PATH = '/data/models'

NAVIGATION_MODELS = {"certified-herbalist", "duck-amigo", "los-angeles", "recertified-herbalist"}
RADARLESS_MODELS = {"radical-turtle"}

params = Params()
params_memory = Params("/dev/shm/params")

def ping_url(url, timeout=5):
  try:
    urllib.request.urlopen(url, timeout=timeout)
    return True
  except (urllib.error.URLError, socket.timeout, http.client.RemoteDisconnected):
    return False

def determine_url(model):
  if ping_url(GITHUB_REPOSITORY_URL):
    return f"{GITHUB_REPOSITORY_URL}/Models/{model}.thneed"
  else:
    return f"{GITLAB_REPOSITORY_URL}/Models/{model}.thneed"

def delete_deprecated_models():
  populate_models()

  available_models = params.get("AvailableModels", encoding='utf-8').split(',')

  current_model = params.get("Model", block=True, encoding='utf-8')
  current_model_file = os.path.join(MODELS_PATH, f"{current_model}.thneed")

  if current_model not in available_models or not os.path.exists(current_model_file):
    params.put("Model", DEFAULT_MODEL)
    params.put("ModelName", DEFAULT_MODEL_NAME)

  for model_file in os.listdir(MODELS_PATH):
    if model_file.endswith('.thneed') and model_file[:-7] not in available_models:
      os.remove(os.path.join(MODELS_PATH, model_file))

def download_model():
  model = params_memory.get("ModelToDownload", encoding='utf-8')
  model_path = os.path.join(MODELS_PATH, f"{model}.thneed")

  if os.path.exists(model_path):
    print(f"Model {model} already exists, skipping download.")
    return

  url = determine_url(model)

  for attempt in range(3):
    try:
      with urllib.request.urlopen(url) as f:
        total_file_size = int(f.getheader('Content-Length'))
        if total_file_size == 0:
          raise ValueError("File is empty")

        with open(model_path, 'wb') as output:
          current_file_size = 0
          for chunk in iter(lambda: f.read(8192), b''):
            output.write(chunk)
            current_file_size += len(chunk)
            progress = (current_file_size / total_file_size) * 100
            params_memory.put_int("ModelDownloadProgress", int(progress))
          os.fsync(output)

      verify_download(model_path, total_file_size)
      return
    except Exception as e:
      handle_download_error(model_path, attempt, e, url)

def verify_download(model_path, total_file_size):
  if os.path.getsize(model_path) == total_file_size:
    print(f"Successfully downloaded the model!")
  else:
    raise Exception("Downloaded file size does not match expected size.")

def handle_download_error(model_path, attempt, exception, url):
  print(f"Attempt {attempt + 1} failed with error: {exception}. Retrying...")
  if os.path.exists(model_path):
    os.remove(model_path)
  if attempt == 2:
    print(f"Failed to download the model after 3 attempts from {url}")

def populate_models():
  url = f"{GITHUB_REPOSITORY_URL}Versions/model_names_{VERSION}.txt" if ping_url(GITHUB_REPOSITORY_URL) else f"{GITLAB_REPOSITORY_URL}Versions/model_names_{VERSION}.txt"
  try:
    with urllib.request.urlopen(url) as response:
      model_info = [line.decode('utf-8').strip().split(' - ') for line in response.readlines()]
    update_params(model_info)
  except Exception as e:
    print(f"Failed to update models list. Error: {e}")

def update_params(model_info):
  available_models = ','.join(model[0] for model in model_info)
  params.put("AvailableModels", available_models)
  params.put("AvailableModelsNames", ','.join(model[1] for model in model_info))
  print("Models list updated successfully.")
