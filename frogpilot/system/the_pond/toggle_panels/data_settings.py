import asyncio

from openpilot.frogpilot.system.the_pond import dashcam, tools
from openpilot.frogpilot.system.the_pond.toggle_panels import rows

TELEMETRY_DESCRIPTION = ("<b>Help make FrogPilot better by sharing filtered driving logs, short speed limit sign clips, learned speed limits, and usage " +
                         "stats.</b><br><br>Your everyday drives help us improve steering, acceleration, and braking, fine-tune support for more cars, " +
                         "train the speed limit sign reader, and find and fix problems. Keeping sharing enabled gives us more real-world examples across " +
                         "cars, roads, and driving conditions.<br><br><b>Privacy protection starts on your device.</b> Driving logs are filtered, and the " +
                         "location included in usage stats is generalized, before they are sent.<br><br><b>Driving logs</b> record vehicle behavior and " +
                         "FrogPilot's driving decisions. Before upload, we remove camera footage, audio, driver face data, account details, SSH keys, " +
                         "navigation routes, and map road names and IDs. Exact GPS coordinates and recording date/time fields are cleared, and <b>the VIN " +
                         "field is removed entirely</b>. Driver-monitoring data is limited to awareness level and whether your car is right-hand drive. " +
                         "We also discard CAN messages captured during startup vehicle identification. Each drive gets an ID that hides when it was " +
                         "recorded, and uploaded logs are linked to your FrogPilot device ID.<br><br>Raw CAN data recorded during driving is kept to help " +
                         "improve car support. On some cars, these vehicle messages can still contain GPS, VIN, date/time, odometer, or seat occupancy " +
                         "information.<br><br><b>Usage stats</b> cover your device, software, car and calibration details, FrogPilot settings, model " +
                         "usage and scores, and lifetime driving totals, including drives from before you turned sharing on. <b>Your location is " +
                         "generalized on your device.</b> It reports the nearest city in your country or US state from a list filtered to populations of " +
                         "<b>50,000 or more</b>. If that region has no qualifying city, it uses its capital instead. This helps protect location privacy " +
                         "in small communities. For location, these stats include only the resulting city, state, and country, never your exact GPS " +
                         "coordinates. The VIN is excluded from car details, and stats are linked to your FrogPilot device ID.<br><br><b>Speed limit sign " +
                         "clips</b> are about 10 seconds of road camera video, recorded when a supported car's dashboard shows a new speed limit, when the " +
                         "speed limit sign reader sees a sign it isn't sure of, and for some of the limits it reads. No GPS coordinates or recording " +
                         "date/time are attached, and clip IDs hide the original recording time. Each clip includes the speed limit or the reader's " +
                         "reading, camera calibration, and basic device, camera, and software details, and is linked to your FrogPilot device ID. Video is " +
                         "uploaded as recorded and can show license plates, people, and identifiable places. Clips picked to train or test the reader may " +
                         "be kept for up to 2 years. Clips may be matched to driving logs.<br><br><b>Learned speed limits</b> are the limits \"Speed Limit " +
                         "Filler\" learns from the signs the camera reads, when that setting is on. They are used to improve speed limits for everyone " +
                         "using FrogPilot. Each one includes the speed limit, the map ID of the road it was learned on, the direction of travel, the GPS " +
                         "location and heading where the sign was seen, the date, and how far the car drove with that limit, and they are linked to your " +
                         "FrogPilot device ID. Each one also includes a cropped photo of the sign it was read from, with the GPS location, heading, and " +
                         "time it was taken, and the photo can show part of the road around the sign. Roads where the sign matched your map's limit are " +
                         "sent too, with the limit and the date, to measure how accurate the sign reader is. Limits learned before you turned sharing on " +
                         "are included. The road IDs and locations show where the car has been driven.<br><br><b>Uploads</b> of driving logs, clips, and " +
                         "learned speed limits happen only while your car is off, on unmetered Wi-Fi or Ethernet. Usage stats are sent at startup and " +
                         "after drives and may use other available internet connections.<br><br><b>Your choice.</b> You can turn off \"Share FrogPilot " +
                         "Data\" anytime to stop sharing these logs, clips, learned speed limits, and usage stats. Basic device registration still sends " +
                         "device type, operating system and software build details, and a public authentication key. Startup and after-drive check-ins " +
                         "continue without your usage stats, settings, or location. Crash reports also continue, tagged with your comma device ID (or your " +
                         "Konik device ID when you use the Konik server) and software details. Python crash reports do not automatically collect local " +
                         "variable values.<br><br>Read FrogPilot's full privacy policy at <b>frogpilot.com/privacy</b>.")

DELETE_STATUS = {"failure": "Delete failed...", "success": "Deleted!"}


async def delete_driving_data(context, body):
  recording_route = dashcam.get_recording_route(context.the_pond)
  return await asyncio.to_thread(dashcam.delete_driving_data, recording_route)


async def delete_error_logs(context, body):
  await tools.delete_error_logs(context.request)
  return True


def telemetry_changed(context, old_value, new_value):
  if new_value:
    context.params.put_bool("FrogPilotTelemetryConfirmed", True)
  return False


def telemetry_description(context):
  dongle_id = context.params.get("FrogPilotDongleId")
  if dongle_id:
    return TELEMETRY_DESCRIPTION + f"<br><br>Your FrogPilot device ID is <b>{dongle_id}</b>."
  return TELEMETRY_DESCRIPTION


def telemetry_shared(context):
  return context.params.get_bool("FrogPilotTelemetry") and context.params.get_bool("FrogPilotTelemetryConfirmed")


ROWS = [
  {
    "actions": {
      "DELETE": {
        "confirm": {"cancel": "Cancel", "confirm": "Delete",
                    "text": "Delete all driving footage and data? Only the flagged one-minute chunks and the drives preserved in \"The Pond\" will be kept."},
        "run": delete_driving_data,
        "status": DELETE_STATUS
      }
    },
    "description": ("<b>Delete every recorded drive to free up space and clear personal footage off the device.</b><br><br>Only the one-minute chunk " +
                    "of footage containing the moment you flagged is kept, not the rest of that drive. Drives preserved in \"The Pond\" are kept " +
                    "whole."),
    "id": "DeleteDrivingData",
    "key": None,
    "order": 0,
    "path": ("System Settings", "DATA"),
    "title": "Delete Driving Data",
    "widget": "action"
  },
  {
    "actions": {
      "DELETE": {
        "confirm": {"cancel": "Cancel", "confirm": "Delete", "text": "Delete all error logs?"},
        "run": delete_error_logs,
        "status": DELETE_STATUS
      }
    },
    "description": ("<b>Delete openpilot's saved crash logs.</b><br><br>Bug reports sent after deleting won't include crash details until a new " +
                    "crash happens."),
    "id": "DeleteErrorLogs",
    "key": None,
    "order": 1,
    "path": ("System Settings", "DATA"),
    "title": "Delete Error Logs",
    "widget": "action"
  },
  {
    "description": ("<b>Delete or rename your recordings of the driving screen.</b><br><br>Recordings are made with the \"Screen Recorder\" button " +
                    "on the driving screen. \"DELETE ALL\" removes every recording at once."),
    "id": "ScreenRecordings",
    "key": None,
    "link": "recordings",
    "options": ({"label": "DELETE", "value": 0}, {"label": "DELETE ALL", "value": 1}, {"label": "RENAME", "value": 2}),
    "order": 2,
    "path": ("System Settings", "DATA"),
    "title": "Screen Recordings",
    "widget": "buttons"
  },
  {
    "description": ("<b>Back up the FrogPilot software, restore a backup to go back to that version, or delete ones you no longer " +
                    "need.</b><br><br>Restoring reboots the device on its own and puts the software back exactly as it was when the backup was made, " +
                    "without changing your settings. Automatic updates turn off after a restore until you update manually. \"DELETE ALL\" also " +
                    "removes the backups FrogPilot makes automatically."),
    "id": "FrogpilotBackups",
    "key": None,
    "link": "backups",
    "options": ({"label": "BACKUP", "value": 0}, {"label": "DELETE", "value": 1}, {"label": "DELETE ALL", "value": 2}, {"label": "RESTORE", "value": 3}),
    "order": 3,
    "path": ("System Settings", "DATA"),
    "title": "FrogPilot Backups",
    "widget": "buttons"
  },
  {
    "description": ("<b>Save a copy of your current settings, restore a saved copy, or delete ones you no longer need.</b><br><br>Restoring applies " +
                    "most settings right away, but a few, such as \"High-Quality Recording\" and \"Use Konik Server\", need a reboot. FrogPilot also " +
                    "saves a copy automatically whenever you change a setting, but it only keeps the newest few and deletes the older ones. Those " +
                    "show up in the list by date and time."),
    "id": "SettingsBackups",
    "key": None,
    "link": "backups",
    "options": ({"label": "BACKUP", "value": 0}, {"label": "DELETE", "value": 1}, {"label": "DELETE ALL", "value": 2}, {"label": "RESTORE", "value": 3}),
    "order": 4,
    "path": ("System Settings", "DATA"),
    "title": "Settings Backups",
    "widget": "buttons"
  },
  {
    "children": ("System Settings", "DATA", "FrogPilot Stats"),
    "description": ("<b>See everything FrogPilot has tracked about your driving, or reset the numbers and start over.</b><br><br>Stats can only be " +
                    "reset while the car is off."),
    "id": "FrogpilotStats",
    "key": None,
    "link": "home",
    "options": ({"label": "RESET", "value": 0}, {"label": "VIEW", "value": 1}),
    "order": 5,
    "path": ("System Settings", "DATA"),
    "title": "FrogPilot Stats",
    "widget": "buttons"
  },
  {
    "confirm_enable": rows.ENABLE_CONFIRMATION,
    "current": telemetry_shared,
    "description": telemetry_description,
    "key": "FrogPilotTelemetry",
    "on_change": telemetry_changed,
    "order": 6,
    "path": ("System Settings", "DATA"),
    "title": "Share FrogPilot Data",
    "widget": "toggle"
  }
]
