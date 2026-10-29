import re

from functools import cache
from pathlib import Path

from openpilot.common.basedir import BASEDIR

from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.system.the_pond.toggle_panels import rows

CAR_NAME_PATTERN = re.compile(r'CarDocs\w*\s*\(\s*"([^"]+)"')
COMMENT_PATTERN = re.compile(r"#[^\n]*")
FOOTNOTES_PATTERN = re.compile(r"footnotes=\[[^\]]*\],\s*")
PLATFORM_PATTERN = re.compile(r"(\w+)\s*=\s*\w+\s*\(")

UNKNOWN_CAR = "Unknown until first drive"


@cache
def get_car_models(make):
  names = []
  platforms = {}

  folder = frogpilot_variables.CAR_MAKE_FOLDERS.get(make.lower())
  if folder is None:
    return names, platforms

  content = (Path(BASEDIR) / "opendbc/car" / folder / "values.py").read_text(encoding="utf-8")
  content = FOOTNOTES_PATTERN.sub("", COMMENT_PATTERN.sub("", content))

  starts = [(match.start(), match[1]) for match in PLATFORM_PATTERN.finditer(content)]
  ends = [start for start, _ in starts[1:]] + [len(content)]
  for (start, platform), end in zip(starts, ends, strict=True):
    for match in CAR_NAME_PATTERN.finditer(content[start:end]):
      name = match[1]
      if name.lower().startswith(make.lower()):
        names.append(name)
        platforms[name] = platform

  names.sort(key=str.lower)
  return names, platforms


def car_make_changed(context, old_make, new_make):
  if new_make != old_make:
    context.params.remove("CarModel")
    context.params.remove("CarModelName")
  return False


def car_model_changed(context, old_platform, new_name):
  _, platforms = get_car_models(context.params.get("CarMake"))
  return context.started and platforms[new_name] != context.car_fingerprint


def car_model_options(context):
  names, _ = get_car_models(context.params.get("CarMake"))
  return [{"label": name, "value": name} for name in names]


def car_row_reason(context, key, available):
  return context.level_reason(key) or context.car_reason(context.car_detected and available)


def car_selection_reason(context):
  return context.level_reason("ForceFingerprint") or context.setting_reason(context.params.get_bool("ForceFingerprint"), "ForceFingerprint")


def disable_openpilot_longitudinal_visible(context):
  available = context.has_openpilot_longitudinal or context.params.get_bool("DisableOpenpilotLongitudinal")
  return context.level_reason("DisableOpenpilotLongitudinal") or context.car_reason(available and context.can_disable_openpilot_long)


def hardware_detected(context):
  if not context.car_detected:
    return UNKNOWN_CAR

  detected = []
  if context.has_pedal:
    detected.append("comma Pedal")
  if context.has_sdsu:
    detected.append("SDSU")
  if context.has_zss:
    detected.append("ZSS")

  if not detected:
    return "None"
  return ", ".join(detected)


def honda_low_speed_pedal_visible(context):
  reason = car_row_reason(context, "HondaLowSpeedPedal", context.is_honda_nidec) or longitudinal_reason(context)
  return reason or context.car_reason(context.has_pedal)


def long_pitch_visible(context):
  reason = car_row_reason(context, "LongPitch", context.is_gm) or longitudinal_reason(context)
  return reason or context.car_reason(not context.is_gm_cc_only)


def longitudinal_reason(context):
  return context.car_reason(context.has_openpilot_longitudinal)


def reboot_when_started(context, old_value, new_value):
  return context.started


def sng_hack_visible(context):
  reason = car_row_reason(context, "SNGHack", context.is_toyota) or longitudinal_reason(context)
  return reason or context.car_reason(not context.has_sng)


def sng_support(context):
  subaru_sng = context.level < context.tuning_level("SubaruSNG") or context.params.get_bool("SubaruSNG")
  return support_text(context, context.has_sng and (not context.is_subaru or subaru_sng))


def support_text(context, supported):
  if not context.car_detected:
    return UNKNOWN_CAR
  if supported:
    return "Yes"
  return "No"


def volt_sng_visible(context):
  reason = car_row_reason(context, "VoltSNG", context.is_gm) or longitudinal_reason(context)
  return reason or context.car_reason(context.is_volt and not context.has_sng)


def write_car_model(context, name):
  _, platforms = get_car_models(context.params.get("CarMake"))
  context.params.put("CarModel", platforms[name])
  context.params.put("CarModelName", name)


ROWS = [
  {
    "children": ("Vehicle Settings", "VEHICLE SETTINGS", "Acura/Honda Settings"),
    "description": ("<b>Settings that only work on Acura and Honda cars with the older Nidec cruise control, covering how openpilot follows and " +
                    "brakes.</b>"),
    "id": "HondaToggles",
    "key": None,
    "order": 0,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS"),
    "title": "Acura/Honda Settings",
    "visible": lambda context: context.panel_reason(("Vehicle Settings", "VEHICLE SETTINGS", "Acura/Honda Settings")),
    "widget": "action"
  },
  {
    "description": ("<b>The make of the car openpilot is using.</b> To pick one yourself, turn on \"Disable Automatic Fingerprint Detection\" first " +
                    "(\"Advanced\" tuning level)."),
    "display": lambda context: context.params.get("CarMake"),
    "enabled": car_selection_reason,
    "key": "CarMake",
    "on_change": car_make_changed,
    "options": tuple({"label": make, "value": make} for make in frogpilot_variables.CAR_MAKES),
    "order": 1,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS"),
    "picker": {"title": "Choose your car make"},
    "title": "Car Make",
    "widget": "select"
  },
  {
    "current": lambda context: context.params.get("CarModelName"),
    "description": ("<b>The model of the car openpilot is using.</b> To pick one yourself, turn on \"Disable Automatic Fingerprint Detection\" first " +
                    "(\"Advanced\" tuning level)."),
    "display": lambda context: context.params.get("CarModelName") or context.params.get("CarModel"),
    "enabled": car_selection_reason,
    "key": "CarModel",
    "keys": ("CarModel", "CarModelName"),
    "on_change": car_model_changed,
    "options": car_model_options,
    "order": 2,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS"),
    "picker": {"title": "Choose your car model"},
    "title": "Car Model",
    "widget": "select",
    "write": write_car_model
  },
  {
    "description": "<b>Lock openpilot to the car you picked and stop it changing on its own.</b>",
    "key": "ForceFingerprint",
    "on_change": lambda context, old_value, new_value: context.started and not new_value,
    "order": 3,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS"),
    "title": "Disable Automatic Fingerprint Detection",
    "widget": "toggle"
  },
  {
    "confirm_enable": {"cancel": "No", "confirm": "Yes", "text": "Are you sure you want to completely disable openpilot longitudinal control?"},
    "description": "<b>Let your car's own cruise control handle the gas and brake instead of openpilot.</b>",
    "key": "DisableOpenpilotLongitudinal",
    "on_change": reboot_when_started,
    "order": 4,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS"),
    "title": "Disable openpilot Longitudinal Control",
    "visible": disable_openpilot_longitudinal_visible,
    "widget": "toggle"
  },
  {
    "children": ("Vehicle Settings", "VEHICLE SETTINGS", "General Motors Settings"),
    "description": ("<b>Settings that only work on Buick, Cadillac, Chevrolet, GMC, and Holden cars, covering pedal response on hills and how " +
                    "openpilot stops and starts.</b>"),
    "id": "GMToggles",
    "key": None,
    "order": 5,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS"),
    "title": "General Motors Settings",
    "visible": lambda context: context.panel_reason(("Vehicle Settings", "VEHICLE SETTINGS", "General Motors Settings")),
    "widget": "action"
  },
  {
    "children": ("Vehicle Settings", "VEHICLE SETTINGS", "Hyundai/Kia/Genesis Settings"),
    "description": ("<b>Settings that only work on Genesis, Hyundai, and Kia cars, covering a steering torque hack.</b><br><br>The steering hack only " +
                    "appears on cars using CAN-FD."),
    "id": "HKGToggles",
    "key": None,
    "order": 6,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS"),
    "title": "Hyundai/Kia/Genesis Settings",
    "visible": lambda context: context.panel_reason(("Vehicle Settings", "VEHICLE SETTINGS", "Hyundai/Kia/Genesis Settings")),
    "widget": "action"
  },
  {
    "children": ("Vehicle Settings", "VEHICLE SETTINGS", "Subaru Settings"),
    "description": ("<b>Settings that only work on Subaru cars.</b><br><br>There is one, and it decides whether your car pulls away by itself after " +
                    "a stop."),
    "id": "SubaruToggles",
    "key": None,
    "order": 7,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS"),
    "title": "Subaru Settings",
    "visible": lambda context: context.panel_reason(("Vehicle Settings", "VEHICLE SETTINGS", "Subaru Settings")),
    "widget": "action"
  },
  {
    "children": ("Vehicle Settings", "VEHICLE SETTINGS", "Toyota/Lexus Settings"),
    "description": ("<b>Settings that only work on Lexus and Toyota cars, covering door locking, dashboard speed, stop-and-go, and openpilot's own " +
                    "tuning.</b><br><br>Which of these you see depends on your exact model and on what hardware is fitted."),
    "id": "ToyotaToggles",
    "key": None,
    "order": 8,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS"),
    "title": "Toyota/Lexus Settings",
    "visible": lambda context: context.panel_reason(("Vehicle Settings", "VEHICLE SETTINGS", "Toyota/Lexus Settings")),
    "widget": "action"
  },
  {
    "children": ("Vehicle Settings", "VEHICLE SETTINGS", "Vehicle Info"),
    "description": ("<b>What openpilot has worked out about your car and what it can do with it.</b><br><br>These rows are read-only. They stay on " +
                    "\"Unknown until first drive\" until openpilot has recognized your car."),
    "id": "VehicleInfo",
    "key": None,
    "order": 9,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS"),
    "title": "Vehicle Info",
    "visible": lambda context: context.panel_reason(("Vehicle Settings", "VEHICLE SETTINGS", "Vehicle Info")),
    "widget": "action"
  },
  {
    "description": ("<b>Soften how hard openpilot corrects its speed behind the car ahead, for smoother following in stop-and-go " +
                    "traffic.</b><br><br>openpilot settles small speed differences more slowly, so it can take a moment longer to close a gap after " +
                    "the car ahead pulls away."),
    "key": "HondaAltTune",
    "on_change": reboot_when_started,
    "order": 0,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Acura/Honda Settings"),
    "title": "Gentle Following",
    "visible": lambda context: car_row_reason(context, "HondaAltTune", context.is_honda_nidec) or longitudinal_reason(context),
    "widget": "toggle"
  },
  {
    "description": ("<b>Let openpilot use your car's full braking force for its hardest stops.</b><br><br>Without it, openpilot's hardest stop only " +
                    "uses about four fifths of the brakes on these cars. With it on, every stop brakes a little firmer for the same request."),
    "key": "HondaMaxBrake",
    "order": 1,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Acura/Honda Settings"),
    "title": "Increased Braking Force",
    "visible": lambda context: car_row_reason(context, "HondaMaxBrake", context.is_honda_nidec) or longitudinal_reason(context),
    "widget": "toggle"
  },
  {
    "description": "<b>Improves acceleration from a standstill for a more responsive throttle feel in city driving.</b>",
    "key": "HondaLowSpeedPedal",
    "order": 2,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Acura/Honda Settings"),
    "title": "Responsive Pedal at Low Speeds",
    "visible": honda_low_speed_pedal_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>Add gas going uphill and ease off going downhill, so openpilot holds a steady pace on hills instead of correcting after the " +
                    "car has already slowed down or sped up.</b><br><br>Slopes gentler than about 1% are ignored. The brakes get the same adjustment " +
                    "above about 22 mph, and it fades out as you slow to about 11 mph so stops feel the same as on flat ground."),
    "key": "LongPitch",
    "order": 0,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "General Motors Settings"),
    "title": "Smooth Pedal Response on Hills",
    "visible": long_pitch_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>Make the car pull away by itself after a full stop on a Chevrolet Volt, which does not do this from the " +
                    "factory.</b><br><br>Without it you have to press the gas or the resume button every time traffic moves off. Keep your foot near " +
                    "the brake the first few times so you can see how it behaves."),
    "key": "VoltSNG",
    "order": 1,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "General Motors Settings"),
    "title": "Stop-and-Go Hack",
    "visible": volt_sng_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>Let openpilot pull the wheel harder through turns, using the trick comma demonstrated on their 2022 \"Taco Bell Run\" " +
                    "drive.</b><br><br>It raises the steering limit everywhere, not just at low speed, and it relaxes one of the safety checks that " +
                    "normally caps steering effort. You will also have to grip the wheel more firmly to take over."),
    "key": "TacoTuneHacks",
    "on_change": lambda context, old_value, new_value: context.started and new_value,
    "order": 0,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Hyundai/Kia/Genesis Settings"),
    "title": "\"Taco Bell Run\" Torque Hack",
    "visible": lambda context: car_row_reason(context, "TacoTuneHacks", context.is_hkg) or context.car_reason(context.is_hkg_can_fd),
    "widget": "toggle"
  },
  {
    "description": ("<b>Get your car moving again by itself once the car ahead pulls away from a full stop.</b><br><br>Subaru's own cruise holds the " +
                    "brakes and waits for you to press resume after a few seconds stopped. FrogPilot watches the car ahead and sends that resume for " +
                    "you. Keep your foot ready near the brake the first few times so you can see how it behaves."),
    "key": "SubaruSNG",
    "on_change": reboot_when_started,
    "order": 0,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Subaru Settings"),
    "title": "Stop and Go",
    "visible": lambda context: car_row_reason(context, "SubaruSNG", context.is_subaru) or context.car_reason(context.has_sng),
    "widget": "toggle"
  },
  {
    "description": ("<b>Lock the doors when you shift out of park and unlock them again when you shift back into it.</b><br><br>This runs whenever " +
                    "the car is on, whether or not openpilot is engaged."),
    "key": "ToyotaDoors",
    "order": 0,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Toyota/Lexus Settings"),
    "sub_toggles": ({"key": "LockDoors", "label": "Lock"}, {"key": "UnlockDoors", "label": "Unlock"}),
    "title": "Automatically Lock/Unlock Doors",
    "visible": lambda context: car_row_reason(context, "ToyotaDoors", context.is_toyota),
    "widget": "button_toggle"
  },
  {
    "buttons": ("Reset",),
    "description": ("<b>Line up the speed openpilot shows on screen with the number on your dashboard, which most cars deliberately read a little " +
                    "high.</b><br><br>With \"Use Wheel Speed\" off, raise it until openpilot's number matches your dashboard. This does not change " +
                    "how fast openpilot actually drives, with one exception: while it is following posted speed limits, a higher number here makes " +
                    "it drive slightly slower."),
    "key": "ClusterOffset",
    "order": 1,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Toyota/Lexus Settings"),
    "title": "Dashboard Speed Offset",
    "value": rows.number(1, 1.05, 0.001, unit="x"),
    "visible": lambda context: car_row_reason(context, "ClusterOffset", context.is_toyota),
    "widget": "value_buttons"
  },
  {
    "description": ("<b>Let openpilot control the gas and brake on an older Toyota by rerouting the cruise control computer's messages through a " +
                    "wiring harness you fit yourself.</b><br><br>The DSU is the box that normally runs your car's radar cruise. Only turn this on " +
                    "after the harness is physically installed, because openpilot cannot check for it."),
    "key": "ToyotaDSUBypass",
    "on_change": reboot_when_started,
    "order": 2,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Toyota/Lexus Settings"),
    "title": "DSU Re-Route Harness",
    "visible": lambda context: car_row_reason(context, "ToyotaDSUBypass", context.is_toyota) or context.car_reason(context.can_use_dsu_bypass),
    "widget": "toggle"
  },
  {
    "description": ("<b>Swap in FrogsGoMoo's own settings for how openpilot comes to a stop.</b><br><br>These are personal preferences rather than a " +
                    "fix for anything, and they are already on. They take over your stopping and starting values from \"Driving Controls\" and hide " +
                    "those rows while this is on, though on a Toyota the starting value has no effect. They also make openpilot's gas and brake " +
                    "control predict the car's acceleration further ahead at every speed, so they change how it drives while cruising as well."),
    "key": "FrogsGoMoosTweak",
    "order": 3,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Toyota/Lexus Settings"),
    "title": "FrogsGoMoo's Personal Tweaks",
    "visible": lambda context: car_row_reason(context, "FrogsGoMoosTweak", context.is_toyota) or longitudinal_reason(context),
    "widget": "toggle"
  },
  {
    "description": ("<b>Lock the doors on their own once you have switched the car off and left it, after the number of seconds you " +
                    "pick.</b><br><br>The countdown only starts once the screen has gone dark, and it starts over if the driver camera still sees a " +
                    "face in the driver's seat or if any door is open. Somebody sitting in the front passenger seat will not hold it off. Set it to " +
                    "\"Never\" to switch it off."),
    "key": "LockDoorsTimer",
    "order": 4,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Toyota/Lexus Settings"),
    "title": "Lock Doors On Ignition Off After",
    "value": rows.number(0, 300, 5, unit=" seconds", labels={0: "Never"}),
    "visible": lambda context: car_row_reason(context, "LockDoorsTimer", context.is_toyota),
    "warning": "<b>Warning:</b> openpilot can't tell whether your keys are still in the car, so keep a spare somewhere safe before you rely on this!",
    "widget": "value"
  },
  {
    "description": ("<b>Make the car pull away by itself after a full stop on a Lexus or Toyota that does not do this from the " +
                    "factory.</b><br><br>Without it you have to press the gas or the resume button every time traffic moves off. It works by telling " +
                    "the car openpilot is never fully stopped, so keep your foot near the brake the first few times. It also lets openpilot engage, " +
                    "and stay engaged, below about 19 mph (30 km/h), where it would otherwise refuse or turn itself off."),
    "key": "SNGHack",
    "order": 5,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Toyota/Lexus Settings"),
    "title": "Stop-and-Go Hack",
    "visible": sng_hack_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>Extra hardware openpilot has found fitted to your car, such as a comma pedal, an SDSU, or a ZSS.</b><br><br>openpilot works " +
                    "these out from your car's wiring on its own. \"None\" is not proof nothing is fitted: a comma pedal is only reported on cars " +
                    "that support one."),
    "display": hardware_detected,
    "id": "HardwareDetected",
    "key": None,
    "order": 0,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Vehicle Info"),
    "title": "3rd Party Hardware Detected",
    "widget": "text"
  },
  {
    "description": ("<b>Whether openpilot can read your car's blind spot sensors, which it uses to hold off a lane change when someone is beside " +
                    "you.</b><br><br>If this says No, check your mirrors yourself before every lane change, because openpilot has nothing to warn it."),
    "display": lambda context: support_text(context, context.has_bsm),
    "id": "BlindSpotSupport",
    "key": None,
    "order": 1,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Vehicle Info"),
    "title": "Blind Spot Support",
    "widget": "text"
  },
  {
    "description": ("<b>Whether a comma pedal would work on your car, which is an add-on that lets openpilot pull away from a stop on cars that " +
                    "cannot do it themselves.</b><br><br>This tells you whether one is worth fitting, not whether you already have one. \"3rd Party " +
                    "Hardware Detected\" above answers that."),
    "display": lambda context: support_text(context, context.can_use_pedal),
    "id": "PedalSupport",
    "key": None,
    "order": 2,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Vehicle Info"),
    "title": "comma Pedal Support",
    "widget": "text"
  },
  {
    "description": ("<b>Whether openpilot handles the gas and brake itself, rather than leaving that to your car's own cruise control.</b><br><br>If " +
                    "this says No, openpilot only steers and your car decides the speed, so the settings under \"Driving Controls\" that shape " +
                    "acceleration and braking will not do anything."),
    "display": lambda context: support_text(context, context.has_openpilot_longitudinal),
    "id": "OpenpilotLongitudinal",
    "key": None,
    "order": 3,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Vehicle Info"),
    "title": "openpilot Longitudinal Support",
    "widget": "text"
  },
  {
    "description": ("<b>Whether openpilot can use your car's radar alongside its camera, which helps it track the car ahead in rain, fog, and " +
                    "darkness.</b><br><br>If this says No, openpilot is working from the camera alone and may pick up the car ahead later in poor " +
                    "visibility."),
    "display": lambda context: support_text(context, context.has_radar),
    "id": "RadarSupport",
    "key": None,
    "order": 4,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Vehicle Info"),
    "title": "Radar Support",
    "widget": "text"
  },
  {
    "description": ("<b>Whether an SDSU would work on your car, which is a small board that lets openpilot control the gas and brake on older " +
                    "Toyotas.</b><br><br>This tells you whether one is worth fitting, not whether you already have one."),
    "display": lambda context: support_text(context, context.can_use_sdsu),
    "id": "SDSUSupport",
    "key": None,
    "order": 5,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Vehicle Info"),
    "title": "SDSU Support",
    "widget": "text"
  },
  {
    "description": ("<b>Whether openpilot pulls away by itself after a full stop, instead of waiting for you to press the gas or the resume " +
                    "button.</b><br><br>If this says No, your car's brand group above may still offer a \"Stop-and-Go Hack\" that adds it."),
    "display": sng_support,
    "id": "SNGSupport",
    "key": None,
    "order": 6,
    "path": ("Vehicle Settings", "VEHICLE SETTINGS", "Vehicle Info"),
    "title": "Stop-and-Go Support",
    "widget": "text"
  }
]
