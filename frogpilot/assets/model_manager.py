#!/usr/bin/env python3
import json
import re
import requests
import shutil
import time
import urllib.parse

from pathlib import Path
from urllib.parse import quote_plus

from openpilot.common.basedir import BASEDIR
from openpilot.frogpilot.assets.download_functions import GITLAB_URL, download_file, get_remote_file_size, get_repository_url, handle_error, handle_request_error, verify_download
from openpilot.frogpilot.common.frogpilot_utilities import delete_file, extract_tar, load_json_file, update_json_file
from openpilot.frogpilot.common.frogpilot_variables import DEFAULT_MODEL, MODELS_PATH, RESOURCES_REPO, TINYGRAD_FILES, params, params_default, params_memory, update_frogpilot_toggles

VERSION = "v16"
VERSION_PATH = MODELS_PATH / "model_version"

CANCEL_DOWNLOAD_PARAM = "CancelModelDownload"
DOWNLOAD_PROGRESS_PARAM = "ModelDownloadProgress"
MODEL_DOWNLOAD_PARAM = "ModelToDownload"
MODEL_DOWNLOAD_ALL_PARAM = "DownloadAllModels"
UPDATE_TINYGRAD_PARAM = "UpdateTinygrad"

DEFAULT_TINYGRAD_SIZE = 87746736
TAR_FILE_NAME = f"Tinygrad_{VERSION}.tar.gz"

TINYGRAD_MODELD_PATH = Path(BASEDIR) / "frogpilot/tinygrad_modeld"
TINYGRAD_REPO_PATH = Path(BASEDIR) / "tinygrad_repo"

class ModelManager:
  def __init__(self, boot_run=False):
    self.downloading_model = False

    self.available_models = (params.get("AvailableModels", encoding="utf-8") or "").split(",")
    self.available_model_names = (params.get("AvailableModelNames", encoding="utf-8") or "").split(",")
    self.model_versions = (params.get("ModelVersions", encoding="utf-8") or "").split(",")

    self.model_sizes_path = MODELS_PATH / "model_sizes.json"
    self.tinygrad_sizes_path = MODELS_PATH / "tinygrad_sizes.json"

    self.model_sizes = load_json_file(self.model_sizes_path)
    self.tinygrad_sizes = load_json_file(self.tinygrad_sizes_path)

    self.session = requests.Session()
    self.session.headers.update({"Accept-Language": "en"})
    self.session.headers.update({"User-Agent": "frogpilot-model-downloader/1.0 (https://github.com/FrogAi/FrogPilot)"})

    if boot_run:
      self.copy_default_model()
      self.validate_models()

  def check_models(self, boot_run, repo_url):
    downloaded_models = [
      model for model in MODELS_PATH.iterdir()
      if (MODELS_PATH / f"{model}.thneed").is_file() or all((MODELS_PATH / f"{model}_{filename}").is_file() for filename, _ in TINYGRAD_FILES)
    ]
    for model_file in downloaded_models:
      if not any(model in model_file.name for model in set(self.available_models)):
        print(f"Removing outdated model: {model_file}")
        delete_file(model_file)

    for tmp_file in MODELS_PATH.glob("tmp*"):
      if tmp_file.is_file():
        delete_file(tmp_file)

    if params.get("Model", encoding="utf-8").removesuffix("_default") not in self.available_models:
      params.put("Model", params_default.get("Model", encoding="utf-8"))

    if not (not boot_run and params.get_bool("AutomaticallyDownloadModels")):
      return

    model_sizes = self.fetch_all_model_sizes(repo_url)
    if not model_sizes:
      print("No model size data available. Skipping model checks...")
      return

    need_to_update_models = False
    for model in self.available_models:
      if self.is_tinygrad_model(model):
        model_file = MODELS_PATH / f"{model}.thneed"
        if not model_file.is_file():
          need_to_update_models = True
          continue

        expected_size = model_sizes.get(model_file.name)
        local_size = self.model_sizes.get(model_file.name)

        if expected_size > 0 and local_size != expected_size:
          print(f"Model {model} is outdated. Deleting {model_file}...")
          delete_file(model_file)
          need_to_update_models = True
      else:
        model_missing = False
        model_outdated = False

        for filename, _ in TINYGRAD_FILES:
          expected_file = MODELS_PATH / f"{model}_{filename}"
          if not expected_file.is_file():
            model_missing = True
            need_to_update_models = True
            break

        for filename, _ in TINYGRAD_FILES:
          model_file = f"{model}_{filename}"

          expected_size = model_sizes.get(model_file)
          local_size = self.model_sizes.get(model_file)

          if expected_size > 0 and local_size != expected_size:
            model_outdated = True
            need_to_update_models = True
            break

        if model_missing or model_outdated:
          print(f"Model {model} is either missing required files or outdated. Deleting...")
          for filename, _ in TINYGRAD_FILES:
            delete_file(MODELS_PATH / f"{model}_{filename}")

    if need_to_update_models:
      params_memory.put_bool(MODEL_DOWNLOAD_ALL_PARAM, True)

  def check_tinygrad(self, repo_url):
    tinygrad_url = f"{repo_url}/Tinygrad/{TAR_FILE_NAME}"

    expected_size = get_remote_file_size(tinygrad_url, self.session)
    local_size = int(self.tinygrad_sizes.get(TAR_FILE_NAME, 0))

    if expected_size > 0 and local_size != expected_size:
      print(f"Tinygrad version {VERSION} is outdated, expected_size: {expected_size}, local_size: {local_size}, flagging for update...")
      params.put_bool("TinygradUpdateAvailable", True)

  def copy_default_model(self):
    classic_default_model_path = MODELS_PATH / "wd-40.thneed"
    source_path = Path(__file__).parents[1] / "classic_modeld/models/supercombo.thneed"
    if source_path.is_file() and (not classic_default_model_path.is_file() or source_path.stat().st_size != classic_default_model_path.stat().st_size):
      shutil.copyfile(source_path, classic_default_model_path)
      print(f"Copied the classic default model from {source_path} to {classic_default_model_path}")
      self.update_model_size(classic_default_model_path)

    default_model_path = MODELS_PATH / "national-public-radio.thneed"
    source_path = Path(__file__).parents[2] / "selfdrive/modeld/models/supercombo.thneed"
    if source_path.is_file() and (not default_model_path.is_file() or source_path.stat().st_size != default_model_path.stat().st_size):
      shutil.copyfile(source_path, default_model_path)
      print(f"Copied the default model from {source_path} to {default_model_path}")
      self.update_model_size(default_model_path)

    for filename, description in TINYGRAD_FILES:
      source = TINYGRAD_MODELD_PATH / "models" / filename
      target = MODELS_PATH / f"{DEFAULT_MODEL}_{filename}"
      if source.is_file() and (not target.is_file() or source.stat().st_size != target.stat().st_size):
        shutil.copyfile(source, target)
        print(f"Copied the tinygrad {description} from {source} to {target}")

  def download_all_models(self):
    repo_url = get_repository_url(self.session)
    if not repo_url:
      handle_error(None, "GitHub and GitLab are offline...", "Repository unavailable", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM)
      return

    self.fetch_models(f"{repo_url}/Versions/model_names_{VERSION}.json", repo_url)

    for model in self.available_models:
      if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
        handle_error(None, "Download cancelled...", "Download cancelled...", MODEL_DOWNLOAD_ALL_PARAM, DOWNLOAD_PROGRESS_PARAM)
        return

      if self.is_tinygrad_model(model):
        already_downloaded = (MODELS_PATH / f"{model}.thneed").is_file()
      else:
        already_downloaded = all((MODELS_PATH / f"{model}_{filename}").is_file() for filename, _ in TINYGRAD_FILES)

      if already_downloaded:
        continue

      print(f"Model {model} is not downloaded. Preparing to download...")
      params_memory.put(DOWNLOAD_PROGRESS_PARAM, f"Downloading \"{self.available_model_names[self.available_models.index(model)]}\"...")
      self.download_model(model)

    params_memory.put(DOWNLOAD_PROGRESS_PARAM, "All models downloaded!")
    params_memory.remove(MODEL_DOWNLOAD_ALL_PARAM)

  def download_model(self, model_to_download):
    self.downloading_model = True

    repo_url = get_repository_url(self.session)
    if not repo_url:
      handle_error(None, "GitHub and GitLab are offline...", "Repository unavailable", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM)
      self.downloading_model = False
      return

    if self.is_tinygrad_model(model_to_download):
      model_path = MODELS_PATH / f"{model_to_download}.thneed"
      model_url = f"{repo_url}/Models/{model_to_download}.thneed"

      print(f"Downloading model: {model_to_download}")
      download_file(CANCEL_DOWNLOAD_PARAM, model_path, DOWNLOAD_PROGRESS_PARAM, model_url, MODEL_DOWNLOAD_PARAM, self.session)

      if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
        delete_file(model_path)

        handle_error(None, "Download cancelled...", "Download cancelled...", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM)
        self.downloading_model = False
        return

      if verify_download(model_path, model_url, self.session):
        print(f"Model {model_to_download} downloaded and verified successfully!")
        self.update_model_size(model_path)

        params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
        params_memory.remove(MODEL_DOWNLOAD_PARAM)

        self.downloading_model = False
        return

      print(f"Verification failed for model {model_to_download}. Retrying from GitLab...")
      fallback_url = f"{GITLAB_URL}/Models/{model_to_download}.thneed"
      download_file(CANCEL_DOWNLOAD_PARAM, model_path, DOWNLOAD_PROGRESS_PARAM, fallback_url, MODEL_DOWNLOAD_PARAM, self.session)

      if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
        delete_file(model_path)

        handle_error(None, "Download cancelled...", "Download cancelled...", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM)
        self.downloading_model = False
        return

      if verify_download(model_path, fallback_url, self.session):
        print(f"Model {model_to_download} downloaded and verified successfully from GitLab!")
        self.update_model_size(model_path)

        params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
        params_memory.remove(MODEL_DOWNLOAD_PARAM)

        self.downloading_model = False
      else:
        handle_error(model_path, "Verification failed...", "GitLab verification failed", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM)
        self.downloading_model = False
    else:
      all_model_sizes = self.fetch_all_model_sizes(repo_url) or {}
      tinygrad_filenames = [f"{model_to_download}_{file_key}" for file_key, _ in TINYGRAD_FILES]

      file_sizes = []
      file_sources = []

      missing = [name for name in tinygrad_filenames if int(all_model_sizes.get(name, 0)) <= 0]
      if missing:
        handle_error(None, "Missing size metadata...", f"Sizes not found for: {', '.join(missing)}...", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM)
        self.downloading_model = False
        return

      for filename in tinygrad_filenames:
        primary_url = f"{repo_url}/Models/compiled/{filename}"
        file_size = int(all_model_sizes.get(filename, 0))
        file_sizes.append(file_size)
        file_sources.append((primary_url, None))

      downloaded_offset_bytes = 0
      known_file_sizes = [size for size in file_sizes if size > 0]
      total_model_bytes = sum(known_file_sizes) if len(known_file_sizes) == len(file_sizes) else 0

      for (file_key, description), part_bytes, (primary_url, fallback_url) in zip(TINYGRAD_FILES, file_sizes, file_sources):
        filename = f"{model_to_download}_{file_key}"
        model_path = MODELS_PATH / filename

        print(f"Downloading {description} for model: {model_to_download}")
        download_file(CANCEL_DOWNLOAD_PARAM, model_path, DOWNLOAD_PROGRESS_PARAM, primary_url, MODEL_DOWNLOAD_PARAM, self.session, offset_bytes=downloaded_offset_bytes, total_bytes=total_model_bytes)

        if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
          delete_file(model_path)

          handle_error(None, "Download cancelled...", "Download cancelled...", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM)
          self.downloading_model = False
          return

        if verify_download(model_path, primary_url, self.session):
          print(f"{description.capitalize()} for {model_to_download} downloaded and verified successfully!")
          if total_model_bytes:
            downloaded_offset_bytes += part_bytes
          continue

        print(f"Verification failed for {filename}. Retrying from GitLab...")
        fallback_url = f"{GITLAB_URL}/Models/compiled/{filename}"
        download_file(CANCEL_DOWNLOAD_PARAM, model_path, DOWNLOAD_PROGRESS_PARAM, fallback_url, MODEL_DOWNLOAD_PARAM, self.session, offset_bytes=downloaded_offset_bytes, total_bytes=total_model_bytes)

        if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
          delete_file(model_path)

          handle_error(None, "Download cancelled...", "Download cancelled...", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM)
          self.downloading_model = False
          return

        if verify_download(model_path, fallback_url, self.session):
          print(f"{description.capitalize()} for {model_to_download} downloaded and verified successfully from GitLab!")
          if total_model_bytes:
            downloaded_offset_bytes += part_bytes
        else:
          handle_error(model_path, "Verification failed...", f"GitLab verification failed for {filename}", MODEL_DOWNLOAD_PARAM, DOWNLOAD_PROGRESS_PARAM)
          self.downloading_model = False
          return

      print(f"Updating model sizes for {model_to_download}...")
      for filename, _ in TINYGRAD_FILES:
        file_path = MODELS_PATH / f"{model_to_download}_{filename}"
        self.update_model_size(file_path)

      params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
      params_memory.remove(MODEL_DOWNLOAD_PARAM)

      self.downloading_model = False

  def fetch_all_model_sizes(self, repo_url):
    is_github = "github" in repo_url
    is_gitlab = "gitlab" in repo_url
    repo_encoded = quote_plus(RESOURCES_REPO)

    model_sizes = {}
    try:
      def fetch_dir_sizes(api_url):
        sizes = {}
        print(f"Fetching model metadata: {api_url}")
        response = self.session.get(api_url, timeout=10)
        response.raise_for_status()
        content = response.json()

        model_files = [file for file in content if "." in file["name"]]

        if is_github:
          for file in model_files:
            sizes[file["name"]] = file.get("size", 0)
        else:
          for file in model_files:
            file_path = quote_plus(file["path"])
            metadata_url = f"https://gitlab.com/api/v4/projects/{repo_encoded}/repository/files/{file_path}/raw?ref=Models"
            head_response = self.session.head(metadata_url, timeout=10)
            if head_response.ok:
              sizes[file["name"]] = int(head_response.headers.get("content-length", 0))
        return sizes

      if is_github:
        top_api_url = f"https://api.github.com/repos/{RESOURCES_REPO}/contents?ref=Models"
        version_api_url = f"https://api.github.com/repos/{RESOURCES_REPO}/contents/compiled?ref=Models"
      elif is_gitlab:
        top_api_url = f"https://gitlab.com/api/v4/projects/{repo_encoded}/repository/tree?ref=Models"
        version_api_url = f"https://gitlab.com/api/v4/projects/{repo_encoded}/repository/tree?path=compiled&ref=Models"
      else:
        print(f"Unsupported repository URL: {repo_url}")
        return model_sizes

      model_sizes.update(fetch_dir_sizes(top_api_url))
      model_sizes.update(fetch_dir_sizes(version_api_url))

      return model_sizes

    except requests.exceptions.RequestException as e:
      handle_request_error(f"Failed to fetch model sizes from {'GitHub' if is_github else 'GitLab'}: {e}", None, None, None)
      return {}

  def fetch_models(self, url, repo_url, boot_run=False):
    try:
      response = self.session.get(url, timeout=10)
      response.raise_for_status()
      model_info = response.json().get("models", [])

      if model_info:
        self.update_model_params(model_info)
        self.check_models(boot_run, repo_url)
        self.check_tinygrad(repo_url)
    except Exception as exception:
      handle_request_error(exception, None, None, None)
      return []

  def is_tinygrad_model(self, model):
    return self.model_versions[self.available_models.index(model)] in {"v1", "v2", "v3", "v4", "v5", "v6"}

  def update_model_params(self, model_info):
    self.available_models = [model["id"] for model in model_info]
    self.available_model_names = [model["name"] for model in model_info]
    self.model_versions = [model["version"] for model in model_info]

    params.put("AvailableModels", ",".join(self.available_models))
    params.put("AvailableModelNames", ",".join(self.available_model_names))
    params.put("ModelVersions", ",".join(self.model_versions))
    print("Models list updated successfully!")

  def update_models(self, boot_run):
    if self.downloading_model:
      return

    repo_url = get_repository_url(self.session)
    if repo_url is None:
      print("GitHub and GitLab are offline...")
      return

    self.fetch_models(f"{repo_url}/Versions/model_names_{VERSION}.json", repo_url, boot_run)

  def update_model_size(self, file_path):
    self.model_sizes[file_path.name] = file_path.stat().st_size
    update_json_file(self.model_sizes_path, self.model_sizes)
    print(f"Updated size for {file_path.name} in {self.model_sizes_path.name}")

  def update_tinygrad_size(self, file_path):
    self.tinygrad_sizes[TAR_FILE_NAME] = file_path.stat().st_size
    update_json_file(self.tinygrad_sizes_path, self.tinygrad_sizes)
    print(f"Updated size for {TAR_FILE_NAME} in {self.tinygrad_sizes_path.name}")

  def update_tinygrad(self):
    repo_url = get_repository_url(self.session)
    if not repo_url:
      handle_error(None, "GitHub and GitLab are offline...", "Repository unavailable", None, None)
      return

    primary_url = f"{repo_url}/Tinygrad/{TAR_FILE_NAME}"
    fallback_url = f"https://gitlab.com/{RESOURCES_REPO}/-/raw/Tinygrad/{TAR_FILE_NAME}"

    tinygrad_tar_path = Path("/data/tmp/tinygrad.tar.gz")
    try:
      print(f"Attempting to download tinygrad from {primary_url}...")
      download_file(CANCEL_DOWNLOAD_PARAM, tinygrad_tar_path, DOWNLOAD_PROGRESS_PARAM, primary_url, UPDATE_TINYGRAD_PARAM, self.session)

      if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
        delete_file(tinygrad_tar_path)

        handle_error(None, "Tinygrad update cancelled...", "Tinygrad update cancelled...", UPDATE_TINYGRAD_PARAM, DOWNLOAD_PROGRESS_PARAM)
        params_memory.remove("CancelModelDownload")
        return

      if not verify_download(tinygrad_tar_path, primary_url, self.session):
        print(f"Verification failed for {primary_url}. Retrying from GitLab...")
        download_file(CANCEL_DOWNLOAD_PARAM, tinygrad_tar_path, DOWNLOAD_PROGRESS_PARAM, fallback_url, UPDATE_TINYGRAD_PARAM, self.session)

      if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
        delete_file(tinygrad_tar_path)

        handle_error(None, "Tinygrad update cancelled...", "Tinygrad update cancelled...", UPDATE_TINYGRAD_PARAM, DOWNLOAD_PROGRESS_PARAM)
        params_memory.remove("CancelModelDownload")
        return

      if not verify_download(tinygrad_tar_path, fallback_url, self.session):
        handle_error(tinygrad_tar_path, "Verification Failed", "Tinygrad verification failed", UPDATE_TINYGRAD_PARAM, DOWNLOAD_PROGRESS_PARAM)
        return

      print("Tinygrad downloaded successfully! Proceeding with installation...")
      self.update_tinygrad_size(tinygrad_tar_path)

      params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Installing...")

      print("Deleting old tinygrad directories...")
      delete_file(TINYGRAD_MODELD_PATH)
      print(f"Removed {TINYGRAD_MODELD_PATH}")
      delete_file(TINYGRAD_REPO_PATH)
      print(f"Removed {TINYGRAD_REPO_PATH}")

      extract_tar(tinygrad_tar_path, Path(BASEDIR))

      print("Tinygrad update completed successfully!")

      params.put_bool("TinygradUpdateAvailable", False)

      params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Updated!")
      params_memory.remove(UPDATE_TINYGRAD_PARAM)

      self.update_tinygrad_models(repo_url)
    except Exception as exception:
      handle_error(tinygrad_tar_path, "Update Failed", f"An unexpected error occurred: {exception}", UPDATE_TINYGRAD_PARAM, DOWNLOAD_PROGRESS_PARAM)

  def update_tinygrad_models(self, repo_url=None):
    print("Updating old Tinygrad models...")

    installed_tinygrad_models = set()
    for filename, _ in TINYGRAD_FILES:
      suffix = f"_{filename}"
      for file_path in MODELS_PATH.glob(f"*{suffix}"):
        model_name = file_path.name.rsplit(suffix, 1)[0]
        if model_name in set(self.available_models):
          installed_tinygrad_models.add(model_name)
        delete_file(file_path)

    self.copy_default_model()

    update_frogpilot_toggles()

    if repo_url is None:
      return

    current_model = params.get("Model", encoding="utf-8").removesuffix("_default")

    models_to_redownload = [current_model]
    models_to_redownload += [model for model in sorted(installed_tinygrad_models) if model != current_model]

    if DEFAULT_MODEL in models_to_redownload:
      models_to_redownload.remove(DEFAULT_MODEL)

    if models_to_redownload:
      print(f"Redownloading the following models: {', '.join(models_to_redownload)}")
      self.fetch_models(f"{repo_url}/Versions/model_names_{VERSION}.json", repo_url, boot_run=True)

      for model in models_to_redownload:
        if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
          handle_error(None, "Download cancelled...", "Download cancelled...", MODEL_DOWNLOAD_ALL_PARAM, DOWNLOAD_PROGRESS_PARAM)
          return

        params_memory.put(DOWNLOAD_PROGRESS_PARAM, f"Downloading \"{self.available_model_names[self.available_models.index(model)]}\"...")
        self.download_model(model)
    else:
      print("No previously installed tinygrad models to redownload")

    update_frogpilot_toggles()

  def validate_models(self):
    current = params.get("Model", encoding="utf-8")
    default = params_default.get("Model", encoding="utf-8")

    if current.endswith("_default") and current != default:
      print(f"Model '{current}' does not match default '{default}', resetting...")
      params.put("Model", default)

    if VERSION_PATH.is_file():
      version_name = VERSION_PATH.read_text().strip()
      if version_name != VERSION or int(self.tinygrad_sizes.get(TAR_FILE_NAME, 0)) == 0:
        self.update_tinygrad_models()

        self.tinygrad_sizes[TAR_FILE_NAME] = DEFAULT_TINYGRAD_SIZE
        update_json_file(self.tinygrad_sizes_path, self.tinygrad_sizes)
        print(f"Updated size for {TAR_FILE_NAME} in {self.tinygrad_sizes_path.name}")

        params.remove("TinygradUpdateAvailable")

    VERSION_PATH.write_text(VERSION)
    print(f"Updated {VERSION_PATH} to {VERSION}")
