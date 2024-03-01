import numpy as np

from openpilot.common.numpy_fast import interp
from openpilot.common.params import Params
from openpilot.system.hardware import HARDWARE


params = Params()
params_memory = Params("/dev/shm/params")

CRUISING_SPEED = 5  # Roughly the speed cars go when not touching the gas while in drive
THRESHOLD = 5       # Time threshold (0.25s)

class FrogPilotFunctions:
