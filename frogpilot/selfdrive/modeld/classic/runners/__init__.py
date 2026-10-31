import os
from openpilot.system.hardware import TICI
from openpilot.frogpilot.selfdrive.modeld.classic.runners.runmodel_pyx import RunModel, Runtime
assert Runtime

USE_THNEED = int(os.getenv('USE_THNEED', str(int(TICI))))
USE_SNPE = int(os.getenv('USE_SNPE', str(int(TICI))))

class ModelRunner(RunModel):
  THNEED = 'THNEED'
  SNPE = 'SNPE'

  def __new__(cls, paths, *args, **kwargs):
    if ModelRunner.THNEED in paths and USE_THNEED:
      from openpilot.frogpilot.selfdrive.modeld.classic.runners.thneedmodel_pyx import ThneedModel as Runner
      runner_type = ModelRunner.THNEED
    elif ModelRunner.SNPE in paths and USE_SNPE:
      from openpilot.frogpilot.selfdrive.modeld.classic.runners.snpemodel_pyx import SNPEModel as Runner
      runner_type = ModelRunner.SNPE
    else:
      raise Exception("Couldn't select a model runner, make sure to pass at least one valid model path")

    return Runner(str(paths[runner_type]), *args, **kwargs)
