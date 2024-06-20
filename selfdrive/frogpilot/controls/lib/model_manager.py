import http.client
import os
import socket
import time
import urllib.error
import urllib.request

from openpilot.common.params import Params
from openpilot.system.version import get_build_metadata

VERSION = 'v3' if get_build_metadata().channel == "FrogPilot" else 'v4'

GITHUB_REPOSITORY_URL = 'https://raw.githubusercontent.com/FrogAi/FrogPilot-Resources/'
GITLAB_REPOSITORY_URL = 'https://gitlab.com/FrogAi/FrogPilot-Resources/-/raw/'

DEFAULT_MODEL = "north-dakota-v2"
DEFAULT_MODEL_NAME = "North Dakota V2 (Default)"
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

def determine_url(model, file_type):
  if ping_url(GITHUB_REPOSITORY_URL):
    return f"{GITHUB_REPOSITORY_URL}/Models/{model}{file_type}"
  else:
    return f"{GITLAB_REPOSITORY_URL}/Models/{model}{file_type}"

def delete_deprecated_models():
  populate_models()

  available_models = params.get("AvailableModels", encoding='utf-8').split(',')

  current_model = params.get("Model", block=True, encoding='utf-8')
  current_model_file = os.path.join(MODELS_PATH, f"{current_model}.thneed")

  if current_model not in available_models or not os.path.exists(current_model_file):
    params.put("Model", DEFAULT_MODEL)
    params.put("ModelName", DEFAULT_MODEL_NAME)

  for model_file in os.listdir(MODELS_PATH):
    if (model_file.endswith('.thneed') or model_file.endswith('_metadata.pkl')) and model_file[:-7] not in available_models:
      os.remove(os.path.join(MODELS_PATH, model_file))

def download_model():
  model = params_memory.get("ModelToDownload", encoding='utf-8')
  model_path = os.path.join(MODELS_PATH, f"{model}.thneed")
  metadata_path = os.path.join(MODELS_PATH, f"{model}_metadata.pkl")

  if os.path.exists(model_path) and os.path.exists(metadata_path):
    print(f"Model {model} already exists, skipping download.")
    return

  url_thneed = determine_url(model, '.thneed')
  url_metadata = determine_url(model, '_metadata.pkl')

  for attempt in range(3):
    try:
      total_size = get_total_size(url_thneed, url_metadata)
      download_file(url_thneed, model_path, 0, total_size)
      download_file(url_metadata, metadata_path, os.path.getsize(model_path), total_size)
      verify_download(model, model_path, metadata_path)
      return
    except Exception as e:
      handle_download_error(model_path, metadata_path, attempt, e, url_thneed)
      time.sleep(2**attempt)

def get_total_size(url_thneed, url_metadata):
  try:
    thneed_size = int(urllib.request.urlopen(url_thneed).getheader('Content-Length'))
    metadata_size = int(urllib.request.urlopen(url_metadata).getheader('Content-Length'))
    return thneed_size + metadata_size
  except Exception as e:
    print(f"Failed to get total size. Error: {e}")
    raise

def download_file(url, path, progress_start, total_size):
  try:
    with urllib.request.urlopen(url) as f:
      total_file_size = int(f.getheader('Content-Length'))
      if total_file_size == 0:
        raise ValueError("File is empty")

      with open(path, 'wb') as output:
        for chunk in iter(lambda: f.read(8192), b''):
          output.write(chunk)
          progress = progress_start + output.tell()
          params_memory.put_int("ModelDownloadProgress", int((progress / total_size) * 100))
        os.fsync(output)

  except urllib.error.HTTPError as e:
    print(f"HTTP Error: {e.code} - {e.reason}")
    raise
  except urllib.error.URLError as e:
    print(f"URL Error: {e.reason}")
    raise
  except socket.timeout:
    print("Socket timeout occurred")
    raise
  except Exception as e:
    print(f"Unexpected error: {e}")
    raise

def verify_download(model, model_path, metadata_path):
  total_size = os.path.getsize(model_path) + os.path.getsize(metadata_path)
  if total_size == (os.path.getsize(model_path) + os.path.getsize(metadata_path)):
    print(f"Successfully downloaded the {model} model and metadata!")
  else:
    raise Exception("Downloaded file sizes do not match expected sizes.")

def handle_download_error(model_path, metadata_path, attempt, exception, url):
  print(f"Attempt {attempt + 1} failed with error: {exception}. Retrying...")
  if os.path.exists(model_path):
    os.remove(model_path)
  if os.path.exists(metadata_path):
    os.remove(metadata_path)
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

def check_metadata():
  for model_file in os.listdir(MODELS_PATH):
    if model_file.endswith('.thneed'):
      model_name = model_file[:-7]
      metadata_file = f"{model_name}_metadata.pkl"
      metadata_path = os.path.join(MODELS_PATH, metadata_file)

      if not os.path.exists(metadata_path):
        print(f"Metadata for {model_name} is missing, downloading...")
        url_metadata = determine_url(model_name, '_metadata.pkl')

        for attempt in range(3):
          try:
            total_size = int(urllib.request.urlopen(url_metadata).getheader('Content-Length'))
            download_file(url_metadata, metadata_path, 0, total_size)
            print(f"Successfully downloaded metadata for {model_name}.")
            break
          except Exception as e:
            handle_download_error('', metadata_path, attempt, e, url_metadata)
            time.sleep(2**attempt)
