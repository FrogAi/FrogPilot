#!/usr/bin/env python3
import glob
import json
import random
import requests
import shutil

from datetime import date, timedelta
from dateutil import easter
from pathlib import Path
from urllib.parse import quote_plus

from openpilot.frogpilot.assets.download_functions import GITLAB_URL, download_file, get_repository_url, handle_error, handle_request_error, verify_download
from openpilot.frogpilot.common.frogpilot_utilities import delete_file, extract_zip, load_json_file, update_json_file
from openpilot.frogpilot.common.frogpilot_variables import ACTIVE_THEME_PATH, RANDOM_EVENTS_PATH, RESOURCES_REPO, THEME_SAVE_PATH, params, params_memory

CANCEL_DOWNLOAD_PARAM = "CancelThemeDownload"
DOWNLOAD_PROGRESS_PARAM = "ThemeDownloadProgress"

HOLIDAY_THEME_PATH = Path(__file__).parent / "holiday_themes"
STOCKOP_THEME_PATH = Path(__file__).parent / "stock_theme"

HOLIDAY_SLUGS = {
  "new_years": "New Year's",
  "valentines_day": "Valentine's Day",
  "st_patricks_day": "St. Patrick's Day",
  "world_frog_day": "World Frog Day",
  "april_fools": "April Fools",
  "easter_week": "Easter",
  "may_the_fourth": "May the Fourth",
  "cinco_de_mayo": "Cinco de Mayo",
  "stitch_day": "Stitch Day",
  "fourth_of_july": "Fourth of July",
  "halloween_week": "Halloween",
  "thanksgiving_week": "Thanksgiving",
  "christmas_week": "Christmas"
}

THEME_COMPONENT_PARAMS = {
  "colors": "ColorToDownload",
  "distance_icons": "DistanceIconToDownload",
  "icons": "IconToDownload",
  "signals": "SignalToDownload",
  "sounds": "SoundToDownload",
  "steering_wheels": "WheelToDownload"
}

class ThemeManager:
  def __init__(self, boot_run=False):
    self.downloading_theme = False
    self.theme_updated = False

    self.holiday_theme = "stock"

    self.previous_asset_mappings = {}

    self.theme_sizes_path = THEME_SAVE_PATH / "theme_sizes.json"

    self.theme_sizes = load_json_file(self.theme_sizes_path)

    self.session = requests.Session()
    self.session.headers.update({"Accept-Language": "en"})
    self.session.headers.update({"User-Agent": "frogpilot-theme-downloader/1.0 (https://github.com/FrogAi/FrogPilot)"})

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
      destination_folder_path = THEME_SAVE_PATH / save_subfolder_path
      destination_folder_path.mkdir(parents=True, exist_ok=True)
      shutil.copytree(source_folder_path, destination_folder_path, dirs_exist_ok=True)

    steering_wheel_image_path = world_frog_day_theme_path / "steering_wheel/wheel.png"
    steering_wheel_save_path = THEME_SAVE_PATH / "steering_wheels/frog.png"
    steering_wheel_save_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(steering_wheel_image_path, steering_wheel_save_path)

  def download_theme(self, theme_component, theme_name, asset_param, frogpilot_toggles):
    self.downloading_theme = True

    repo_url = get_repository_url(self.session)
    if not repo_url:
      handle_error(None, "GitHub and GitLab are offline...", "Repository unavailable", asset_param, DOWNLOAD_PROGRESS_PARAM)
      self.downloading_theme = False
      return

    if theme_component == "distance_icons":
      download_link = f"{repo_url}/Distance-Icons/{theme_name}"
      download_path = THEME_SAVE_PATH / "theme_packs" / theme_name / theme_component
      extensions = [".zip"]
    elif theme_component == "steering_wheels":
      download_link = f"{repo_url}/Steering-Wheels/{theme_name}"
      download_path = THEME_SAVE_PATH / theme_component / theme_name
      extensions = [".gif", ".png"]
    else:
      download_link = f"{repo_url}/Themes/{theme_name}/{theme_component}"
      download_path = THEME_SAVE_PATH / "theme_packs" / theme_name / theme_component
      extensions = [".zip"]

    for extension in extensions:
      theme_path = download_path.with_suffix(extension)
      theme_url = download_link + extension

      delete_file(theme_path)

      print(f"Downloading theme from GitHub: {theme_name}")
      download_file(CANCEL_DOWNLOAD_PARAM, theme_path, DOWNLOAD_PROGRESS_PARAM, theme_url, asset_param, self.session)

      if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
        delete_file(theme_path)
        handle_error(None, "Download cancelled...", "Download cancelled...", asset_param, DOWNLOAD_PROGRESS_PARAM)

        self.downloading_theme = False
        return

      if verify_download(theme_path, theme_url, self.session):
        print(f"Theme {theme_name} downloaded and verified successfully from GitHub!")
        self.update_theme_size(theme_component, theme_name, theme_path.stat().st_size)

        if extension == ".zip":
          params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Unpacking theme...")
          extract_zip(theme_path, download_path)

        params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
        params_memory.remove(asset_param)

        self.downloading_theme = False

        self.update_themes(frogpilot_toggles)
        return
      elif self.handle_verification_failure(extension, theme_component, theme_name, asset_param, theme_path, download_path, frogpilot_toggles):
        return

    handle_error(download_path, "Download failed...", "Download failed...", asset_param, DOWNLOAD_PROGRESS_PARAM)
    self.downloading_theme = False

  def fetch_assets(self, repo_url, frogpilot_toggles):
    is_github = "github" in repo_url
    is_gitlab = "gitlab" in repo_url

    repo_encoded = quote_plus(RESOURCES_REPO)

    assets = {"themes": {}, "wheels": []}
    try:
      def list_files(branch):
        if is_github:
          response = self.session.get(f"https://api.github.com/repos/{RESOURCES_REPO}/git/trees/{branch}?recursive=1", timeout=10)
          response.raise_for_status()
          return [
            {
              "path": item.get("path", ""),
              "name": Path(item.get("path", "")).name,
              "type": item.get("type"),
              "size": item.get("size", 0),
            }
            for item in response.json().get("tree", [])
            if item.get("type") == "blob"
          ]
        if is_gitlab:
          response = self.session.get(f"https://gitlab.com/api/v4/projects/{repo_encoded}/repository/tree?ref={branch}&recursive=true", timeout=10)
          response.raise_for_status()
          return [
            {
              "path": item.get("path", ""),
              "name": item.get("name", ""),
              "type": item.get("type"),
              "size": 0,
            }
            for item in response.json()
            if item.get("type") in ("blob", "file")
          ]
        print(f"Unsupported repository URL: {repo_url}")
        return []

      def file_size(branch, path, fallback):
        if is_github:
          return int(fallback or 0)
        response = self.session.head(f"https://gitlab.com/api/v4/projects/{repo_encoded}/repository/files/{quote_plus(path)}/raw?ref={branch}", timeout=10)
        return int(response.headers.get("content-length", 0)) if response.ok else 0

      for branch in ["Distance-Icons", "Steering-Wheels"]:
        for item in list_files(branch):
          if item.get("type") not in ("file", "blob"):
            continue

          path = item["path"]
          size = file_size(branch, path, item.get("size", 0))

          if branch == "Steering-Wheels":
            assets["wheels"].append(path)
            theme_name = Path(path).stem
            local_files = list((THEME_SAVE_PATH / "steering_wheels").glob(f"{theme_name}.*"))
            if local_files and size > 0:
              local_size = self.theme_sizes.get("wheels", {}).get(theme_name)
              if local_size != size:
                self.download_theme("steering_wheels", theme_name, THEME_COMPONENT_PARAMS["steering_wheels"], frogpilot_toggles)

          elif branch == "Distance-Icons":
            component_name = "distance_icons"
            theme_name = Path(path).stem
            assets["themes"].setdefault(theme_name, set()).add(component_name)

            local_path = THEME_SAVE_PATH / "theme_packs" / theme_name / component_name
            if local_path.exists() and size > 0:
              local_size = self.theme_sizes.get("themes", {}).get(theme_name, {}).get(component_name)
              if local_size != size:
                self.download_theme(component_name, theme_name, THEME_COMPONENT_PARAMS[component_name], frogpilot_toggles)

      branch = "Themes"
      for item in list_files(branch):
        if item.get("type") not in ("file", "blob") or "/" not in item["path"]:
          continue

        expected_size = file_size(branch, item["path"], item.get("size", 0))

        theme_name, sub_path = item["path"].split("/", 1)
        theme_path = sub_path.lower()

        for key in ("colors", "icons", "signals", "sounds"):
          if key in theme_path:
            assets["themes"].setdefault(theme_name, set()).add(key)

            local_path = THEME_SAVE_PATH / "theme_packs" / theme_name / key
            if local_path.exists():
              local_size = self.theme_sizes.get("themes", {}).get(theme_name, {}).get(key)
              if local_size != expected_size:
                print(f"{key} {theme_name} is outdated, redownloading...")
                self.download_theme(key, theme_name, THEME_COMPONENT_PARAMS[key], frogpilot_toggles)
            break

      assets["themes"] = {key: sorted(list(value)) for key, value in assets["themes"].items()}
      assets["wheels"].sort()
      return assets

    except requests.exceptions.RequestException as error:
      print(f"Request failed: {error}")
      handle_request_error(f"Failed to fetch theme sizes from {'GitHub' if is_github else 'GitLab'}: {error}", None, None, None)
      return {}

  @staticmethod
  def format_name(name, component):
    base = Path(name).stem
    creator = ""
    if "~" in base:
      base, creator = base.split("~", 1)

    parts = base.replace("_", " ").replace("-", " ").split()
    display = " ".join(part.capitalize() for part in parts)

    if creator:
      return f"{display} - by: {creator}"
    return display

  @staticmethod
  def get_full_themes():
    theme_packs_path = THEME_SAVE_PATH / "theme_packs"
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

  def handle_verification_failure(self, extension, theme_component, theme_name, asset_param, theme_path, download_path, frogpilot_toggles):
    if theme_component == "distance_icons":
      download_link = f"{GITLAB_URL}/Distance-Icons/{theme_name}"
    elif theme_component == "steering_wheels":
      download_link = f"{GITLAB_URL}/Steering-Wheels/{theme_name}"
    else:
      download_link = f"{GITLAB_URL}/Themes/{theme_name}/{theme_component}"

    delete_file(theme_path)

    theme_url = download_link + extension
    print(f"Downloading theme from GitLab: {theme_name}")
    download_file(CANCEL_DOWNLOAD_PARAM, theme_path, DOWNLOAD_PROGRESS_PARAM, theme_url, asset_param, self.session)

    if verify_download(theme_path, theme_url, self.session):
      print(f"Theme {theme_name} downloaded and verified successfully from GitLab!")
      self.update_theme_size(theme_component, theme_name, theme_path.stat().st_size)

      if extension == ".zip":
        params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Unpacking theme...")
        extract_zip(theme_path, download_path)

      params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
      params_memory.remove(asset_param)

      self.downloading_theme = False

      self.update_themes(frogpilot_toggles)
      return True

    handle_error(None, "Download failed...", "Download failed...", asset_param, DOWNLOAD_PROGRESS_PARAM)
    self.downloading_theme = False
    return False

  @staticmethod
  def is_within_week_of(target_date, current_date):
    start_of_week = target_date - timedelta(days=target_date.weekday())
    return start_of_week <= current_date < target_date

  @staticmethod
  def randomize_distance_icons(available_themes, selected_theme):
    theme_packs_path = THEME_SAVE_PATH / "theme_packs"
    if not theme_packs_path.exists():
      return "stock"

    candidates = []
    for theme_pack in theme_packs_path.iterdir():
      if not theme_pack.is_dir():
        continue

      distance_icons_dir = theme_pack / "distance_icons"
      if not distance_icons_dir.is_dir():
        continue

      icon_name = theme_pack.name.lower()

      theme_association = [theme for theme in available_themes if theme.replace("-animated", "") in icon_name]
      if theme_association and selected_theme not in icon_name:
        continue

      weight = 5 if selected_theme in icon_name else 1
      candidates.extend([theme_pack.name] * weight)

    return random.choice(candidates) if candidates else "stock"

  @staticmethod
  def randomize_theme_asset(available_themes):
    if not available_themes:
      return "stock"

    return random.choice(available_themes)

  @staticmethod
  def randomize_wheel_image(available_themes, selected_theme):
    steering_wheels_path = THEME_SAVE_PATH / "steering_wheels"
    if not steering_wheels_path.exists():
      return "stock"

    candidates = []
    for wheel_file in steering_wheels_path.iterdir():
      if not wheel_file.is_file():
        continue

      name = wheel_file.stem.lower()

      theme_association = [theme for theme in available_themes if theme.replace("-animated", "") in name]
      if theme_association and selected_theme not in name:
        continue

      weight = 5 if selected_theme in name else 1
      candidates.extend([wheel_file.stem] * weight)

    return random.choice(candidates) if candidates else "stock"

  def update_active_theme(self, time_validated, frogpilot_toggles, boot_run=False, randomize_theme=False):
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
    elif (boot_run or randomize_theme) and frogpilot_toggles.random_themes:
      available_themes = self.get_full_themes()
      selected_theme = self.randomize_theme_asset(available_themes)

      asset_mappings = {
        "color_scheme": ("colors", selected_theme.replace("-animated", "")),
        "distance_icons": ("distance_icons", self.randomize_distance_icons(available_themes, selected_theme.replace("-animated", ""))),
        "icon_pack": ("icons", selected_theme),
        "sound_pack": ("sounds", selected_theme.replace("-animated", "")),
        "turn_signal_pack": ("signals", selected_theme.replace("-animated", "")),
        "wheel_image": ("wheel_image", self.randomize_wheel_image(available_themes, selected_theme.replace("-animated", "")))
      }

    elif not frogpilot_toggles.random_themes:
      asset_mappings = {
        "color_scheme": ("colors", frogpilot_toggles.color_scheme),
        "distance_icons": ("distance_icons", frogpilot_toggles.distance_icons),
        "icon_pack": ("icons", frogpilot_toggles.icon_pack),
        "sound_pack": ("sounds", frogpilot_toggles.sound_pack),
        "turn_signal_pack": ("signals", frogpilot_toggles.signal_icons),
        "wheel_image": ("wheel_image", frogpilot_toggles.wheel_image)
      }
    else:
      return

    if asset_mappings != self.previous_asset_mappings:
      for asset, (asset_type, current_value) in asset_mappings.items():
        print(f"Updating {asset}: {asset_type} with value {current_value}")

        if asset_type == "wheel_image":
          self.update_wheel_image(current_value, boot_run=boot_run)
        else:
          self.update_theme_asset(asset_type, current_value, boot_run=boot_run)

      self.previous_asset_mappings = asset_mappings

      self.theme_updated = True

  def update_holiday(self):
    current_date = date.today()

    holidays = self.get_holiday_theme_dates(current_date.year)
    for holiday, holiday_date in holidays.items():
      if (holiday.endswith("_week") and self.is_within_week_of(holiday_date, current_date)) or (current_date == holiday_date):
        return holiday

    return "stock"

  def update_theme_asset(self, asset_type, theme, boot_run=False):
    save_location = ACTIVE_THEME_PATH / asset_type

    if self.holiday_theme != "stock":
      asset_location = HOLIDAY_THEME_PATH / self.holiday_theme / asset_type
    elif theme in HOLIDAY_SLUGS:
      asset_location = HOLIDAY_THEME_PATH / theme / asset_type
    elif f"{theme}_week" in HOLIDAY_SLUGS:
      asset_location = HOLIDAY_THEME_PATH / f"{theme}_week" / asset_type
    else:
      asset_location = THEME_SAVE_PATH / "theme_packs" / theme / asset_type

    if not asset_location.exists() or theme == "stock":
      asset_location = STOCKOP_THEME_PATH / asset_type
      print(f"Using the stock {asset_type[:-1]} instead")

    delete_file(save_location, print_error=not boot_run)

    save_location.parent.mkdir(parents=True, exist_ok=True)
    save_location.symlink_to(asset_location, target_is_directory=True)
    print(f"Linked {save_location} to {asset_location}")

  def update_theme_params(self, downloadable_colors, downloadable_distance_icons, downloadable_icons, downloadable_signals, downloadable_sounds, downloadable_wheels):
    def update_param(key, assets, subfolder):
      if subfolder == "steering_wheels":
        themes_path = THEME_SAVE_PATH / subfolder
        existing_assets = {self.format_name(item.name, "steering_wheels") for item in themes_path.glob("*") if item.is_file()}
      else:
        themes_path = THEME_SAVE_PATH / "theme_packs"
        existing_assets = {self.format_name(item.parent.name, subfolder) for item in themes_path.glob(f"*/{subfolder}") if item.is_dir()}

      params.put(key, ",".join(sorted(set(assets) - existing_assets)))
      print(f"{key} updated successfully")

    update_param("DownloadableColors", downloadable_colors, "colors")
    update_param("DownloadableDistanceIcons", downloadable_distance_icons, "distance_icons")
    update_param("DownloadableIcons", downloadable_icons, "icons")
    update_param("DownloadableSignals", downloadable_signals, "signals")
    update_param("DownloadableSounds", downloadable_sounds, "sounds")
    update_param("DownloadableWheels", downloadable_wheels, "steering_wheels")

    downloaded_themes = {}
    for theme_dir in (THEME_SAVE_PATH / "theme_packs").iterdir():
      components = []
      for component in ["colors", "distance_icons", "icons", "signals", "sounds"]:
        if (theme_dir / component).is_dir():
          components.append(component)

      if components:
        theme_name = self.format_name(theme_dir.name, "theme_packs")
        downloaded_themes[theme_name] = sorted(components)

    downloaded_wheels = []
    for wheel_file in (THEME_SAVE_PATH / "steering_wheels").iterdir():
      if wheel_file.is_file():
        downloaded_wheels.append(self.format_name(wheel_file.name, "steering_wheels"))

    params.put("ThemesDownloaded", json.dumps({
      "themes": {key: downloaded_themes[key] for key in sorted(downloaded_themes)},
      "steering_wheels": sorted(downloaded_wheels)
    }))

    print("ThemesDownloaded updated successfully")

  def update_theme_size(self, theme_component, theme_name, file_size):
    if theme_component == "steering_wheels":
      key = "wheels"
    else:
      key = "themes"

    if key not in self.theme_sizes:
      self.theme_sizes[key] = {}

    if key == "wheels":
      self.theme_sizes[key][theme_name] = file_size
    else:
      if theme_name not in self.theme_sizes[key]:
        self.theme_sizes[key][theme_name] = {}
      self.theme_sizes[key][theme_name][theme_component] = file_size

    update_json_file(self.theme_sizes_path, self.theme_sizes)

  def update_themes(self, frogpilot_toggles, boot_run=False):
    if self.downloading_theme:
      return

    repo_url = get_repository_url(self.session)
    if repo_url is None:
      print("GitHub and GitLab are offline...")
      return

    assets = self.fetch_assets(repo_url, frogpilot_toggles)
    if not assets:
      return

    downloadable_colors = []
    downloadable_distance_icons = []
    downloadable_icons = []
    downloadable_signals = []
    downloadable_sounds = []

    for theme, available_assets in assets["themes"].items():
      theme_name = self.format_name(theme, "theme_packs")
      print(f"Theme found: {theme_name}")

      if "colors" in available_assets:
        downloadable_colors.append(theme_name)
      if "distance_icons" in available_assets:
        downloadable_distance_icons.append(theme_name)
      if "icons" in available_assets:
        downloadable_icons.append(theme_name)
      if "signals" in available_assets:
        downloadable_signals.append(theme_name)
      if "sounds" in available_assets:
        downloadable_sounds.append(theme_name)

    downloadable_wheels = [self.format_name(wheel, "steering_wheels") for wheel in assets["wheels"]]

    print(f"Downloadable Colors: {downloadable_colors}")
    print(f"Downloadable Icons: {downloadable_icons}")
    print(f"Downloadable Signals: {downloadable_signals}")
    print(f"Downloadable Sounds: {downloadable_sounds}")
    print(f"Downloadable Distance Icons: {downloadable_distance_icons}")
    print(f"Downloadable Wheels: {downloadable_wheels}")

    if boot_run:
      self.validate_themes(downloadable_colors, downloadable_distance_icons, downloadable_icons, downloadable_signals, downloadable_sounds, downloadable_wheels, frogpilot_toggles)

    self.update_theme_params(downloadable_colors, downloadable_distance_icons, downloadable_icons, downloadable_signals, downloadable_sounds, downloadable_wheels)

  def update_wheel_image(self, image, boot_run=False, random_event=False):
    wheel_save_location = ACTIVE_THEME_PATH / "steering_wheel"

    if self.holiday_theme != "stock":
      wheel_location = HOLIDAY_THEME_PATH / self.holiday_theme / "steering_wheel"
    elif random_event:
      wheel_location = RANDOM_EVENTS_PATH / "steering_wheels"
    elif image == "stock":
      wheel_location = STOCKOP_THEME_PATH / "steering_wheel"
    elif image in HOLIDAY_SLUGS:
      wheel_location = HOLIDAY_THEME_PATH / image / "steering_wheel"
    elif f"{image}_week" in HOLIDAY_SLUGS:
      wheel_location = HOLIDAY_THEME_PATH / f"{image}_week" / "steering_wheel"
    else:
      wheel_location = THEME_SAVE_PATH / "steering_wheels"

    if not wheel_location.exists():
      wheel_location = STOCKOP_THEME_PATH / "steering_wheel"
      print("Using the stock steering wheel instead")

    delete_file(wheel_save_location, print_error=not boot_run)
    wheel_save_location.mkdir(parents=True, exist_ok=True)

    image_name = image.replace(" ", "_").lower()
    matching_files = [images for images in wheel_location.iterdir() if images.stem.lower() in {image_name, "wheel"}]
    if matching_files:
      source_file = matching_files[0]
      destination_file = wheel_save_location / f"wheel{source_file.suffix}"
      destination_file.symlink_to(source_file)
      print(f"Linked {destination_file} to {source_file}")

  def validate_themes(self, downloadable_colors, downloadable_distance_icons, downloadable_icons, downloadable_signals, downloadable_sounds, downloadable_wheels, frogpilot_toggles):
    downloaded_data = json.loads(params.get("ThemesDownloaded") or "{}")

    for display_name, components in downloaded_data.get("themes", {}).items():
      raw_name = display_name.lower().replace(" ", "_").replace("(", "").replace(")", "")
      theme_folder_name = raw_name.replace("_animated", "-animated")

      for component in components:
        component_path = THEME_SAVE_PATH / "theme_packs" / theme_folder_name / component
        if not component_path.is_dir() or not any(component_path.iterdir()):
          print(f"Missing or empty component '{component}' for theme '{theme_folder_name}'. Downloading...")
          self.download_theme(component, theme_folder_name, THEME_COMPONENT_PARAMS.get(component), frogpilot_toggles)
          self.update_active_theme(True, frogpilot_toggles)

    wheels_path = THEME_SAVE_PATH / "steering_wheels"
    for display_name in downloaded_data.get("steering_wheels", []):
      file_stem = display_name.replace(" ", "_").lower()
      matching_files = list(wheels_path.glob(f"{file_stem}.*"))
      if not matching_files:
        print(f"Missing steering wheel '{display_name}'. Downloading...")
        self.download_theme("steering_wheels", file_stem, THEME_COMPONENT_PARAMS["steering_wheels"], frogpilot_toggles)
        self.update_active_theme(True, frogpilot_toggles)

    for dir_path in THEME_SAVE_PATH.glob("**/*"):
      if dir_path.is_dir() and not any(dir_path.iterdir()):
        print(f"Deleting empty folder: {dir_path}")
        delete_file(dir_path)
      elif dir_path.is_file() and dir_path.name.startswith("tmp"):
        print(f"Deleting temp file: {dir_path}")
        delete_file(dir_path)

    print("Theme validation complete.")
