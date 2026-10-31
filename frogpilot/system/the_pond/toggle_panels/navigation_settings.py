from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.system.the_pond import utilities

WIFI_INTERFACES = ("wlan0",)


def amap_key_reason(context):
  if context.params.get_int("SearchInput") == frogpilot_variables.AMAP_SEARCH_INPUT:
    return None
  return {"kind": "state", "text": "Shown while \"Destination Search Provider\" is Amap"}


def mapbox_setup_reason(context):
  if context.params.get_int("SearchInput") != frogpilot_variables.AMAP_SEARCH_INPUT:
    return None
  return {"kind": "state", "text": "Shown while \"Destination Search Provider\" is Mapbox"}


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
    address += f" (Code {context.params.get('ThePondPairingCode', encoding='utf-8')})"
  return address


ROWS = [
  {
    "description": ("<b>Lets you search for a destination with Amap when \"Destination Search Provider\" is set to Amap.</b><br><br>This is a Web " +
                    "Service key you make yourself in the console on Amap's website, lbs.amap.com. Add a key to your app there and choose " +
                    "\"Web Service\" (Web 服务) as its service platform, since other kinds of Amap keys don't work here."),
    "key": "AMapKey1",
    "link": "keys",
    "options": ({"label": "ADD | REMOVE", "value": 0},),
    "order": 0,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Amap Key",
    "visible": amap_key_reason,
    "widget": "buttons"
  },
  {
    "description": ("<b>Chooses who finds the places you search for on the \"Navigation\" page in \"The Pond\".</b><br><br>Mapbox is the default. " +
                    "Amap is for searching places in China and needs your own \"Amap Key\". The route and the map on your driving screen come from " +
                    "Mapbox either way, so navigation needs both Mapbox keys with either choice."),
    "key": "SearchInput",
    "options": ({"label": "Mapbox", "value": 0}, {"label": "Amap", "value": 1}),
    "order": 1,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Destination Search Provider",
    "widget": "select"
  },
  {
    "description": ("<b>Open this address in a browser on the same Wi-Fi to reach \"The Pond\", where you manage your drives, recordings, themes, " +
                    "navigation, and more from a phone or computer.</b>"),
    "display": settings_address,
    "id": "ManageYourSettingsAt",
    "key": None,
    "order": 2,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Manage Your Settings At",
    "widget": "text"
  },
  {
    "description": ("<b>Ask for a code before a browser can use \"The Pond\".</b><br><br>Without a code, any device on the same Wi-Fi or Tailscale " +
                    "network can open your drives, camera footage, logs, location, and saved places, and can change your settings, unlock your " +
                    "doors, reboot the device, switch branches, run any command on the device from its shell, restore a backup that replaces its " +
                    "software, or sign the device into a Tailscale account. The code shows next to the address above. Turning this off signs every " +
                    "browser out, and turning it back on makes a new code."),
    "key": "ThePondPairing",
    "on_change": pairing_changed,
    "order": 3,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Pairing Code",
    "widget": "toggle"
  },
  {
    "description": ("<b>Lets you search for a destination and preview the route without paying for comma's subscription.</b><br><br>It also lets " +
                    "openpilot get speed limits from Mapbox. You make this key yourself on Mapbox's website. Navigation stays locked until both this " +
                    "and the \"Secret Mapbox Key\" are set, so add both. \"Mapbox Setup Instructions\" walks you through it."),
    "key": "MapboxPublicKey",
    "link": "keys",
    "options": ({"label": "ADD | REMOVE", "value": 0}, {"label": "TEST", "value": 1}),
    "order": 4,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Public Mapbox Key",
    "widget": "buttons"
  },
  {
    "description": ("<b>Draws the map itself on your driving screen and gets your navigation route from Mapbox, alongside what the \"Public Mapbox " +
                    "Key\" does for searching.</b><br><br>Keep this one to yourself, since it can be used to run up charges on your Mapbox account. " +
                    "It does not take effect until the device has rebooted."),
    "key": "MapboxSecretKey",
    "link": "keys",
    "options": ({"label": "ADD | REMOVE", "value": 0}, {"label": "TEST", "value": 1}),
    "order": 5,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Secret Mapbox Key",
    "widget": "buttons"
  },
  {
    "children": ("Maps and Navigation", "NAVIGATION", "Mapbox Setup Instructions"),
    "description": ("<b>Walks you through getting your own free Mapbox keys so navigation works without comma's subscription.</b><br><br>The guide " +
                    "only shows the steps for where you are in setup, so it changes as you add each key."),
    "id": "MapboxSetupInstructions",
    "key": None,
    "link": "keys",
    "order": 6,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Mapbox Setup Instructions",
    "visible": mapbox_setup_reason,
    "widget": "action"
  },
  {
    "description": ("<b>Learns speed limits from the signs the camera reads on roads where your downloaded maps have no limit or the wrong " +
                    "one.</b><br><br>Each learned limit is saved for that road and direction and used in place of the map's limit the next time " +
                    "you drive there, and it is forgotten once the map matches the signs. Roads with conditional limits, such as ones that change " +
                    "with the time of day or the weather, are skipped. Needs downloaded maps. With \"Share FrogPilot Data\" on, the learned limits, " +
                    "their sign photos, and the roads where the signs matched your map are sent to FrogPilot while parked on unmetered Wi-Fi or " +
                    "Ethernet and used to improve speed limits for everyone using FrogPilot."),
    "key": "SpeedLimitFiller",
    "order": 7,
    "path": ("Maps and Navigation", "NAVIGATION"),
    "title": "Speed Limit Filler",
    "widget": "toggle"
  }
]
