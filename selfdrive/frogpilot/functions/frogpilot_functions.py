import numpy as np

from openpilot.common.numpy_fast import interp
from openpilot.common.params import Params
from openpilot.system.hardware import HARDWARE


params_memory = Params("/dev/shm/params")

CITY_SPEED_LIMIT = 25
CRUISING_SPEED = 5  # Roughly the speed cars go when not touching the gas while in drive
PROBABILITY = 0.6   # 60% chance of condition being true
THRESHOLD = 5       # Time threshold (0.25s)

class FrogPilotFunctions:
  def __init__(self) -> None:
    self.params = Params()
