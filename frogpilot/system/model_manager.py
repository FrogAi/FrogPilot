#!/usr/bin/env python3
import os
import random
import requests
import threading

from urllib.parse import quote_plus

from openpilot.frogpilot.common import frogpilot_download_utilities, frogpilot_paths, frogpilot_utilities, frogpilot_variables

VERSION = "v17"

class ModelManager:
  def __init__(self, boot_run=False):
    self.download_lock = threading.Lock()
    self.download_state = frogpilot_download_utilities.DownloadState()

    self.downloading_automatically = False
    self.models_updated = False

    self.available_models = self.split_param("AvailableModels")
    self.available_model_names = self.split_param("AvailableModelNames")
    self.model_versions = self.split_param("ModelVersions")

    self.model_sizes = frogpilot_utilities.load_json_file(frogpilot_paths.MODEL_SIZES_PATH)

    self.session = requests.Session()
    self.session.headers.update({"Accept-Language": "en"})
    self.session.headers.update({"User-Agent": "frogpilot-model-downloader/1.0 (https://github.com/FrogAi/FrogPilot)"})

    if boot_run:
      self.validate_models()
      self.link_default_models()

  def split_param(self, key):
    return [value for value in (frogpilot_variables.params.get(key, encoding="utf-8") or "").split(",") if value]

  def download_model(self, model, repo_url, model_sizes):
    thneed = self.is_thneed_model(model)
    files = frogpilot_utilities.model_files(model, thneed)

    if thneed:
      folder = "Models"
      file_sizes = [0]
      total_bytes = 0
    else:
      folder = "Models/compiled"

      missing = [filename for filename, _ in files if int(model_sizes.get(filename, 0)) <= 0]
      if missing:
        print(f"Sizes not found for: {', '.join(missing)}...")
        self.download_state.handle_error(None, "Missing size metadata...")
        return False

      file_sizes = [int(model_sizes[filename]) for filename, _ in files]
      total_bytes = sum(file_sizes)

    offset_bytes = 0
    for (filename, description), file_size in zip(files, file_sizes, strict=True):
      destination = frogpilot_paths.MODELS_PATH / filename
      path = f"{folder}/{filename}"

      print(f"Downloading {description} for model: {model}")
      for url in (f"{repo_url}/{path}", f"{frogpilot_download_utilities.GITLAB_URL}/{path}"):
        self.download_state.download_file(destination, self.session, url, offset_bytes, total_bytes)

        if self.download_state.cancelled:
          frogpilot_utilities.delete_path(destination)
          self.download_state.handle_error(None, "Download cancelled...")
          return False

        if frogpilot_download_utilities.verify_download(destination, url, self.session):
          break

        print(f"Verification failed for {filename}. Retrying...")
      else:
        self.download_state.handle_error(destination, "Verification failed...")
        return False

      print(f"{description.capitalize()} for {model} downloaded and verified successfully!")
      self.update_model_size(destination)

      offset_bytes += file_size

    os.sync()
    self.models_updated = True
    return True

  def download_models(self, models, finished_message, model_sizes=None):
    repo_url = frogpilot_download_utilities.get_repository_url(self.session)
    if not repo_url:
      self.download_state.handle_error(None, "GitHub and GitLab are offline...")
      return

    for model in models:
      if self.is_downloaded(model):
        continue

      if not model_sizes and not self.is_thneed_model(model):
        model_sizes = self.fetch_all_model_sizes(repo_url) or self.fetch_all_model_sizes(frogpilot_download_utilities.GITLAB_URL)

      print(f"Model {model} is not downloaded. Preparing to download...")
      if not self.download_model(model, repo_url, model_sizes):
        return

    self.download_state.progress = finished_message

  def fetch_all_model_sizes(self, repo_url):
    is_github = "github" in repo_url
    is_gitlab = "gitlab" in repo_url
    repo_encoded = quote_plus(frogpilot_variables.RESOURCES_REPO)

    model_sizes = {}
    try:
      def fetch_dir_sizes(api_url):
        sizes = {}
        print(f"Fetching model metadata: {api_url}")
        response = self.session.get(api_url, timeout=10)
        response.raise_for_status()
        content = response.json()

        listed_files = [file for file in content if "." in file["name"]]

        if is_github:
          for file in listed_files:
            sizes[file["name"]] = file.get("size", 0)
        else:
          for file in listed_files:
            file_path = quote_plus(file["path"])
            metadata_url = f"https://gitlab.com/api/v4/projects/{repo_encoded}/repository/files/{file_path}/raw?ref=Models"
            head_response = self.session.head(metadata_url, timeout=10)
            if head_response.ok:
              sizes[file["name"]] = int(head_response.headers.get("content-length", 0))
        return sizes

      if is_github:
        top_api_url = f"https://api.github.com/repos/{frogpilot_variables.RESOURCES_REPO}/contents?ref=Models"
        version_api_url = f"https://api.github.com/repos/{frogpilot_variables.RESOURCES_REPO}/contents/compiled?ref=Models"
      elif is_gitlab:
        top_api_url = f"https://gitlab.com/api/v4/projects/{repo_encoded}/repository/tree?per_page=100&ref=Models"
        version_api_url = f"https://gitlab.com/api/v4/projects/{repo_encoded}/repository/tree?path=compiled&per_page=100&ref=Models"
      else:
        print(f"Unsupported repository URL: {repo_url}")
        return model_sizes

      model_sizes.update(fetch_dir_sizes(top_api_url))
      model_sizes.update(fetch_dir_sizes(version_api_url))

      return model_sizes

    except requests.exceptions.RequestException as exception:
      print(f"Failed to fetch model sizes from {'GitHub' if is_github else 'GitLab'}: {exception}")
      return {}

  def fetch_models(self, url):
    try:
      response = self.session.get(url, timeout=10)
      response.raise_for_status()
      model_info = response.json().get("models", [])
    except (requests.exceptions.RequestException, ValueError) as exception:
      print(f"Failed to fetch the model list: {exception}")
      return False

    if not model_info:
      return False

    self.update_model_params(model_info)
    return True

  def is_downloaded(self, model):
    return all((frogpilot_paths.MODELS_PATH / filename).is_file() for filename, _ in frogpilot_utilities.model_files(model, self.is_thneed_model(model)))

  def is_thneed_model(self, model):
    return self.model_version(model) in frogpilot_variables.THNEED_MODEL_VERSIONS

  def link_default_models(self):
    default_models = [
      (frogpilot_paths.CLASSIC_MODEL_PATH / "supercombo.thneed", frogpilot_paths.MODELS_PATH / "wd-40.thneed"),
      (frogpilot_paths.STOCK_MODEL_PATH / "supercombo.thneed", frogpilot_paths.MODELS_PATH / "national-public-radio.thneed"),
    ]
    for filename, _ in frogpilot_variables.TINYGRAD_FILES:
      source = frogpilot_paths.TINYGRAD_MODELD_PATH / "models" / filename
      target = frogpilot_paths.MODELS_PATH / f"{frogpilot_variables.DEFAULT_MODEL}_{filename}"
      default_models.append((source, target))

    for source, target in default_models:
      if not source.is_file() or (target.is_symlink() and target.readlink() == source):
        continue

      target.unlink(missing_ok=True)
      target.symlink_to(source)
      print(f"Linked {target} to {source}")
      self.update_model_size(target)

  def missing_models(self):
    return [model for model in self.available_models if not self.is_downloaded(model)]

  def model_version(self, model):
    if model in self.available_models:
      index = self.available_models.index(model)
      if index < len(self.model_versions):
        return self.model_versions[index]
    return frogpilot_variables.DEFAULT_MODEL_VERSION

  def randomize_model(self, new_drive):
    blacklisted_models = (frogpilot_variables.params.get("BlacklistedModels", encoding="utf-8") or "").split(",")
    randomized_model = frogpilot_variables.params.get("RandomizedModel", encoding="utf-8")

    models = dict.fromkeys([*self.available_models, frogpilot_variables.DEFAULT_MODEL])
    selectable_models = [model for model in models if self.is_downloaded(model) and model not in blacklisted_models]
    if not selectable_models:
      selectable_models = [frogpilot_variables.DEFAULT_MODEL]

    if randomized_model in selectable_models:
      if not new_drive:
        return

      if len(selectable_models) > 1:
        selectable_models.remove(randomized_model)

    frogpilot_variables.params.put("RandomizedModel", random.choice(selectable_models))

  def remove_outdated_models(self, model_sizes):
    if not model_sizes:
      print("No model size data available. Skipping model checks...")
      return

    for model in self.available_models:
      if model == frogpilot_variables.DEFAULT_MODEL:
        continue

      filenames = [filename for filename, _ in frogpilot_utilities.model_files(model, self.is_thneed_model(model))]

      outdated = any(model_sizes.get(filename, 0) > 0 and self.model_sizes.get(filename) != model_sizes.get(filename) for filename in filenames)
      if not outdated or not any((frogpilot_paths.MODELS_PATH / filename).is_file() for filename in filenames):
        continue

      print(f"Model {model} is outdated. Deleting...")
      for filename in filenames:
        frogpilot_utilities.delete_path(frogpilot_paths.MODELS_PATH / filename)

      self.models_updated = True

  def remove_unavailable_models(self):
    downloaded_models = [
      model_file for model_file in frogpilot_paths.MODELS_PATH.iterdir()
      if model_file.suffix == ".thneed" or any(model_file.name.endswith(f"_{filename}") for filename, _ in frogpilot_variables.TINYGRAD_FILES)
    ]

    available_files = set()
    for model in [*self.available_models, frogpilot_variables.DEFAULT_MODEL]:
      available_files.add(f"{model}.thneed")
      available_files.update(f"{model}_{filename}" for filename, _ in frogpilot_variables.TINYGRAD_FILES)

    for model_file in downloaded_models:
      if model_file.name not in available_files:
        print(f"Removing outdated model: {model_file}")
        frogpilot_utilities.delete_path(model_file)
        self.models_updated = True

    for tmp_file in [*frogpilot_paths.MODELS_PATH.glob("*.tmp"), *frogpilot_paths.MODELS_PATH.glob("tmp*")]:
      if tmp_file.is_file():
        frogpilot_utilities.delete_path(tmp_file)

    if frogpilot_variables.params.get("Model", encoding="utf-8").removesuffix("_default") not in self.available_models:
      frogpilot_variables.params.put("Model", frogpilot_variables.params_default.get("Model", encoding="utf-8"))

  def update_model_params(self, model_info):
    self.available_models = [model["id"] for model in model_info]
    self.available_model_names = [model["name"] for model in model_info]
    self.model_versions = [model["version"] for model in model_info]

    desired_curvature_head_models = [model["id"] for model in model_info if model.get("desired_curvature_head")]
    lat_smooth_seconds = [str(float(model.get("lat_smooth_seconds", frogpilot_variables.LAT_SMOOTH_SECONDS))) for model in model_info]

    model_params = {
      "AvailableModels": ",".join(self.available_models),
      "AvailableModelNames": ",".join(self.available_model_names),
      "DesiredCurvatureHeadModels": ",".join(desired_curvature_head_models),
      "ModelLatSmoothSeconds": ",".join(lat_smooth_seconds),
      "ModelVersions": ",".join(self.model_versions),
    }

    for key, value in model_params.items():
      if frogpilot_variables.params.get(key, encoding="utf-8") != value:
        frogpilot_variables.params.put(key, value)
        self.models_updated = True
    print("Models list updated successfully!")

  def update_model_size(self, file_path):
    self.model_sizes[file_path.name] = file_path.stat().st_size
    frogpilot_utilities.update_json_file(frogpilot_paths.MODEL_SIZES_PATH, self.model_sizes)
    print(f"Updated size for {file_path.name} in {frogpilot_paths.MODEL_SIZES_PATH.name}")

  def update_models(self, automatically_download, started):
    repo_url = frogpilot_download_utilities.get_repository_url(self.session)
    if repo_url is None:
      print("GitHub and GitLab are offline...")
      return

    for url in dict.fromkeys((repo_url, frogpilot_download_utilities.GITLAB_URL)):
      if self.fetch_models(f"{url}/Versions/model_names_{VERSION}.json"):
        repo_url = url
        break
    else:
      return

    if not started:
      self.remove_unavailable_models()

    if not automatically_download:
      return

    model_sizes = None
    if not started:
      model_sizes = self.fetch_all_model_sizes(repo_url)
      self.remove_outdated_models(model_sizes)

    missing_models = self.missing_models()
    if not missing_models:
      return

    self.downloading_automatically = True
    try:
      self.download_state.cancelled = False
      self.download_state.progress = "Downloading..."
      self.download_models(missing_models, "All models downloaded!", model_sizes)
    finally:
      self.downloading_automatically = False

  def update_tinygrad_models(self):
    print("Updating old Tinygrad models...")

    for filename, _ in frogpilot_variables.TINYGRAD_FILES:
      for file_path in frogpilot_paths.MODELS_PATH.glob(f"*_{filename}"):
        frogpilot_utilities.delete_path(file_path)

  def validate_models(self):
    current = frogpilot_variables.params.get("Model", encoding="utf-8")
    default = frogpilot_variables.params_default.get("Model", encoding="utf-8")

    if current.endswith("_default") and current != default:
      print(f"Model '{current}' does not match default '{default}', resetting...")
      frogpilot_variables.params.put("Model", default)

    version_name = frogpilot_paths.VERSION_PATH.read_text().strip() if frogpilot_paths.VERSION_PATH.is_file() else ""
    if version_name != VERSION:
      self.update_tinygrad_models()

    frogpilot_paths.VERSION_PATH.write_text(VERSION)
    print(f"Updated {frogpilot_paths.VERSION_PATH} to {VERSION}")

    if len(self.available_models) != len(self.available_model_names) or len(self.available_models) != len(self.model_versions):
      print("Model lists are out of sync. Resetting parameters...")
      self.available_models = [frogpilot_variables.DEFAULT_MODEL]
      self.available_model_names = [frogpilot_variables.DEFAULT_MODEL_NAME]
      self.model_versions = [frogpilot_variables.DEFAULT_MODEL_VERSION]

      frogpilot_variables.params.put("AvailableModels", frogpilot_variables.DEFAULT_MODEL)
      frogpilot_variables.params.put("AvailableModelNames", frogpilot_variables.DEFAULT_MODEL_NAME)
      frogpilot_variables.params.put("ModelLatSmoothSeconds", str(frogpilot_variables.LAT_SMOOTH_SECONDS))
      frogpilot_variables.params.put("ModelVersions", frogpilot_variables.DEFAULT_MODEL_VERSION)
