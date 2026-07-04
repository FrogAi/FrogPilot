#!/usr/bin/env python3
import os
import requests
import sys

from pathlib import Path

from openpilot.common.basedir import BASEDIR

from openpilot.frogpilot.assets.download_functions import download_file, verify_download
from openpilot.frogpilot.assets.model_manager import CANCEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM, MODEL_DOWNLOAD_PARAM
from openpilot.frogpilot.common.frogpilot_utilities import delete_file, run_cmd
from openpilot.frogpilot.common.frogpilot_variables import MODELS_PATH

METADATA_SCRIPT = Path(BASEDIR) / "frogpilot/tinygrad_modeld/get_model_metadata.py"
TINYGRAD_REPO_PATH = Path(BASEDIR) / "tinygrad_repo"

MODELS_SOURCE = "https://api.github.com/repos/FrogAi/FrogPilot-Resources/contents/uncompiled?ref=Models"
MODELS_SOURCE_RAW = "https://raw.githubusercontent.com/FrogAi/FrogPilot-Resources/Models/uncompiled"

COMPILED_DIR = Path(MODELS_PATH) / "compiled"
UNCOMPILED_DIR = Path(MODELS_PATH) / "uncompiled_downloads"

def compile_model(onnx_path):
  onnx_path = Path(onnx_path).resolve()
  compiled_path = COMPILED_DIR / f"{onnx_path.stem}_tinygrad.pkl"

  env = os.environ.copy()
  env["PYTHONPATH"] = f"{env.get('PYTHONPATH','')}:{TINYGRAD_REPO_PATH}"

  run_cmd([sys.executable, str(TINYGRAD_REPO_PATH / "examples/openpilot/compile3.py"), str(onnx_path), str(compiled_path)], f"{onnx_path.name} compiled successfully!", "Failed to compile the model...", env=env)
  run_cmd([sys.executable, str(METADATA_SCRIPT), str(onnx_path)], f"Successfully extracted metadata from {onnx_path.name}!", f"Failed to extract metadata from {onnx_path.name}...")

  delete_file(onnx_path)

def download_models():
  session = requests.Session()
  session.headers.update({"Accept-Language": "en"})
  session.headers.update({"User-Agent": "frogpilot-model-compiler/1.0 (https://github.com/FrogAi/FrogPilot)"})

  COMPILED_DIR.mkdir(parents=True, exist_ok=True)
  UNCOMPILED_DIR.mkdir(parents=True, exist_ok=True)

  local_paths = []
  for name in list_remote_onnx_files(session):
    url = f"{MODELS_SOURCE_RAW}/{name}"
    destination = UNCOMPILED_DIR / name

    download_file(CANCEL_DOWNLOAD_PARAM, destination, DOWNLOAD_PROGRESS_PARAM, url, MODEL_DOWNLOAD_PARAM, session)

    if not verify_download(destination, url, session):
      continue

    print(f"{name} downloaded!")

    local_paths.append(destination)
  return local_paths

def list_remote_onnx_files(session):
  response = session.get(MODELS_SOURCE, timeout=10)
  response.raise_for_status()

  entries = response.json()
  return sorted(entry["name"] for entry in entries if entry["name"].lower().endswith(".onnx"))

def main():
  downloaded_models = download_models()
  for path in downloaded_models:
    compile_model(path)

  if UNCOMPILED_DIR.exists():
    delete_file(UNCOMPILED_DIR)

if __name__ == "__main__":
  main()
