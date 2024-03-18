import datetime

MIN_DATE = datetime.datetime(year=2024, month=3, day=1)

def system_time_valid():
  return datetime.datetime.now() > MIN_DATE
