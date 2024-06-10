from openpilot.common.params_pyx import Params, UnknownKeyName

class FrogPilotFunctions:
  @classmethod
  def convert_params(cls, params, params_storage, params_tracking):
    def convert_param(key, action_func):
      try:
        if params_storage.check_key(key):
          if params_storage.get_bool(key):
            action_func()
      except UnknownKeyName:
        pass

    def convert_param_mappings(param_mappings, remove_from, min_value=-1):
      for key, (getter, setter) in param_mappings.items():
        try:
          value = getter(key)
          if value > min_value:
            setter(key, value)
            remove_from.remove(key)
        except UnknownKeyName:
          pass

    install_date = params.get("InstallDate")
    if install_date and install_date.decode('utf-8').startswith("November 21, 2023"):
      params.remove("InstallDate")

    version = 5

    try:
      if params_storage.check_key("ParamConversionVersion"):
        if params_storage.get_int("ParamConversionVersion") == version:
          print("Params already converted, moving on.")
          return
        print("Converting params...")
    except UnknownKeyName:
      pass

    def onroad_uploads():
      params.put("DeviceManagement", "True")
      params.put("NoUploads", "True")

    convert_param("DisableOnroadUploads", onroad_uploads)

    param_mappings = {
      "FrogPilotDrives": (params.get_int, params_tracking.put_int),
      "FrogPilotKilometers": (params.get_float, params_tracking.put_float),
      "FrogPilotMinutes": (params.get_float, params_tracking.put_float)
    }
    convert_param_mappings(param_mappings, params, 0)

    param_storage_mappings = {
      "FrogPilotDrives": (params_storage.get_int, params_tracking.put_int),
      "FrogPilotKilometers": (params_storage.get_float, params_tracking.put_float),
      "FrogPilotMinutes": (params_storage.get_float, params_tracking.put_float)
    }
    convert_param_mappings(param_storage_mappings, params_storage, 0)

    params.remove("CarMake")
    params.remove("CarModel")
    params.remove("ForceFingerprint")

    print("Params successfully converted!")
    params_storage.put_int("ParamConversionVersion", version)

    original_boot_logo_location = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/bg.jpg'
    original_boot_logo_save_location = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/original_bg.jpg'
    shutil.copy(original_boot_logo_location, original_boot_logo_save_location)
    print("Successfully replaced original_bg.jpg with bg.jpg.")
