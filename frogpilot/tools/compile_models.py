#!/usr/bin/env python3
import os
import requests
import shutil
import subprocess
import sys

from pathlib import Path

from openpilot.frogpilot.common import frogpilot_download_utilities, frogpilot_paths, frogpilot_utilities, frogpilot_variables

MODELS_SOURCE = f"https://api.github.com/repos/{frogpilot_variables.RESOURCES_REPO}/contents/uncompiled?ref=Models"
MODELS_SOURCE_RAW = f"{frogpilot_download_utilities.GITHUB_URL}/Models/uncompiled"

def compile_model(onnx_path):
  onnx_path = Path(onnx_path).resolve()
  compiled_path = frogpilot_paths.COMPILED_DIR / f"{onnx_path.stem}_tinygrad.pkl"

  env = os.environ.copy()
  if env.get("PYTHONPATH"):
    env["PYTHONPATH"] += f":{frogpilot_paths.TINYGRAD_REPO_PATH}"
  else:
    env["PYTHONPATH"] = str(frogpilot_paths.TINYGRAD_REPO_PATH)

  compile_script = frogpilot_paths.TINYGRAD_REPO_PATH / "examples/openpilot/compile3.py"
  subprocess.run([sys.executable, str(compile_script), str(onnx_path), str(compiled_path)], check=True, env=env)
  print(f"{onnx_path.name} compiled successfully!")

  subprocess.run([sys.executable, str(frogpilot_paths.METADATA_SCRIPT), str(onnx_path)], check=True)
  print(f"Successfully extracted metadata from {onnx_path.name}!")

  metadata_path = onnx_path.with_name(f"{onnx_path.stem}_metadata.pkl")
  shutil.move(metadata_path, frogpilot_paths.COMPILED_DIR / metadata_path.name)

  frogpilot_utilities.delete_path(onnx_path)

def download_models():
  download_state = frogpilot_download_utilities.DownloadState()

  session = requests.Session()
  session.headers.update({"Accept-Language": "en"})
  session.headers.update({"User-Agent": "frogpilot-model-compiler/1.0 (https://github.com/FrogAi/FrogPilot)"})

  frogpilot_paths.COMPILED_DIR.mkdir(parents=True, exist_ok=True)
  frogpilot_paths.UNCOMPILED_DIR.mkdir(parents=True, exist_ok=True)

  local_paths = []
  for name in list_remote_onnx_files(session):
    url = f"{MODELS_SOURCE_RAW}/{name}"
    destination = frogpilot_paths.UNCOMPILED_DIR / name

    if download_state.download_file(destination, session, url) is None:
      raise RuntimeError(f"Failed to download {name}: {download_state.progress}")

    if not frogpilot_download_utilities.verify_download(destination, url, session):
      raise RuntimeError(f"Failed to verify {name}")

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

  if frogpilot_paths.UNCOMPILED_DIR.exists():
    frogpilot_utilities.delete_path(frogpilot_paths.UNCOMPILED_DIR)

if __name__ == "__main__":
  main()
