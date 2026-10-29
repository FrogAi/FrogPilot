from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.system.the_pond import models

RANDOMIZER_DOWNLOAD_NOTICE = {"action": "DOWNLOAD ALL", "cancel": "No", "confirm": "Yes",
                              "text": "The \"Model Randomizer\" only picks from models you have downloaded. Download every model now?"}
RANDOMIZER_OFFLINE_NOTICE = {"action": None, "cancel": "", "confirm": "Ok",
                             "text": ("The \"Model Randomizer\" only picks from models you have downloaded. Park your car and connect to the internet " +
                                      "to download them.")}


async def download_all_models(context, body):
  context.the_pond.send_request("downloadAllModels")
  return True


def model_randomizer_changed(context, old_value, new_value):
  if new_value and not models.is_downloading(context.the_pond) and len(models.get_downloaded_models()) < len(frogpilot_variables.get_models()):
    if context.online:
      context.notice = RANDOMIZER_DOWNLOAD_NOTICE
    else:
      context.notice = RANDOMIZER_OFFLINE_NOTICE
  return False


def randomizer_rows_visible(context):
  return context.level_reason("ModelRandomizer") or context.setting_reason(context.params.get_bool("ModelRandomizer"), "ModelRandomizer")


def select_model_visible(context):
  if context.level_reason("ModelRandomizer") is not None:
    return None
  return context.setting_reason(not context.params.get_bool("ModelRandomizer"), "ModelRandomizer", wanted=False)


ROWS = [
  {
    "description": ("<b>Download new driving models on their own as they are released, so they are ready when you want to try one.</b><br><br>This " +
                    "runs whenever the device is on Wi-Fi or another unmetered connection, including while you are driving. It also grabs every " +
                    "model that is not already on the device, not just newly released ones, so anything you removed with \"Delete Driving Models\" " +
                    "comes back."),
    "key": "AutomaticallyDownloadModels",
    "order": 0,
    "path": ("Driving Controls", "DRIVING MODEL"),
    "title": "Automatically Download New Models",
    "widget": "toggle"
  },
  {
    "description": ("<b>Remove driving models you have downloaded to free up storage.</b><br><br>\"DELETE\" lets you pick which ones, \"DELETE ALL\" " +
                    "removes the rest. The model you are currently using and the one FrogPilot ships with are always kept. Turn \"Automatically " +
                    "Download New Models\" off first, or anything you delete is downloaded again within the hour whenever the device is on Wi-Fi or " +
                    "another unmetered connection."),
    "id": "DeleteModel",
    "key": None,
    "link": "models",
    "options": ({"label": "DELETE", "value": 0}, {"label": "DELETE ALL", "value": 1}),
    "order": 1,
    "path": ("Driving Controls", "DRIVING MODEL"),
    "title": "Delete Driving Models",
    "widget": "buttons"
  },
  {
    "description": ("<b>Download driving models onto the device so you can switch to them.</b><br><br>\"DOWNLOAD\" lets you pick which ones, " +
                    "\"DOWNLOAD ALL\" fetches everything. Your car has to be parked and online, and models are large, so this can take a while."),
    "id": "DownloadModel",
    "key": None,
    "link": "models",
    "options": ({"label": "DOWNLOAD", "value": 0}, {"label": "DOWNLOAD ALL", "value": 1}),
    "order": 2,
    "path": ("Driving Controls", "DRIVING MODEL"),
    "title": "Download Driving Models",
    "widget": "buttons"
  },
  {
    "description": ("<b>Stop the \"Model Randomizer\" from picking driving models you did not like.</b><br><br>Blocking a model here has no " +
                    "effect on choosing it yourself under \"Select Driving Model\"."),
    "id": "ManageBlacklistedModels",
    "key": "BlacklistedModels",
    "link": "models",
    "options": ({"label": "ADD", "value": 0}, {"label": "REMOVE", "value": 1}, {"label": "REMOVE ALL", "value": 2}),
    "order": 3,
    "path": ("Driving Controls", "DRIVING MODEL"),
    "title": "Manage Model Blacklist",
    "visible": randomizer_rows_visible,
    "widget": "buttons"
  },
  {
    "children": ("Driving Controls", "DRIVING MODEL", "Manage Model Ratings"),
    "description": ("<b>See how you rated each driving model and how many drives you gave it, or wipe those ratings and start " +
                    "fresh.</b><br><br>These are for your own comparison. The \"Model Randomizer\" picks at random and does not favor your " +
                    "higher-rated models."),
    "id": "ManageScores",
    "key": "ModelDrivesAndScores",
    "link": "models",
    "options": ({"label": "RESET", "value": 0}, {"label": "VIEW", "value": 1}),
    "order": 4,
    "path": ("Driving Controls", "DRIVING MODEL"),
    "title": "Manage Model Ratings",
    "visible": randomizer_rows_visible,
    "widget": "buttons"
  },
  {
    "actions": {
      "DOWNLOAD ALL": {
        "guard": lambda context: models.download_error(context.the_pond, models.get_missing_models()),
        "run": download_all_models,
        "status": {"success": "Downloading..."}
      }
    },
    "description": ("<b>Picks a different driving model for you at the start of every drive, then asks how it went when you park, so you can work " +
                    "out which one you like best.</b><br><br>It only chooses from models you have downloaded and have not blacklisted, and it only " +
                    "asks for a rating after drives longer than 15 minutes. Your ratings are saved under \"Manage Model Ratings\" for you to compare."),
    "key": "ModelRandomizer",
    "on_change": model_randomizer_changed,
    "order": 5,
    "path": ("Driving Controls", "DRIVING MODEL"),
    "title": "Model Randomizer",
    "widget": "toggle"
  },
  {
    "description": ("<b>Choose which driving model does the driving.</b><br><br>The model is the part of openpilot that decides how to steer, speed " +
                    "up, and slow down, so switching it changes how the car feels. Only models you have downloaded are listed, and changing it while " +
                    "driving asks you to reboot."),
    "id": "SelectModel",
    "key": "DrivingModel",
    "link": "models",
    "order": 6,
    "path": ("Driving Controls", "DRIVING MODEL"),
    "title": "Select Driving Model",
    "visible": select_model_visible,
    "widget": "select"
  }
]
