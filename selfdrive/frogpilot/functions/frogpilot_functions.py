import numpy as np

from openpilot.common.numpy_fast import interp
from openpilot.common.params import Params
from openpilot.system.hardware import HARDWARE


params_memory = Params("/dev/shm/params")

CITY_SPEED_LIMIT = 25
CRUISING_SPEED = 5  # Roughly the speed cars go when not touching the gas while in drive
PROBABILITY = 0.6   # 60% chance of condition being true
THRESHOLD = 5       # Time threshold (0.25s)

# Acceleration profiles - Credit goes to the DragonPilot team!
                 # MPH = [0., 18,  36,  63,  94]
A_CRUISE_MIN_BP_CUSTOM = [0., 8., 16., 28., 42.]
                 # MPH = [0., 6.71, 13.4, 17.9, 24.6, 33.6, 44.7, 55.9, 67.1, 123]
A_CRUISE_MAX_BP_CUSTOM = [0.,    3,   6.,   8.,  11.,  15.,  20.,  25.,  30., 55.]

A_CRUISE_MIN_VALS_ECO = [-0.001, -0.010, -0.28, -0.56, -0.56]
A_CRUISE_MAX_VALS_ECO = [3.5, 3.2, 2.3, 2.0, 1.15, .80, .58, .36, .30, .091]

A_CRUISE_MIN_VALS_SPORT = [-0.50, -0.52, -0.55, -0.57, -0.60]
A_CRUISE_MAX_VALS_SPORT = [3.5, 3.5, 3.3, 2.8, 1.5, 1.0, .75, .6, .38, .2]

class FrogPilotFunctions:
  def __init__(self) -> None:
    self.params = Params()

  @staticmethod
  def get_min_accel_eco(v_ego):
    return interp(v_ego, A_CRUISE_MIN_BP_CUSTOM, A_CRUISE_MIN_VALS_ECO)

  @staticmethod
  def get_max_accel_eco(v_ego):
    return interp(v_ego, A_CRUISE_MAX_BP_CUSTOM, A_CRUISE_MAX_VALS_ECO)

  @staticmethod
  def get_min_accel_sport(v_ego):
    return interp(v_ego, A_CRUISE_MIN_BP_CUSTOM, A_CRUISE_MIN_VALS_SPORT)

  @staticmethod
  def get_max_accel_sport(v_ego):
    return interp(v_ego, A_CRUISE_MAX_BP_CUSTOM, A_CRUISE_MAX_VALS_SPORT)
