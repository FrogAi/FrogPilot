import importlib.util
import types

from cereal import car, custom, log
from openpilot.common.params import Params
from openpilot.selfdrive.selfdrived import events
from openpilot.selfdrive.selfdrived.events import ET, FrogPilotEventName
from openpilot.system.hardware import HARDWARE

from openpilot.frogpilot.controls.lib.frogpilot_events import FrogPilotEvents


def test_comma_four_default_text(monkeypatch):
  monkeypatch.setattr(HARDWARE, "get_device_type", lambda: "mici")
  spec = importlib.util.spec_from_file_location("comma_four_events", events.__file__)
  comma_four_events = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(comma_four_events)

  comma_four_alert = comma_four_events.FROGPILOT_EVENTS[FrogPilotEventName.customStartupAlert][ET.PERMANENT]
  comma_three_alert = events.FROGPILOT_EVENTS[FrogPilotEventName.customStartupAlert][ET.PERMANENT]
  params = Params()
  default_bottom = params.get_default_value("StartupMessageBottom")
  default_top = params.get_default_value("StartupMessageTop")

  cases = (
    (default_top, default_bottom, ("Buckle Up!", "Frog-approved 🐸")),
    ("Custom top", default_bottom, ("Custom top", default_bottom)),
    (default_top, "Custom bottom", (default_top, "Custom bottom")),
    ("Custom top", "Custom bottom", ("Custom top", "Custom bottom")),
  )

  for top, bottom, expected in cases:
    toggles = types.SimpleNamespace(startup_alert_bottom=bottom, startup_alert_top=top)

    comma_four = comma_four_alert(car.CarParams.new_message(), car.CarState.new_message(), {}, False, 0, None, toggles)
    comma_three = comma_three_alert(car.CarParams.new_message(), car.CarState.new_message(), {}, False, 0, None, toggles)

    assert (comma_four.alert_text_1, comma_four.alert_text_2) == expected, (top, bottom)
    assert (comma_three.alert_text_1, comma_three.alert_text_2) == (top, bottom), (top, bottom)


def test_holiday_alert_after_comma_four_startup():
  planner = types.SimpleNamespace(frogpilot_vcruise=types.SimpleNamespace(forcing_stop=False, slc=types.SimpleNamespace(speed_limit_changed_timer=0)),
                                  lead_one=types.SimpleNamespace(status=False), lead_relevant=False, params=types.SimpleNamespace(get=lambda key: None))
  toggles = types.SimpleNamespace(current_holiday_theme="halloween", nnff=False, random_events=False, speed_limit_changed_alert=False)
  events_tracker = FrogPilotEvents(planner, None)

  def update(frogpilotSelfdriveState):
    sm = {
      "carControl": car.CarControl.new_message(),
      "carState": car.CarState.new_message(),
      "frogpilotCarState": custom.FrogPilotCarState.new_message(),
      "frogpilotModelV2": custom.FrogPilotModelDataV2.new_message(),
      "frogpilotSelfdriveState": frogpilotSelfdriveState,
      "selfdriveState": log.SelfdriveState.new_message(),
    }
    events_tracker.update(False, sm, toggles)

    return FrogPilotEventName.holidayActive in events_tracker.events.names

  startup = custom.FrogPilotSelfdriveState.new_message(alertText1="Buckle Up!", alertText2="Frog-approved 🐸", alertType="customStartupAlert/permanent")

  during_startup = update(startup)
  after_startup = update(custom.FrogPilotSelfdriveState.new_message())

  assert not during_startup
  assert after_startup
