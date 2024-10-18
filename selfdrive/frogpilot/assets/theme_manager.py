import glob
import os
import re
import requests
import shutil
import zipfile

from datetime import date, timedelta
from dateutil import easter

from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params

from openpilot.selfdrive.frogpilot.assets.download_functions import GITHUB_URL, GITLAB_URL, download_file, get_repository_url, handle_error, handle_request_error, link_valid, verify_download
from openpilot.selfdrive.frogpilot.frogpilot_functions import ACTIVE_THEME_PATH, THEME_SAVE_PATH, update_frogpilot_toggles

CANCEL_DOWNLOAD_PARAM = "CancelThemeDownload"
DOWNLOAD_PROGRESS_PARAM = "ThemeDownloadProgress"

def update_theme_asset(asset_type, theme, holiday_theme, params):
  save_location = os.path.join(ACTIVE_THEME_PATH, asset_type)

  if holiday_theme:
    asset_location = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "holiday_themes", holiday_theme, asset_type)
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
      asset_location = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "stock_theme", asset_type)
      print("Using the stock icon pack instead")
    else:
      if os.path.exists(save_location):
        shutil.rmtree(save_location)
      print(f"Using the stock {asset_type[:-1]} instead")
      return
  elif asset_type == "colors":
    params.put_bool("UseStockColors", False)

  if os.path.exists(save_location):
    shutil.rmtree(save_location)

  shutil.copytree(asset_location, save_location)
  print(f"Copied {asset_location} to {save_location}")

def update_wheel_image(image, holiday_theme=None, random_event=True):
  wheel_save_location = os.path.join(ACTIVE_THEME_PATH, "steering_wheel")

  if holiday_theme:
    wheel_location = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "holiday_themes", holiday_theme, "steering_wheel")
  elif random_event:
    wheel_location = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "random_events", "icons")
  elif image == "stock":
    wheel_location = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "stock_theme", "steering_wheel")
  else:
    wheel_location = os.path.join(THEME_SAVE_PATH, "steering_wheels")

  if os.path.exists(wheel_save_location):
    shutil.rmtree(wheel_save_location)
  os.makedirs(wheel_save_location, exist_ok=True)

  image_name = image.replace(" ", "_").lower()
  for filename in os.listdir(wheel_location):
    if os.path.splitext(filename)[0].lower() in (image_name, "wheel"):
      source_file = os.path.join(wheel_location, filename)
      destination_file = os.path.join(wheel_save_location, f"wheel{os.path.splitext(filename)[1]}")
      shutil.copy2(source_file, destination_file)
      print(f"Copied {source_file} to {destination_file}")
      break

class ThemeManager:
  def __init__(self):
    self.params = Params()
    self.params_memory = Params("/dev/shm/params")

    self.previous_assets = {}

  @staticmethod
  def calculate_thanksgiving(year):
    november_first = date(year, 11, 1)
    day_of_week = november_first.weekday()
    return november_first + timedelta(days=(3 - day_of_week + 21) % 7 + 21)

  @staticmethod
  def is_within_week_of(target_date, now):
    start_of_week = target_date - timedelta(days=target_date.weekday())
    end_of_week = start_of_week + timedelta(days=6)
    return start_of_week <= now <= end_of_week

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
          self.params.put("CurrentHolidayTheme", holiday)
          self.params_memory.put_bool("UpdateTheme", True)
        return

    if "holiday_theme" in self.previous_assets:
      self.params.remove("CurrentHolidayTheme")
      self.params_memory.put_bool("UpdateTheme", True)
      self.previous_assets.pop("holiday_theme")

  def update_active_theme(self):
    if not os.path.exists(THEME_SAVE_PATH):
      return

    holiday_themes = self.params.get_bool("HolidayThemes")
    current_holiday_theme = self.params.get("CurrentHolidayTheme", encoding='utf-8') if holiday_themes else None

    if not current_holiday_theme and self.params.get_bool("PersonalizeOpenpilot"):
      asset_mappings = {
        "color_scheme": ("colors", self.params.get("CustomColors", encoding='utf-8')),
        "distance_icons": ("distance_icons", self.params.get("CustomDistanceIcons", encoding='utf-8')),
        "icon_pack": ("icons", self.params.get("CustomIcons", encoding='utf-8')),
        "sound_pack": ("sounds", self.params.get("CustomSounds", encoding='utf-8')),
        "turn_signal_pack": ("signals", self.params.get("CustomSignals", encoding='utf-8')),
        "wheel_image": ("wheel_image", self.params.get("WheelIcon", encoding='utf-8'))
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
      if current_value != self.previous_assets.get(asset) or current_holiday_theme != self.previous_assets.get("holiday_theme"):
        print(f"Updating {asset}: {asset_type} with value {current_holiday_theme if current_holiday_theme else current_value}")

        if asset_type == "wheel_image":
          update_wheel_image(current_value, current_holiday_theme, random_event=False)
        else:
          update_theme_asset(asset_type, current_value, current_holiday_theme, self.params)

        self.previous_assets[asset] = current_value

    self.previous_assets["holiday_theme"] = current_holiday_theme
    update_frogpilot_toggles()

  def extract_zip(self, zip_file, extract_path):
    print(f"Extracting {zip_file} to {extract_path}")
    with zipfile.ZipFile(zip_file, 'r') as zip_ref:
      zip_ref.extractall(extract_path)
    os.remove(zip_file)
    print(f"Extraction completed and zip file deleted.")

  def handle_existing_theme(self, theme_name, theme_param):
    print(f"Theme {theme_name} already exists, skipping download...")
    self.params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Theme already exists...")
    self.params_memory.remove(theme_param)

  def handle_verification_failure(self, extentions, theme_component, theme_name, theme_param, download_path):
    if theme_component == "distance_icons":
      download_link = f"{GITLAB_URL}Distance-Icons/{theme_name}"
    elif theme_component == "steering_wheels":
      download_link = f"{GITLAB_URL}Steering-Wheels/{theme_name}"
    else:
      download_link = f"{GITLAB_URL}Themes/{theme_name}/{theme_component}"

    for ext in extentions:
      theme_path = download_path + ext
      theme_url = download_link + ext
      print(f"Downloading theme from GitLab: {theme_name}")
      download_file(CANCEL_DOWNLOAD_PARAM, theme_path, DOWNLOAD_PROGRESS_PARAM, theme_url, theme_param, self.params_memory)

      if verify_download(theme_path, theme_url):
        print(f"Theme {theme_name} downloaded and verified successfully from GitLab!")
        if ext == ".zip":
          self.params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Unpacking theme...")
          self.extract_zip(theme_path, os.path.join(THEME_SAVE_PATH, theme_name))
        self.params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
        self.params_memory.remove(theme_param)
        return True

    handle_error(download_path, "GitLab verification failed", "Verification failed", theme_param, DOWNLOAD_PROGRESS_PARAM, self.params_memory)
    return False

  def download_theme(self, theme_component, theme_name, theme_param):
    repo_url = get_repository_url()
    if not repo_url:
      handle_error(None, "GitHub and GitLab are offline...", "Repository unavailable", theme_param, DOWNLOAD_PROGRESS_PARAM, self.params_memory)
      return

    if theme_component == "distance_icons":
      download_link = f"{repo_url}Distance-Icons/{theme_name}"
      download_path = os.path.join(THEME_SAVE_PATH, theme_component, theme_name)
      extentions = [".zip"]
    elif theme_component == "steering_wheels":
      download_link = f"{repo_url}Steering-Wheels/{theme_name}"
      download_path = os.path.join(THEME_SAVE_PATH, theme_component, theme_name)
      extentions = [".gif", ".png"]
    else:
      download_link = f"{repo_url}Themes/{theme_name}/{theme_component}"
      download_path = os.path.join(THEME_SAVE_PATH, "theme_packs", theme_name, theme_component)
      extentions = [".zip"]

    for ext in extentions:
      theme_path = download_path + ext
      if os.path.isfile(theme_path):
        handle_error(theme_path, "Theme already exists...", "Theme already exists...", theme_param, DOWNLOAD_PROGRESS_PARAM, self.params_memory)
        return

      theme_url = download_link + ext
      print(f"Downloading theme from GitHub: {theme_name}")
      download_file(CANCEL_DOWNLOAD_PARAM, theme_path, DOWNLOAD_PROGRESS_PARAM, theme_url, theme_param, self.params_memory)

      if verify_download(theme_path, theme_url):
        print(f"Theme {theme_name} downloaded and verified successfully from GitHub!")
        if ext == ".zip":
          self.params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Unpacking theme...")
          self.extract_zip(theme_path, download_path)
        self.params_memory.put(DOWNLOAD_PROGRESS_PARAM, "Downloaded!")
        self.params_memory.remove(theme_param)
        return

    self.handle_verification_failure(extentions, theme_component, theme_name, theme_param, download_path)

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
  def fetch_folders(url):
    try:
      response = requests.get(url, timeout=10)
      response.raise_for_status()
      return re.findall(r'href="[^"]*\/tree\/[^"]*\/([^"]*)"', response.text)
    except Exception as error:
      handle_request_error(error, None, None, None, None)
      return []

  def update_theme_params(self, downloadable_colors, downloadable_distance_icons, downloadable_icons, downloadable_signals, downloadable_sounds, downloadable_wheels):
    def filter_existing_assets(assets, subfolder):
      existing_themes = {
        theme.replace('_', ' ').title()
        for theme in os.listdir(os.path.join(THEME_SAVE_PATH, "theme_packs"))
        if os.path.isdir(os.path.join(THEME_SAVE_PATH, "theme_packs", theme, subfolder))
      }
      return sorted(set(assets) - existing_themes)

    self.params.put("DownloadableColors", ','.join(filter_existing_assets(downloadable_colors, "colors")))
    print("Colors list updated successfully.")

    distance_icons_directory = os.path.join(THEME_SAVE_PATH, "distance_icons")
    self.params.put("DownloadableDistanceIcons", ','.join(sorted(set(downloadable_distance_icons) - {
        distance_icons.replace('_', ' ').split('.')[0].title()
        for distance_icons in os.listdir(distance_icons_directory)
      }))
    )

    self.params.put("DownloadableIcons", ','.join(filter_existing_assets(downloadable_icons, "icons")))
    print("Icons list updated successfully.")

    self.params.put("DownloadableSignals", ','.join(filter_existing_assets(downloadable_signals, "signals")))
    print("Signals list updated successfully.")

    self.params.put("DownloadableSounds", ','.join(filter_existing_assets(downloadable_sounds, "sounds")))
    print("Sounds list updated successfully.")

    wheel_directory = os.path.join(THEME_SAVE_PATH, "steering_wheels")
    self.params.put("DownloadableWheels", ','.join(sorted(set(downloadable_wheels) - {
        wheel.replace('_', ' ').split('.')[0].title()
        for wheel in os.listdir(wheel_directory) if wheel != "img_chffr_wheel.png"
      }))
    )

  def validate_themes(self):
    asset_mappings = {
      "CustomColors": "colors",
      "CustomDistanceIcons": "distance_icons",
      "CustomIcons": "icons",
      "CustomSounds": "sounds",
      "CustomSignals": "signals",
      "WheelIcon": "steering_wheels"
    }

    for theme_param, theme_component in asset_mappings.items():
      theme_name = self.params.get(theme_param, encoding='utf-8')
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
        self.update_active_theme()

  def update_themes(self, boot_run=False):
    if not os.path.exists(THEME_SAVE_PATH):
      return

    repo_url = get_repository_url()
    if repo_url is None:
      print("GitHub and GitLab are offline...")
      return

    if boot_run:
      self.validate_themes()

    if repo_url == GITHUB_URL:
      base_url = "https://github.com/FrogAi/FrogPilot-Resources/blob/Themes/"
      distance_icons_files = self.fetch_files("https://github.com/FrogAi/FrogPilot-Resources/blob/Distance-Icons")
      wheel_files = self.fetch_files("https://github.com/FrogAi/FrogPilot-Resources/blob/Steering-Wheels")
    else:
      base_url = "https://gitlab.com/FrogAi/FrogPilot-Resources/-/blob/Themes/"
      distance_icons_files = self.fetch_files("https://github.com/FrogAi/FrogPilot-Resources/blob/Distance-Icons")
      wheel_files = self.fetch_files("https://gitlab.com/FrogAi/FrogPilot-Resources/-/blob/Steering-Wheels")

    theme_folders = self.fetch_folders(base_url)
    downloadable_colors = []
    downloadable_icons = []
    downloadable_signals = []
    downloadable_sounds = []

    for theme in theme_folders:
      theme_name = theme.replace('_', ' ').split('.')[0].title()

      if link_valid(f"{base_url}{theme}/colors.zip"):
        downloadable_colors.append(theme_name)
      if link_valid(f"{base_url}{theme}/icons.zip"):
        downloadable_icons.append(theme_name)
      if link_valid(f"{base_url}{theme}/signals.zip"):
        downloadable_signals.append(theme_name)
      if link_valid(f"{base_url}{theme}/sounds.zip"):
        downloadable_sounds.append(theme_name)

    downloadable_distance_icons = [distance_icons.replace('_', ' ').split('.')[0].title() for distance_icons in distance_icons_files]
    downloadable_wheels = [wheel.replace('_', ' ').split('.')[0].title() for wheel in wheel_files]

    self.update_theme_params(downloadable_colors, downloadable_distance_icons, downloadable_icons, downloadable_signals, downloadable_sounds, downloadable_wheels)
