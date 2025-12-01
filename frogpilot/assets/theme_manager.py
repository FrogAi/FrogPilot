#!/usr/bin/env python3
import requests
import shutil
import threading

from datetime import date, timedelta
from dateutil import easter
from pathlib import Path
from urllib.parse import quote_plus

from openpilot.frogpilot.common import frogpilot_download_utilities, frogpilot_utilities, frogpilot_variables

HOLIDAY_THEME_PATH = Path(__file__).parent / "holiday_themes"
STOCKOP_THEME_PATH = Path(__file__).parent / "stock_theme"

DOWNLOADABLE_PARAMS = {
  "colors": "DownloadableColors",
  "distance_icons": "DownloadableDistanceIcons",
  "icons": "DownloadableIcons",
  "signals": "DownloadableSignals",
  "sounds": "DownloadableSounds",
  "steering_wheels": "DownloadableWheels",
}

HOLIDAY_SLUGS = (
  "new_years",
  "valentines_day",
  "st_patricks_day",
  "world_frog_day",
  "april_fools",
  "easter_week",
  "may_the_fourth",
  "cinco_de_mayo",
  "stitch_day",
  "fourth_of_july",
  "halloween_week",
  "thanksgiving_week",
  "christmas_week",
)

class ThemeManager:
  def __init__(self, params, boot_run=False):
    self.params = params

    self.download_lock = threading.Lock()
    self.download_state = frogpilot_download_utilities.DownloadState()

    self.theme_updated = False

    self.download_count = 0
    self.download_failed_count = 0
    self.download_success_count = 0
    self.theme_update_count = 0

    self.wheel_before_random_event = None

    self.holiday_theme = "stock"

    self.previous_asset_mappings = {}

    self.theme_sizes_path = frogpilot_variables.THEME_SAVE_PATH / "theme_sizes.json"

    self.theme_sizes = frogpilot_utilities.load_json_file(self.theme_sizes_path)

    self.session = requests.Session()
    self.session.headers.update({
      "Accept": "application/vnd.github.v3+json",
      "Accept-Language": "en",
      "User-Agent": "frogpilot-theme-downloader/1.0 (https://github.com/FrogAi/FrogPilot)"
    })

    if boot_run:
      self.copy_default_theme()

  @staticmethod
  def calculate_thanksgiving(year):
    november_first = date(year, 11, 1)
    days_to_thursday = (3 - november_first.weekday()) % 7
    first_thursday = november_first + timedelta(days=days_to_thursday)
    return first_thursday + timedelta(days=21)

  @staticmethod
  def copy_default_theme():
    world_frog_day_theme_path = HOLIDAY_THEME_PATH / "world_frog_day"

    for theme_subfolder_name, save_subfolder_path in [
      ("colors", "theme_packs/frog/colors"),
      ("distance_icons", "theme_packs/frog-animated/distance_icons"),
      ("icons", "theme_packs/frog-animated/icons"),
      ("signals", "theme_packs/frog/signals"),
      ("sounds", "theme_packs/frog/sounds"),
    ]:
      source_folder_path = world_frog_day_theme_path / theme_subfolder_name
      destination_folder_path = frogpilot_variables.THEME_SAVE_PATH / save_subfolder_path
      if not destination_folder_path.exists():
        shutil.copytree(source_folder_path, destination_folder_path)

    steering_wheel_image_path = world_frog_day_theme_path / "steering_wheel/wheel.png"
    steering_wheel_save_path = frogpilot_variables.THEME_SAVE_PATH / "steering_wheels/frog.png"
    if not steering_wheel_save_path.exists():
      steering_wheel_save_path.parent.mkdir(parents=True, exist_ok=True)
      shutil.copy2(steering_wheel_image_path, steering_wheel_save_path)

  def _install_zip(self, theme_path, download_path):
    stage_path = download_path.with_name(f".{download_path.name}.new")
    backup_path = download_path.with_name(f".{download_path.name}.old")
    frogpilot_utilities.delete_file(stage_path)
    frogpilot_utilities.delete_file(backup_path)

    try:
      frogpilot_utilities.extract_zip(theme_path, stage_path)
      if self.download_state.cancelled:
        frogpilot_utilities.delete_file(stage_path)
        return False

      if download_path.exists():
        download_path.rename(backup_path)
      try:
        stage_path.rename(download_path)
      except OSError:
        if backup_path.exists():
          backup_path.rename(download_path)
        raise
      frogpilot_utilities.delete_file(backup_path)
      return True
    except Exception:
      frogpilot_utilities.delete_file(stage_path)
      raise

  def download_theme(self, theme_component, theme_name, repo_url):
    if not repo_url:
      frogpilot_download_utilities.handle_error(None, "GitHub and GitLab are offline...", self.download_state)
      return

    if theme_component == "distance_icons":
      relative_link = f"Distance-Icons/{theme_name}"
      download_path = frogpilot_variables.THEME_SAVE_PATH / "theme_packs" / theme_name / theme_component
      extension = ".zip"
    elif theme_component == "steering_wheels":
      relative_link = f"Steering-Wheels/{theme_name}"
      download_path = frogpilot_variables.THEME_SAVE_PATH / theme_component / theme_name
      extension = ".gif"
    else:
      relative_link = f"Themes/{theme_name}/{theme_component}"
      download_path = frogpilot_variables.THEME_SAVE_PATH / "theme_packs" / theme_name / theme_component
      extension = ".zip"

    if theme_component == "steering_wheels":
      theme_path = frogpilot_variables.THEME_SAVE_PATH / f"{theme_name}{extension}"
    else:
      theme_path = download_path.with_suffix(extension)

    repo_urls = [repo_url] if repo_url == frogpilot_download_utilities.GITLAB_URL else [repo_url, frogpilot_download_utilities.GITLAB_URL]

    for attempt, current_repo_url in enumerate(repo_urls):
      theme_url = f"{current_repo_url}/{relative_link}{extension}"

      frogpilot_utilities.delete_file(theme_path)

      downloaded_path = frogpilot_download_utilities.download_file(theme_path, self.download_state, self.session, theme_url)

      if attempt == 0 and self.download_state.cancelled:
        frogpilot_download_utilities.handle_error(downloaded_path, "Download cancelled...", self.download_state)
        return

      if downloaded_path:
        self.install_theme(downloaded_path.suffix, theme_component, theme_name, downloaded_path, download_path)
        return

    frogpilot_download_utilities.handle_error(None, "Download failed...", self.download_state)

  def download_themes(self, themes):
    repo_url = frogpilot_download_utilities.get_repository_url(self.session)

    for theme_component, theme_name in themes:
      if self.download_state.cancelled:
        self.download_state.progress = "Download cancelled..."
        break

      self.download_state.progress = "Downloading..."
      self.download_theme(theme_component, theme_name, repo_url)

      if self.download_state.progress == "Downloaded!":
        self.download_success_count += 1
      elif self.download_state.cancelled:
        self.download_state.progress = "Download cancelled..."
        break
      elif self.download_state.progress == "GitHub and GitLab are offline...":
        break
      else:
        self.download_failed_count += 1

  def fetch_assets(self, repo_url):
    is_github = repo_url != frogpilot_download_utilities.GITLAB_URL

    repo_encoded = quote_plus(frogpilot_variables.RESOURCES_REPO)

    assets = {"themes": {}, "wheels": []}
    try:
      def list_files(branch):
        if is_github:
          response = self.session.get(f"https://api.github.com/repos/{frogpilot_variables.RESOURCES_REPO}/git/trees/{branch}?recursive=1", timeout=10)
          response.raise_for_status()
          return [
            {"path": item.get("path", ""), "size": item.get("size", 0)}
            for item in response.json().get("tree", [])
            if item.get("type") == "blob"
          ]

        items = []
        page = "1"
        while page:
          response = self.session.get(f"https://gitlab.com/api/v4/projects/{repo_encoded}/repository/tree?ref={branch}&recursive=true&per_page=100&page={page}", timeout=10)
          response.raise_for_status()
          items.extend(
            {"path": item.get("path", ""), "size": 0}
            for item in response.json()
            if item.get("type") in ("blob", "file")
          )
          page = response.headers.get("X-Next-Page", "")
        return items

      def file_size(branch, path, fallback):
        if is_github:
          return int(fallback or 0)
        response = self.session.head(f"https://gitlab.com/api/v4/projects/{repo_encoded}/repository/files/{quote_plus(path)}/raw?ref={branch}", timeout=10)
        return int(response.headers.get("content-length", 0)) if response.ok else 0

      for branch in ["Distance-Icons", "Steering-Wheels"]:
        for item in list_files(branch):
          path = item["path"]

          if branch == "Steering-Wheels":
            assets["wheels"].append(path)
            theme_name = Path(path).stem
            local_files = list((frogpilot_variables.THEME_SAVE_PATH / "steering_wheels").glob(f"{theme_name}.*"))
            if local_files:
              size = file_size(branch, path, item.get("size", 0))
              local_size = self.theme_sizes.get("wheels", {}).get(theme_name)
              if size > 0 and local_size != size:
                self.download_theme("steering_wheels", theme_name, repo_url)

          elif branch == "Distance-Icons":
            component_name = "distance_icons"
            theme_name = Path(path).stem
            assets["themes"].setdefault(theme_name, set()).add(component_name)

            local_path = frogpilot_variables.THEME_SAVE_PATH / "theme_packs" / theme_name / component_name
            if local_path.exists():
              size = file_size(branch, path, item.get("size", 0))
              local_size = self.theme_sizes.get("themes", {}).get(theme_name, {}).get(component_name)
              if size > 0 and local_size != size:
                self.download_theme(component_name, theme_name, repo_url)

      branch = "Themes"
      for item in list_files(branch):
        if "/" not in item["path"]:
          continue

        theme_name, sub_path = item["path"].split("/", 1)
        theme_path = sub_path.lower()

        for key in ("colors", "icons", "signals", "sounds"):
          if key in theme_path:
            assets["themes"].setdefault(theme_name, set()).add(key)

            local_path = frogpilot_variables.THEME_SAVE_PATH / "theme_packs" / theme_name / key
            if local_path.exists():
              expected_size = file_size(branch, item["path"], item.get("size", 0))
              local_size = self.theme_sizes.get("themes", {}).get(theme_name, {}).get(key)
              if expected_size > 0 and local_size != expected_size:
                self.download_theme(key, theme_name, repo_url)
            break

      assets["themes"] = {key: sorted(list(value)) for key, value in assets["themes"].items()}
      assets["wheels"].sort()
      return assets

    except requests.exceptions.RequestException as error:
      print(f"Failed to fetch theme sizes from {'GitHub' if is_github else 'GitLab'}: {error}")
      return {}

  @staticmethod
  def format_name(name):
    base = Path(name).stem
    creator = ""
    if "~" in base:
      base, creator = base.split("~", 1)

    variant = ""
    if base.endswith("-animated"):
      base = base[: -len("-animated")]
      variant = " (Animated)"

    parts = base.replace("_", " ").split()
    display = " ".join(part.capitalize() for part in parts) + variant

    if creator:
      return f"{display} - by: {creator}"
    return display

  @staticmethod
  def get_full_themes():
    theme_packs_path = frogpilot_variables.THEME_SAVE_PATH / "theme_packs"
    if not theme_packs_path.exists():
      return []

    valid_themes = set()
    for theme_directory in theme_packs_path.iterdir():
      if not theme_directory.is_dir():
        continue

      base_name = theme_directory.name.replace("-animated", "")

      animated_path = theme_packs_path / f"{base_name}-animated"
      base_path = theme_packs_path / base_name

      base_valid = all((base_path / asset).is_dir() for asset in {"colors", "sounds"})
      animated_icons_exist = (animated_path / "icons").is_dir()
      base_icons_exist = (base_path / "icons").is_dir()

      if base_valid and (animated_icons_exist or base_icons_exist):
        if animated_icons_exist:
          valid_themes.add(f"{base_name}-animated")
        else:
          valid_themes.add(base_name)

    return sorted(valid_themes)

  @staticmethod
  def get_holiday_theme_dates(year):
    return {
      "new_years": date(year, 1, 1),
      "valentines_day": date(year, 2, 14),
      "st_patricks_day": date(year, 3, 17),
      "world_frog_day": date(year, 3, 20),
      "april_fools": date(year, 4, 1),
      "easter_week": easter.easter(year),
      "may_the_fourth": date(year, 5, 4),
      "cinco_de_mayo": date(year, 5, 5),
      "stitch_day": date(year, 6, 26),
      "fourth_of_july": date(year, 7, 4),
      "halloween_week": date(year, 10, 31),
      "thanksgiving_week": ThemeManager.calculate_thanksgiving(year),
      "christmas_week": date(year, 12, 25)
    }

  def install_theme(self, extension, theme_component, theme_name, theme_path, download_path):
    theme_size = theme_path.stat().st_size

    if extension == ".zip":
      self.download_state.progress = "Unpacking theme..."
      try:
        installed = self._install_zip(theme_path, download_path)
      except Exception:
        frogpilot_download_utilities.handle_error(theme_path, "Download failed...", self.download_state)
        return
      if not installed:
        frogpilot_download_utilities.handle_error(None, "Download cancelled...", self.download_state)
        return

      if (frogpilot_variables.ACTIVE_THEME_PATH / theme_component).resolve() == download_path.resolve():
        self.theme_update_count += 1
        self.theme_updated = True
    else:
      other_extension = ".gif" if extension == ".png" else ".png"

      theme_path.replace(download_path.with_suffix(extension))
      frogpilot_utilities.delete_file(download_path.with_suffix(other_extension))

      if (frogpilot_variables.ACTIVE_THEME_PATH / "steering_wheel" / f"wheel{other_extension}").resolve() == download_path.with_suffix(other_extension).resolve():
        self.update_wheel_image(theme_name)

      if (frogpilot_variables.ACTIVE_THEME_PATH / "steering_wheel" / f"wheel{extension}").resolve() == download_path.with_suffix(extension).resolve():
        self.theme_update_count += 1

    self.update_theme_size(theme_component, theme_name, theme_size)
    self.refresh_theme_params()
    self.download_state.progress = "Downloaded!"

  @staticmethod
  def is_within_week_of(target_date, current_date):
    start_of_week = target_date - timedelta(days=target_date.weekday())
    return start_of_week <= current_date < target_date

  def refresh_theme_params(self):
    self.update_theme_params({component: self.params.get(key).split(",") for component, key in DOWNLOADABLE_PARAMS.items()})

  @staticmethod
  def unformat_name(display_name):
    base = display_name
    creator = ""
    if " - by: " in base:
      base, creator = base.split(" - by: ", 1)

    raw_name = base.lower().replace(" ", "_").replace("(", "").replace(")", "")
    name = raw_name.replace("_animated", "-animated")

    if creator:
      return f"{name}~{creator}"
    return name

  def update_active_theme(self, time_validated, frogpilot_toggles, boot_run=False):
    if time_validated and frogpilot_toggles.holiday_themes:
      self.holiday_theme = self.update_holiday()
    else:
      self.holiday_theme = "stock"

    if self.holiday_theme != "stock":
      asset_mappings = {
        "color_scheme": ("colors", self.holiday_theme),
        "distance_icons": ("distance_icons", self.holiday_theme),
        "icon_pack": ("icons", self.holiday_theme),
        "sound_pack": ("sounds", self.holiday_theme),
        "turn_signal_pack": ("signals", self.holiday_theme),
        "wheel_image": ("wheel_image", self.holiday_theme)
      }
    else:
      asset_mappings = {
        "color_scheme": ("colors", frogpilot_toggles.color_scheme),
        "distance_icons": ("distance_icons", frogpilot_toggles.distance_icons),
        "icon_pack": ("icons", frogpilot_toggles.icon_pack),
        "sound_pack": ("sounds", frogpilot_toggles.sound_pack),
        "turn_signal_pack": ("signals", frogpilot_toggles.signal_icons),
        "wheel_image": ("wheel_image", frogpilot_toggles.wheel_image)
      }

    if asset_mappings != self.previous_asset_mappings:
      links_changed = False

      for asset_type, current_value in asset_mappings.values():
        if asset_type == "wheel_image":
          links_changed |= self.update_wheel_image(current_value)
        else:
          links_changed |= self.update_theme_asset(asset_type, current_value)

      self.previous_asset_mappings = asset_mappings

      if links_changed:
        self.theme_update_count += 1
      self.theme_updated = True

  def update_holiday(self):
    current_date = date.today()

    holidays = self.get_holiday_theme_dates(current_date.year)
    for holiday, holiday_date in holidays.items():
      if (holiday.endswith("_week") and self.is_within_week_of(holiday_date, current_date)) or (current_date == holiday_date):
        return holiday

    return "stock"

  def update_theme_asset(self, asset_type, theme):
    save_location = frogpilot_variables.ACTIVE_THEME_PATH / asset_type

    if self.holiday_theme != "stock":
      asset_location = HOLIDAY_THEME_PATH / self.holiday_theme / asset_type
    elif theme in HOLIDAY_SLUGS:
      asset_location = HOLIDAY_THEME_PATH / theme / asset_type
    elif f"{theme}_week" in HOLIDAY_SLUGS:
      asset_location = HOLIDAY_THEME_PATH / f"{theme}_week" / asset_type
    else:
      asset_location = frogpilot_variables.THEME_SAVE_PATH / "theme_packs" / theme / asset_type

    if not asset_location.exists() or theme == "stock":
      asset_location = STOCKOP_THEME_PATH / asset_type

    if save_location.resolve() == asset_location.resolve():
      return False

    frogpilot_utilities.delete_file(save_location)

    save_location.parent.mkdir(parents=True, exist_ok=True)
    save_location.symlink_to(asset_location, target_is_directory=True)
    return True

  def update_theme_params(self, downloadable):
    for component, key in DOWNLOADABLE_PARAMS.items():
      if component == "steering_wheels":
        existing_assets = {self.format_name(item.name) for item in (frogpilot_variables.THEME_SAVE_PATH / component).glob("*") if item.is_file()}
      else:
        existing_assets = {self.format_name(item.parent.name) for item in (frogpilot_variables.THEME_SAVE_PATH / "theme_packs").glob(f"*/{component}") if item.is_dir()}

      self.params.put(key, ",".join(sorted(set(downloadable[component]) - existing_assets)))

    downloaded_themes = {}
    for theme_dir in (frogpilot_variables.THEME_SAVE_PATH / "theme_packs").iterdir():
      components = []
      for component in ["colors", "distance_icons", "icons", "signals", "sounds"]:
        if (theme_dir / component).is_dir():
          components.append(component)

      if components:
        theme_name = self.format_name(theme_dir.name)
        downloaded_themes[theme_name] = sorted(components)

    downloaded_wheels = []
    for wheel_file in (frogpilot_variables.THEME_SAVE_PATH / "steering_wheels").iterdir():
      if wheel_file.is_file():
        downloaded_wheels.append(self.format_name(wheel_file.name))

    self.params.put("ThemesDownloaded", {
      "themes": {key: downloaded_themes[key] for key in sorted(downloaded_themes)},
      "steering_wheels": sorted(downloaded_wheels)
    })

  def update_theme_size(self, theme_component, theme_name, file_size):
    if theme_component == "steering_wheels":
      self.theme_sizes.setdefault("wheels", {})[theme_name] = file_size
    else:
      self.theme_sizes.setdefault("themes", {}).setdefault(theme_name, {})[theme_component] = file_size

    frogpilot_utilities.update_json_file(self.theme_sizes_path, self.theme_sizes)

  def update_themes(self, boot_run=False):
    repo_url = frogpilot_download_utilities.get_repository_url(self.session)
    if repo_url is None:
      print("GitHub and GitLab are offline...")
      return

    assets = self.fetch_assets(repo_url)
    if not assets:
      return

    downloadable = {component: [] for component in DOWNLOADABLE_PARAMS}
    for theme, components in assets["themes"].items():
      for component in components:
        downloadable[component].append(self.format_name(theme))
    downloadable["steering_wheels"] = [self.format_name(wheel) for wheel in assets["wheels"]]

    if boot_run:
      self.validate_themes(repo_url)

    self.update_theme_params(downloadable)

  def update_wheel_image(self, image, random_event=False):
    wheel_save_location = frogpilot_variables.ACTIVE_THEME_PATH / "steering_wheel"

    if wheel_save_location.is_dir():
      current_wheels = [file.resolve() for file in wheel_save_location.iterdir()]
    else:
      current_wheels = None

    if self.holiday_theme != "stock":
      wheel_location = HOLIDAY_THEME_PATH / self.holiday_theme / "steering_wheel"
    elif random_event:
      wheel_location = frogpilot_variables.RANDOM_EVENTS_PATH / "steering_wheels"
    elif image == "none":
      if current_wheels == []:
        return False

      frogpilot_utilities.delete_file(wheel_save_location)
      wheel_save_location.mkdir(parents=True, exist_ok=True)
      return True
    elif image == "stock":
      wheel_location = STOCKOP_THEME_PATH / "steering_wheel"
    elif image in HOLIDAY_SLUGS:
      wheel_location = HOLIDAY_THEME_PATH / image / "steering_wheel"
    elif f"{image}_week" in HOLIDAY_SLUGS:
      wheel_location = HOLIDAY_THEME_PATH / f"{image}_week" / "steering_wheel"
    else:
      wheel_location = frogpilot_variables.THEME_SAVE_PATH / "steering_wheels"

    if not wheel_location.exists():
      wheel_location = STOCKOP_THEME_PATH / "steering_wheel"

    image_name = image.replace(" ", "_").lower()
    source_file = next((file for file in wheel_location.iterdir() if file.stem.lower() in {image_name, "wheel"}), None)
    if source_file is None:
      return False

    if random_event and self.wheel_before_random_event is None:
      self.wheel_before_random_event = current_wheels

    if current_wheels == [source_file.resolve()]:
      return False

    frogpilot_utilities.delete_file(wheel_save_location)
    wheel_save_location.mkdir(parents=True, exist_ok=True)

    destination_file = wheel_save_location / f"wheel{source_file.suffix}"
    destination_file.symlink_to(source_file)
    return True

  def restore_wheel_image(self):
    if self.wheel_before_random_event is None:
      return

    wheel_save_location = frogpilot_variables.ACTIVE_THEME_PATH / "steering_wheel"

    frogpilot_utilities.delete_file(wheel_save_location)
    wheel_save_location.mkdir(parents=True, exist_ok=True)

    for source_file in self.wheel_before_random_event:
      (wheel_save_location / f"wheel{source_file.suffix}").symlink_to(source_file)

    self.wheel_before_random_event = None

  def validate_themes(self, repo_url):
    downloaded_data = self.params.get("ThemesDownloaded")

    redownloaded = False
    for display_name, components in downloaded_data.get("themes", {}).items():
      theme_folder_name = self.unformat_name(display_name)

      for component in components:
        component_path = frogpilot_variables.THEME_SAVE_PATH / "theme_packs" / theme_folder_name / component
        if not component_path.is_dir() or not any(component_path.iterdir()):
          self.download_theme(component, theme_folder_name, repo_url)
          redownloaded |= self.download_state.progress == "Downloaded!"

    wheels_path = frogpilot_variables.THEME_SAVE_PATH / "steering_wheels"
    for display_name in downloaded_data.get("steering_wheels", []):
      file_stem = self.unformat_name(display_name)
      matching_files = list(wheels_path.glob(f"{file_stem}.*"))
      if not matching_files:
        self.download_theme("steering_wheels", file_stem, repo_url)
        redownloaded |= self.download_state.progress == "Downloaded!"

    if redownloaded:
      self.previous_asset_mappings = {}
      self.theme_updated = True

    for dir_path in frogpilot_variables.THEME_SAVE_PATH.glob("**/*"):
      if dir_path.is_dir() and not any(dir_path.iterdir()):
        frogpilot_utilities.delete_file(dir_path)
      elif dir_path.is_file() and (dir_path.name.startswith("tmp") or dir_path.suffix == ".tmp"):
        frogpilot_utilities.delete_file(dir_path)
