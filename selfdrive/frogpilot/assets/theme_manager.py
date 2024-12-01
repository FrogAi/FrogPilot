import glob
import json
import os
import re
import requests
import shutil
import zipfile

from datetime import date, timedelta
from dateutil import easter

from openpilot.common.basedir import BASEDIR

from openpilot.selfdrive.frogpilot.assets.download_functions import GITHUB_URL, GITLAB_URL, download_file, get_repository_url, handle_error, handle_request_error, verify_download
from openpilot.selfdrive.frogpilot.frogpilot_variables import ACTIVE_THEME_PATH, RANDOM_EVENTS_PATH, THEME_SAVE_PATH, params, params_memory, update_frogpilot_toggles

CANCEL_DOWNLOAD_PARAM = "CancelThemeDownload"
DOWNLOAD_PROGRESS_PARAM = "ThemeDownloadProgress"

HOLIDAY_THEME_PATH = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "holiday_themes")
STOCKOP_THEME_PATH = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "stock_theme")

def update_theme_asset(asset_type, theme, holiday_theme):
  save_location = os.path.join(ACTIVE_THEME_PATH, asset_type)

  if holiday_theme:
    asset_location = os.path.join(HOLIDAY_THEME_PATH, holiday_theme, asset_type)
  elif asset_type == "distance_icons":
    asset_location = os.path.join(THEME_SAVE_PATH, "distance_icons", theme)
  else:
    asset_location = os.path.join(THEME_SAVE_PATH, "theme_packs", theme, asset_type)

  if not os.path.exists(asset_location):
    if asset_type == "colors":
      params.put_bool("UseStockColors", True)
      print("Using the stock color scheme instead")
      return
    elif asset_type in ("distance_icons", "icons"):
      asset_location = os.path.join(STOCKOP_THEME_PATH, asset_type)
      print("Using the stock icon pack instead")
    else:
      if os.path.exists(save_location):
        if os.path.islink(save_location):
          os.unlink(save_location)
        elif os.path.isdir(save_location):
          shutil.rmtree(save_location)
      print(f"Using the stock {asset_type[:-1]} instead")
      return
  elif asset_type == "colors":
    params.put_bool("UseStockColors", False)

  os.makedirs(os.path.dirname(save_location), exist_ok=True)

  if os.path.exists(save_location):
    if os.path.islink(save_location):
      os.unlink(save_location)
    elif os.path.isdir(save_location):
      shutil.rmtree(save_location)

  os.symlink(asset_location, save_location, target_is_directory=True)
  print(f"Linked {save_location} to {asset_location}")

def update_wheel_image(image, holiday_theme=None, random_event=True):
  wheel_save_location = os.path.join(ACTIVE_THEME_PATH, "steering_wheel")

  if holiday_theme:
    wheel_location = os.path.join(HOLIDAY_THEME_PATH, holiday_theme, "steering_wheel")
  elif random_event:
    wheel_location = os.path.join(RANDOM_EVENTS_PATH, "icons")
  elif image == "stock":
    wheel_location = os.path.join(STOCKOP_THEME_PATH, "steering_wheel")
  else:
    wheel_location = os.path.join(THEME_SAVE_PATH, "steering_wheels")

  if not os.path.exists(wheel_location):
    return

  if os.path.exists(wheel_save_location):
    if os.path.islink(wheel_save_location):
      os.unlink(wheel_save_location)
    elif os.path.isdir(wheel_save_location):
      shutil.rmtree(wheel_save_location)

  os.makedirs(wheel_save_location, exist_ok=True)

  image_name = image.replace(" ", "_").lower()

  matching_files = [
    f for f in os.listdir(wheel_location)
    if os.path.splitext(f)[0].lower() in (image_name, "wheel")
  ]

  if matching_files:
    filename = matching_files[0]
    source_file = os.path.join(wheel_location, filename)
    ext = os.path.splitext(filename)[1]
    destination_file = os.path.join(wheel_save_location, f"wheel{ext}")

    if os.path.exists(destination_file):
      os.unlink(destination_file)

    os.symlink(source_file, destination_file)
    print(f"Linked {destination_file} to {source_file}")


class ThemeManager:
  def __init__(self):
    self.previous_assets = {}

  @staticmethod
  def calculate_thanksgiving(year):
    november_first = date(year, 11, 1)
    days_to_thursday = (3 - november_first.weekday()) % 7
    first_thursday = november_first + timedelta(days=days_to_thursday)
    thanksgiving_date = first_thursday + timedelta(days=21)
    return thanksgiving_date

  @staticmethod
  def is_within_week_of(target_date, now):
    start_of_week = target_date - timedelta(days=target_date.weekday())
    return start_of_week <= now < target_date

  def update_holiday(self):
    now = date.today()
    year = now.year

    holidays = {
      "new_years": date(year, 1, 1),
      "valentines": date(year, 2, 14),
      "st_patricks": date(year, 3, 17),
      "world_frog_day": date(year, 3, 20),
      "april_fools": date(year, 4, 1),
      "easter_week": easter.easter(year),
      "may_the_fourth": date(year, 5, 4),
      "cinco_de_mayo": date(year, 5, 5),
      "fourth_of_july": date(year, 7, 4),
      "halloween_week": date(year, 10, 31),
      "thanksgiving_week": self.calculate_thanksgiving(year),
      "christmas_week": date(year, 12, 25)
    }

    for holiday, holiday_date in holidays.items():
      if (holiday.endswith("_week") and self.is_within_week_of(holiday_date, now)) or (now == holiday_date):
        if holiday != self.previous_assets.get("holiday_theme"):
          params.put("CurrentHolidayTheme", holiday)
          update_frogpilot_toggles()
          self.previous_assets["holiday_theme"] = holiday
        return

    if "holiday_theme" in self.previous_assets:
      params.remove("CurrentHolidayTheme")
      update_frogpilot_toggles()
      self.previous_assets.pop("holiday_theme")

  def update_active_theme(self, frogpilot_toggles):
    if frogpilot_toggles.current_holiday_theme is not None:
      asset_mappings = {
        "color_scheme": ("colors", frogpilot_toggles.current_holiday_theme),
        "distance_icons": ("distance_icons", frogpilot_toggles.current_holiday_theme),
        "icon_pack": ("icons", frogpilot_toggles.current_holiday_theme),
        "sound_pack": ("sounds", frogpilot_toggles.current_holiday_theme),
        "turn_signal_pack": ("signals", frogpilot_toggles.current_holiday_theme),
        "wheel_image": ("wheel_image", frogpilot_toggles.current_holiday_theme)
      }
    elif frogpilot_toggles.personalize_openpilot:
      asset_mappings = {
        "color_scheme": ("colors", frogpilot_toggles.color_scheme),
        "distance_icons": ("distance_icons", frogpilot_toggles.distance_icons),
        "icon_pack": ("icons", frogpilot_toggles.icon_pack),
        "sound_pack": ("sounds", frogpilot_toggles.sound_pack),
        "turn_signal_pack": ("signals", frogpilot_toggles.signal_icons),
        "wheel_image": ("wheel_image", frogpilot_toggles.wheel_image)
      }
    else:
      asset_mappings = {
        "color_scheme": ("colors", "stock"),
        "distance_icons": ("distance_icons", "stock"),
        "icon_pack": ("icons", "stock"),
        "sound_pack": ("sounds", "stock"),
        "turn_signal_pack": ("signals", "stock"),
        "wheel_image": ("wheel_image", "stock")
      }

    for asset, (asset_type, current_value) in asset_mappings.items():
      if current_value != self.previous_assets.get(asset):
        print(f"Updating {asset}: {asset_type} with value {current_value}")

        if asset_type == "wheel_image":
          update_wheel_image(current_value, frogpilot_toggles.current_holiday_theme, random_event=False)
        else:
          update_theme_asset(asset_type, current_value, frogpilot_toggles.current_holiday_theme)

        self.previous_assets[asset] = current_value

  def extract_zip(self, zip_file, extract_path):
    print(f"Extracting {zip_file} to {extract_path}")
    with zipfile.ZipFile(zip_file, 'r') as zip_ref:
      zip_ref.extractall(extract_path)
    os.remove(zip_file)
    print(f"Extraction completed")

  @staticmethod
  def fetch_files(url):
    try:
      response = requests.get(url, timeout=10)
      response.raise_for_status()
      return [name for name in re.findall(r'href="[^"]*\/blob\/[^"]*\/([^"]*)"', response.text) if name.lower() != "license"]
    except Exception as error:
      handle_request_error(error, None, None, None, None)
      return []

  @staticmethod
  def fetch_assets(repo_url):
    repo = "FrogAi/FrogPilot-Resources"
    branches = ["Themes", "Distance-Icons", "Steering-Wheels"]

    assets = {
      "themes": {},
      "distance_icons": [],
      "wheels": []
    }

    if "github" in repo_url:
      base_api_url = "https://api.github.com/repos"
    elif "gitlab" in repo_url:
      base_api_url = "https://gitlab.com/api/v4/projects"
      repo = repo.replace("/", "%2F")
    else:
      print(f"Unsupported repository URL: {repo_url}")
      return assets

    for branch in branches:
      if "github" in repo_url:
        api_url = f"{base_api_url}/{repo}/git/trees/{branch}?recursive=1"
      elif "gitlab" in repo_url:
        api_url = f"{base_api_url}/{repo}/repository/tree?ref={branch}&recursive=true"

      try:
        print(f"Fetching assets from branch '{branch}': {api_url}")
        response = requests.get(api_url, timeout=10)
        response.raise_for_status()
        content = response.json()

        if "github" in repo_url:
          items = content.get('tree', [])
        elif "gitlab" in repo_url:
          items = content

        for item in items:
          if item["type"] != "blob":
            continue

          item_path = item["path"].lower()
          if branch == "Themes":
            theme_name = item["path"].split('/')[0]
            assets["themes"].setdefault(theme_name, set())
            if "icons" in item_path:
              assets["themes"][theme_name].add("icons")
            elif "signals" in item_path:
              assets["themes"][theme_name].add("signals")
            elif "sounds" in item_path:
              assets["themes"][theme_name].add("sounds")
            else:
              assets["themes"][theme_name].add("colors")

          elif branch == "Distance-Icons":
            assets["distance_icons"].append(item["path"])

          elif branch == "Steering-Wheels":
            assets["wheels"].append(item["path"])

      except requests.exceptions.RequestException as error:
        print(f"Error occurred when fetching from branch '{branch}': {error}")

    assets["themes"] = {k: list(v) for k, v in assets["themes"].items()}
    return assets

  def handle_existing_theme(self, theme_name, theme_param):
    print(f"Theme {theme_name} already exists, skipping download...")
    params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Theme already exists...")
    params_memory.remove(theme_param)

  def handle_verification_failure(self, extensions, theme_component, theme_name, theme_param, download_path):
    if theme_component == "distance_icons":
      download_link = f"{GITLAB_URL}Distance-Icons/{theme_name}"
    elif theme_component == "steering_wheels":
      download_link = f"{GITLAB_URL}Steering-Wheels/{theme_name}"
    else:
      download_link = f"{GITLAB_URL}Themes/{theme_name}/{theme_component}"

    for ext in extensions:
      theme_path = download_path + ext
      temp_theme_path = f"{os.path.splitext(theme_path)[0]}_temp{ext}"
      theme_url = download_link + ext
      print(f"Downloading theme from GitLab: {theme_name}")
      download_file(CANCEL_DOWNLOAD_PARAM, theme_path, temp_theme_path, DOWNLOAD_PROGRESS_PARAM, theme_url, theme_param, params_memory)

      if verify_download(theme_path, temp_theme_path, theme_url):
        print(f"Theme {theme_name} downloaded and verified successfully from GitLab!")
        if ext == ".zip":
          params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Unpacking theme...")
          self.extract_zip(theme_path, os.path.join(THEME_SAVE_PATH, theme_name))
        params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
        params_memory.remove(theme_param)
        return True

    handle_error(download_path, "GitLab verification failed", "Verification failed", theme_param, DOWNLOAD_PROGRESS_PARAM, params_memory)
    return False

  def download_theme(self, theme_component, theme_name, theme_param):
    repo_url = get_repository_url()
    if not repo_url:
      handle_error(None, "GitHub and GitLab are offline...", "Repository unavailable", theme_param, DOWNLOAD_PROGRESS_PARAM, params_memory)
      return

    if theme_component == "distance_icons":
      download_link = f"{repo_url}Distance-Icons/{theme_name}"
      download_path = os.path.join(THEME_SAVE_PATH, theme_component, theme_name)
      extensions = [".zip"]
    elif theme_component == "steering_wheels":
      download_link = f"{repo_url}Steering-Wheels/{theme_name}"
      download_path = os.path.join(THEME_SAVE_PATH, theme_component, theme_name)
      extensions = [".gif", ".png"]
    else:
      download_link = f"{repo_url}Themes/{theme_name}/{theme_component}"
      download_path = os.path.join(THEME_SAVE_PATH, "theme_packs", theme_name, theme_component)
      extensions = [".zip"]

    for ext in extensions:
      theme_path = download_path + ext
      temp_theme_path = f"{os.path.splitext(theme_path)[0]}_temp{ext}"

      if os.path.isfile(theme_path):
        handle_error(theme_path, "Theme already exists...", "Theme already exists...", theme_param, DOWNLOAD_PROGRESS_PARAM, params_memory)
        return

      theme_url = download_link + ext
      print(f"Downloading theme from GitHub: {theme_name}")
      download_file(CANCEL_DOWNLOAD_PARAM, theme_path, temp_theme_path, DOWNLOAD_PROGRESS_PARAM, theme_url, theme_param, params_memory)

      if verify_download(theme_path, temp_theme_path, theme_url):
        print(f"Theme {theme_name} downloaded and verified successfully from GitHub!")
        if ext == ".zip":
          params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Unpacking theme...")
          self.extract_zip(theme_path, download_path)
        params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
        params_memory.remove(theme_param)
        return

    self.handle_verification_failure(extensions, theme_component, theme_name, theme_param, download_path)

  def update_theme_params(self, downloadable_colors, downloadable_distance_icons, downloadable_icons, downloadable_signals, downloadable_sounds, downloadable_wheels):
    def filter_existing_assets(assets, subfolder):
      existing_themes = {
        theme.replace('_', ' ').title()
        for theme in os.listdir(os.path.join(THEME_SAVE_PATH, "theme_packs"))
        if os.path.isdir(os.path.join(THEME_SAVE_PATH, "theme_packs", theme, subfolder))
      }
      return sorted(set(assets) - existing_themes)

    params.put("DownloadableColors", ','.join(filter_existing_assets(downloadable_colors, "colors")))
    print("Colors list updated successfully")

    distance_icons_directory = os.path.join(THEME_SAVE_PATH, "distance_icons")
    params.put("DownloadableDistanceIcons", ','.join(sorted(set(downloadable_distance_icons) - {
        distance_icons.replace('_', ' ').split('.')[0].title()
        for distance_icons in os.listdir(distance_icons_directory)
      }))
    )

    params.put("DownloadableIcons", ','.join(filter_existing_assets(downloadable_icons, "icons")))
    print("Icons list updated successfully")

    params.put("DownloadableSignals", ','.join(filter_existing_assets(downloadable_signals, "signals")))
    print("Signals list updated successfully")

    params.put("DownloadableSounds", ','.join(filter_existing_assets(downloadable_sounds, "sounds")))
    print("Sounds list updated successfully")

    wheel_directory = os.path.join(THEME_SAVE_PATH, "steering_wheels")
    params.put("DownloadableWheels", ','.join(sorted(set(downloadable_wheels) - {
        wheel.replace('_', ' ').split('.')[0].title()
        for wheel in os.listdir(wheel_directory) if wheel != "img_chffr_wheel.png"
      }))
    )

  def validate_themes(self, frogpilot_toggles):
    asset_mappings = {
      "CustomColors": ("colors", frogpilot_toggles.color_scheme),
      "CustomDistanceIcons": ("distance_icons", frogpilot_toggles.distance_icons),
      "CustomIcons": ("icons", frogpilot_toggles.icon_pack),
      "CustomSounds": ("sounds", frogpilot_toggles.sound_pack),
      "CustomSignals": ("signals", frogpilot_toggles.signal_icons),
      "WheelIcon": ("steering_wheels", frogpilot_toggles.wheel_image)
    }

    for theme_param, (theme_component, theme_name) in asset_mappings.items():
      if not theme_name or theme_name == "stock":
        continue

      if theme_component == "distance_icons":
        theme_path = os.path.join(THEME_SAVE_PATH, theme_component, theme_name)
      elif theme_component == "steering_wheels":
        pattern = os.path.join(THEME_SAVE_PATH, theme_component, theme_name + ".*")
        matching_files = glob.glob(pattern)

        if matching_files:
          theme_path = matching_files[0]
        else:
          theme_path = None
      else:
        theme_path = os.path.join(THEME_SAVE_PATH, "theme_packs", theme_name, theme_component)

      if theme_path is None or not os.path.exists(theme_path):
        print(f"{theme_name} for {theme_component} not found. Downloading...")
        self.download_theme(theme_component, theme_name, theme_param)
        self.previous_assets = {}

  def update_themes(self, frogpilot_toggles, boot_run=False):
    repo_url = get_repository_url()
    if repo_url is None:
      print("GitHub and GitLab are offline...")
      return

    if boot_run:
      self.validate_themes(frogpilot_toggles)

    assets = self.fetch_assets(repo_url)

    downloadable_colors = []
    downloadable_icons = []
    downloadable_signals = []
    downloadable_sounds = []

    for theme, available_assets in assets["themes"].items():
      theme_name = theme.replace('_', ' ').split('.')[0].title()
      print(f"Checking theme: {theme_name}")

      if "colors" in available_assets:
        downloadable_colors.append(theme_name)
      if "icons" in available_assets:
        downloadable_icons.append(theme_name)
      if "signals" in available_assets:
        downloadable_signals.append(theme_name)
      if "sounds" in available_assets:
        downloadable_sounds.append(theme_name)

    downloadable_distance_icons = [distance_icon.replace('_', ' ').split('.')[0].title() for distance_icon in assets["distance_icons"]]
    downloadable_wheels = [wheel.replace('_', ' ').split('.')[0].title() for wheel in assets["wheels"]]

    print(f"Downloadable Colors: {downloadable_colors}")
    print(f"Downloadable Icons: {downloadable_icons}")
    print(f"Downloadable Signals: {downloadable_signals}")
    print(f"Downloadable Sounds: {downloadable_sounds}")
    print(f"Downloadable Distance Icons: {downloadable_distance_icons}")
    print(f"Downloadable Wheels: {downloadable_wheels}")

    self.update_theme_params(downloadable_colors, downloadable_distance_icons, downloadable_icons, downloadable_signals, downloadable_sounds, downloadable_wheels)
