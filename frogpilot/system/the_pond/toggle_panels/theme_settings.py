from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

DOWNLOAD_STOPPED = ("Download cancelled...", "GitHub and GitLab are offline...")

RANDOM_THEMES_NOTICE = ("\"Random Themes\" picks from the built-in Frog theme, the holiday themes when \"Include Holiday Themes\" is on, and themes " +
                        "you've already downloaded, so grab the ones you want it to use!")


def theme_name(context, key):
  params = context.params
  if params.get_bool("RandomThemes") and context.level >= context.tuning_level("RandomThemes"):
    return ""

  theme = params.get(key)
  if theme in frogpilot_variables.BUILTIN_THEMES:
    return frogpilot_variables.BUILTIN_THEMES[theme]
  return utilities.format_theme_name(theme, key == "WheelIcon")


def download_status(context):
  process_state = context.the_pond.sm["frogpilotProcessState"]
  count = process_state.themeDownloadCount
  finished = process_state.themeDownloadFailedCount + process_state.themeDownloadSuccessCount
  progress = process_state.themeDownloadProgress

  if finished == count or progress in DOWNLOAD_STOPPED:
    return "Idle"
  if count > 1 and progress.endswith("%"):
    return f"Downloading {finished + 1} of {count} ({progress})"
  if progress in ("Downloading...", "Downloaded!"):
    return "Downloading..."
  return progress


def distance_icon_pack_visible(context):
  params = context.params
  return (context.level_reason("DistanceIconPack") or context.car_reason(context.has_openpilot_longitudinal) or
          context.setting_reason(params.get_bool("CustomUI"), "CustomUI") or
          context.setting_reason(params.get_bool("OnroadDistanceButton"), "OnroadDistanceButton"))


def random_themes_visible(context):
  return context.level_reason("RandomThemes") or context.setting_reason(context.params.get_bool("CustomThemes"), "CustomThemes")


def random_themes_changed(context, old_value, new_value):
  if new_value:
    context.notice = {"action": None, "cancel": "", "confirm": "Ok", "text": RANDOM_THEMES_NOTICE}
  return False


def startup_alert(context):
  params = context.params
  message = (params.get("StartupMessageTop"), params.get("StartupMessageBottom"))
  if message == (params.get_stock_value("StartupMessageTop"), params.get_stock_value("StartupMessageBottom")):
    return "Stock"
  if message == (params.get_default_value("StartupMessageTop"), params.get_default_value("StartupMessageBottom")):
    return "FrogPilot"
  return "Custom"


ROWS = [
  {
    "children": ("Theme and Appearance", "THEME", "Custom Themes"),
    "description": ("<b>Swap openpilot's colors, icons, sounds, turn signal animations, steering wheel picture, and personality button for a theme " +
                    "pack you download.</b><br><br>You mix and match freely, so one theme's colors can run alongside another's sounds. Packs are " +
                    "made by other drivers, and you can build your own with the \"Theme Maker\" in \"The Pond\"."),
    "key": "CustomThemes",
    "order": 0,
    "path": ("Theme and Appearance", "THEME"),
    "title": "Custom Themes",
    "widget": "manage"
  },
  {
    "description": ("<b>Change the colors openpilot draws on the driving screen, mainly the path ahead of you and the lane " +
                    "lines.</b><br><br>\"Stock\" is openpilot's normal green path with white lane lines. A scheme also recolors the marker on the " +
                    "car ahead and the sidebar boxes, but the road edges are always red and never change. Holiday options match the holiday they are " +
                    "named after, and a downloaded pack brings its own set of colors."),
    "display": lambda context: theme_name(context, "ColorScheme"),
    "key": "ColorScheme",
    "link": "themes",
    "options": ({"label": "DELETE", "value": 0}, {"label": "DOWNLOAD", "value": 1}, {"label": "SELECT", "value": 2}),
    "order": 0,
    "path": ("Theme and Appearance", "THEME", "Custom Themes"),
    "title": "Color Scheme",
    "widget": "theme_asset"
  },
  {
    "description": ("<b>Change the settings, home, and flag buttons on openpilot's sidebar.</b><br><br>\"Stock\" puts the normal three back. A pack " +
                    "replaces all three at once and nothing else, so every other icon openpilot draws stays stock."),
    "display": lambda context: theme_name(context, "IconPack"),
    "key": "IconPack",
    "link": "themes",
    "options": ({"label": "DELETE", "value": 0}, {"label": "DOWNLOAD", "value": 1}, {"label": "SELECT", "value": 2}),
    "order": 1,
    "path": ("Theme and Appearance", "THEME", "Custom Themes"),
    "title": "Icon Pack",
    "widget": "theme_asset"
  },
  {
    "description": ("<b>Change the icons on the driving personality button, the one you tap on the driving screen to switch between Aggressive, " +
                    "Standard, and Relaxed.</b><br><br>Each pack draws four icons: one each for Aggressive, Standard, and Relaxed, plus one that takes " +
                    "over while Traffic Mode is on. This row only appears while that button is switched on under \"Driving Personality Button\"."),
    "display": lambda context: theme_name(context, "DistanceIconPack"),
    "key": "DistanceIconPack",
    "link": "themes",
    "options": ({"label": "DELETE", "value": 0}, {"label": "DOWNLOAD", "value": 1}, {"label": "SELECT", "value": 2}),
    "order": 2,
    "path": ("Theme and Appearance", "THEME", "Custom Themes"),
    "title": "Personality Button",
    "visible": distance_icon_pack_visible,
    "widget": "theme_asset"
  },
  {
    "description": ("<b>Change the chimes openpilot plays for its alerts, like the sound when it starts driving or warns you about " +
                    "something.</b><br><br>\"Stock\" uses openpilot's normal chimes. A pack only replaces the sound files it actually ships and " +
                    "anything it leaves out stays stock, so the holiday packs mostly bring just their own engage and disengage chimes. How loud each " +
                    "one plays is set separately under \"Alert Volumes\" in \"Alerts and Sounds\"."),
    "display": lambda context: theme_name(context, "SoundPack"),
    "key": "SoundPack",
    "link": "themes",
    "options": ({"label": "DELETE", "value": 0}, {"label": "DOWNLOAD", "value": 1}, {"label": "SELECT", "value": 2}),
    "order": 3,
    "path": ("Theme and Appearance", "THEME", "Custom Themes"),
    "title": "Sound Pack",
    "widget": "theme_asset"
  },
  {
    "description": ("<b>Change the steering wheel picture in the top right corner of the driving screen, which spins as openpilot " +
                    "steers.</b><br><br>\"Stock\" uses openpilot's normal wheel and \"None\" hides it completely. Some downloaded wheels are " +
                    "animated."),
    "display": lambda context: theme_name(context, "WheelIcon"),
    "key": "WheelIcon",
    "link": "themes",
    "options": ({"label": "DELETE", "value": 0}, {"label": "DOWNLOAD", "value": 1}, {"label": "SELECT", "value": 2}),
    "order": 4,
    "path": ("Theme and Appearance", "THEME", "Custom Themes"),
    "title": "Steering Wheel",
    "widget": "theme_asset"
  },
  {
    "description": ("<b>Play an animation across the driving screen for as long as your turn signal is on.</b><br><br>The animation runs toward " +
                    "whichever side you signaled. \"None\" turns it off, and each downloaded pack brings its own animation."),
    "display": lambda context: theme_name(context, "SignalAnimation"),
    "key": "SignalAnimation",
    "link": "themes",
    "options": ({"label": "DELETE", "value": 0}, {"label": "DOWNLOAD", "value": 1}, {"label": "SELECT", "value": 2}),
    "order": 5,
    "path": ("Theme and Appearance", "THEME", "Custom Themes"),
    "title": "Turn Signal",
    "widget": "theme_asset"
  },
  {
    "description": "",
    "display": download_status,
    "id": "DownloadStatus",
    "key": None,
    "order": 6,
    "path": ("Theme and Appearance", "THEME", "Custom Themes"),
    "title": "Download Status",
    "widget": "text"
  },
  {
    "description": ("<b>Dress openpilot up for thirteen holidays through the year, swapping the colors, icons, sounds, turn signals, steering wheel, " +
                    "and personality button all at once.</b><br><br>Smaller ones like April Fools or Cinco de Mayo run on the day itself. Easter, " +
                    "Halloween, Thanksgiving, and Christmas start on the Monday of that week and finish on the day, so they last anywhere from one " +
                    "day to a full week depending on where the date falls.<br><br>While a holiday is running it replaces the themes you picked, and " +
                    "your own choices come back the next day."),
    "key": "HolidayThemes",
    "order": 1,
    "path": ("Theme and Appearance", "THEME"),
    "title": "Holiday Themes",
    "widget": "toggle"
  },
  {
    "description": ("<b>Paint the driving path in shifting rainbow colors that scroll faster the quicker you go, like the Rainbow Road track from " +
                    "Mario Kart.</b><br><br>The rainbow replaces whatever color the path normally uses, including one that came with a theme you " +
                    "downloaded. With \"Acceleration Path\" also on, the green and red speed colors take over whenever openpilot speeds up or slows " +
                    "down, so the rainbow only shows while you hold a steady speed."),
    "key": "RainbowPath",
    "order": 2,
    "path": ("Theme and Appearance", "THEME"),
    "title": "Rainbow Path",
    "widget": "toggle"
  },
  {
    "description": ("<b>Play a rare joke alert, with its own sound and sometimes its own steering wheel picture, when something unusual happens on a " +
                    "drive.</b><br><br>Taking off hard, a corner sharper than openpilot can steer through, or a collision warning can each set one " +
                    "off. Every alert can only happen once per drive, a swapped steering wheel goes back to normal after about five seconds, and " +
                    "none of them change how openpilot drives."),
    "key": "RandomEvents",
    "order": 3,
    "path": ("Theme and Appearance", "THEME"),
    "title": "Random Events",
    "widget": "toggle"
  },
  {
    "description": ("<b>Start every drive with a theme picked at random from the built-in Frog theme and the packs you have already " +
                    "downloaded.</b><br><br>A downloaded pack only joins the mix once you have its colors, icons, and sounds, and the same theme can " +
                    "come up twice in a row. With \"Include Holiday Themes\" on, the holiday themes are in the mix too. While this is on, the rows " +
                    "inside \"Custom Themes\" stop offering \"SELECT\", and turning it back off gives you your own picks again."),
    "key": "RandomThemes",
    "on_change": random_themes_changed,
    "order": 4,
    "path": ("Theme and Appearance", "THEME"),
    "sub_toggles": ({"key": "RandomThemesHolidays", "label": "Include Holiday Themes"},),
    "title": "Random Themes",
    "visible": random_themes_visible,
    "widget": "button_toggle"
  },
  {
    "description": ("<b>Change the two lines of text openpilot shows on screen at the start of every drive.</b><br><br>\"STOCK\" is openpilot's " +
                    "usual safety reminder and \"FROGPILOT\" is the frog version. \"CUSTOM\" lets you write your own, up to 35 characters on the top " +
                    "line and 45 on the bottom."),
    "display": startup_alert,
    "id": "StartupAlert",
    "key": None,
    "link": "startup",
    "options": ({"label": "STOCK", "value": 0}, {"label": "FROGPILOT", "value": 1}, {"label": "CUSTOM", "value": 2}),
    "order": 5,
    "path": ("Theme and Appearance", "THEME"),
    "title": "Startup Alert",
    "widget": "buttons"
  }
]
