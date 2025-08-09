#!/usr/bin/env python3
import glob
import json
import random
import requests
import shutil

from datetime import date, timedelta
from dateutil import easter
from pathlib import Path

from openpilot.frogpilot.assets.download_functions import GITLAB_URL, download_file, get_repository_url, handle_error, handle_request_error, verify_download
from openpilot.frogpilot.common.frogpilot_utilities import delete_file, extract_zip
from openpilot.frogpilot.common.frogpilot_variables import ACTIVE_THEME_PATH, RANDOM_EVENTS_PATH, THEME_SAVE_PATH, params, params_memory, update_frogpilot_toggles

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

def calculate_thanksgiving(year):
  november_first = date(year, 11, 1)
  days_to_thursday = (3 - november_first.weekday()) % 7
  first_thursday = november_first + timedelta(days=days_to_thursday)
  return first_thursday + timedelta(days=21)

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
    "thanksgiving_week": calculate_thanksgiving(year),
    "christmas_week": date(year, 12, 21)
  }

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

def randomize_theme_asset(available_themes):
  if not available_themes:
    return "stock"

  return random.choice(available_themes)

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

def update_theme_asset(asset_type, theme, holiday_theme):
  save_location = ACTIVE_THEME_PATH / asset_type

  if holiday_theme != "stock":
    asset_location = HOLIDAY_THEME_PATH / holiday_theme / asset_type
  elif theme in HOLIDAY_SLUGS or f"{theme}_week" in HOLIDAY_SLUGS:
    asset_location = HOLIDAY_THEME_PATH / theme / asset_type
  else:
    asset_location = THEME_SAVE_PATH / "theme_packs" / theme / asset_type

  if not asset_location.exists() or theme == "stock":
    if (STOCKOP_THEME_PATH / asset_type).is_dir():
      asset_location = STOCKOP_THEME_PATH / asset_type
      print(f"Using the stock {asset_type[:-1]} instead")
    else:
      if save_location.exists() or save_location.is_symlink():
        if save_location.is_symlink() or save_location.is_file():
          save_location.unlink()
        elif save_location.is_dir():
          shutil.rmtree(save_location)
      print(f"Using the stock {asset_type[:-1]} instead")
      return

  if save_location.exists() or save_location.is_symlink():
    if save_location.is_symlink() or save_location.is_file():
      save_location.unlink()
    elif save_location.is_dir():
      shutil.rmtree(save_location)

  save_location.parent.mkdir(parents=True, exist_ok=True)
  save_location.symlink_to(asset_location, target_is_directory=True)
  print(f"Linked {save_location} to {asset_location}")

def update_wheel_image(image, holiday_theme="stock", random_event=True):
  wheel_save_location = ACTIVE_THEME_PATH / "steering_wheel"

  if holiday_theme != "stock":
    wheel_location = HOLIDAY_THEME_PATH / holiday_theme / "steering_wheel"
  elif random_event:
    wheel_location = RANDOM_EVENTS_PATH / "steering_wheels"
  elif image == "stock":
    wheel_location = STOCKOP_THEME_PATH / "steering_wheel"
  elif image in HOLIDAY_SLUGS or f"{image}_week" in HOLIDAY_SLUGS:
    wheel_location = HOLIDAY_THEME_PATH / image / "steering_wheel"
  else:
    wheel_location = THEME_SAVE_PATH / "steering_wheels"

  if not wheel_location.exists():
    wheel_location = STOCKOP_THEME_PATH / "steering_wheel"
    print("Using the stock steering wheel instead")

  if wheel_save_location.exists():
    if wheel_save_location.is_symlink():
      wheel_save_location.unlink()
    elif wheel_save_location.is_dir():
      shutil.rmtree(wheel_save_location)

  wheel_save_location.mkdir(parents=True, exist_ok=True)

  image_name = image.replace(" ", "_").lower()
  matching_files = [images for images in wheel_location.iterdir() if images.stem.lower() in {image_name, "wheel"}]
  if matching_files:
    source_file = matching_files[0]
    destination_file = wheel_save_location / f"wheel{source_file.suffix}"

    if destination_file.exists():
      destination_file.unlink()
    destination_file.symlink_to(source_file)
    print(f"Linked {destination_file} to {source_file}")

class ThemeManager:
  def __init__(self):
    self.downloading_theme = False
    self.theme_updated = False

    self.holiday_theme = "stock"

    self.previous_asset_mappings = {}

  @staticmethod
  def is_within_week_of(target_date, current_date):
    start_of_week = target_date - timedelta(days=target_date.weekday())
    return start_of_week <= current_date < target_date

  def update_holiday(self):
    current_date = date.today()

    holidays = get_holiday_theme_dates(current_date.year)
    for holiday, holiday_date in holidays.items():
      if (holiday.endswith("_week") and self.is_within_week_of(holiday_date, current_date)) or (current_date == holiday_date):
        return holiday

    return "stock"

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
      available_themes = get_full_themes()
      selected_theme = randomize_theme_asset(available_themes)

      asset_mappings = {
        "color_scheme": ("colors", selected_theme.replace("-animated", "")),
        "distance_icons": ("distance_icons", randomize_distance_icons(available_themes, selected_theme.replace("-animated", ""))),
        "icon_pack": ("icons", selected_theme),
        "sound_pack": ("sounds", selected_theme.replace("-animated", "")),
        "turn_signal_pack": ("signals", selected_theme.replace("-animated", "")),
        "wheel_image": ("wheel_image", randomize_wheel_image(available_themes, selected_theme.replace("-animated", "")))
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
          update_wheel_image(current_value, self.holiday_theme, random_event=False)
        else:
          update_theme_asset(asset_type, current_value, self.holiday_theme)

      self.previous_asset_mappings = asset_mappings

      self.theme_updated = True

  @staticmethod
  def handle_verification_failure(ext, theme_component, theme_name, theme_param, theme_path, download_path):
    if theme_component == "steering_wheels":
      download_link = f"{GITLAB_URL}/Steering-Wheels/{theme_name}"
    else:
      download_link = f"{GITLAB_URL}/Themes/{theme_name}/{theme_component}"

    if theme_path.is_file():
      delete_file(theme_path)

    theme_url = download_link + ext
    print(f"Downloading theme from GitLab: {theme_name}")
    download_file(CANCEL_DOWNLOAD_PARAM, theme_path, DOWNLOAD_PROGRESS_PARAM, theme_url, theme_param, params_memory)

    if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
      handle_error(None, "Download cancelled...", "Download cancelled...", theme_param, DOWNLOAD_PROGRESS_PARAM, params_memory)
      self.downloading_theme = False
      return

    if verify_download(theme_path, theme_url):
      print(f"Theme {theme_name} downloaded and verified successfully from GitLab!")
      if ext == ".zip":
        params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Unpacking theme...")
        extract_zip(theme_path, download_path)
      params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
      return True
    else:
      return False

  def download_theme(self, theme_component, theme_name, theme_param):
    self.downloading_theme = True

    repo_url = get_repository_url()
    if not repo_url:
      handle_error(None, "GitHub and GitLab are offline...", "Repository unavailable", theme_param, DOWNLOAD_PROGRESS_PARAM, params_memory)
      self.downloading_theme = False
      return

    if theme_component == "steering_wheels":
      download_link = f"{repo_url}/Steering-Wheels/{theme_name}"
      download_path = THEME_SAVE_PATH / theme_component / theme_name
      extensions = [".gif", ".png"]
    else:
      download_link = f"{repo_url}/Themes/{theme_name}/{theme_component}"
      download_path = THEME_SAVE_PATH / "theme_packs" / theme_name / theme_component
      extensions = [".zip"]

    for ext in extensions:
      theme_path = download_path.with_suffix(ext)
      if theme_path.is_file():
        delete_file(theme_path)

      theme_url = download_link + ext
      print(f"Downloading theme from GitHub: {theme_name}")
      download_file(CANCEL_DOWNLOAD_PARAM, theme_path, DOWNLOAD_PROGRESS_PARAM, theme_url, theme_param, params_memory)

      if params_memory.get_bool(CANCEL_DOWNLOAD_PARAM):
        handle_error(None, "Download cancelled...", "Download cancelled...", theme_param, DOWNLOAD_PROGRESS_PARAM, params_memory)
        self.downloading_theme = False
        return

      if verify_download(theme_path, theme_url):
        print(f"Theme {theme_name} downloaded and verified successfully from GitHub!")
        if ext == ".zip":
          params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Unpacking theme...")
          extract_zip(theme_path, download_path)
        params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
        self.downloading_theme = False
        return
      elif self.handle_verification_failure(ext, theme_component, theme_name, theme_param, theme_path, download_path):
        self.downloading_theme = False
        return

    handle_error(download_path, "Download failed...", "Download failed...", theme_param, DOWNLOAD_PROGRESS_PARAM, params_memory)
    self.downloading_theme = False

  @staticmethod
  def fetch_assets(repo_url):
    repo = "FrogAi/FrogPilot-Resources"
    branches = ["Distance-Icons", "Steering-Wheels", "Themes"]

    assets = {
      "themes": {},
      "wheels": []
    }

    try:
      for branch in branches:
        if "github" in repo_url:
          api_url = f"https://api.github.com/repos/{repo}/git/trees/{branch}?recursive=1"
        elif "gitlab" in repo_url:
          api_url = f"https://gitlab.com/api/v4/projects/{repo.replace('/', '%2F')}/repository/tree?ref={branch}&recursive=true"
        else:
          print(f"Unsupported repository URL: {repo_url}")
          return assets

        print(f"Fetching assets from branch '{branch}': {api_url}")
        response = requests.get(api_url, timeout=10)
        response.raise_for_status()
        content = response.json()

        if "github" in repo_url:
          content = content.get("tree", [])

        for item in content:
          if item["type"] != "blob":
            continue

          if branch == "Steering-Wheels":
            assets["wheels"].append(item["path"])
          elif branch == "Themes":
            theme_name = item["path"].split("/")[0]
            assets["themes"].setdefault(theme_name, set())

            item_path = item["path"].lower()
            if "colors" in item_path:
              assets["themes"][theme_name].add("colors")
            elif "distance_icons" in item_path:
              assets["themes"][theme_name].add("distance_icons")
            elif "icons" in item_path:
              assets["themes"][theme_name].add("icons")
            elif "signals" in item_path:
              assets["themes"][theme_name].add("signals")
            elif "sounds" in item_path:
              assets["themes"][theme_name].add("sounds")

      return {**assets, "themes": {k: list(v) for k, v in assets["themes"].items()}}
    except requests.exceptions.RequestException as error:
      handle_request_error(f"Failed to fetch theme sizes from {'GitHub' if 'github' in repo_url else 'GitLab'}: {error}", None, None, None, None)
      return {}

  def update_theme_params(self, downloadable_colors, downloadable_distance_icons, downloadable_icons, downloadable_signals, downloadable_sounds, downloadable_wheels):
    def update_param(key, assets, subfolder):
      if subfolder == "steering_wheels":
        themes_path = THEME_SAVE_PATH / subfolder
        existing_assets = {item.stem.replace("_", " ").title() for item in themes_path.glob("*") if item.is_file()}
      else:
        themes_path = THEME_SAVE_PATH / "theme_packs"
        existing_assets = {item.parent.name.replace("_", " ").title() for item in themes_path.glob(f"*/{subfolder}") if item.is_dir()}
      params.put(key, ",".join(sorted(set(assets) - existing_assets)))
      print(f"{key} updated successfully")

    update_param("DownloadableColors", downloadable_colors, "colors")
    update_param("DownloadableDistanceIcons", downloadable_distance_icons, "distance_icons")
    update_param("DownloadableIcons", downloadable_icons, "icons")
    update_param("DownloadableSignals", downloadable_signals, "signals")
    update_param("DownloadableSounds", downloadable_sounds, "sounds")
    update_param("DownloadableWheels", downloadable_wheels, "steering_wheels")

  def validate_themes(self, downloadable_colors, downloadable_distance_icons, downloadable_icons, downloadable_signals, downloadable_sounds, downloadable_wheels, frogpilot_toggles):
    asset_mappings = {
      "CustomColors": ("colors", frogpilot_toggles.color_scheme, downloadable_colors),
      "CustomDistanceIcons": ("distance_icons", frogpilot_toggles.distance_icons, downloadable_distance_icons),
      "CustomIcons": ("icons", frogpilot_toggles.icon_pack, downloadable_icons),
      "CustomSounds": ("sounds", frogpilot_toggles.sound_pack, downloadable_sounds),
      "CustomSignals": ("signals", frogpilot_toggles.signal_icons, downloadable_signals),
      "WheelIcon": ("steering_wheels", frogpilot_toggles.wheel_image, downloadable_wheels)
    }

    for theme_param, (theme_component, theme_name, downloadable_list) in asset_mappings.items():
      if not downloadable_list:
        continue

      if theme_name.lower() in {"none", "stock"}:
        continue

      if theme_component == "steering_wheels":
        theme_path = THEME_SAVE_PATH / "steering_wheels" / theme_name
        matching_files = list(theme_path.parent.glob(f"{theme_name}.*"))
        if not matching_files:
          print(f"  {theme_name} for {theme_component} not found. Downloading...")
          self.download_theme(theme_component, theme_name, theme_param)
          update_frogpilot_toggles()
        elif theme_name.replace("_", " ").split(".")[0].title() not in downloadable_list:
          if theme_path.exists():
            print(f"{theme_name} for {theme_component} is outdated. Deleting...")
            delete_file(theme_path)
          continue
      else:
        theme_path = THEME_SAVE_PATH / "theme_packs" / theme_name / theme_component
        if not theme_path.exists():
          print(f"  {theme_name} for {theme_component} not found. Downloading...")
          self.download_theme(theme_component, theme_name, theme_param)
          update_frogpilot_toggles()
        elif theme_name.replace("_", " ").split(".")[0].title() not in downloadable_list:
          if theme_path.exists():
            print(f"{theme_name} for {theme_component} is outdated. Deleting...")
            delete_file(theme_path)
          continue

    for dir_path in THEME_SAVE_PATH.glob("**/*"):
      if dir_path.is_dir() and not any(dir_path.iterdir()):
        print(f"Deleting empty folder: {dir_path}")
        dir_path.rmdir()
      elif dir_path.is_file() and dir_path.name.startswith("tmp"):
        print(f"Deleting temp file: {dir_path}")
        dir_path.unlink()

    print("Theme validation complete.")

  def update_themes(self, frogpilot_toggles, boot_run=False):
    if self.downloading_theme:
      return

    repo_url = get_repository_url()
    if repo_url is None:
      print("GitHub and GitLab are offline...")
      return

    assets = self.fetch_assets(repo_url)
    if not assets:
      return

    downloadable_colors = []
    downloadable_distance_icons = []
    downloadable_icons = []
    downloadable_signals = []
    downloadable_sounds = []

    for theme, available_assets in assets["themes"].items():
      theme_name = theme.replace("_", " ").split(".")[0].title()
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

    downloadable_wheels = [wheel.replace("_", " ").split(".")[0].title() for wheel in assets["wheels"]]

    print(f"Downloadable Colors: {downloadable_colors}")
    print(f"Downloadable Icons: {downloadable_icons}")
    print(f"Downloadable Signals: {downloadable_signals}")
    print(f"Downloadable Sounds: {downloadable_sounds}")
    print(f"Downloadable Distance Icons: {downloadable_distance_icons}")
    print(f"Downloadable Wheels: {downloadable_wheels}")

    if boot_run:
      self.validate_themes(downloadable_colors, downloadable_distance_icons, downloadable_icons, downloadable_signals, downloadable_sounds, downloadable_wheels, frogpilot_toggles)

    self.update_theme_params(downloadable_colors, downloadable_distance_icons, downloadable_icons, downloadable_signals, downloadable_sounds, downloadable_wheels)
