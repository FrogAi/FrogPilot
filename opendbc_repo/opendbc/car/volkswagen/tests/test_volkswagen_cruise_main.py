from collections import defaultdict
from types import SimpleNamespace

from cereal import custom
from opendbc.car import Bus, structs
from opendbc.car.tests.test_car_interfaces import Toggles
from opendbc.car.volkswagen.carcontroller import CarController
from opendbc.car.volkswagen.carstate import CarState
from opendbc.car.volkswagen.interface import CarInterface
from opendbc.car.volkswagen.values import CAR, DBC
from opendbc.safety import ALTERNATIVE_EXPERIENCE

MSG_GRA_ACC_01 = 0x12B


def car_params(always_on_lateral):
  CP = CarInterface.get_non_essential_params(CAR.VOLKSWAGEN_GOLF_MK7)
  if always_on_lateral:
    CP.alternativeExperience |= ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL

  return CP


class Drive:
  def __init__(self, always_on_lateral=True, live_toggle=None):
    if live_toggle is None:
      live_toggle = always_on_lateral

    self.CS = CarState(car_params(always_on_lateral))
    self.CS.frogpilot_toggles = Toggles(always_on_lateral=live_toggle)

    parser = SimpleNamespace(ts_nanos=defaultdict(lambda: defaultdict(int)), vl=defaultdict(lambda: defaultdict(int)))
    parser.ts_nanos["GRA_ACC_01"]["GRA_Hauptschalter"] = 1
    self.can_parsers = {Bus.pt: parser, Bus.cam: parser}

  def run(self, *statuses):
    available = []
    for status, frames, *main_button in statuses:
      if main_button:
        main_button_pressed = main_button[0]
      else:
        main_button_pressed = 0

      for _ in range(frames):
        self.can_parsers[Bus.pt].vl["TSK_06"]["TSK_Status"] = status
        self.can_parsers[Bus.pt].vl["GRA_ACC_01"]["GRA_Hauptschalter"] = main_button_pressed
        self.CS.fp_ret = custom.FrogPilotCarState.new_message()

        available.append(self.CS.update(self.can_parsers).cruiseState.available)

    return available


class Start:
  def __init__(self, always_on_lateral=True, main_button_type=1, main_off_seen=False, live_toggle=None, car_safety_mode_set=True):
    CP = car_params(always_on_lateral)

    self.CC = CarController(DBC[CP.carFingerprint], CP)
    if live_toggle is None:
      live_toggle = always_on_lateral

    self.CC.frogpilot_toggles = Toggles(always_on_lateral=live_toggle)

    gra_stock_values = {
      "COUNTER": 0,
      "GRA_ButtonTypeInfo": 2,
      "GRA_Codierung": 0,
      "GRA_Hauptschalter": 0,
      "GRA_Tip_Stufe_2": 0,
      "GRA_Typ_Hauptschalter": main_button_type,
    }

    self.CS = SimpleNamespace(acc_type=0, car_safety_mode_set=car_safety_mode_set, cruise_main_off_seen=main_off_seen, eps_stock_values=defaultdict(int),
                              esp_hold_confirmation=False, gra_stock_values=gra_stock_values, ldw_stock_values=defaultdict(int), out=structs.CarState(),
                              tsk_status=0, upscale_lead_car_signal=False)

  def run(self, tsk_status, frames):
    presses = []
    for frame in range(frames):
      self.CS.gra_stock_values["COUNTER"] = frame // 3 % 16
      self.CS.out.accFaulted = tsk_status in (6, 7)
      self.CS.out.cruiseState.enabled = tsk_status in (3, 4, 5)
      self.CS.tsk_status = tsk_status

      _, can_sends = self.CC.update(structs.CarControl().as_reader(), self.CS, 0)

      presses += [frame for addr, dat, _ in can_sends if addr == MSG_GRA_ACC_01 and dat[1] >> 4 & 1]

    return presses


def test_always_on_lateral_off():
  drive = Drive(always_on_lateral=False)

  started = drive.run((1, 60), (0, 18), (6, 12), (2, 100))
  faulted = drive.run((6, 10))

  assert started == [False] * 90 + [True] * 100
  assert not any(faulted)


def test_faults_keep_main():
  drive = Drive()
  drive.run((2, 200))

  on = drive.run((6, 100), (7, 100), (2, 10))
  off = drive.run((0, 10), (6, 100), (0, 10))

  assert all(on)
  assert not any(off)


def test_follows_safety_mode():
  assert Drive(always_on_lateral=False, live_toggle=True).run((1, 60), (0, 18), (6, 12), (2, 10)) == [False] * 90 + [True] * 10
  assert Start(always_on_lateral=False, live_toggle=True).run(2, 300) == []
  assert Start(always_on_lateral=True, live_toggle=False).run(2, 300) == [0, 3, 6, 9, 12]


def test_main_off_press():
  start = Start()

  waiting = start.run(1, 100) + start.run(0, 100) + start.run(6, 100)
  pressed = start.run(2, 300)
  switched_on_again = start.run(0, 100) + start.run(2, 300)

  assert waiting == []
  assert pressed == [0, 3, 6, 9, 12]
  assert switched_on_again == []


def test_main_off_press_not_after_engaged():
  start = Start()

  assert start.run(3, 50) + start.run(2, 300) == []


def test_main_off_press_not_sent():
  for start in (Start(always_on_lateral=False), Start(main_button_type=0), Start(main_off_seen=True)):
    assert start.run(2, 300) == []

  for tsk_status in (3, 4, 5):
    assert Start().run(tsk_status, 300) == []


def test_main_off_press_restarts_after_fault():
  start = Start()

  cut_short = start.run(2, 4)
  faulted = start.run(6, 48)
  pressed = start.run(2, 300)

  assert cut_short == [0, 3]
  assert faulted == []
  assert pressed == [0, 3, 6, 9, 12]
  assert start.run(6, 48) + start.run(2, 300) == []


def test_main_off_press_waits_for_safety_mode():
  start = Start(car_safety_mode_set=False)

  before_safety_mode = start.run(2, 300)
  start.CS.car_safety_mode_set = True
  after_safety_mode = start.run(2, 300)

  assert before_safety_mode == []
  assert after_safety_mode == [0, 3, 6, 9, 12]


def test_main_switched():
  drive = Drive()
  drive.run((2, 200))

  assert drive.run((0, 1)) == [False]
  assert drive.run((2, 1)) == [True]


def test_settled_status_1_ignored():
  drive = Drive()
  drive.run((2, 200))

  assert not any(drive.run((1, 10), (0, 10), (1, 10)))


def test_start_driver_main_press():
  for holding in ((0, 10), (6, 10)):
    drive = Drive()

    held = drive.run((1, 60), (0, 18), holding)
    pressed = drive.run((holding[0], 4, 1), holding)
    switched_on = drive.run((2, 10))

    assert all(held), holding
    assert not any(pressed), holding
    assert all(switched_on), holding
    assert drive.CS.cruise_main_off_seen, holding


def test_start_lever_already_on():
  drive = Drive()
  drive.can_parsers[Bus.pt].ts_nanos["GRA_ACC_01"]["GRA_Hauptschalter"] = 0
  unparsed = drive.run((1, 1))

  drive.can_parsers[Bus.pt].ts_nanos["GRA_ACC_01"]["GRA_Hauptschalter"] = 1

  assert all(unparsed + drive.run((1, 60, 1), (0, 18, 1), (6, 12, 1), (2, 100, 1)))
  assert not drive.CS.cruise_main_off_seen


def test_start_main_off():
  drive = Drive()

  available = drive.run((1, 60), (0, 200))
  switched_on = drive.run((2, 100))

  assert all(available[:96])
  assert not any(available[210:])
  assert available.count(False) == len(available) - available.index(False)
  assert all(switched_on)
  assert drive.CS.cruise_main_off_seen


def test_start_main_restored():
  starts = [
    ((1, 60), (0, 16), (6, 672), (2, 100)),
    ((1, 60), (0, 18), (6, 12), (2, 100)),
    ((1, 93), (2, 10), (6, 58), (2, 100)),
    ((1, 150), (0, 36), (6, 50), (2, 100)),
  ]

  for start in starts:
    drive = Drive()

    assert all(drive.run(*start)), start
    assert not drive.CS.cruise_main_off_seen, start
