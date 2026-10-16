DEVELOPER_CONFIRMATION = ("\"Developer\" unlocks settings that can drastically change how openpilot drives, and any you changed before will start being " +
                          "used again.\n\nOnly continue if you know what they do.")

REBOOT_KEYS = ("ForceTorqueController", "LateralTune", "NNFF", "NNFFLite")


def first_open(params):
  frogpilot_hours = int(params.get("FrogPilotStats").get("FrogPilotSeconds", 0) / (60 * 60))
  openpilot_hours = (params.get("KonikMinutes") + params.get("openpilotMinutes")) // 60

  if frogpilot_hours < 1 and openpilot_hours < 10:
    return 0, ("Welcome to FrogPilot! Since you're new to openpilot, the \"Minimal\" toggle preset has been applied, but you can change this at any time " +
               "via the \"Tuning Level\" button!")
  if frogpilot_hours < 1 and openpilot_hours < 100:
    return 0, ("Welcome to FrogPilot! Since you're new to FrogPilot, the \"Minimal\" toggle preset has been applied, but you can change this at any time " +
               "via the \"Tuning Level\" button!")
  if frogpilot_hours < 50 and openpilot_hours < 100:
    return 0, ("Since you're fairly new to FrogPilot, the \"Minimal\" toggle preset has been applied, but you can change this at any time via the " +
               "\"Tuning Level\" button!")
  if frogpilot_hours < 100 and openpilot_hours >= 100:
    return 1, ("Since you're experienced with openpilot, the \"Standard\" toggle preset has been applied, but you can change this at any time via the " +
               "\"Tuning Level\" button!")
  if frogpilot_hours < 100:
    return 1, ("Since you're experienced with FrogPilot, the \"Standard\" toggle preset has been applied, but you can change this at any time via the " +
               "\"Tuning Level\" button!")
  return 2, ("Since you're very experienced with FrogPilot, the \"Advanced\" toggle preset has been applied, but you can change this at any time via the " +
             "\"Tuning Level\" button!")


def tuning_level_changed(context, old_level, new_level):
  context.params.put_bool("TuningLevelConfirmed", True)

  if new_level < context.tuning_level("ForceOnroad"):
    context.params.put_bool("ForceOffroad", False)
    context.params.put_bool("ForceOnroad", False)

  reboot_required = False
  for key in REBOOT_KEYS:
    key_level = context.tuning_level(key)
    crosses_level = (old_level >= key_level) != (new_level >= key_level)
    differs_from_default = context.params.get_bool(key) != context.params.get_default_value(key)
    reboot_required |= crosses_level and differs_from_default
  return context.started and not context.is_angle_car and reboot_required


def device_screen_visible(context):
  levels = (context.tuning_level("DeviceManagement"), context.tuning_level("ScreenManagement"))
  if context.level >= min(levels):
    return None
  return {"kind": "level", "level": min(levels)}


def driving_view_visible(context):
  if context.has_openpilot_longitudinal:
    return None
  return context.level_reason("Compass")


ROWS = [
  {
    "checkable": True,
    "description": ("<b>Choose how much control you want over FrogPilot's settings.</b> Anything above your level is hidden and uses FrogPilot's " +
                    "recommended setting instead. Nothing you've set is lost, and it comes back when you move up.<br><br>Minimal - FrogPilot decides " +
                    "nearly everything for you<br>Standard - Recommended for most drivers<br>Advanced - Extra fine-tuning once you know how your car " +
                    "drives<br>Developer - Everything, including settings that can drastically change how openpilot drives"),
    "key": "TuningLevel",
    "on_change": tuning_level_changed,
    "options": (
      {"label": "Minimal", "value": 0},
      {"label": "Standard", "value": 1},
      {"label": "Advanced", "value": 2},
      {"confirm": DEVELOPER_CONFIRMATION, "confirm_button": "Continue", "label": "Developer", "value": 3}
    ),
    "order": 0,
    "path": (),
    "title": "Tuning Level",
    "widget": "buttons"
  },
  {
    "description": ("<b>Set the volume for each of openpilot's alerts, and add extra alerts stock openpilot doesn't have.</b> Extra alerts include a " +
                    "chime when the light turns green or when the car ahead starts moving."),
    "id": "AlertsAndSounds",
    "key": None,
    "options": ({"children": ("Alerts and Sounds", "MANAGE"), "label": "MANAGE", "value": 0},),
    "order": 1,
    "path": (),
    "title": "Alerts and Sounds",
    "widget": "buttons"
  },
  {
    "description": ("<b>Adjust how openpilot accelerates, brakes, steers, and changes lanes.</b><br><br>\"GAS / BRAKE\" only appears on cars where " +
                    "openpilot handles the gas and brake."),
    "id": "DrivingControls",
    "key": None,
    "options": (
      {"children": ("Driving Controls", "DRIVING MODEL"), "label": "DRIVING MODEL", "value": 0,
       "visible": lambda context: context.level_reason("DrivingModel")},
      {"children": ("Driving Controls", "GAS / BRAKE"), "label": "GAS / BRAKE", "value": 1,
       "visible": lambda context: context.car_reason(context.has_openpilot_longitudinal)},
      {"children": ("Driving Controls", "STEERING"), "label": "STEERING", "value": 2}
    ),
    "order": 2,
    "path": (),
    "title": "Driving Controls",
    "widget": "buttons"
  },
  {
    "description": ("<b>Download the speed limit data openpilot uses, and set up Mapbox as a fallback speed limit source.</b> Speed limits come from " +
                    "offline map data for the states or countries you pick, so they work without cell signal."),
    "id": "MapsAndNavigation",
    "key": None,
    "options": (
      {"children": ("Maps and Navigation", "NAVIGATION"), "label": "NAVIGATION", "value": 0},
      {"children": ("Maps and Navigation", "SPEED LIMIT MAPS"), "label": "SPEED LIMIT MAPS", "value": 1}
    ),
    "order": 3,
    "path": (),
    "title": "Maps and Navigation",
    "widget": "buttons"
  },
  {
    "description": ("<b>Manage your saved data, how the device and screen behave, and tools for fixing problems.</b> This is also where your drive " +
                    "stats and backups of your settings live."),
    "id": "SystemSettings",
    "key": None,
    "options": (
      {"children": ("System Settings", "DATA"), "label": "DATA", "value": 0},
      {"children": ("System Settings", "DEVICE / SCREEN"), "label": "DEVICE / SCREEN", "value": 1, "visible": device_screen_visible},
      {"children": ("System Settings", "UTILITIES"), "label": "UTILITIES", "value": 2}
    ),
    "order": 4,
    "path": (),
    "title": "System Settings",
    "widget": "buttons"
  },
  {
    "description": ("<b>Change what appears on the driving screen, and how openpilot looks and sounds.</b> Anything from hiding on-screen icons to " +
                    "full theme packs with new colors, sounds, and turn signal animations."),
    "id": "ThemeAndAppearance",
    "key": None,
    "options": (
      {"children": ("Theme and Appearance", "DRIVING VIEW"), "label": "DRIVING VIEW", "value": 0, "visible": driving_view_visible},
      {"children": ("Theme and Appearance", "THEME"), "label": "THEME", "value": 1}
    ),
    "order": 5,
    "path": (),
    "title": "Theme and Appearance",
    "widget": "buttons"
  },
  {
    "description": ("<b>Tell openpilot what car you drive, turn on features made for your brand, and change what your steering wheel buttons " +
                    "do.</b><br><br>Brand features include things like smoother stop-and-go and automatic door locks. \"WHEEL BUTTONS\" only appears " +
                    "once your \"Tuning Level\" is \"Advanced\" or higher."),
    "id": "VehicleSettings",
    "key": None,
    "options": (
      {"children": ("Vehicle Settings", "VEHICLE SETTINGS"), "label": "VEHICLE SETTINGS", "value": 0},
      {"children": ("Vehicle Settings", "WHEEL BUTTONS"), "label": "WHEEL BUTTONS", "value": 1,
       "visible": lambda context: context.level_reason("WheelControls")}
    ),
    "order": 6,
    "path": (),
    "title": "Vehicle Settings",
    "widget": "buttons"
  }
]
