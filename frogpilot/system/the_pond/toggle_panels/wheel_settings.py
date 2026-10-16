from openpilot.frogpilot.common import frogpilot_variables

ALWAYS_AVAILABLE_FUNCTIONS = (frogpilot_variables.BUTTON_FUNCTIONS["NOTHING"], frogpilot_variables.BUTTON_FUNCTIONS["PAUSE_LATERAL"])


def button_option(value, label):
  if value in ALWAYS_AVAILABLE_FUNCTIONS:
    return {"label": label, "value": value}
  return {"label": label, "value": value, "visible": longitudinal_reason}


def lkas_button_visible(context):
  lkas_used_for_lateral = context.params.get_bool("AlwaysOnLateral") and context.params.get_bool("AlwaysOnLateralLKAS")

  reason = context.level_reason("LKASButtonControl") or context.car_reason(context.has_lkas_button)
  return reason or context.setting_reason(not context.lkas_allowed_for_aol or not lkas_used_for_lateral, "AlwaysOnLateralLKAS", wanted=False)


def longitudinal_reason(context):
  return context.car_reason(context.has_openpilot_longitudinal)


BUTTON_OPTIONS = tuple(button_option(value, label) for value, label in frogpilot_variables.BUTTON_FUNCTION_LABELS.items())

ROWS = [
  {
    "description": ("<b>Action performed when the \"Distance\" button is pressed.</b><br><br>On GM cars with adaptive cruise, changing the " +
                    "\"Personality Profile\" takes a second press within 3.5 seconds of the first."),
    "key": "DistanceButtonControl",
    "options": BUTTON_OPTIONS,
    "order": 0,
    "path": ("Vehicle Settings", "WHEEL BUTTONS"),
    "picker": {"title": "Select a function to assign to this button"},
    "title": "Distance Button",
    "widget": "select"
  },
  {
    "description": ("<b>Action performed when the \"Distance\" button is pressed for more than 0.5 seconds.</b><br><br>On GM cars the hold is 0.75 " +
                    "seconds instead."),
    "key": "LongDistanceButtonControl",
    "options": BUTTON_OPTIONS,
    "order": 1,
    "path": ("Vehicle Settings", "WHEEL BUTTONS"),
    "picker": {"title": "Select a function to assign to this button"},
    "title": "Distance Button (Long Press)",
    "widget": "select"
  },
  {
    "description": "<b>Action performed when the \"Distance\" button is pressed for more than 2.5 seconds.</b>",
    "key": "VeryLongDistanceButtonControl",
    "options": BUTTON_OPTIONS,
    "order": 2,
    "path": ("Vehicle Settings", "WHEEL BUTTONS"),
    "picker": {"title": "Select a function to assign to this button"},
    "title": "Distance Button (Very Long Press)",
    "widget": "select"
  },
  {
    "description": "<b>Action performed when the \"LKAS\" button is pressed.</b>",
    "key": "LKASButtonControl",
    "options": BUTTON_OPTIONS,
    "order": 3,
    "path": ("Vehicle Settings", "WHEEL BUTTONS"),
    "picker": {"title": "Select a function to assign to this button"},
    "title": "LKAS Button",
    "visible": lkas_button_visible,
    "widget": "select"
  }
]
