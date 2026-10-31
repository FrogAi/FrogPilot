import collections
import types

from cereal import car, custom
from openpilot.selfdrive.car.volkswagen.carcontroller import CarController
from openpilot.selfdrive.car.volkswagen.carstate import CarState
from openpilot.selfdrive.car.volkswagen.values import CAR, DBC

MSG_GRA_ACC_01 = 0x12B

TransmissionType = car.CarParams.TransmissionType


class Drive:
  def __init__(self, always_on_lateral=True):
    CP = car.CarParams.new_message(carFingerprint=CAR.VOLKSWAGEN_GOLF_MK7, transmissionType=TransmissionType.automatic)

    self.CS = CarState(CP, custom.FrogPilotCarParams.new_message())

    self.pt_cp = types.SimpleNamespace(vl=collections.defaultdict(lambda: collections.defaultdict(int)))
    self.toggles = types.SimpleNamespace(always_on_lateral=always_on_lateral)

  def run(self, *statuses):
    available = []
    for status, frames in statuses:
      for _ in range(frames):
        self.pt_cp.vl["TSK_06"]["TSK_Status"] = status

        ret, _ = self.CS.update(self.pt_cp, self.pt_cp, self.pt_cp, TransmissionType.automatic, self.toggles)

        available.append(ret.cruiseState.available)

    return available


class Start:
  def __init__(self, always_on_lateral=True, main_button_type=1, main_off_seen=False):
    CP = car.CarParams.new_message(carFingerprint=CAR.VOLKSWAGEN_GOLF_MK7, pcmCruise=True)

    self.CC = CarController(DBC[CP.carFingerprint]["pt"], CP, None)

    gra_stock_values = {
      "COUNTER": 0,
      "GRA_ButtonTypeInfo": 2,
      "GRA_Codierung": 0,
      "GRA_Hauptschalter": 0,
      "GRA_Tip_Stufe_2": 0,
      "GRA_Typ_Hauptschalter": main_button_type,
    }

    self.CS = types.SimpleNamespace(cruise_main_off_seen=main_off_seen, gra_stock_values=gra_stock_values, ldw_stock_values={}, out=car.CarState.new_message(),
                                    tsk_status=0)
    self.toggles = types.SimpleNamespace(always_on_lateral=always_on_lateral)

  def run(self, tsk_status, frames):
    presses = []
    for frame in range(frames):
      self.CS.gra_stock_values["COUNTER"] = frame // 3 % 16
      self.CS.tsk_status = tsk_status

      _, can_sends = self.CC.update(car.CarControl.new_message().as_reader(), self.CS, 0, self.toggles)

      presses += [dat[1] >> 4 & 1 for addr, _, dat, _ in can_sends if addr == MSG_GRA_ACC_01]

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


def test_main_off_press():
  start = Start()

  waiting = start.run(1, 100) + start.run(0, 100) + start.run(6, 100)
  pressed = start.run(2, 300)
  switched_on_again = start.run(0, 100) + start.run(2, 300)

  assert waiting == []
  assert pressed == [1] * 5
  assert switched_on_again == []


def test_main_off_press_not_sent():
  for start in (Start(always_on_lateral=False), Start(main_button_type=0), Start(main_off_seen=True)):
    assert start.run(2, 300) == []

  for tsk_status in (3, 4, 5):
    assert Start().run(tsk_status, 300) == []


def test_main_switched():
  drive = Drive()
  drive.run((2, 200))

  assert drive.run((0, 1)) == [False]
  assert drive.run((2, 1)) == [True]


def test_settled_status_1_ignored():
  drive = Drive()
  drive.run((2, 200))

  assert not any(drive.run((1, 10), (0, 10), (1, 10)))


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
