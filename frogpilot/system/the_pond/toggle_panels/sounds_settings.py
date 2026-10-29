from openpilot.frogpilot.system.the_pond.toggle_panels import rows

VOLUME_LABELS = {0: "Muted", 101: "Auto"}


def loud_blindspot_alert_visible(context):
  return context.level_reason("LoudBlindspotAlert") or context.car_reason(context.has_bsm)


def speed_limit_changed_alert_visible(context):
  params = context.params
  navigation_ui = context.level < context.tuning_level("NavigationUI") or params.get_bool("NavigationUI")
  show_speed_limits = navigation_ui and (context.level < context.tuning_level("ShowSpeedLimits") or params.get_bool("ShowSpeedLimits"))
  speed_limit_controller = context.has_openpilot_longitudinal and params.get_bool("SpeedLimitController")
  shown = show_speed_limits or params.get_bool("DebugMode") or speed_limit_controller

  reason = context.level_reason("SpeedLimitChangedAlert")
  if reason is not None:
    return reason
  if navigation_ui:
    return context.setting_reason(shown, "ShowSpeedLimits")
  return context.setting_reason(shown, "NavigationUI")


ROWS = [
  {
    "children": ("Alerts and Sounds", "MANAGE", "Alert Volumes"),
    "description": ("<b>Set your own volume for each type of openpilot alert instead of letting openpilot pick.</b> Every alert starts on \"Auto\", " +
                    "which raises the volume as your car gets noisier and lowers it when things are quiet. Turning this back off puts all of them " +
                    "back on \"Auto\"."),
    "key": "AlertVolumeControl",
    "order": 0,
    "path": ("Alerts and Sounds", "MANAGE"),
    "title": "Alert Volumes",
    "widget": "manage"
  },
  {
    "description": ("<b>Set the volume for the sound openpilot makes when it stops driving and hands the car back to you.</b><br><br>You hear this " +
                    "when you tap the brake, press \"Cancel\", or when your car's cruise control drops out. Most faults that force openpilot to give " +
                    "up use a warning chime instead, which comes from \"Soft Warning Volume\" or \"Urgent Warning Volume\"."),
    "key": "DisengageVolume",
    "order": 0,
    "path": ("Alerts and Sounds", "MANAGE", "Alert Volumes"),
    "title": "Disengage Volume",
    "value": rows.number(0, 101, unit="%", labels=VOLUME_LABELS, fast=True),
    "widget": "value_buttons"
  },
  {
    "description": ("<b>Set the volume for the sound openpilot makes when the driver camera thinks you've stopped watching the road.</b><br><br>You " +
                    "hear this if you look down at your phone, or if the camera cannot see your face and you go too long without touching the wheel. " +
                    "Ignore it long enough and it escalates into the \"DISENGAGE IMMEDIATELY\" warning, which plays at \"Urgent Warning Volume\" " +
                    "instead. openpilot will not hand the car back on its own, so taking over is on you. It never plays quieter than \"Auto\" would, " +
                    "so it can always reach you."),
    "key": "PromptDistractedVolume",
    "order": 1,
    "path": ("Alerts and Sounds", "MANAGE", "Alert Volumes"),
    "title": "Distracted Driver Volume",
    "value": rows.number(25, 101, unit="%", labels=VOLUME_LABELS, fast=True),
    "widget": "value_buttons"
  },
  {
    "description": ("<b>Set the volume for the sound openpilot makes when it starts driving.</b><br><br>You hear this right after you press \"SET\" " +
                    "or \"RESUME\" on your steering wheel."),
    "key": "EngageVolume",
    "order": 2,
    "path": ("Alerts and Sounds", "MANAGE", "Alert Volumes"),
    "title": "Engage Volume",
    "value": rows.number(0, 101, unit="%", labels=VOLUME_LABELS, fast=True),
    "widget": "value_buttons"
  },
  {
    "description": ("<b>Set the volume for the quick chimes openpilot uses when it needs you to notice something.</b><br><br>You hear these for " +
                    "things like a car sitting in your blind spot when you signal, or a turn too sharp for openpilot to steer through on its own. " +
                    "The \"Goat Scream\" alert plays at this volume as well. This one stops at 25% so it can always reach you."),
    "key": "PromptVolume",
    "order": 3,
    "path": ("Alerts and Sounds", "MANAGE", "Alert Volumes"),
    "title": "Prompt Volume",
    "value": rows.number(25, 101, unit="%", labels=VOLUME_LABELS, fast=True),
    "widget": "value_buttons"
  },
  {
    "description": ("<b>Set the volume for openpilot's serious warnings, such as \"BRAKE! Risk of Collision\" when it thinks you're about to hit " +
                    "something.</b><br><br>The \"Louder Blind Spot Alert\" plays at this volume too. It never plays quieter than \"Auto\" would, so " +
                    "it can always reach you."),
    "key": "WarningSoftVolume",
    "order": 4,
    "path": ("Alerts and Sounds", "MANAGE", "Alert Volumes"),
    "title": "Soft Warning Volume",
    "value": rows.number(25, 101, unit="%", labels=VOLUME_LABELS, fast=True),
    "widget": "value_buttons"
  },
  {
    "description": ("<b>Set the volume for openpilot's most urgent warnings, the ones telling you to take the wheel right " +
                    "now.</b><br><br>\"DISENGAGE IMMEDIATELY\" plays at this volume, which is what you get once you've ignored the driver camera " +
                    "long enough that openpilot no longer trusts you're paying attention. It never plays quieter than \"Auto\" would, so it can " +
                    "always reach you."),
    "key": "WarningImmediateVolume",
    "order": 5,
    "path": ("Alerts and Sounds", "MANAGE", "Alert Volumes"),
    "title": "Urgent Warning Volume",
    "value": rows.number(25, 101, unit="%", labels=VOLUME_LABELS, fast=True),
    "widget": "value_buttons"
  },
  {
    "description": ("<b>Set the volume for the sound openpilot makes when you try to turn it on and it refuses.</b><br><br>You hear this when " +
                    "something is in the way, like an open door, an unbuckled seatbelt, or the parking brake still being on."),
    "key": "RefuseVolume",
    "order": 6,
    "path": ("Alerts and Sounds", "MANAGE", "Alert Volumes"),
    "title": "Won't Engage Volume",
    "value": rows.number(0, 101, unit="%", labels=VOLUME_LABELS, fast=True),
    "widget": "value_buttons"
  },
  {
    "children": ("Alerts and Sounds", "MANAGE", "Extra Alerts"),
    "description": ("<b>Turn on extra alerts stock openpilot doesn't have, for things you would otherwise have to catch yourself.</b><br><br>These " +
                    "cover the light turning green, the car ahead pulling away, and the speed limit changing. The blind spot one in here is not a " +
                    "new alert: openpilot already chimes when you signal for a lane change and there's a car beside you, and this only swaps that " +
                    "chime for a louder one."),
    "key": "CustomAlerts",
    "order": 1,
    "path": ("Alerts and Sounds", "MANAGE"),
    "title": "Extra Alerts",
    "widget": "manage"
  },
  {
    "description": ("<b>Swap the chime for \"Turn Exceeds Steering Limit\" out for a screaming goat.</b><br><br>That alert means the turn is sharper " +
                    "than openpilot can steer through on its own, so it's asking you to help turn the wheel. It plays at whatever you set \"Prompt " +
                    "Volume\" to."),
    "key": "GoatScream",
    "order": 0,
    "path": ("Alerts and Sounds", "MANAGE", "Extra Alerts"),
    "title": "Goat Scream",
    "widget": "toggle"
  },
  {
    "description": ("<b>Play a chime when you're stopped at a light and openpilot sees the road ahead open up.</b><br><br>This only fires when " +
                    "nobody is stopped in front of you, and it stays quiet in \"Traffic Mode\". For a line of cars at a light you need \"Lead " +
                    "Departing Alert\" switched on as well, and that one starts off.<br><br><i><b>Disclaimer</b>: openpilot does not actually read " +
                    "traffic lights. It is going off what the camera sees, so it can chime when the light has not changed.</i>"),
    "key": "GreenLightAlert",
    "order": 1,
    "path": ("Alerts and Sounds", "MANAGE", "Extra Alerts"),
    "title": "Green Light Alert",
    "widget": "toggle"
  },
  {
    "description": ("<b>Play a chime when you're stopped in traffic and the car in front of you starts moving again.</b><br><br>It waits until that " +
                    "car has genuinely pulled away rather than just crept forward, so it won't chime the moment they roll an inch."),
    "key": "LeadDepartingAlert",
    "order": 2,
    "path": ("Alerts and Sounds", "MANAGE", "Extra Alerts"),
    "title": "Lead Departing Alert",
    "widget": "toggle"
  },
  {
    "description": ("<b>Upgrade \"Car Detected in Blindspot\" from openpilot's quiet prompt chime to its warning chime.</b><br><br>You get this " +
                    "alert when you signal for a lane change while openpilot is steering and there is a car beside you where you cannot see it. It " +
                    "needs \"Lane Changes\" switched on and at least your \"Minimum Lane Change Speed\" (20 mph or 32 km/h by default), so " +
                    "signaling slower than that gets you nothing. Because it becomes a warning, its volume comes from \"Soft Warning Volume\" " +
                    "instead of \"Prompt Volume\"."),
    "key": "LoudBlindspotAlert",
    "order": 3,
    "path": ("Alerts and Sounds", "MANAGE", "Extra Alerts"),
    "title": "Louder Blind Spot Alert",
    "visible": loud_blindspot_alert_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>Play a chime whenever the speed limit openpilot is reading changes.</b><br><br>That limit comes from your car's dashboard, " +
                    "your downloaded map data, signs read by the camera, or Mapbox, depending on which of those you have set up. You hear it right " +
                    "as the limit changes, such as entering a school zone or coming off the highway."),
    "key": "SpeedLimitChangedAlert",
    "order": 4,
    "path": ("Alerts and Sounds", "MANAGE", "Extra Alerts"),
    "title": "Speed Limit Changed Alert",
    "visible": speed_limit_changed_alert_visible,
    "widget": "toggle"
  }
]
