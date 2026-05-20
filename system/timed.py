#!/usr/bin/env python3
import datetime
import subprocess
import time
from functools import cache
from pathlib import Path
from typing import NoReturn
from zoneinfo import available_timezones

from timezonefinder import TimezoneFinder

import cereal.messaging as messaging
from openpilot.common.time_helpers import min_date, MAX_DATE, system_time_valid
from openpilot.common.swaglog import cloudlog
from openpilot.common.params import Params
from openpilot.common.gps import get_gps_location_service
from openpilot.system.hardware import AGNOS


def set_time(new_time):
  diff = datetime.datetime.now() - new_time
  if abs(diff) < datetime.timedelta(seconds=10):
    cloudlog.debug(f"Time diff too small: {diff}")
    return

  cloudlog.debug(f"Setting time to {new_time}")
  try:
    subprocess.run(f"TZ=UTC date -s '{new_time}'", shell=True, check=True)
  except subprocess.CalledProcessError:
    cloudlog.exception("timed.failed_setting_time")


# FrogPilot variables
@cache
def valid_timezones():
  return available_timezones()


def set_timezone(timezone):
  if timezone not in valid_timezones():
    cloudlog.error(f"Timezone not supported {timezone}")
    return False

  cloudlog.debug(f"Setting timezone to {timezone}")
  try:
    if AGNOS:
      tzpath = Path("/usr/share/zoneinfo") / timezone
      subprocess.run(["sudo", "ln", "-snf", str(tzpath), "/data/etc/tmptime"], check=True)
      subprocess.run(["sudo", "mv", "/data/etc/tmptime", "/data/etc/localtime"], check=True)
      subprocess.run(["sudo", "sh", "-c", "cat > /data/etc/timezone"], input=f"{timezone}\n", text=True, check=True)
    else:
      subprocess.run(["sudo", "timedatectl", "set-timezone", timezone], check=True)
  except subprocess.CalledProcessError:
    cloudlog.exception(f"Error setting timezone to {timezone}")
    return False

  return True


def main() -> NoReturn:
  """
    timed has two responsibilities:
    - getting the current time from GPS
    - publishing the time in the logs

    AGNOS will also use NTP to update the time.
  """

  params = Params()
  gps_location_service = get_gps_location_service(params)

  pm = messaging.PubMaster(['clocks'])
  sm = messaging.SubMaster([gps_location_service])

  # FrogPilot variables
  tf = TimezoneFinder()

  last_timezone = params.get("Timezone")
  if last_timezone is not None:
    set_timezone(last_timezone)

  while True:
    sm.update(1000)

    msg = messaging.new_message('clocks')
    msg.valid = system_time_valid()
    msg.clocks.wallTimeNanos = time.time_ns()
    pm.send('clocks', msg)

    gps = sm[gps_location_service]
    gps_time = datetime.datetime.fromtimestamp(gps.unixTimestampMillis / 1000.)
    if not sm.updated[gps_location_service] or (time.monotonic() - sm.logMonoTime[gps_location_service] / 1e9) > 2.0:
      continue
    if not gps.hasFix:
      continue
    if gps_time < min_date() or gps_time > MAX_DATE:
      continue

    set_time(gps_time)

    # FrogPilot variables
    timezone = tf.timezone_at(lng=gps.longitude, lat=gps.latitude)
    if timezone is not None and timezone != last_timezone:
      if set_timezone(timezone):
        params.put("Timezone", timezone)
        last_timezone = timezone

    time.sleep(10)

if __name__ == "__main__":
  main()
