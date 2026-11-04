from opendbc.car import get_safety_config, structs
from opendbc.car.interfaces import CarInterfaceBase
from opendbc.car.tesla.carcontroller import CarController
from opendbc.car.tesla.carstate import CarState
from opendbc.car.tesla.values import TeslaSafetyFlags, CAR, CANBUS, TeslaFlags


class CarInterface(CarInterfaceBase):
  CarState = CarState
  CarController = CarController

  @staticmethod
  def _get_params(ret: structs.CarParams, candidate, fingerprint, car_fw, alpha_long, is_release, docs) -> structs.CarParams:
    ret.brand = "tesla"

    ret.safetyConfigs = [get_safety_config(structs.CarParams.SafetyModel.tesla)]

    ret.steerLimitTimer = 0.4
    ret.steerActuatorDelay = 0.1
    ret.steerAtStandstill = True

    ret.steerControlType = structs.CarParams.SteerControlType.angle
    ret.radarUnavailable = True

    ret.alphaLongitudinalAvailable = True
    if alpha_long:
      ret.openpilotLongitudinalControl = True
      ret.safetyConfigs[0].safetyParam |= TeslaSafetyFlags.LONG_CONTROL.value

      ret.vEgoStopping = 0.1
      ret.vEgoStarting = 0.1
      ret.stoppingDecelRate = 0.3

    ret.dashcamOnly = candidate in (CAR.TESLA_MODEL_X) # dashcam only, pending find invalidLkasSetting signal

    # FrogPilot variables
    # Tesla expanded DAS_steeringControl->DAS_steeringControlType to 3 bits: first in the FSD 14 builds for HW4 around 10-26-2025,
    # then in the other HW4 builds around 03-02-2026, and for HW3 and HW2.5 with 2026.8.6 around 04-03-2026.
    # The values were pushed to the right by 1 bit and kept the same definitions, plus 4 = FSD was added.
    # These messages are only sent by the 3-bit firmware:
    # - HW3/HW4: DAS_redundantBrakingControl (0x489)
    # - HW2.5/HW3/HW4: DI_autonomyHealth (0x054)
    das_steering_3_bit = 0x489 in fingerprint[CANBUS.autopilot_party] or 0x054 in fingerprint[CANBUS.party]

    if not das_steering_3_bit and not docs:
      ret.dashcamOnly = True
      ret.flags |= TeslaFlags.STEERING_2_BIT.value

    return ret
