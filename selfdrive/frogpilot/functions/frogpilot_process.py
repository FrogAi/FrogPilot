import datetime
import time
import threading

from openpilot.common.params import Params
from openpilot.common.realtime import Ratekeeper

def calculate_easter(year):
  a = year % 19
  b = year // 100
  c = year % 100
  d = b // 4
  e = b % 4
  f = (b + 8) // 25
  g = (b - f + 1) // 3
  h = (19 * a + b - d - g + 15) % 30
  i = c // 4
  k = c % 4
  l = (32 + 2 * e + 2 * i - h - k) % 7
  m = (a + 11 * h + 22 * l) // 451
  month = (h + l - 7 * m + 114) // 31
  day = ((h + l - 7 * m + 114) % 31) + 1
  return datetime.datetime(year, month, day)

def calculate_thanksgiving(year):
  fourth_thursday = 4
  november_first = datetime.datetime(year, 11, 1)
  day_of_week = november_first.weekday()
  thanksgiving = november_first + datetime.timedelta(days=((fourth_thursday - day_of_week) % 7) + 21)
  return thanksgiving

def is_within_week_of(target_date, current_date):
  start_of_week = target_date - datetime.timedelta(days=target_date.weekday())
  end_of_week = start_of_week + datetime.timedelta(days=6)
  return start_of_week <= current_date <= end_of_week

def check_date(params_memory):
  current_date = datetime.datetime.now()
  year = current_date.year
  previous_theme_id = params_memory.get_int("CurrentHolidayTheme")

  holidays = {
    "april_fools_day": (datetime.datetime(year, 4, 1), 1),
    "christmas": (datetime.datetime(year, 12, 25), 2),
    "cinco_de_mayo": (datetime.datetime(year, 5, 5), 3),
    "easter": (calculate_easter(year), 4),
    "fourth_of_july": (datetime.datetime(year, 7, 4), 5),
    "halloween": (datetime.datetime(year, 10, 31), 6),
    "new_years_day": (datetime.datetime(year, 1, 1), 7),
    "st_patricks_day": (datetime.datetime(year, 3, 17), 8),
    "thanksgiving": (calculate_thanksgiving(year), 9),
    "valentines_day": (datetime.datetime(year, 2, 14), 10),
    "world_frog_day": (datetime.datetime(year, 3, 20), 11)
  }

  for holiday, (date, theme_id) in holidays.items():
    if holiday in ["easter", "thanksgiving"] or holiday.endswith("_week"):
      if is_within_week_of(date, current_date):
        params_memory.put_int("CurrentHolidayTheme", theme_id)
        if theme_id != previous_theme_id:
          threading.Thread(target=update_holiday_theme, args=(params_memory,)).start()
        return
    elif current_date.date() == date.date():
      params_memory.put_int("CurrentHolidayTheme", theme_id)
      if theme_id != previous_theme_id:
        threading.Thread(target=update_holiday_theme, args=(params_memory,)).start()
      return
  else:
    params_memory.put_int("CurrentHolidayTheme", 0)
    if previous_theme_id != 0:
      threading.Thread(target=update_holiday_theme, args=(params_memory,)).start()

def update_holiday_theme(params_memory):
  params_memory.put_bool("FrogPilotTogglesUpdated", True)
  time.sleep(1)
  params_memory.put_bool("FrogPilotTogglesUpdated", False)

def wait_for_next_minute():
  now = datetime.datetime.now()
  next_minute = (now + datetime.timedelta(minutes=1)).replace(second=0, microsecond=0)
  wait_seconds = (next_minute - now).total_seconds()
  time.sleep(wait_seconds)

def frogpilot_process_thread():
  params_memory = Params("/dev/shm/params")

  rk = Ratekeeper(1.0)

  while True:
    current_time = datetime.datetime.now()
    if current_time.second == 0:
      check_date(params_memory)
    rk.keep_time()

def main():
  wait_for_next_minute()
  frogpilot_process_thread()

if __name__ == "__main__":
  main()
