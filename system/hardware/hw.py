import os
from pathlib import Path

from openpilot.system.hardware import PC

DEFAULT_DOWNLOAD_CACHE_ROOT = "/tmp/comma_download_cache"

class Paths:
  @staticmethod
  def comma_home() -> str:
    return os.path.join(str(Path.home()), ".comma" + os.environ.get("OPENPILOT_PREFIX", ""))

  @staticmethod
  def log_root(HD=False, konik=False, raw=False) -> str:
    if os.environ.get('LOG_ROOT', False):
      return os.environ['LOG_ROOT']
    elif PC:
      return str(Path(Paths.comma_home()) / "media" / "0" / "realdata")
    elif not raw and Path("/cache/use_HD").is_file() or HD:
      return '/data/media/0/realdata_HD/'
    elif not raw and Path("/cache/use_konik").is_file() or konik:
      return '/data/media/0/realdata_konik/'
    else:
      return '/data/media/0/realdata/'

  @staticmethod
  def swaglog_root() -> str:
    if PC:
      return os.path.join(Paths.comma_home(), "log")
    else:
      return "/data/log/"

  @staticmethod
  def swaglog_ipc() -> str:
    return "ipc:///tmp/logmessage" + os.environ.get("OPENPILOT_PREFIX", "")

  @staticmethod
  def download_cache_root() -> str:
    if os.environ.get('COMMA_CACHE', False):
      return os.environ['COMMA_CACHE'] + "/"
    return DEFAULT_DOWNLOAD_CACHE_ROOT + os.environ.get("OPENPILOT_PREFIX", "") + "/"

  @staticmethod
  def persist_root() -> str:
    if PC:
      return os.path.join(Paths.comma_home(), "persist")
    else:
      return "/persist/"

  @staticmethod
  def stats_root() -> str:
    if PC:
      return str(Path(Paths.comma_home()) / "stats")
    else:
      return "/data/stats/"

  @staticmethod
  def config_root() -> str:
    if PC:
      return Paths.comma_home()
    else:
      return "/tmp/.comma"
