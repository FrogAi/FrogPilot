from openpilot.frogpilot.system.the_pond.toggle_panels import rows


def advanced_lateral_tune_changed(context, old_value, new_value):
  return context.started and not context.is_angle_car and context.params.get_bool("ForceTorqueController")


def auto_tune_reason(context):
  if context.has_auto_tune:
    return context.setting_reason(context.params.get_bool("ForceAutoTuneOff"), "ForceAutoTuneOff")
  return context.setting_reason(not is_forcing_auto_tune(context), "ForceAutoTune", wanted=False)


def force_auto_tune_visible(context):
  torque_steering = context.is_torque_car or is_forcing_torque_controller(context) or is_using_nnff(context)

  reason = context.level_reason("ForceAutoTune") or context.car_reason(not context.has_auto_tune) or context.car_reason(not context.is_angle_car)
  return reason or context.setting_reason(torque_steering, "ForceTorqueController")


def force_torque_controller_visible(context):
  reason = context.level_reason("ForceTorqueController") or context.car_reason(not context.is_angle_car)
  return reason or context.car_reason(not context.is_torque_car or is_forcing_torque_controller(context))


def is_forcing_auto_tune(context):
  torque_steering = context.is_torque_car or is_forcing_torque_controller(context) or is_using_nnff(context)
  return not context.has_auto_tune and not context.is_angle_car and torque_steering and context.params.get_bool("ForceAutoTune")


def is_forcing_torque_controller(context):
  return not context.is_angle_car and context.params.get_bool("ForceTorqueController")


def is_using_nnff(context):
  return context.has_nnff_log and context.params.get_bool("LateralTune") and context.params.get_bool("NNFF")


def lane_change_time_visible(context):
  return context.level_reason("LaneChangeTime") or context.setting_reason(context.params.get_bool("NudgelessLaneChange"), "NudgelessLaneChange")


def lane_detection_width_visible(context):
  shown = context.params.get_bool("NudgelessLaneChange") or context.frogpilot_toggles.conditional_signal_lane_detection
  return context.level_reason("LaneDetectionWidth") or context.setting_reason(shown, "NudgelessLaneChange")


def lateral_tune_changed(context, old_value, new_value):
  using_nnff = context.has_nnff_log and (context.level < context.tuning_level("NNFF") or context.params.get_bool("NNFF"))
  using_nnff_lite = context.level < context.tuning_level("NNFFLite") or context.params.get_bool("NNFFLite")
  return context.started and not context.is_angle_car and (using_nnff or using_nnff_lite)


def lateral_tune_visible(context):
  children_shown = context.panel_reason(("Driving Controls", "STEERING", "Lateral Tuning")) is None
  return context.level_reason("LateralTune") or context.car_reason(not context.is_angle_car or children_shown)


def nnff_lite_visible(context):
  reason = context.level_reason("NNFFLite") or context.setting_reason(not is_using_nnff(context), "NNFF", wanted=False)
  return reason or context.car_reason(not context.is_angle_car)


def reboot_when_started(context, old_value, new_value):
  return context.started


def steer_kp_visible(context):
  torque_steering = context.is_torque_car or is_forcing_torque_controller(context) or is_using_nnff(context)

  reason = context.level_reason("SteerKP") or context.car_reason(context.steer_kp != 0)
  return reason or context.setting_reason(torque_steering, "ForceTorqueController") or context.car_reason(not context.is_angle_car)


def steer_ratio_visible(context):
  reason = context.level_reason("SteerRatio") or context.car_reason(context.steer_ratio != 0)
  return reason or context.setting_reason(not is_forcing_auto_tune(context), "ForceAutoTune", wanted=False)


def steer_torque_visible(context, key, car_value):
  torque_steering = context.is_torque_car or is_forcing_torque_controller(context)

  reason = context.level_reason(key) or context.car_reason(car_value != 0) or auto_tune_reason(context)
  return reason or context.setting_reason(torque_steering, "ForceTorqueController") or context.setting_reason(not is_using_nnff(context), "NNFF", wanted=False)


ROWS = [
  {
    "children": ("Driving Controls", "STEERING", "Advanced Lateral Tuning"),
    "description": ("<b>Hand-set the steering numbers openpilot normally works out for itself, and switch that learning on or off.</b><br><br>Wrong " +
                    "values show up as a wheel that feels twitchy or lazy. Every number has a \"Reset\" button that puts your car's original value " +
                    "back."),
    "key": "AdvancedLateralTune",
    "on_change": advanced_lateral_tune_changed,
    "order": 0,
    "path": ("Driving Controls", "STEERING"),
    "title": "Advanced Lateral Tuning",
    "visible": lambda context: context.level_reason("AdvancedLateralTune"),
    "widget": "manage"
  },
  {
    "buttons": ("Reset",),
    "default": lambda context: context.steer_actuator_delay,
    "description": ("<b>How long your car takes to respond after openpilot turns the wheel.</b><br><br>Raise it if your car reacts late. Lower it if " +
                    "the steering feels jumpy. openpilot learns this on its own by default."),
    "key": "SteerDelay",
    "order": 0,
    "path": ("Driving Controls", "STEERING", "Advanced Lateral Tuning"),
    "reset_confirm": {"cancel": "No", "confirm": "Yes", "text": "Reset <b>Actuator Delay</b> to its default value?"},
    "title": lambda context: rows.car_title(context, "Actuator Delay", context.steer_actuator_delay),
    "value": rows.number(0.01, 1, 0.01),
    "visible": lambda context: context.level_reason("SteerDelay") or context.car_reason(context.steer_actuator_delay != 0),
    "widget": "value_buttons"
  },
  {
    "buttons": ("Reset",),
    "default": lambda context: context.friction,
    "description": ("<b>How much extra effort openpilot uses to get the wheel moving off center.</b><br><br>Raise it if the wheel sticks near center " +
                    "and openpilot is slow to start correcting. Lower it if the wheel jitters on a straight road."),
    "key": "SteerFriction",
    "order": 1,
    "path": ("Driving Controls", "STEERING", "Advanced Lateral Tuning"),
    "reset_confirm": {"cancel": "No", "confirm": "Yes", "text": "Reset <b>Friction</b> to its default value?"},
    "title": lambda context: rows.car_title(context, "Friction", context.friction),
    "value": rows.number(0.01, 1, 0.01),
    "visible": lambda context: steer_torque_visible(context, "SteerFriction", context.friction),
    "widget": "value_buttons"
  },
  {
    "buttons": ("Reset",),
    "default": lambda context: context.steer_kp,
    "description": ("<b>How hard openpilot pushes the wheel to pull your car back to the middle of the lane.</b><br><br>Raise it if your car sits " +
                    "off to one side or is slow to come back. Lower it if the wheel feels twitchy or keeps overshooting. openpilot never changes " +
                    "this one on its own."),
    "key": "SteerKP",
    "order": 2,
    "path": ("Driving Controls", "STEERING", "Advanced Lateral Tuning"),
    "reset_confirm": {"cancel": "No", "confirm": "Yes", "text": "Reset <b>Kp Factor</b> to its default value?"},
    "title": lambda context: rows.car_title(context, "Kp Factor", context.steer_kp),
    "value": rows.number(lambda context: context.steer_kp * 0.5, lambda context: context.steer_kp * 1.5, 0.01),
    "visible": steer_kp_visible,
    "widget": "value_buttons"
  },
  {
    "buttons": ("Reset",),
    "default": lambda context: context.lat_accel_factor,
    "description": ("<b>How much steering effort openpilot uses to turn your car, where lower values make it steer harder and higher values make it " +
                    "steer more gently.</b><br><br>Lower it if your car drifts wide in curves. Raise it if the car turns in more sharply than you " +
                    "want."),
    "key": "SteerLatAccel",
    "order": 3,
    "path": ("Driving Controls", "STEERING", "Advanced Lateral Tuning"),
    "reset_confirm": {"cancel": "No", "confirm": "Yes", "text": "Reset <b>Lateral Acceleration</b> to its default value?"},
    "title": lambda context: rows.car_title(context, "Lateral Acceleration", context.lat_accel_factor),
    "value": rows.number(lambda context: context.lat_accel_factor * 0.5, lambda context: context.lat_accel_factor * 1.5, 0.01),
    "visible": lambda context: steer_torque_visible(context, "SteerLatAccel", context.lat_accel_factor),
    "widget": "value_buttons"
  },
  {
    "buttons": ("Reset",),
    "default": lambda context: context.steer_ratio,
    "description": ("<b>How far your steering wheel turns to swing the front wheels a set amount, where raising it makes openpilot turn the wheel " +
                    "further for the same corner.</b><br><br>Lower it if openpilot feels twitchy or keeps overshooting the middle of the lane. Raise " +
                    "it if openpilot reacts too slowly and lets the car drift wide. openpilot learns this on its own by default."),
    "key": "SteerRatio",
    "order": 4,
    "path": ("Driving Controls", "STEERING", "Advanced Lateral Tuning"),
    "reset_confirm": {"cancel": "No", "confirm": "Yes", "text": "Reset <b>Steer Ratio</b> to its default value?"},
    "title": lambda context: rows.car_title(context, "Steer Ratio", context.steer_ratio),
    "value": rows.number(lambda context: context.steer_ratio * 0.5, lambda context: context.steer_ratio * 1.5, 0.01),
    "visible": steer_ratio_visible,
    "widget": "value_buttons"
  },
  {
    "description": ("<b>Let openpilot work out its own steering values on a car that doesn't do this automatically.</b><br><br>What openpilot learns " +
                    "replaces the numbers you set, so those rows disappear while this is on."),
    "key": "ForceAutoTune",
    "order": 5,
    "path": ("Driving Controls", "STEERING", "Advanced Lateral Tuning"),
    "title": "Force Auto-Tune On",
    "visible": force_auto_tune_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>Use the friction, lateral acceleration, and steer ratio you set instead of the values openpilot works out on its " +
                    "own.</b><br><br>Only offered on cars that normally tune themselves."),
    "key": "ForceAutoTuneOff",
    "order": 6,
    "path": ("Driving Controls", "STEERING", "Advanced Lateral Tuning"),
    "title": "Force Auto-Tune Off",
    "visible": lambda context: context.level_reason("ForceAutoTuneOff") or context.car_reason(context.has_auto_tune),
    "widget": "toggle"
  },
  {
    "description": ("<b>Switch openpilot to steering by effort instead of by wheel angle, which usually holds the lane more smoothly through " +
                    "curves.</b><br><br>Only offered on cars that don't already steer this way. Changing this while driving asks you to reboot."),
    "key": "ForceTorqueController",
    "on_change": reboot_when_started,
    "order": 7,
    "path": ("Driving Controls", "STEERING", "Advanced Lateral Tuning"),
    "title": "Force Torque Controller",
    "visible": force_torque_controller_visible,
    "widget": "toggle"
  },
  {
    "children": ("Driving Controls", "STEERING", "Always On Lateral"),
    "description": ("<b>openpilot keeps steering for you even when it isn't controlling the gas and brake, so it holds your lane when you press a " +
                    "pedal, cancel, or haven't engaged openpilot at all.</b><br><br>It steers from the moment your car's cruise control is switched " +
                    "on until you switch that back off or shift out of drive, and it pauses while you brake below the speed set in \"Pause on Brake " +
                    "Press Below\", except at a full stop. On the newer Hyundai, Kia, and Genesis cars, the LKAS button takes the place of cruise " +
                    "control while \"Enable With LKAS\" is on."),
    "key": "AlwaysOnLateral",
    "on_change": lambda context, old_value, new_value: context.started and new_value,
    "order": 1,
    "path": ("Driving Controls", "STEERING"),
    "title": "Always On Lateral",
    "visible": lambda context: context.level_reason("AlwaysOnLateral"),
    "widget": "manage"
  },
  {
    "description": ("<b>Use the LKAS button to arm steering, so openpilot keeps steering even when it is not engaged.</b><br><br>openpilot does not " +
                    "read your car's LKAS status for this. It starts every drive disarmed and each press of the LKAS button flips it, so expect one " +
                    "press after starting the car. With this off, the LKAS button is free to reassign under \"LKAS Button\", and steering stops as " +
                    "soon as openpilot is no longer engaged, unless openpilot handles the gas and brake, in which case the cruise control button " +
                    "arms it instead."),
    "key": "AlwaysOnLateralLKAS",
    "order": 0,
    "path": ("Driving Controls", "STEERING", "Always On Lateral"),
    "title": "Enable With LKAS",
    "visible": lambda context: context.level_reason("AlwaysOnLateralLKAS") or context.car_reason(context.lkas_allowed_for_aol),
    "widget": "toggle"
  },
  {
    "description": "<b>Pause \"Always On Lateral\" below the set speed while the brake pedal is pressed, except at a full stop.</b>",
    "key": "PauseAOLOnBrake",
    "order": 1,
    "path": ("Driving Controls", "STEERING", "Always On Lateral"),
    "title": "Pause on Brake Press Below",
    "value": rows.number(0, 99, unit=" mph", labels={0: "Off"}, fast=True,
                         metric=rows.number(0, 150, unit=" km/h", labels={0: "Off"}, fast=True)),
    "widget": "value"
  },
  {
    "children": ("Driving Controls", "STEERING", "Lane Changes"),
    "description": "<b>Allow openpilot to change lanes.</b>",
    "key": "LaneChanges",
    "order": 2,
    "path": ("Driving Controls", "STEERING"),
    "title": "Lane Changes",
    "visible": lambda context: context.level_reason("LaneChanges"),
    "widget": "manage"
  },
  {
    "description": ("<b>With your turn signal on, openpilot starts the lane change on its own instead of waiting for a small push on the wheel from " +
                    "you.</b><br><br>It waits out \"Lane Change Delay\" before moving over, skips the move below the minimum lane change speed, and " +
                    "stays out of lanes narrower than any \"Minimum Lane Width\" you set. It only holds off for a car beside you if your car came " +
                    "with factory blind spot monitoring, so without that hardware there is no blind spot check at all. Check that the lane is clear " +
                    "yourself before you signal."),
    "key": "NudgelessLaneChange",
    "order": 0,
    "path": ("Driving Controls", "STEERING", "Lane Changes"),
    "title": "Automatic Lane Changes",
    "widget": "toggle"
  },
  {
    "description": "<b>Delay between turn signal activation and the start of an automatic lane change.</b>",
    "key": "LaneChangeTime",
    "order": 1,
    "path": ("Driving Controls", "STEERING", "Lane Changes"),
    "title": "Lane Change Delay",
    "value": rows.number(0, 5, 0.1, labels=rows.unit_labels(0, 5, 0.1, " second", " seconds", zero="Instant")),
    "visible": lane_change_time_visible,
    "widget": "value"
  },
  {
    "description": ("<b>The slowest speed at which openpilot will change lanes for you.</b><br><br>Below this speed you steer into the lane change " +
                    "yourself. Set it to \"Any speed\" to let openpilot change lanes at any speed, but that also switches off \"Steer Into Turns " +
                    "Below Lane Change Speed\", which only ever runs below this number."),
    "key": "MinimumLaneChangeSpeed",
    "order": 2,
    "path": ("Driving Controls", "STEERING", "Lane Changes"),
    "title": "Minimum Lane Change Speed",
    "value": rows.number(0, 99, unit=" mph", labels={0: "Any speed"}, fast=True,
                         metric=rows.number(0, 150, unit=" km/h", labels={0: "Any speed"}, fast=True)),
    "widget": "value"
  },
  {
    "description": "<b>Prevent automatic lane changes into lanes narrower than the set width.</b>",
    "key": "LaneDetectionWidth",
    "order": 3,
    "path": ("Driving Controls", "STEERING", "Lane Changes"),
    "title": "Minimum Lane Width",
    "value": rows.number(0, 15, 0.1, labels=rows.unit_labels(0, 15, 0.1, " foot", " feet", zero="Off"), fast=True,
                         metric=rows.number(0, 5, 0.1, labels=rows.unit_labels(0, 5, 0.1, " meter", " meters", zero="Off"), fast=True)),
    "visible": lane_detection_width_visible,
    "widget": "value"
  },
  {
    "description": ("<b>Only one lane change per turn signal.</b><br><br>Switch the signal off and back on to change lanes again. Lane changes you " +
                    "start yourself by pushing the wheel count toward this too."),
    "key": "OneLaneChange",
    "order": 4,
    "path": ("Driving Controls", "STEERING", "Lane Changes"),
    "title": "One Lane Change Per Signal",
    "widget": "toggle"
  },
  {
    "children": ("Driving Controls", "STEERING", "Lateral Tuning"),
    "description": ("<b>Switch openpilot's steering over to a neural network for a smoother wheel on cars that support it, and have it steer into " +
                    "turns when you signal below your minimum lane change speed.</b>"),
    "key": "LateralTune",
    "on_change": lateral_tune_changed,
    "order": 3,
    "path": ("Driving Controls", "STEERING"),
    "title": "Lateral Tuning",
    "visible": lateral_tune_visible,
    "widget": "manage"
  },
  {
    "description": ("<b>openpilot steers using a model trained on real driving data from cars like yours, which usually holds the lane more steadily " +
                    "and tracks curves more closely.</b><br><br>It only appears when a trained model matches your car. While it's on, the " +
                    "\"Friction\" and \"Lateral Acceleration\" rows disappear, but some car models still use the friction value. Built by Twilsonco."),
    "key": "NNFF",
    "on_change": reboot_when_started,
    "order": 0,
    "path": ("Driving Controls", "STEERING", "Lateral Tuning"),
    "title": "Neural Network Feedforward (NNFF)",
    "visible": lambda context: context.level_reason("NNFF") or context.car_reason(context.has_nnff_log) or context.car_reason(not context.is_angle_car),
    "widget": "toggle"
  },
  {
    "description": ("<b>openpilot looks ahead at how sharply the road is about to bend and starts adjusting its steering early, which smooths how it " +
                    "enters and leaves curves.</b><br><br>Use this when the full \"Neural Network Feedforward (NNFF)\" setting isn't offered for " +
                    "your car. It borrows the look-ahead without the trained model, so the improvement is smaller. On cars that normally use " +
                    "openpilot's PID steering, this also switches them to the torque controller."),
    "key": "NNFFLite",
    "on_change": reboot_when_started,
    "order": 1,
    "path": ("Driving Controls", "STEERING", "Lateral Tuning"),
    "title": "Neural Network Feedforward (NNFF) Lite",
    "visible": nnff_lite_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>With your turn signal on below your minimum lane change speed, openpilot steers with you into the turn instead of pulling " +
                    "back toward the lane you're leaving.</b><br><br>Use it for intersections and tight exit ramps, where openpilot normally works " +
                    "against your steering. You still choose where and when to turn."),
    "key": "TurnDesires",
    "order": 2,
    "path": ("Driving Controls", "STEERING", "Lateral Tuning"),
    "title": "Steer Into Turns Below Lane Change Speed",
    "widget": "toggle"
  },
  {
    "children": ("Driving Controls", "STEERING", "Quality of Life"),
    "description": "<b>Hand the wheel back to yourself below a speed you set.</b>",
    "key": "QOLLateral",
    "order": 4,
    "path": ("Driving Controls", "STEERING"),
    "title": "Quality of Life",
    "visible": lambda context: context.level_reason("QOLLateral"),
    "widget": "manage"
  },
  {
    "description": ("<b>openpilot stops steering below the speed you set and hands the wheel back to you, and \"Turn Signal Only\" narrows that to " +
                    "just the moments a turn signal is flashing.</b><br><br>The gas and brake carry on as normal and nothing warns you when the " +
                    "steering stops, so be ready to take the wheel at low speed. This pauses \"Always On Lateral\" too."),
    "key": "PauseLateralSpeed",
    "order": 0,
    "path": ("Driving Controls", "STEERING", "Quality of Life"),
    "sub_toggles": ({"key": "PauseLateralOnSignal", "label": "Turn Signal Only"},),
    "title": "Pause Steering Below",
    "value": rows.number(0, 99, unit=" mph", labels={0: "Off"}, fast=True,
                         metric=rows.number(0, 150, unit=" km/h", labels={0: "Off"}, fast=True)),
    "widget": "value_buttons"
  }
]
