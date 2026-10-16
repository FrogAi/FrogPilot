from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.the_pond.toggle_panels import rows

ONROAD_ONLY_REASON = {"key": "DisableOnroadUploads", "kind": "setting", "title": "Disable Onroad Only", "wanted": False}


def higher_bitrate_visible(context):
  branch_allowed = not context.development_branch and not context.vetting_branch
  return (context.level_reason("HigherBitrate") or context.branch_reason(branch_allowed) or
          context.setting_reason(context.params.get_bool("NoUploads"), "NoUploads") or onroad_only_reason(context))


def konik_forced():
  return frogpilot_variables.NOT_VETTED_PATH.is_file()


def konik_server_changed(context, old_value, new_value):
  context.notice = {"action": "REBOOT", "cancel": "Reboot Later", "confirm": "Reboot Now", "text": "Reboot required to take effect."}
  return False


def konik_server_current(context):
  return konik_forced() or context.params.get_bool("UseKonikServer")


def konik_server_enabled(context):
  return context.branch_reason(not konik_forced())


def no_logging_visible(context):
  return context.level_reason("NoLogging") or context.branch_reason(not context.vetting_branch)


def no_uploads_visible(context):
  return context.level_reason("NoUploads") or context.branch_reason(not context.development_branch and not context.vetting_branch)


def onroad_only_reason(context):
  if context.params.get_bool("DisableOnroadUploads"):
    return ONROAD_ONLY_REASON
  return None


async def reboot_for_konik(context, body):
  if context.params.get_bool("UseKonikServer"):
    frogpilot_variables.KONIK_PATH.touch()
  else:
    frogpilot_variables.KONIK_PATH.unlink(missing_ok=True)

  context.params.put_bool("DoSoftReboot", True)
  return True


def replay_labels():
  labels = {0: "Off", 30: "30 seconds", 60: "1 minute"}
  for seconds in range(90, 301, 30):
    labels[seconds] = f"{seconds / 60:g} minutes"
  return labels


def shutdown_labels():
  labels = {}
  for index in range(34):
    if index == 0:
      labels[index] = "5 mins"
    elif index <= 3:
      labels[index] = f"{index * 15} mins"
    elif index == 4:
      labels[index] = f"{index - 3} hour"
    else:
      labels[index] = f"{index - 3} hours"
  return labels


def update_higher_bitrate(context, old_value, new_value):
  params = context.params
  use_higher_bitrate = params.get_bool("HigherBitrate") and params.get_bool("NoUploads") and not params.get_bool("DisableOnroadUploads")
  if frogpilot_variables.HD_PATH.is_file() == use_higher_bitrate:
    return False

  if use_higher_bitrate:
    frogpilot_variables.HD_PATH.touch()
  else:
    frogpilot_variables.HD_PATH.unlink()
  return True


ROWS = [
  {
    "children": ("System Settings", "DEVICE / SCREEN", "Device Settings"),
    "description": "<b>Change how the device powers off, handles heat, and records your drives.</b>",
    "key": "DeviceManagement",
    "order": 0,
    "path": ("System Settings", "DEVICE / SCREEN"),
    "title": "Device Settings",
    "widget": "manage"
  },
  {
    "description": ("<b>How long the device stays on after you finish driving before it shuts itself off.</b><br><br>Shorter times use less of your " +
                    "car's battery. The lowest setting is 5 minutes."),
    "key": "DeviceShutdown",
    "order": 0,
    "path": ("System Settings", "DEVICE / SCREEN", "Device Settings"),
    "title": "Device Shutdown Timer",
    "value": rows.number(0, 33, labels=shutdown_labels()),
    "widget": "value"
  },
  {
    "confirm_enable": rows.ENABLE_CONFIRMATION,
    "description": ("<b>Stop the device from recording your drives.</b><br><br>No driving logs or camera footage are saved, so you won't be able to " +
                    "review your drives later or send a useful bug report. Screen recordings and the device's own system logs are still saved."),
    "key": "NoLogging",
    "order": 1,
    "path": ("System Settings", "DEVICE / SCREEN", "Device Settings"),
    "title": "Disable Logging",
    "visible": no_logging_visible,
    "widget": "toggle"
  },
  {
    "confirm_enable": rows.ENABLE_CONFIRMATION,
    "description": ("<b>Stop the device from uploading your drives to \"comma connect\" or Konik and from sharing FrogPilot driving logs and speed " +
                    "limit sign clips.</b><br><br>Your drives are still saved on the device. comma uses uploads for debugging and official support, " +
                    "so turning this on limits the help they can give. Files that comma connect or Konik asks the device to upload, usage stats, " +
                    "Speed Limit Filler data, device registration, crash reports, and weather and Mapbox lookups are not affected. \"Disable Onroad " +
                    "Only\" pauses uploads while you drive and lets them finish once you park, but only while the device is on Wi-Fi or Ethernet."),
    "key": "NoUploads",
    "on_change": update_higher_bitrate,
    "order": 2,
    "path": ("System Settings", "DEVICE / SCREEN", "Device Settings"),
    "sub_toggles": ({"key": "DisableOnroadUploads", "label": "Disable Onroad Only", "on_change": update_higher_bitrate},),
    "title": "Disable Uploads",
    "visible": no_uploads_visible,
    "widget": "button_toggle"
  },
  {
    "description": ("<b>Record your drives in higher video quality.</b><br><br>This row only appears once \"Disable Uploads\" is on and \"Disable " +
                    "Onroad Only\" is off, since the larger files are not meant to be uploaded. The device needs to reboot for it to take effect."),
    "key": "HigherBitrate",
    "on_change": update_higher_bitrate,
    "order": 3,
    "path": ("System Settings", "DEVICE / SCREEN", "Device Settings"),
    "title": "High-Quality Recording",
    "visible": higher_bitrate_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>Shut the device down when your car's battery drops below the voltage you pick.</b><br><br>This only happens while parked, " +
                    "and keeps the device from draining the battery too far to start the car."),
    "key": "LowVoltageShutdown",
    "order": 4,
    "path": ("System Settings", "DEVICE / SCREEN", "Device Settings"),
    "title": "Low-Voltage Cutoff",
    "value": rows.number(11.8, 12.5, 0.1, unit=" volts"),
    "widget": "value"
  },
  {
    "confirm_enable": rows.ENABLE_CONFIRMATION,
    "description": ("<b>Let the device run about 6 degrees Celsius hotter than normal before openpilot disengages because of the " +
                    "heat.</b><br><br>Normally openpilot disengages and will not re-engage once the device gets hot, and drops back to the offroad " +
                    "screen if it keeps climbing. This only makes the first happen later: the device still drops back to the offroad screen at the " +
                    "normal temperature. Running the device that hot can shorten its life or damage it, so only use this if you understand the risk."),
    "key": "IncreaseThermalLimits",
    "order": 5,
    "path": ("System Settings", "DEVICE / SCREEN", "Device Settings"),
    "title": "Raise Temperature Limits",
    "widget": "toggle"
  },
  {
    "actions": {
      "REBOOT": {
        "guard": lambda context: "openpilot_steering" if frogpilot_utilities.is_openpilot_steering(context.the_pond.sm) else None,
        "messages": {"openpilot_steering": "The device can't reboot while openpilot is steering. Disengage, then reboot from the Device panel."},
        "restart": "Soft rebooting",
        "run": reboot_for_konik
      }
    },
    "current": konik_server_current,
    "description": ("<b>Upload your drives to \"stable.konik.ai\" instead of \"connect.comma.ai\".</b><br><br>Registering with Konik sends the " +
                    "device's IMEI and serial number. The device needs to reboot for this to take effect."),
    "enabled": konik_server_enabled,
    "key": "UseKonikServer",
    "on_change": konik_server_changed,
    "order": 6,
    "path": ("System Settings", "DEVICE / SCREEN", "Device Settings"),
    "title": "Use Konik Server",
    "widget": "toggle"
  },
  {
    "children": ("System Settings", "DEVICE / SCREEN", "Screen Settings"),
    "description": "<b>Change how bright the screen is, how long it stays on, and whether you can record it.</b>",
    "key": "ScreenManagement",
    "order": 1,
    "path": ("System Settings", "DEVICE / SCREEN"),
    "title": "Screen Settings",
    "widget": "manage"
  },
  {
    "description": ("<b>Save what just happened on your driving screen.</b><br><br>Choose how far back to keep, then tap \"CAPTURE\" to save a " +
                    "moment you want to review or share. There's no need to remember to start recording beforehand."),
    "key": "InstantReplay",
    "order": 0,
    "path": ("System Settings", "DEVICE / SCREEN", "Screen Settings"),
    "title": "Capture Recent Footage",
    "value": rows.number(0, 300, 30, labels=replay_labels()),
    "widget": "value"
  },
  {
    "description": ("<b>How bright the screen is while you're not driving.</b><br><br>\"Auto\" only follows the light around you while you are " +
                    "driving. While you are parked it is a fixed 50%, whatever the light is like."),
    "key": "ScreenBrightness",
    "order": 1,
    "path": ("System Settings", "DEVICE / SCREEN", "Screen Settings"),
    "title": "Screen Brightness (Offroad)",
    "value": rows.number(1, 101, unit="%", labels={101: "Auto"}, fast=True),
    "widget": "value"
  },
  {
    "description": ("<b>How bright the screen is while you're driving.</b><br><br>\"Auto\" matches the light around you, and \"Screen Off\" keeps " +
                    "the display dark until you tap it. A tap brightens anything below 5% to 5% until the screen times out."),
    "key": "ScreenBrightnessOnroad",
    "order": 2,
    "path": ("System Settings", "DEVICE / SCREEN", "Screen Settings"),
    "title": "Screen Brightness (Onroad)",
    "value": rows.number(0, 101, unit="%", labels={0: "Screen Off", 101: "Auto"}, fast=True),
    "widget": "value"
  },
  {
    "description": ("<b>Add a button to the driving screen that records what's on it.</b><br><br>Your recordings are saved on the device and can be " +
                    "renamed or deleted under \"Screen Recordings\" in the \"DATA\" panel."),
    "key": "ScreenRecorder",
    "order": 3,
    "note": "Start and stop recordings on the device.",
    "path": ("System Settings", "DEVICE / SCREEN", "Screen Settings"),
    "sub_toggles": (),
    "title": "Screen Recorder",
    "widget": "button_toggle"
  },
  {
    "description": "<b>How long the screen stays on after you tap it while not driving.</b>",
    "key": "ScreenTimeout",
    "order": 4,
    "path": ("System Settings", "DEVICE / SCREEN", "Screen Settings"),
    "title": "Screen Timeout (Offroad)",
    "value": rows.number(5, 60, 5, unit=" seconds"),
    "widget": "value"
  },
  {
    "description": "<b>How long the screen stays on after you tap it while driving.</b>",
    "key": "ScreenTimeoutOnroad",
    "order": 5,
    "path": ("System Settings", "DEVICE / SCREEN", "Screen Settings"),
    "title": "Screen Timeout (Onroad)",
    "value": rows.number(5, 60, 5, unit=" seconds"),
    "widget": "value"
  },
  {
    "description": ("<b>Turn the screen off while driving, and wake it up automatically for alerts or when openpilot engages or " +
                    "disengages.</b><br><br>Tapping the screen wakes it up too."),
    "key": "StandbyMode",
    "order": 6,
    "path": ("System Settings", "DEVICE / SCREEN", "Screen Settings"),
    "title": "Standby Mode",
    "widget": "toggle"
  }
]
