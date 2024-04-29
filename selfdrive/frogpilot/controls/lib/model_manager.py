import os
import stat
import time
import urllib.request

from openpilot.common.params import Params
from openpilot.system.version import get_short_branch

VERSION = 'v1' if get_short_branch() == "FrogPilot" else 'v2'
REPOSITORY_URL = 'https://github.com/FrogAi/FrogPilot-Resources/releases/download'

DEFAULT_MODEL = "wd-40"
DEFAULT_MODEL_NAME = "WD40 (Default)"
MODELS_PATH = '/data/models'

NAVIGATION_MODELS = {"certified-herbalist", "duck-amigo", "los-angeles", "recertified-herbalist"}
RADARLESS_MODELS = {"radical-turtle"}

params = Params()
params_memory = Params("/dev/shm/params")

def delete_deprecated_models():
  populate_models()

  available_models = params.get("AvailableModels", encoding='utf-8').split(',')

  if available_models:
    current_model = params.get("Model", block=True, encoding='utf-8')
    current_model_file = os.path.join(MODELS_PATH, f"{current_model}.thneed")

    if current_model not in available_models or not os.path.exists(current_model_file):
      params.put("Model", DEFAULT_MODEL)
      params.put("ModelName", DEFAULT_MODEL_NAME)

    for model_file in os.listdir(MODELS_PATH):
      if model_file.endswith('.thneed') and model_file[:-7] not in available_models:
        os.remove(os.path.join(MODELS_PATH, model_file))
  else:
    params.put("Model", DEFAULT_MODEL)
    params.put("ModelName", DEFAULT_MODEL_NAME)

def download_model():
  model = params_memory.get("ModelToDownload", encoding='utf-8')
  model_path = os.path.join(MODELS_PATH, f"{model}.thneed")
  url = f"{REPOSITORY_URL}/{model}/{model}.thneed"

  os.makedirs(MODELS_PATH, exist_ok=True)

  for attempt in range(5):
    try:
      with urllib.request.urlopen(url) as f:
        total_file_size = int(f.getheader('Content-Length'))
        if total_file_size == 0:
          raise ValueError("File is empty")

        with open(model_path, 'wb') as output:
          current_file_size = 0
          while chunk := f.read(8192):
            output.write(chunk)
            current_file_size += len(chunk)
            progress = (current_file_size / total_file_size) * 100
            params_memory.put_int("ModelDownloadProgress", int(progress))
          os.fsync(output)

      if os.path.getsize(model_path) == total_file_size:
        print(f"Successfully downloaded the {model} model!")
        break
      else:
        raise Exception("Downloaded model file size does not match expected size. Retrying...")

    except Exception as e:
      print(f"Attempt {attempt + 1} failed with error: {e}. Retrying...")
      if os.path.exists(model_path):
        os.remove(model_path)
      time.sleep(5)
  else:
    print(f"Failed to download the {model} model after {attempt + 1} attempts. Giving up... :(")

def populate_models():
  model_names_url = f"https://raw.githubusercontent.com/FrogAi/FrogPilot-Resources/master/model_names_{VERSION}.txt"

  for attempt in range(5):
    try:
      with urllib.request.urlopen(model_names_url) as response:
        model_info = [line.decode('utf-8').strip().split(' - ') for line in response.readlines() if ' - ' in line.decode('utf-8')]

      available_models = ','.join(model[0] for model in model_info)
      available_models_names = [model[1] for model in model_info]

      params.put("AvailableModels", available_models)
      params.put("AvailableModelsNames", ','.join(available_models_names))

      current_model_name = params.get("ModelName", encoding='utf-8')
      if current_model_name not in available_models_names and "(Default)" in current_model_name:
        updated_model_name = current_model_name.replace("(Default)", "").strip()
        params.put("ModelName", updated_model_name)

    except Exception as e:
      print(f"Failed to update models list. Error: {e}. Retrying...")
      time.sleep(5)
  else:
    print(f"Failed to update models list after 5 attempts. Giving up... :(")
