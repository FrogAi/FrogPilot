import json

from openpilot.common.time import system_time_valid

from openpilot.frogpilot.common.frogpilot_api import FrogPilotAPI
from openpilot.frogpilot.system.the_pond import utilities

ACCOUNT_MESSAGES = {"clock": "Connect to the internet to set the clock first."}

ACCOUNT_STATUS = {"failure": "Couldn't open FrogPilot.com. Try again later.", "success": ""}

ACCOUNTS_CLOSED = {"kind": "state", "text": "Shown once FrogPilot.com accounts open"}


def account_link(context):
  return json.loads(context.the_pond.sm["frogpilotAccountState"].link or "{}")


def account_visible(context):
  link = account_link(context)
  if link.get("linked") or link.get("available"):
    return None
  return ACCOUNTS_CLOSED


def clock_guard(context):
  if system_time_valid():
    return None
  return "clock"


async def delete_data(context, body):
  return FrogPilotAPI(context.params).account_url("delete")


async def link_account(context, body):
  return FrogPilotAPI(context.params).account_url("pair")


def linked_title(context):
  return f"Linked to FrogPilot.com as @{account_link(context).get('username', '')}"


def linked_visible(context):
  if account_link(context).get("linked"):
    return None
  return {"kind": "state", "text": "Shown while this device is linked to FrogPilot.com"}


async def unlink_account(context, body):
  await utilities.unlink_account(context.the_pond)
  return account_link(context).get("linked") is False


def unlinked_visible(context):
  link = account_link(context)
  if link.get("linked"):
    return {"kind": "state", "text": "Shown while this device isn't linked to FrogPilot.com"}
  if link.get("available"):
    return None
  return ACCOUNTS_CLOSED


ROWS = [
  {
    "actions": {
      "DELETE": {
        "confirm": {"cancel": "Cancel", "confirm": "Continue",
                    "text": "This opens FrogPilot.com, where you'll see exactly what will be deleted before anything is."},
        "guard": clock_guard,
        "messages": ACCOUNT_MESSAGES,
        "opens": True,
        "run": delete_data,
        "status": ACCOUNT_STATUS
      }
    },
    "description": ("<b>Delete the data FrogPilot keeps about this device on FrogPilot.com.</b><br><br>You'll see exactly what will be deleted, " +
                    "and confirm it there, before anything is."),
    "id": "DeleteFrogPilotData",
    "key": None,
    "order": 0,
    "path": ("System Settings", "ACCOUNT"),
    "title": "Delete My FrogPilot Data",
    "visible": unlinked_visible,
    "widget": "action"
  },
  {
    "actions": {
      "LINK": {
        "guard": clock_guard,
        "messages": ACCOUNT_MESSAGES,
        "opens": True,
        "run": link_account,
        "status": ACCOUNT_STATUS
      }
    },
    "description": ("<b>Link this device to your FrogPilot.com account, so its stats and reports show there.</b><br><br>This opens " +
                    "FrogPilot.com, where you sign in if asked and confirm the link."),
    "id": "LinkFrogPilotAccount",
    "key": None,
    "order": 1,
    "path": ("System Settings", "ACCOUNT"),
    "title": "Link to FrogPilot.com Account",
    "visible": unlinked_visible,
    "widget": "action"
  },
  {
    "actions": {
      "UNLINK": {
        "confirm": {"cancel": "Cancel", "confirm": "Unlink", "text": "Unlink this device from your FrogPilot.com account?",
                    "title": "Unlink from FrogPilot.com"},
        "run": unlink_account,
        "status": {"failure": "Couldn't unlink this device. Check that it's online, then try again.", "success": "Unlinked from FrogPilot.com"}
      }
    },
    "description": ("<b>Unlink this device from your FrogPilot.com account.</b><br><br>Its stats and reports stop showing there, and the " +
                    "account is emailed that the device was unlinked."),
    "id": "UnlinkFrogPilotAccount",
    "key": None,
    "order": 2,
    "path": ("System Settings", "ACCOUNT"),
    "title": linked_title,
    "visible": linked_visible,
    "widget": "action"
  }
]
