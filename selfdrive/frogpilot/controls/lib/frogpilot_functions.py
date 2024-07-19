from openpilot.common.params_pyx import Params, UnknownKeyName

def convert_params(params, params_storage):
  def convert_param(key, action_func):
    try:
      if params_storage.check_key(key) and params_storage.get_bool(key):
        action_func()
    except UnknownKeyName:
      pass

  version = 8

  try:
    if params_storage.check_key("ParamConversionVersion") and params_storage.get_int("ParamConversionVersion") == version:
      print("Params already converted, moving on.")
      return
  except UnknownKeyName:
    pass

  print("Converting params...")
  convert_param("ModelSelector", lambda: params.put_nonblocking("ModelManagement", "True"))
  convert_param("DragonPilotTune", lambda: params.put_nonblocking("FrogsGoMooTune", "True"))

  print("Params successfully converted!")
  params_storage.put_int_nonblocking("ParamConversionVersion", version)

def frogpilot_boot_functions(params, params_storage):
  convert_params(params, params_storage)
