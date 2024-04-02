import datetime
import time
import threading

from openpilot.common.params import Params

class ThemeManager:
  def __init__(self):
    self.params = Params()
    self.params_memory = Params("/dev/shm/params")

    self.previous_theme_id = 0

  @staticmethod
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

  @staticmethod
  def calculate_thanksgiving(year):
    fourth_thursday = 4
    november_first = datetime.datetime(year, 11, 1)
    day_of_week = november_first.weekday()
    thanksgiving = november_first + datetime.timedelta(days=((fourth_thursday - day_of_week) % 7) + 21)
    return thanksgiving

  @staticmethod
  def is_within_week_of(target_date, current_date):
    start_of_week = target_date - datetime.timedelta(days=target_date.weekday())
    end_of_week = start_of_week + datetime.timedelta(days=6)
    return start_of_week <= current_date <= end_of_week

  def update_holiday(self):
    current_date = datetime.datetime.now()
    year = current_date.year

    holidays = {
      "april_fools": (datetime.datetime(year, 4, 1), 1),
      "christmas_week": (datetime.datetime(year, 12, 25), 2),
      "cinco_de_mayo": (datetime.datetime(year, 5, 5), 3),
      "easter_week": (self.calculate_easter(year), 4),
      "fourth_of_july": (datetime.datetime(year, 7, 4), 5),
      "halloween_week": (datetime.datetime(year, 10, 31), 6),
      "new_years": (datetime.datetime(year, 1, 1), 7),
      "st_patricks": (datetime.datetime(year, 3, 17), 8),
      "thanksgiving_week": (self.calculate_thanksgiving(year), 9),
      "valentines": (datetime.datetime(year, 2, 14), 10),
      "world_frog_day": (datetime.datetime(year, 3, 20), 11)
    }

    for holiday, (date, theme_id) in holidays.items():
      if holiday.endswith("_week"):
        if self.is_within_week_of(date, current_date):
          if theme_id != self.previous_theme_id:
            update_holiday = threading.Thread(target=update_holiday_theme, args=(theme_id,))
            update_holiday.start()
          self.previous_theme_id = theme_id
          return

      elif current_date.date() == date.date():
        if theme_id != self.previous_theme_id:
          update_holiday = threading.Thread(target=update_holiday_theme, args=(theme_id,))
          update_holiday.start()
        self.previous_theme_id = theme_id
        return

    if self.previous_theme_id != 0:
      update_holiday = threading.Thread(target=update_holiday_theme, args=(0,))
      update_holiday.start()
    self.previous_theme_id = 0

  def update_holiday_theme(self, theme_id):
    self.params_memory.put_int("CurrentHolidayTheme", theme_id)
    self.params_memory.put_bool("FrogPilotTogglesUpdated", True)
    time.sleep(1)
    self.params_memory.put_bool("FrogPilotTogglesUpdated", False)
