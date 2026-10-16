from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

WIFI_INTERFACES = ("wlan0",)


def pairing_changed(context, old_value, new_value):
  params = context.params
  if new_value:
    params.put("ThePondPairingCode", utilities.new_pairing_code())
    context.cookies.append(utilities.pair_browser(params, context.request))
  else:
    params.remove("ThePondPairedBrowsers")
    params.remove("ThePondPairingCode")
  return False


def settings_address(context):
  ip_address = utilities.get_ip_address(WIFI_INTERFACES)
  if ip_address is None:
    return "Offline..."

  address = f"{ip_address}:{frogpilot_variables.THE_POND_PORT}"
  if context.params.get_bool("ThePondPairing"):
    address += f" (Code {context.params.get('ThePondPairingCode')})"
  return address


ROWS = [
  {
    "description": ("<b>Open this address in a browser on the same Wi-Fi to reach \"The Pond\", where you manage your drives, recordings, themes, " +
                    "and more from a phone or computer.</b>"),
    "display": settings_address,
    "id": "ManageYourSettingsAt",
    "key": None,
    "order": 0,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Manage Your Settings At",
    "widget": "text"
  },
  {
    "description": ("<b>Ask for a code before a browser can use \"The Pond\".</b><br><br>Without a code, any device on the same Wi-Fi or Tailscale " +
                    "network can open your drives, camera footage, logs, location, and saved places, and can change your settings, unlock your " +
                    "doors, reboot the device, switch branches, type commands into the device's terminal, restore a backup that replaces its " +
                    "software, or sign the device into a Tailscale account. The code shows next to the address above. Turning this off signs every " +
                    "browser out, and turning it back on makes a new code."),
    "key": "ThePondPairing",
    "on_change": pairing_changed,
    "order": 1,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Pairing Code",
    "widget": "toggle"
  },
  {
    "description": ("<b>Lets openpilot get speed limits from Mapbox.</b><br><br>You make this key yourself on Mapbox's website. \"Mapbox Setup " +
                    "Instructions\" walks you through it."),
    "key": "MapboxPublicKey",
    "link": "keys",
    "options": ({"label": "ADD | REMOVE", "value": 0}, {"label": "TEST", "value": 1}),
    "order": 2,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Public Mapbox Key",
    "widget": "buttons"
  },
  {
    "description": ("<b>Draws the map itself on your driving screen, alongside what the \"Public Mapbox Key\" does for searching.</b><br><br>Keep " +
                    "this one to yourself, since it can be used to run up charges on your Mapbox account. After you add it you are asked whether to " +
                    "reboot now or later, and it does not take effect until the device has actually rebooted."),
    "key": "MapboxSecretKey",
    "link": "keys",
    "options": ({"label": "ADD | REMOVE", "value": 0}, {"label": "TEST", "value": 1}),
    "order": 3,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Secret Mapbox Key",
    "widget": "buttons"
  },
  {
    "children": ("Maps and Navigation", "NAVIGATION", "Mapbox Setup Instructions"),
    "description": ("<b>Walks you through getting your own free Mapbox keys.</b><br><br>The guide only shows the steps for where you are in setup, " +
                    "so it changes as you add each key. Tap the instructions to come back here."),
    "id": "MapboxSetupInstructions",
    "key": None,
    "link": "keys",
    "order": 4,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Mapbox Setup Instructions",
    "widget": "action"
  },
  {
    "description": ("<b>Collect missing or incorrect speed limits automatically while you drive.</b><br><br>Saved corrections are reused on later " +
                    "drives for the same road and direction, except on roads with conditional speed limits.<br><br>FrogPilot compares speed limits " +
                    "from your dashboard, where supported, and Mapbox, which needs your Public Mapbox Key and a working internet connection. " +
                    "Downloaded maps are required because FrogPilot uses their OSM way IDs to identify each road.<br><br>With \"Share FrogPilot " +
                    "Data\" on, collected road speed limits are also sent to FrogPilot while parked on unmetered Wi-Fi or Ethernet to help expand " +
                    "speed limit coverage for all FrogPilot users.<br><br>Need a step-by-step guide? Visit <b>#speed-limit-filler</b> in the " +
                    "FrogPilot Discord!"),
    "key": "SpeedLimitFiller",
    "order": 5,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Speed Limit Filler",
    "widget": "toggle"
  }
]
