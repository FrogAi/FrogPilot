from openpilot.frogpilot.system.the_pond import utilities
from openpilot.frogpilot.system.the_pond.toggle_panels import rows

PERSONALITY_PROFILE_KEYS = ("AggressivePersonalityProfile", "RelaxedPersonalityProfile", "StandardPersonalityProfile")
PERSONALITY_SUFFIXES = ("Follow", "JerkAcceleration", "JerkDanger", "JerkDeceleration", "JerkSpeed", "JerkSpeedDecrease")
SLC_PRIORITY_KEYS = ("SLCPriority1", "SLCPriority2", "SLCPriority3")


def dashboard_reason(context):
  return context.car_reason(context.has_dash_speed_limits)


PRIMARY_PRIORITIES = (
  {"label": "Camera", "value": "Camera"},
  {"label": "Dashboard", "value": "Dashboard", "visible": dashboard_reason},
  {"label": "Map Data", "value": "Map Data"},
  {"label": "Highest", "value": "Highest"},
  {"label": "Lowest", "value": "Lowest"}
)
OTHER_PRIORITIES = (
  {"label": "None", "value": "None"},
  {"label": "Camera", "value": "Camera"},
  {"label": "Dashboard", "value": "Dashboard", "visible": dashboard_reason},
  {"label": "Map Data", "value": "Map Data"}
)


def calibrated_lateral_acceleration(context):
  return f"{min(context.params.get('CalibratedLateralAcceleration'), context.frogpilot_toggles.maxLateralAccel):.2f} m/s²"


def slc_priority_display(context):
  primary = [option["value"] for option in PRIMARY_PRIORITIES]

  priorities = []
  for key in SLC_PRIORITY_KEYS:
    priority = context.params.get(key)
    if priority == "Dashboard" and not context.has_dash_speed_limits:
      continue
    if priority in primary:
      priorities.append(priority)
    if priority in ("Highest", "Lowest"):
      break
  return ", ".join(priorities)


def car_on_guard(context):
  if context.started:
    return "car_on"
  return None


def custom_personalities_visible(context):
  reasons = [context.level_reason(key) for key in PERSONALITY_PROFILE_KEYS]
  if None in reasons:
    return None
  return reasons[0]


def frogs_go_moos_tweak_reason(context):
  tweak = context.is_toyota and context.params.get_bool("FrogsGoMoosTweak")
  return context.setting_reason(not tweak, "FrogsGoMoosTweak", wanted=False)


def personality_reset_actions(personality):
  async def reset(context, body):
    for suffix in PERSONALITY_SUFFIXES:
      key = personality + suffix
      context.params.put(key, context.params.get_default_value(key))
    return True

  confirm = {"cancel": "No", "confirm": "Yes", "text": f"Are you sure you want to completely reset your settings for the <b>{personality}</b> personality?"}
  return {"RESET": {"confirm": confirm, "run": reset}}


async def reset_curve_data(context, body):
  utilities.reset_curve_data(context.params)
  return True


def slc_mapbox_filler_visible(context):
  return context.level_reason("SLCMapboxFiller") or context.set_reason("MapboxPublicKey")


def start_accel_visible(context):
  human_acceleration = context.params.get_bool("LongitudinalTune") and context.params.get_bool("HumanAcceleration")
  return (context.level_reason("StartAccel") or context.car_reason(context.has_starting_state) or
          context.setting_reason(not human_acceleration, "HumanAcceleration", wanted=False))


def v_ego_starting_visible(context):
  return context.level_reason("VEgoStarting") or context.car_reason(context.has_starting_state) or frogs_go_moos_tweak_reason(context)


def without_pcm_cruise_visible(context, key):
  return context.level_reason(key) or context.car_reason(not context.has_pcm_cruise)


ROWS = [
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Acceleration and Braking"),
    "description": "<b>Change how openpilot speeds up and slows down.</b>",
    "key": "LongitudinalTune",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE"),
    "title": "Acceleration and Braking",
    "widget": "manage"
  },
  {
    "description": ("<b>How quickly openpilot speeds up.</b><br><br>\"Standard\" is openpilot's normal acceleration, \"Eco\" is gentle and " +
                    "efficient, \"Sport\" is firmer and more responsive, and \"Sport+\" accelerates as hard as your car allows. None of these apply " +
                    "while \"Experimental Mode\" is running, including when \"Conditional Experimental Mode\" switches it on for you."),
    "key": "AccelerationProfile",
    "options": ({"label": "Standard", "value": 0}, {"label": "Eco", "value": 1}, {"label": "Sport", "value": 2}, {"label": "Sport+", "value": 3}),
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Acceleration and Braking"),
    "title": "Acceleration Profile",
    "widget": "select"
  },
  {
    "description": ("<b>How gently openpilot slows down when the road ahead is clear.</b><br><br>\"Standard\" is openpilot's normal braking, \"Eco\" " +
                    "brakes about half as hard so you coast more, and \"Eco+\" just lets off the gas and coasts, so it slows quicker uphill and " +
                    "barely slows on a steep downhill until the road levels out. Whenever there is a car ahead, or while \"Experimental Mode\" is " +
                    "running, openpilot goes back to full-strength braking so it can still stop in time."),
    "key": "DecelerationProfile",
    "options": ({"label": "Standard", "value": 0}, {"label": "Eco", "value": 1}, {"label": "Eco+", "value": 2}),
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Acceleration and Braking"),
    "title": "Deceleration Profile",
    "widget": "select"
  },
  {
    "description": ("<b>openpilot builds speed more like a person, holding back while your set speed is low and easing off as you close in on " +
                    "it.</b><br><br>At a 25 mph set speed you get roughly half the acceleration you otherwise would. On cars with a fixed launch " +
                    "push, this replaces that push with the same smooth target openpilot uses everywhere else, and the \"Start Acceleration\" row " +
                    "disappears while this is on."),
    "key": "HumanAcceleration",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Acceleration and Braking"),
    "title": "Human-Like Acceleration",
    "widget": "toggle"
  },
  {
    "description": ("<b>openpilot follows the car ahead more naturally, reading where that car is headed so it eases off and starts braking earlier " +
                    "and softer instead of reacting late.</b><br><br>On the default driving model this only works while openpilot can clearly see " +
                    "the car ahead."),
    "key": "HumanFollowing",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Acceleration and Braking"),
    "title": "Human-Like Following",
    "widget": "toggle"
  },
  {
    "description": ("<b>openpilot watches the cars in the lane it is moving into and adjusts its speed for them during a lane " +
                    "change.</b><br><br>Without this it only reacts to the car directly ahead until the move is finished. It only takes over while " +
                    "openpilot is already tracking a car ahead, so with a clear lane in front of you nothing changes."),
    "key": "HumanLaneChanges",
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE", "Acceleration and Braking"),
    "title": "Human-Like Lane Changes",
    "visible": lambda context: context.level_reason("HumanLaneChanges") or context.car_reason(context.has_radar),
    "widget": "toggle"
  },
  {
    "description": ("<b>How sure openpilot has to be that something in front of you is really a car before it starts following it and braking for " +
                    "it.</b><br><br>Lower numbers pick up cars sooner and farther away, with more chances of reacting to something that is not a " +
                    "moving car. Higher numbers wait for a clearer look, so there are fewer false alarms but openpilot notices the car ahead " +
                    "later.<br><br>Default: 35%."),
    "key": "LeadDetectionThreshold",
    "order": 5,
    "path": ("Driving Controls", "GAS / BRAKE", "Acceleration and Braking"),
    "title": "Lead Detection Sensitivity",
    "value": rows.number(25, 50, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>openpilot slows down more for left and right turns, using the trick comma built for their 2022 \"Taco Bell Run\" " +
                    "drive.</b><br><br>It does nothing while \"Curve Speed Controller\" is on. Otherwise it never switches off, so it is still " +
                    "working on fast highway curves. What stops at about 45 mph is the ramp: openpilot allows more cornering force the faster you " +
                    "go, up to that speed, then holds it flat. That makes the slowdown most noticeable in slow, tight turns."),
    "key": "TacoTune",
    "order": 6,
    "path": ("Driving Controls", "GAS / BRAKE", "Acceleration and Braking"),
    "title": "\"Taco Bell Run\" Turn Speed Hack",
    "widget": "toggle"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Advanced Longitudinal Tuning"),
    "description": "<b>Hand-set the acceleration and braking numbers openpilot normally takes from your car.</b>",
    "key": "AdvancedLongitudinalTune",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE"),
    "title": "Advanced Longitudinal Tuning",
    "widget": "manage"
  },
  {
    "default": lambda context: context.longitudinal_actuator_delay,
    "description": ("<b>How long your car takes to respond after openpilot presses the gas or brake.</b><br><br>Raise it if your car feels slow to " +
                    "react. Lower it if it feels too eager or overshoots."),
    "key": "LongitudinalActuatorDelay",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Advanced Longitudinal Tuning"),
    "title": lambda context: rows.car_title(context, "Actuator Delay", context.longitudinal_actuator_delay),
    "value": rows.number(0, 1, 0.01, unit=" seconds", labels={1: "1.00 second"}),
    "widget": "value"
  },
  {
    "description": ("<b>The hardest openpilot is ever allowed to accelerate, no matter which profile you pick.</b><br><br>Lower it for a calmer car " +
                    "everywhere. This caps acceleration only, never braking."),
    "key": "MaxDesiredAcceleration",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Advanced Longitudinal Tuning"),
    "title": "Maximum Acceleration",
    "value": rows.number(0.1, 4, 0.1, unit=" m/s²"),
    "widget": "value"
  },
  {
    "default": lambda context: context.start_accel,
    "description": ("<b>How hard openpilot pulls away in the first moment after a stop.</b><br><br>Raise it for quicker takeoffs. Lower it for " +
                    "gentler ones. Only some cars use a fixed launch push like this, so on most cars this row does nothing."),
    "key": "StartAccel",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Advanced Longitudinal Tuning"),
    "title": lambda context: rows.car_title(context, "Start Acceleration", context.start_accel),
    "value": rows.number(0, 4, 0.01, unit=" m/s²", fast=True),
    "visible": start_accel_visible,
    "widget": "value"
  },
  {
    "default": lambda context: context.v_ego_starting,
    "description": ("<b>The speed that marks the end of pulling away from a stop, which sets how long openpilot holds its takeoff " +
                    "acceleration.</b><br><br>Raise it to keep that takeoff push going up to a higher speed. Lower it to hand back to normal gas and " +
                    "brake control almost as soon as the wheels turn, which makes the start softer. Only some cars have a separate takeoff stage at " +
                    "all, so on most cars this row does nothing."),
    "key": "VEgoStarting",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Advanced Longitudinal Tuning"),
    "title": lambda context: rows.car_title(context, "Start Speed", context.v_ego_starting),
    "value": rows.number(0.01, 1, 0.01, unit=" m/s"),
    "visible": v_ego_starting_visible,
    "widget": "value"
  },
  {
    "default": lambda context: context.stop_accel,
    "description": ("<b>How hard openpilot holds the brakes as your car finishes stopping and while it sits still.</b><br><br>The number is negative " +
                    "because it is braking, so -2.00 holds harder than -0.50. Go more negative if your car creeps or rolls back on a hill, and " +
                    "closer to zero if the last moment of the stop feels too abrupt."),
    "key": "StopAccel",
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE", "Advanced Longitudinal Tuning"),
    "title": lambda context: rows.car_title(context, "Stop Acceleration", context.stop_accel),
    "value": rows.number(-4, 0, 0.01, unit=" m/s²", fast=True),
    "widget": "value"
  },
  {
    "default": lambda context: context.stopping_decel_rate,
    "description": ("<b>How quickly openpilot builds up brake pressure as it comes to a stop.</b><br><br>Raise it for shorter, firmer stops. Lower " +
                    "it for longer, smoother ones."),
    "key": "StoppingDecelRate",
    "order": 5,
    "path": ("Driving Controls", "GAS / BRAKE", "Advanced Longitudinal Tuning"),
    "title": lambda context: rows.car_title(context, "Stopping Rate", context.stopping_decel_rate),
    "value": rows.number(0.001, 12, 0.001, unit=" m/s²/s", fast=True),
    "visible": lambda context: context.level_reason("StoppingDecelRate") or frogs_go_moos_tweak_reason(context),
    "widget": "value"
  },
  {
    "default": lambda context: context.v_ego_stopping,
    "description": ("<b>When the speed openpilot plans to be doing over the next second drops below this, it treats your car as stopped and switches " +
                    "to holding the brakes.</b><br><br>Raise it to settle into the stop earlier and more smoothly. Lower it to keep normal braking " +
                    "going longer, at the risk of rolling past your mark."),
    "key": "VEgoStopping",
    "order": 6,
    "path": ("Driving Controls", "GAS / BRAKE", "Advanced Longitudinal Tuning"),
    "title": lambda context: rows.car_title(context, "Stop Speed", context.v_ego_stopping),
    "value": rows.number(0.01, 1, 0.01, unit=" m/s"),
    "visible": lambda context: context.level_reason("VEgoStopping") or frogs_go_moos_tweak_reason(context),
    "widget": "value"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Conditional Experimental Mode"),
    "description": ("<b>Automatically switch to \"Experimental Mode\" when set conditions are met.</b> Allows the model to handle challenging " +
                    "situations with smarter decision making."),
    "key": "ConditionalExperimental",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE"),
    "title": "Conditional Experimental Mode",
    "widget": "manage"
  },
  {
    "description": ("<b>Switch to \"Experimental Mode\" below this speed when there is no car ahead of you.</b><br><br>It helps openpilot handle " +
                    "slow, fiddly situations more smoothly."),
    "key": "CESpeed",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Conditional Experimental Mode"),
    "title": "Below",
    "value": rows.number(0, 99, unit=" mph", labels={0: "Off"}, fast=True,
                         metric=rows.number(0, 150, unit=" km/h", labels={0: "Off"}, fast=True)),
    "widget": "value"
  },
  {
    "description": ("<b>Switch to \"Experimental Mode\" below this speed when openpilot is tracking a car ahead of you.</b><br><br>This replaces the " +
                    "\"Below\" value whenever there is a car ahead. It switches on once you are moving at walking pace or faster, and can then stay " +
                    "on as you crawl or stop behind that car."),
    "key": "CESpeedLead",
    "order": 0,
    "pair": "CESpeed",
    "path": ("Driving Controls", "GAS / BRAKE", "Conditional Experimental Mode"),
    "title": "With Lead",
    "value": rows.number(0, 99, unit=" mph", labels={0: "Off"}, fast=True,
                         metric=rows.number(0, 150, unit=" km/h", labels={0: "Off"}, fast=True)),
    "widget": "value"
  },
  {
    "description": ("<b>Switch to \"Experimental Mode\" when openpilot sees a curve coming up.</b><br><br>The model picks its own speed for the " +
                    "curve instead of holding your set speed."),
    "key": "CECurves",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Conditional Experimental Mode"),
    "sub_toggles": ({"key": "CECurvesLead", "label": "With Lead"},),
    "title": "Curve Detected Ahead",
    "widget": "button_toggle"
  },
  {
    "description": ("<b>Switch to \"Experimental Mode\" for a predicted stop, except while following a detected lead.</b><br><br>It keeps checking " +
                    "for a possible stop behind that lead and can trigger once the lead is no longer tracked. \"Traffic Mode\" turns this condition " +
                    "off.<br><br><i><b>Disclaimer</b>: openpilot does not explicitly detect traffic lights or stop signs. In \"Experimental Mode\", " +
                    "openpilot makes end-to-end driving decisions from camera input, which means it may stop even when there's no clear reason!</i>"),
    "key": "CEStopLights",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Conditional Experimental Mode"),
    "title": "\"Detected\" Stop Lights/Signs",
    "widget": "toggle"
  },
  {
    "description": ("<b>Switch to \"Experimental Mode\" when you need to brake for the car ahead.</b><br><br>\"Slower Lead\" covers a car that is " +
                    "slowing down or much slower than you, and \"Stopped Lead\" covers a car that is stopped or about to stop. Gently catching up to " +
                    "a slightly slower car stays in normal driving. Both start off, so pick at least one with the buttons on this row or nothing " +
                    "happens."),
    "key": "CELead",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Conditional Experimental Mode"),
    "sub_toggles": ({"key": "CESlowerLead", "label": "Slower Lead"}, {"key": "CEStoppedLead", "label": "Stopped Lead"}),
    "title": "Lead Detected Ahead",
    "widget": "button_toggle"
  },
  {
    "description": ("<b>Switch to \"Experimental Mode\" when you signal below the set speed. Turn on \"Not For Detected Lanes\" to suppress this " +
                    "when an adjacent lane is detected as you start signalling.</b><br><br>With the button off, any signal below the set speed can " +
                    "trigger it. With it on, the space beside you must be narrower than the \"Minimum Lane Width\" under \"Lane Changes\" in the " +
                    "\"STEERING\" panel when you start signalling, or you must be below the \"Minimum Lane Change Speed\". That width starts at " +
                    "zero, so raise it to use lane detection. Lane estimates can miss an adjacent lane."),
    "key": "CESignalSpeed",
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE", "Conditional Experimental Mode"),
    "sub_toggles": ({"key": "CESignalLaneDetection", "label": "Not For Detected Lanes"},),
    "title": "Turn Signal Below",
    "value": rows.number(0, 99, unit=" mph", labels={0: "Off"}, fast=True,
                         metric=rows.number(0, 150, unit=" km/h", labels={0: "Off"}, fast=True)),
    "widget": "value_buttons"
  },
  {
    "description": "<b>Show which condition switched \"Experimental Mode\" on, right on the driving screen.</b>",
    "key": "ShowCEMStatus",
    "order": 5,
    "path": ("Driving Controls", "GAS / BRAKE", "Conditional Experimental Mode"),
    "title": "Status Widget",
    "widget": "toggle"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Curve Speed Controller"),
    "description": ("<b>openpilot slows down on its own for curves ahead, and you pick how fast it takes them with \"Curve Speed " +
                    "Profile\".</b><br><br>It comes set to \"Adaptive\", which learns how you prefer to take curves."),
    "key": "CurveSpeedController",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE"),
    "title": "Curve Speed Controller",
    "widget": "manage"
  },
  {
    "description": ("<b>How hard you corner, learned from your own driving.</b><br><br>The \"Adaptive\" profile uses this to take curves the way you " +
                    "do, but never harder than your steering has proven it can hold. A higher number means carrying more speed through curves. Lower " +
                    "means taking them gentler."),
    "display": calibrated_lateral_acceleration,
    "key": "CalibratedLateralAcceleration",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Curve Speed Controller"),
    "title": "Calibrated Lateral Acceleration",
    "widget": "text"
  },
  {
    "description": ("<b>How much of your own cornering openpilot has learned from.</b><br><br>This only grows while you're the one controlling the " +
                    "speed, so it fills up as you drive curves yourself. At 100% openpilot has gathered enough of your cornering to match the way " +
                    "you take curves."),
    "display": lambda context: f"{context.params.get('CalibrationProgress'):.0f}%",
    "key": "CalibrationProgress",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Curve Speed Controller"),
    "title": "Calibration Progress",
    "widget": "text"
  },
  {
    "description": ("<b>How fast openpilot takes curves.</b><br><br>\"Gentle\" and \"Standard\" hold to a fixed, relaxed pace, \"Sport\" uses your " +
                    "car's maximum configured or learned cornering limit, and \"Adaptive\" learns how you prefer to take curves."),
    "key": "CurveSpeedProfile",
    "options": ({"label": "Gentle", "value": 0}, {"label": "Standard", "value": 1}, {"label": "Sport", "value": 2}, {"label": "Adaptive", "value": 3}),
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Curve Speed Controller"),
    "title": "Curve Speed Profile",
    "widget": "select"
  },
  {
    "description": ("<b>How fast the \"Sport\" profile is allowed to take curves.</b><br><br>This is also the maximum for every other curve speed " +
                    "profile. openpilot learns the limit from your car when possible and otherwise uses the value configured for it."),
    "display": lambda context: f"{context.frogpilot_toggles.maxLateralAccel:.2f} m/s²",
    "key": "MaxLateralAcceleration",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Curve Speed Controller"),
    "title": "Maximum Lateral Acceleration",
    "widget": "text"
  },
  {
    "actions": {
      "RESET": {
        "confirm": {"cancel": "No", "confirm": "Yes", "text": "Are you sure you want to completely reset your curvature data?"},
        "guard": car_on_guard,
        "messages": {"car_on": "Curve data can't be reset while the car is on. Turn the car off and try again."},
        "run": reset_curve_data
      }
    },
    "description": ("<b>Throw away everything openpilot has learned about how you take curves and start over.</b><br><br>\"Adaptive\" goes back to " +
                    "its starting value and relearns as you drive. Only available while the car is off."),
    "id": "ResetCurveData",
    "key": None,
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE", "Curve Speed Controller"),
    "title": "Reset Curve Data",
    "widget": "action"
  },
  {
    "description": ("<b>Show the speed openpilot is aiming for through the curve, right on the driving screen.</b><br><br>It also shows a " +
                    "\"Training...\" note while openpilot is learning from the way you take a curve yourself."),
    "key": "ShowCSCStatus",
    "order": 5,
    "path": ("Driving Controls", "GAS / BRAKE", "Curve Speed Controller"),
    "title": "Status Widget",
    "widget": "toggle"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Driving Personalities"),
    "description": "<b>Change what Aggressive, Standard and Relaxed actually do, so they match how you like to drive.</b>",
    "key": "CustomPersonalities",
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE"),
    "title": "Driving Personalities",
    "visible": custom_personalities_visible,
    "widget": "manage"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Aggressive"),
    "description": "<b>Customize the \"Aggressive\" personality profile.</b> Designed for assertive driving with tighter gaps.",
    "id": "AggressivePersonalityProfile",
    "key": None,
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities"),
    "title": "Aggressive",
    "visible": lambda context: None,
    "widget": "navigation"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Standard"),
    "description": "<b>Customize the \"Standard\" personality profile.</b> Designed for balanced driving with moderate gaps.",
    "id": "StandardPersonalityProfile",
    "key": None,
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities"),
    "title": "Standard",
    "widget": "navigation"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Relaxed"),
    "description": "<b>Customize the \"Relaxed\" personality profile.</b> Designed for smoother, more comfortable driving with larger gaps.",
    "id": "RelaxedPersonalityProfile",
    "key": None,
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities"),
    "title": "Relaxed",
    "visible": lambda context: None,
    "widget": "navigation"
  },
  {
    "description": ("<b>How many seconds of space openpilot keeps behind the car ahead with the \"Aggressive\" profile.</b><br><br>Raise it for more " +
                    "room. Lower it for tighter gaps.<br><br>Default: 1.25 seconds."),
    "key": "AggressiveFollow",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Aggressive"),
    "title": "Following Distance",
    "value": rows.number(1, 3, 0.01, labels=rows.unit_labels(1, 3, 0.01, " second", " seconds"), fast=True),
    "widget": "value"
  },
  {
    "description": ("<b>How smoothly openpilot changes its acceleration any time your car is not slowing down with the \"Aggressive\" profile, not " +
                    "just when pulling away from a stop.</b><br><br>Raise it for gentler starts and pickup. Lower it for faster but more abrupt " +
                    "ones. \"Speed-Up Response\" is a second smoothness control over those same moments, and this one is switched off entirely while " +
                    "your car is sitting still. \"Traffic Mode\" ignores both and uses its own fixed values."),
    "key": "AggressiveJerkAcceleration",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Aggressive"),
    "title": "Acceleration Smoothness",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>How smoothly openpilot changes its braking any time your car is slowing down with the \"Aggressive\" profile, not just when " +
                    "coming to a stop.</b><br><br>Raise it for gentler stops and slowdowns. Lower it for quicker but sharper ones. \"Slowdown " +
                    "Response\" is a second smoothness control over those same moments, and this one is switched off entirely while your car is " +
                    "sitting still. \"Traffic Mode\" ignores both and uses its own fixed values."),
    "key": "AggressiveJerkDeceleration",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Aggressive"),
    "title": "Braking Smoothness",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>How hard openpilot works to protect your following distance with the \"Aggressive\" profile.</b><br><br>Raise it and " +
                    "openpilot reacts sooner and harder when the gap starts closing. Lower it and it tolerates the gap shrinking before doing " +
                    "anything."),
    "key": "AggressiveJerkDanger",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Aggressive"),
    "title": "Safety Gap Bias",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>How smoothly openpilot sheds speed any time your car is slowing down with the \"Aggressive\" profile, like easing off for a " +
                    "slower car ahead.</b><br><br>Raise it for more gradual slowdowns. Lower it for faster but sharper ones. It works alongside " +
                    "\"Braking Smoothness\", a separate control over those same moments. \"Traffic Mode\" ignores both and uses its own fixed values."),
    "key": "AggressiveJerkSpeedDecrease",
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Aggressive"),
    "title": "Slowdown Response",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>How smoothly openpilot builds speed any time your car is not slowing down with the \"Aggressive\" profile, like catching up " +
                    "to your set speed.</b><br><br>Raise it for more gradual pickup. Lower it for quicker but more jolting pickup. It works " +
                    "alongside \"Acceleration Smoothness\", a separate control over those same moments. \"Traffic Mode\" ignores both and uses its " +
                    "own fixed values."),
    "key": "AggressiveJerkSpeed",
    "order": 5,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Aggressive"),
    "title": "Speed-Up Response",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "actions": personality_reset_actions("Aggressive"),
    "description": ("<b>Put every \"Aggressive\" profile value back the way it shipped.</b><br><br>Only affects this profile. Your other profiles " +
                    "are left alone."),
    "id": "ResetAggressivePersonality",
    "key": None,
    "order": 6,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Aggressive"),
    "title": "Reset to Defaults",
    "widget": "action"
  },
  {
    "description": ("<b>How many seconds of space openpilot keeps behind the car ahead with the \"Standard\" profile.</b><br><br>Raise it for more " +
                    "room. Lower it for tighter gaps.<br><br>Default: 1.45 seconds."),
    "key": "StandardFollow",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Standard"),
    "title": "Following Distance",
    "value": rows.number(1, 3, 0.01, labels=rows.unit_labels(1, 3, 0.01, " second", " seconds"), fast=True),
    "widget": "value"
  },
  {
    "description": ("<b>How smoothly openpilot changes its acceleration any time your car is not slowing down with the \"Standard\" profile, not " +
                    "just when pulling away from a stop.</b><br><br>Raise it for gentler starts and pickup. Lower it for faster but more abrupt " +
                    "ones. \"Speed-Up Response\" is a second smoothness control over those same moments, and this one is switched off entirely while " +
                    "your car is sitting still. \"Traffic Mode\" ignores both and uses its own fixed values."),
    "key": "StandardJerkAcceleration",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Standard"),
    "title": "Acceleration Smoothness",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>How smoothly openpilot changes its braking any time your car is slowing down with the \"Standard\" profile, not just when " +
                    "coming to a stop.</b><br><br>Raise it for gentler stops and slowdowns. Lower it for quicker but sharper ones. \"Slowdown " +
                    "Response\" is a second smoothness control over those same moments, and this one is switched off entirely while your car is " +
                    "sitting still. \"Traffic Mode\" ignores both and uses its own fixed values."),
    "key": "StandardJerkDeceleration",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Standard"),
    "title": "Braking Smoothness",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>How hard openpilot works to protect your following distance with the \"Standard\" profile.</b><br><br>Raise it and openpilot " +
                    "reacts sooner and harder when the gap starts closing. Lower it and it tolerates the gap shrinking before doing anything."),
    "key": "StandardJerkDanger",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Standard"),
    "title": "Safety Gap Bias",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>How smoothly openpilot sheds speed any time your car is slowing down with the \"Standard\" profile, like easing off for a " +
                    "slower car ahead.</b><br><br>Raise it for more gradual slowdowns. Lower it for faster but sharper ones. It works alongside " +
                    "\"Braking Smoothness\", a separate control over those same moments. \"Traffic Mode\" ignores both and uses its own fixed values."),
    "key": "StandardJerkSpeedDecrease",
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Standard"),
    "title": "Slowdown Response",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>How smoothly openpilot builds speed any time your car is not slowing down with the \"Standard\" profile, like catching up to " +
                    "your set speed.</b><br><br>Raise it for more gradual pickup. Lower it for quicker but more jolting pickup. It works alongside " +
                    "\"Acceleration Smoothness\", a separate control over those same moments. \"Traffic Mode\" ignores both and uses its own fixed " +
                    "values."),
    "key": "StandardJerkSpeed",
    "order": 5,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Standard"),
    "title": "Speed-Up Response",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "actions": personality_reset_actions("Standard"),
    "description": ("<b>Put every \"Standard\" profile value back the way it shipped.</b><br><br>Only affects this profile. Your other profiles are " +
                    "left alone."),
    "id": "ResetStandardPersonality",
    "key": None,
    "order": 6,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Standard"),
    "title": "Reset to Defaults",
    "widget": "action"
  },
  {
    "description": ("<b>How many seconds of space openpilot keeps behind the car ahead with the \"Relaxed\" profile.</b><br><br>Raise it for more " +
                    "room. Lower it for tighter gaps.<br><br>Default: 1.75 seconds."),
    "key": "RelaxedFollow",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Relaxed"),
    "title": "Following Distance",
    "value": rows.number(1, 3, 0.01, labels=rows.unit_labels(1, 3, 0.01, " second", " seconds"), fast=True),
    "widget": "value"
  },
  {
    "description": ("<b>How smoothly openpilot changes its acceleration any time your car is not slowing down with the \"Relaxed\" profile, not just " +
                    "when pulling away from a stop.</b><br><br>Raise it for gentler starts and pickup. Lower it for faster but more abrupt ones. " +
                    "\"Speed-Up Response\" is a second smoothness control over those same moments, and this one is switched off entirely while your " +
                    "car is sitting still. \"Traffic Mode\" ignores both and uses its own fixed values."),
    "key": "RelaxedJerkAcceleration",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Relaxed"),
    "title": "Acceleration Smoothness",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>How smoothly openpilot changes its braking any time your car is slowing down with the \"Relaxed\" profile, not just when " +
                    "coming to a stop.</b><br><br>Raise it for gentler stops and slowdowns. Lower it for quicker but sharper ones. \"Slowdown " +
                    "Response\" is a second smoothness control over those same moments, and this one is switched off entirely while your car is " +
                    "sitting still. \"Traffic Mode\" ignores both and uses its own fixed values."),
    "key": "RelaxedJerkDeceleration",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Relaxed"),
    "title": "Braking Smoothness",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>How hard openpilot works to protect your following distance with the \"Relaxed\" profile.</b><br><br>Raise it and openpilot " +
                    "reacts sooner and harder when the gap starts closing. Lower it and it tolerates the gap shrinking before doing anything."),
    "key": "RelaxedJerkDanger",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Relaxed"),
    "title": "Safety Gap Bias",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>How smoothly openpilot sheds speed any time your car is slowing down with the \"Relaxed\" profile, like easing off for a " +
                    "slower car ahead.</b><br><br>Raise it for more gradual slowdowns. Lower it for faster but sharper ones. It works alongside " +
                    "\"Braking Smoothness\", a separate control over those same moments. \"Traffic Mode\" ignores both and uses its own fixed values."),
    "key": "RelaxedJerkSpeedDecrease",
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Relaxed"),
    "title": "Slowdown Response",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>How smoothly openpilot builds speed any time your car is not slowing down with the \"Relaxed\" profile, like catching up to " +
                    "your set speed.</b><br><br>Raise it for more gradual pickup. Lower it for quicker but more jolting pickup. It works alongside " +
                    "\"Acceleration Smoothness\", a separate control over those same moments. \"Traffic Mode\" ignores both and uses its own fixed " +
                    "values."),
    "key": "RelaxedJerkSpeed",
    "order": 5,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Relaxed"),
    "title": "Speed-Up Response",
    "value": rows.number(25, 200, unit="%"),
    "widget": "value"
  },
  {
    "actions": personality_reset_actions("Relaxed"),
    "description": ("<b>Put every \"Relaxed\" profile value back the way it shipped.</b><br><br>Only affects this profile. Your other profiles are " +
                    "left alone."),
    "id": "ResetRelaxedPersonality",
    "key": None,
    "order": 6,
    "path": ("Driving Controls", "GAS / BRAKE", "Driving Personalities", "Relaxed"),
    "title": "Reset to Defaults",
    "widget": "action"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Quality of Life"),
    "description": "<b>Smaller changes to how openpilot handles the gas and brake.</b>",
    "key": "QOLLongitudinal",
    "order": 5,
    "path": ("Driving Controls", "GAS / BRAKE"),
    "title": "Quality of Life",
    "widget": "manage"
  },
  {
    "description": ("<b>How much your set speed moves with each tap of the + or - cruise button.</b><br><br>Set it to 1 to land on any speed " +
                    "exactly, or higher to get where you are going in fewer taps. When this is 5 or another multiple of 5, a tap from a set speed in " +
                    "between only goes as far as the next multiple of it, so with 5 a set speed of 63 becomes 65 or 60."),
    "key": "CustomCruise",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life"),
    "title": "Cruise Interval",
    "value": rows.number(1, 99, unit=" mph",
                         metric=rows.number(1, 150, unit=" km/h")),
    "visible": lambda context: without_pcm_cruise_visible(context, "CustomCruise"),
    "widget": "value"
  },
  {
    "description": ("<b>How much your set speed moves while you hold the + or - cruise button down.</b><br><br>The default is 5, against 1 for a " +
                    "single tap. When this is 5 or another multiple of 5, the first step from a set speed in between only goes as far as the next " +
                    "multiple of it, so with 5 a set speed of 63 becomes 65 or 60."),
    "key": "CustomCruiseLong",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life"),
    "title": "Cruise Interval (Hold)",
    "value": rows.number(1, 99, unit=" mph",
                         metric=rows.number(1, 150, unit=" km/h")),
    "visible": lambda context: without_pcm_cruise_visible(context, "CustomCruiseLong"),
    "widget": "value"
  },
  {
    "description": ("<b>openpilot comes to a full stop whenever it thinks it sees a red light or stop sign, whether or not \"Experimental Mode\" is " +
                    "running.</b><br><br>It only kicks in when openpilot is not already tracking a car ahead, so behind a queue at a light your " +
                    "normal following does the stopping instead. Touching the gas cancels a forced stop. \"Traffic Mode\" turns this " +
                    "off.<br><br><i><b>Heads up</b>: openpilot never actually reads traffic lights or stop signs. It decides from what the camera " +
                    "sees, so it can stop when there is no reason to.</i>"),
    "key": "ForceStops",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life"),
    "title": "Force Stop at \"Detected\" Stop Lights/Signs",
    "widget": "toggle"
  },
  {
    "description": ("<b>Adds a set amount of extra room between you and the car ahead, and keeps that room at every speed, not just when you are " +
                    "stopped.</b><br><br>You notice it most at red lights, where a few feet stops openpilot creeping up close. While moving, that " +
                    "same room means openpilot starts slowing a little sooner. \"Traffic Mode\" ignores this setting."),
    "key": "IncreasedStoppedDistance",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life"),
    "title": "Increase Stopped Distance by:",
    "value": rows.number(0, 10, labels=rows.unit_labels(0, 10, 0.1, " foot", " feet", zero="Off"), decimals=1,
                         metric=rows.number(0, 3, labels=rows.unit_labels(0, 3, 0.1, " meter", " meters", zero="Off"), decimals=1)),
    "widget": "value"
  },
  {
    "description": ("<b>Lets your car's \"Eco\" and \"Sport\" gear modes take over how openpilot speeds up, how it slows down, or " +
                    "both.</b><br><br>Pick \"Acceleration\", \"Deceleration\" or both with the buttons on this row, since neither starts on and " +
                    "nothing changes until you do.<br><br>\"Eco\" gear makes openpilot accelerate gently and \"Sport\" gear makes it accelerate " +
                    "firmly. For braking, \"Eco\" gear halves how hard openpilot can brake and \"Sport\" gear uses openpilot's normal braking. The braking " +
                    "change only applies when there is no car ahead."),
    "key": "MapGears",
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life"),
    "sub_toggles": ({"key": "MapAcceleration", "label": "Acceleration"}, {"key": "MapDeceleration", "label": "Deceleration"}),
    "title": "Map Accel/Decel to Gears",
    "visible": lambda context: context.level_reason("MapGears") or context.car_reason(context.can_map_gears),
    "widget": "button_toggle"
  },
  {
    "description": ("<b>Adds an extra amount on top of the \"Cruise Interval (Hold)\" step, but only when you press and hold the + cruise " +
                    "button.</b><br><br>The - button does not mirror it. Holding - takes the normal hold step down and then moves down by one more " +
                    "\"Cruise Interval (Hold)\" amount minus this offset, so with a hold interval of 5 and 5 chosen here you just get the normal " +
                    "hold step down, as you also do with any offset above the hold interval. A quick tap is never affected, and 0 turns this off."),
    "key": "SetSpeedOffset",
    "order": 5,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life"),
    "title": "Offset Set Speed by:",
    "value": rows.number(0, 99, unit=" mph", labels={0: "Off"},
                         metric=rows.number(0, 150, unit=" km/h", labels={0: "Off"})),
    "visible": lambda context: without_pcm_cruise_visible(context, "SetSpeedOffset"),
    "widget": "value"
  },
  {
    "description": ("<b>Every tap of the + cruise button raises your set speed by 5 instead of 1, the same amount you already get from holding " +
                    "it.</b><br><br>Leave it off when you want to land on an exact speed like 63."),
    "key": "ReverseCruise",
    "order": 6,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life"),
    "title": "Reverse Cruise Increase",
    "visible": lambda context: context.level_reason("ReverseCruise") or context.car_reason(context.has_pcm_cruise and context.is_toyota),
    "widget": "toggle"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets"),
    "description": ("<b>openpilot drives more cautiously on its own when the weather turns bad.</b><br><br>It checks the current weather where you " +
                    "are and applies whichever set of adjustments below matches. Every one of those adjustments starts at 0, so a weather icon shows " +
                    "up on the driving screen but nothing about how openpilot drives changes until you open the sets below and put in your own " +
                    "numbers."),
    "key": "WeatherPresets",
    "order": 7,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life"),
    "title": "Weather Condition Offsets",
    "visible": lambda context: context.level_reason("WeatherPresets"),
    "widget": "manage"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Low Visibility"),
    "description": ("<b>How openpilot drives when fog or haze cuts your visibility.</b><br><br>These add to your normal settings rather than " +
                    "replacing them."),
    "id": "LowVisibilityOffsets",
    "key": None,
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets"),
    "title": "Low Visibility",
    "widget": "navigation"
  },
  {
    "description": ("<b>Adds extra seconds of space between you and the car ahead in low visibility.</b><br><br>Your total gap never goes past 3.00 " +
                    "seconds, so this only adds what is left below that."),
    "key": "IncreaseFollowingLowVisibility",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Low Visibility"),
    "title": "Increase Following Distance by:",
    "value": rows.number(0, 3, 0.01, labels=rows.unit_labels(0, 3, 0.01, " second", " seconds"), fast=True),
    "widget": "value"
  },
  {
    "description": ("<b>Adds extra room between you and the car ahead in low visibility, at every speed and not just when stopped.</b><br><br>This " +
                    "stacks on top of the same setting under \"Quality of Life\". \"Traffic Mode\" ignores both."),
    "key": "IncreasedStoppedDistanceLowVisibility",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Low Visibility"),
    "title": "Increase Stopped Distance by:",
    "value": rows.number(0, 10, labels=rows.unit_labels(0, 10, 0.1, " foot", " feet", zero="Off"), decimals=1,
                         metric=rows.number(0, 3, labels=rows.unit_labels(0, 3, 0.1, " meter", " meters", zero="Off"), decimals=1)),
    "widget": "value"
  },
  {
    "description": ("<b>Holds openpilot back from accelerating as hard in low visibility.</b><br><br>This does not apply while \"Experimental Mode\" " +
                    "is running."),
    "key": "ReduceAccelerationLowVisibility",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Low Visibility"),
    "title": "Reduce Acceleration by:",
    "value": rows.number(0, 99, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>Eases off how hard openpilot corners in low visibility.</b><br><br>Only does anything while \"Curve Speed Controller\" is " +
                    "on. Raise it for gentler, safer cornering on a slippery road. Curve speed drops by less than this number, because cornering " +
                    "force rises with the square of speed."),
    "key": "ReduceLateralAccelerationLowVisibility",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Low Visibility"),
    "title": "Reduce Cornering Force by:",
    "value": rows.number(0, 99, unit="%"),
    "widget": "value"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Rain"),
    "description": "<b>How openpilot drives in the rain.</b><br><br>These add to your normal settings rather than replacing them.",
    "id": "RainOffsets",
    "key": None,
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets"),
    "title": "Rain",
    "widget": "navigation"
  },
  {
    "description": ("<b>Adds extra seconds of space between you and the car ahead in rain.</b><br><br>Your total gap never goes past 3.00 seconds, " +
                    "so this only adds what is left below that."),
    "key": "IncreaseFollowingRain",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Rain"),
    "title": "Increase Following Distance by:",
    "value": rows.number(0, 3, 0.01, labels=rows.unit_labels(0, 3, 0.01, " second", " seconds"), fast=True),
    "widget": "value"
  },
  {
    "description": ("<b>Adds extra room between you and the car ahead in rain, at every speed and not just when stopped.</b><br><br>This stacks on " +
                    "top of the same setting under \"Quality of Life\". \"Traffic Mode\" ignores both."),
    "key": "IncreasedStoppedDistanceRain",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Rain"),
    "title": "Increase Stopped Distance by:",
    "value": rows.number(0, 10, labels=rows.unit_labels(0, 10, 0.1, " foot", " feet", zero="Off"), decimals=1,
                         metric=rows.number(0, 3, labels=rows.unit_labels(0, 3, 0.1, " meter", " meters", zero="Off"), decimals=1)),
    "widget": "value"
  },
  {
    "description": ("<b>Holds openpilot back from accelerating as hard in rain.</b><br><br>Raise it for softer, more controlled pickup on a slippery " +
                    "road. This does not apply while \"Experimental Mode\" is running."),
    "key": "ReduceAccelerationRain",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Rain"),
    "title": "Reduce Acceleration by:",
    "value": rows.number(0, 99, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>Eases off how hard openpilot corners in rain.</b><br><br>Only does anything while \"Curve Speed Controller\" is on. Raise it " +
                    "for gentler, safer cornering on a slippery road. Curve speed drops by less than this number, because cornering force rises with " +
                    "the square of speed."),
    "key": "ReduceLateralAccelerationRain",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Rain"),
    "title": "Reduce Cornering Force by:",
    "value": rows.number(0, 99, unit="%"),
    "widget": "value"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Rainstorms"),
    "description": "<b>How openpilot drives in heavy rain.</b><br><br>These add to your normal settings rather than replacing them.",
    "id": "RainStormOffsets",
    "key": None,
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets"),
    "title": "Rainstorms",
    "widget": "navigation"
  },
  {
    "description": ("<b>Adds extra seconds of space between you and the car ahead in a rainstorm.</b><br><br>Your total gap never goes past 3.00 " +
                    "seconds, so this only adds what is left below that."),
    "key": "IncreaseFollowingRainStorm",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Rainstorms"),
    "title": "Increase Following Distance by:",
    "value": rows.number(0, 3, 0.01, labels=rows.unit_labels(0, 3, 0.01, " second", " seconds"), fast=True),
    "widget": "value"
  },
  {
    "description": ("<b>Adds extra room between you and the car ahead in a rainstorm, at every speed and not just when stopped.</b><br><br>This " +
                    "stacks on top of the same setting under \"Quality of Life\". \"Traffic Mode\" ignores both."),
    "key": "IncreasedStoppedDistanceRainStorm",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Rainstorms"),
    "title": "Increase Stopped Distance by:",
    "value": rows.number(0, 10, labels=rows.unit_labels(0, 10, 0.1, " foot", " feet", zero="Off"), decimals=1,
                         metric=rows.number(0, 3, labels=rows.unit_labels(0, 3, 0.1, " meter", " meters", zero="Off"), decimals=1)),
    "widget": "value"
  },
  {
    "description": ("<b>Holds openpilot back from accelerating as hard in a rainstorm.</b><br><br>Raise it for softer, more controlled pickup on a " +
                    "slippery road. This does not apply while \"Experimental Mode\" is running."),
    "key": "ReduceAccelerationRainStorm",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Rainstorms"),
    "title": "Reduce Acceleration by:",
    "value": rows.number(0, 99, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>Eases off how hard openpilot corners in a rainstorm.</b><br><br>Only does anything while \"Curve Speed Controller\" is on. " +
                    "Raise it for gentler, safer cornering on a slippery road. Curve speed drops by less than this number, because cornering force " +
                    "rises with the square of speed."),
    "key": "ReduceLateralAccelerationRainStorm",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Rainstorms"),
    "title": "Reduce Cornering Force by:",
    "value": rows.number(0, 99, unit="%"),
    "widget": "value"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Snow"),
    "description": "<b>How openpilot drives in snow.</b><br><br>These add to your normal settings rather than replacing them.",
    "id": "SnowOffsets",
    "key": None,
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets"),
    "title": "Snow",
    "widget": "navigation"
  },
  {
    "description": ("<b>Adds extra seconds of space between you and the car ahead in snow.</b><br><br>Your total gap never goes past 3.00 seconds, " +
                    "so this only adds what is left below that."),
    "key": "IncreaseFollowingSnow",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Snow"),
    "title": "Increase Following Distance by:",
    "value": rows.number(0, 3, 0.01, labels=rows.unit_labels(0, 3, 0.01, " second", " seconds"), fast=True),
    "widget": "value"
  },
  {
    "description": ("<b>Adds extra room between you and the car ahead in snow, at every speed and not just when stopped.</b><br><br>This stacks on " +
                    "top of the same setting under \"Quality of Life\". \"Traffic Mode\" ignores both."),
    "key": "IncreasedStoppedDistanceSnow",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Snow"),
    "title": "Increase Stopped Distance by:",
    "value": rows.number(0, 10, labels=rows.unit_labels(0, 10, 0.1, " foot", " feet", zero="Off"), decimals=1,
                         metric=rows.number(0, 3, labels=rows.unit_labels(0, 3, 0.1, " meter", " meters", zero="Off"), decimals=1)),
    "widget": "value"
  },
  {
    "description": ("<b>Holds openpilot back from accelerating as hard in snow.</b><br><br>Raise it for softer, more controlled pickup on a slippery " +
                    "road. This does not apply while \"Experimental Mode\" is running."),
    "key": "ReduceAccelerationSnow",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Snow"),
    "title": "Reduce Acceleration by:",
    "value": rows.number(0, 99, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>Eases off how hard openpilot corners in snow.</b><br><br>Only does anything while \"Curve Speed Controller\" is on. Raise it " +
                    "for gentler, safer cornering on a slippery road. Curve speed drops by less than this number, because cornering force rises with " +
                    "the square of speed."),
    "key": "ReduceLateralAccelerationSnow",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets", "Snow"),
    "title": "Reduce Cornering Force by:",
    "value": rows.number(0, 99, unit="%"),
    "widget": "value"
  },
  {
    "description": ("<b>Use your own \"OpenWeatherMap\" key for weather requests every minute.</b><br><br>Requires a One Call API 4.0 subscription. " +
                    "Without a working personal key, FrogPilot tries shared weather with a 15-minute request interval. Your saved key is kept."),
    "id": "SetWeatherKey",
    "key": "WeatherToken",
    "link": "keys",
    "options": ({"label": "ADD / REMOVE", "value": 0}, {"label": "TEST", "value": 1}),
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE", "Quality of Life", "Weather Condition Offsets"),
    "title": "Set Your Own Key",
    "widget": "buttons"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller"),
    "description": ("<b>Hold openpilot's max speed to the posted speed limit.</b><br><br>The limit comes from your downloaded maps, Mapbox, speed " +
                    "limit signs read by the road camera, or, on supported Ford, Genesis, Hyundai, Kia, Lexus and Toyota models, your dashboard."),
    "key": "SpeedLimitController",
    "order": 6,
    "path": ("Driving Controls", "GAS / BRAKE"),
    "title": "Speed Limit Controller",
    "widget": "manage"
  },
  {
    "description": ("<b>The speed used by \"Speed Limit Controller\" when no speed limit is found.</b><br><br>- <b>Set Speed</b>: Use the cruise set " +
                    "speed<br>- <b>Experimental Mode</b>: Let openpilot pick the speed from what the camera sees, never going above your set " +
                    "speed<br>- <b>Previous Limit</b>: Keep using the last confirmed limit"),
    "key": "SLCFallback",
    "options": ({"label": "Set Speed", "value": 0}, {"label": "Experimental Mode", "value": 1}, {"label": "Previous Limit", "value": 2}),
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller"),
    "title": "Fallback Speed",
    "widget": "select"
  },
  {
    "description": ("<b>The speed used by \"Speed Limit Controller\" after you manually drive faster than the posted limit.</b><br><br>- " +
                    "<b>None</b>: Go back to the posted limit as soon as you are off the gas<br>- <b>Set With Gas Pedal</b>: Use the highest speed " +
                    "reached while pressing the gas<br>- <b>Max Set Speed</b>: Use the cruise set speed<br><br>Overrides clear when openpilot " +
                    "disengages."),
    "key": "SLCOverride",
    "options": ({"label": "None", "value": 0}, {"label": "Set With Gas Pedal", "value": 1}, {"label": "Max Set Speed", "value": 2}),
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller"),
    "title": "Override Speed",
    "widget": "select"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Changes"),
    "description": "<b>Smaller changes to how \"Speed Limit Controller\" behaves.</b>",
    "id": "SLCQOL",
    "key": None,
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller"),
    "title": "Speed Limit Changes",
    "widget": "navigation"
  },
  {
    "description": ("<b>Ask before changing to a new speed limit, with the \"Lower Limits\" and \"Higher Limits\" buttons choosing which changes " +
                    "need your approval.</b><br><br>Neither button starts on, so pick at least one or openpilot keeps accepting every new limit " +
                    "without asking. To accept, tap the flashing widget on the driving screen or press the Cruise Increase button. To deny, press " +
                    "Cruise Decrease or ignore it for 30 seconds."),
    "key": "SLCConfirmation",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Changes"),
    "sub_toggles": ({"key": "SLCConfirmationLower", "label": "Lower Limits"}, {"key": "SLCConfirmationHigher", "label": "Higher Limits"}),
    "title": "Confirm New Speed Limits",
    "widget": "button_toggle"
  },
  {
    "description": "<b>How far ahead openpilot looks for a higher speed limit coming up.</b><br><br>This reads from your downloaded map data.",
    "key": "SLCLookaheadHigher",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Changes"),
    "title": "Higher Limit Lookahead Time",
    "value": rows.number(0, 30, unit=" seconds", labels={1: "1 second"}),
    "widget": "value"
  },
  {
    "description": "<b>How far ahead openpilot looks for a lower speed limit coming up.</b><br><br>This reads from your downloaded map data.",
    "key": "SLCLookaheadLower",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Changes"),
    "title": "Lower Limit Lookahead Time",
    "value": rows.number(0, 30, unit=" seconds", labels={1: "1 second"}),
    "widget": "value"
  },
  {
    "description": ("<b>Engaging openpilot sets your max speed to the current speed limit with your \"Speed Limit Offsets\" added on " +
                    "top.</b><br><br>The offsets do not start at zero, so set them all to 0 if you want the max speed to land on the posted number. " +
                    "This only happens when openpilot has no set speed to go back to, since engaging with the Resume or + button brings back your " +
                    "last set speed instead."),
    "key": "SetSpeedLimit",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Changes"),
    "title": "Match Speed Limit on Engage",
    "visible": lambda context: without_pcm_cruise_visible(context, "SetSpeedLimit"),
    "widget": "toggle"
  },
  {
    "description": ("<b>Fall back to Mapbox for the speed limit when none of your chosen sources have one.</b><br><br>Needs your Public Mapbox Key " +
                    "and a working internet connection."),
    "key": "SLCMapboxFiller",
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Changes"),
    "title": "Use Mapbox as Fallback",
    "visible": slc_mapbox_filler_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>Choose which sources openpilot checks for the speed limit and in what order, or have it always use the highest or lowest " +
                    "limit being reported.</b><br><br>Pick up to three sources and openpilot uses the first one that currently has a limit. " +
                    "\"Camera\" is openpilot reading speed limit signs with the road camera. \"Highest\" and \"Lowest\" ignore the order and take " +
                    "the fastest or slowest limit any source reports, so one wrong map entry can hold you well below the posted limit."),
    "display": slc_priority_display,
    "key": "SLCPriority",
    "keys": SLC_PRIORITY_KEYS,
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller"),
    "picker": {
      "fill": "None",
      "steps": (
        {"options": PRIMARY_PRIORITIES, "title": "Select your primary priority"},
        {"options": OTHER_PRIORITIES, "title": "Select your secondary priority"},
        {"options": OTHER_PRIORITIES, "title": "Select your tertiary priority"}
      ),
      "stops": ("Highest", "Lowest", "None")
    },
    "title": "Speed Limit Source Priority",
    "widget": "action"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Offsets"),
    "description": "<b>Drive a set amount above or below the posted speed limit.</b><br><br>Each speed range below gets its own offset.",
    "id": "SLCOffsets",
    "key": None,
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller"),
    "title": "Speed Limit Offsets",
    "widget": "navigation"
  },
  {
    "description": "<b>How far above or below the posted limit openpilot drives on roads posted between 0 and 24 mph.</b>",
    "description_metric": "<b>How far above or below the posted limit openpilot drives on roads posted between 0 and 29 km/h.</b>",
    "key": "Offset1",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Offsets"),
    "title": "Speed Offset (0–24 mph)",
    "title_metric": "Speed Offset (0–29 km/h)",
    "value": rows.number(-99, 99, unit=" mph", labels={0: "Off"},
                         metric=rows.number(-150, 150, unit=" km/h", labels={0: "Off"})),
    "widget": "value"
  },
  {
    "description": "<b>How far above or below the posted limit openpilot drives on roads posted between 25 and 34 mph.</b>",
    "description_metric": "<b>How far above or below the posted limit openpilot drives on roads posted between 30 and 49 km/h.</b>",
    "key": "Offset2",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Offsets"),
    "title": "Speed Offset (25–34 mph)",
    "title_metric": "Speed Offset (30–49 km/h)",
    "value": rows.number(-99, 99, unit=" mph", labels={0: "Off"},
                         metric=rows.number(-150, 150, unit=" km/h", labels={0: "Off"})),
    "widget": "value"
  },
  {
    "description": "<b>How far above or below the posted limit openpilot drives on roads posted between 35 and 44 mph.</b>",
    "description_metric": "<b>How far above or below the posted limit openpilot drives on roads posted between 50 and 59 km/h.</b>",
    "key": "Offset3",
    "order": 2,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Offsets"),
    "title": "Speed Offset (35–44 mph)",
    "title_metric": "Speed Offset (50–59 km/h)",
    "value": rows.number(-99, 99, unit=" mph", labels={0: "Off"},
                         metric=rows.number(-150, 150, unit=" km/h", labels={0: "Off"})),
    "widget": "value"
  },
  {
    "description": "<b>How far above or below the posted limit openpilot drives on roads posted between 45 and 54 mph.</b>",
    "description_metric": "<b>How far above or below the posted limit openpilot drives on roads posted between 60 and 79 km/h.</b>",
    "key": "Offset4",
    "order": 3,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Offsets"),
    "title": "Speed Offset (45–54 mph)",
    "title_metric": "Speed Offset (60–79 km/h)",
    "value": rows.number(-99, 99, unit=" mph", labels={0: "Off"},
                         metric=rows.number(-150, 150, unit=" km/h", labels={0: "Off"})),
    "widget": "value"
  },
  {
    "description": "<b>How far above or below the posted limit openpilot drives on roads posted between 55 and 64 mph.</b>",
    "description_metric": "<b>How far above or below the posted limit openpilot drives on roads posted between 80 and 99 km/h.</b>",
    "key": "Offset5",
    "order": 4,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Offsets"),
    "title": "Speed Offset (55–64 mph)",
    "title_metric": "Speed Offset (80–99 km/h)",
    "value": rows.number(-99, 99, unit=" mph", labels={0: "Off"},
                         metric=rows.number(-150, 150, unit=" km/h", labels={0: "Off"})),
    "widget": "value"
  },
  {
    "description": "<b>How far above or below the posted limit openpilot drives on roads posted between 65 and 74 mph.</b>",
    "description_metric": "<b>How far above or below the posted limit openpilot drives on roads posted between 100 and 119 km/h.</b>",
    "key": "Offset6",
    "order": 5,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Offsets"),
    "title": "Speed Offset (65–74 mph)",
    "title_metric": "Speed Offset (100–119 km/h)",
    "value": rows.number(-99, 99, unit=" mph", labels={0: "Off"},
                         metric=rows.number(-150, 150, unit=" km/h", labels={0: "Off"})),
    "widget": "value"
  },
  {
    "description": "<b>How far above or below the posted limit openpilot drives on roads posted between 75 and 99 mph.</b>",
    "description_metric": "<b>How far above or below the posted limit openpilot drives on roads posted between 120 and 140 km/h.</b>",
    "key": "Offset7",
    "order": 6,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Speed Limit Offsets"),
    "title": "Speed Offset (75–99 mph)",
    "title_metric": "Speed Offset (120–140 km/h)",
    "value": rows.number(-99, 99, unit=" mph", labels={0: "Off"},
                         metric=rows.number(-150, 150, unit=" km/h", labels={0: "Off"})),
    "widget": "value"
  },
  {
    "children": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Visual Settings"),
    "description": "<b>Change how \"Speed Limit Controller\" appears on the driving screen.</b>",
    "id": "SLCVisuals",
    "key": None,
    "order": 5,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller"),
    "title": "Visual Settings",
    "widget": "navigation"
  },
  {
    "description": ("<b>The speed limit sign on the driving screen shows the posted limit with your offset printed underneath it.</b><br><br>With " +
                    "this off, the offset is added into the number on the sign instead, so a 65 mph road with a +10 offset shows 75 rather than 65."),
    "key": "ShowSLCOffset",
    "order": 0,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Visual Settings"),
    "title": "Show Speed Limit Offset",
    "widget": "toggle"
  },
  {
    "description": ("<b>Show every speed limit source and what each one currently reports, on the driving screen.</b><br><br>Useful for working out " +
                    "which source to trust before setting your priority order."),
    "key": "SpeedLimitSources",
    "order": 1,
    "path": ("Driving Controls", "GAS / BRAKE", "Speed Limit Controller", "Visual Settings"),
    "title": "Show Speed Limit Sources",
    "widget": "toggle"
  }
]
