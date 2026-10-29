from pathlib import Path

from openpilot.common.basedir import BASEDIR

ACTIVE_THEME_PATH = Path(BASEDIR) / "frogpilot/assets/active_theme"
HOLIDAY_THEMES_PATH = Path(BASEDIR) / "frogpilot/assets/holiday_themes"
MODELS_FOLDER = "openpilot-0_10_3"
MODELS_PATH = Path("/data/models") / MODELS_FOLDER
MODELS_LIST_PATH = MODELS_PATH / "models.json"
MODEL_RUNNERS_PATH = Path(BASEDIR) / "frogpilot/common/model_runners"
RANDOM_EVENTS_PATH = Path(BASEDIR) / "frogpilot/assets/random_events"
STOCK_MODEL_PATH = Path(BASEDIR) / "selfdrive/modeld/models"
STOCK_THEME_PATH = Path(BASEDIR) / "frogpilot/assets/stock_theme"
STOCK_THEME_COLORS_PATH = STOCK_THEME_PATH / "colors/colors.json"
THEME_COLORS_PATH = ACTIVE_THEME_PATH / "colors/colors.json"
THEME_SAVE_PATH = Path("/data/themes")
STEERING_WHEELS_PATH = THEME_SAVE_PATH / "steering_wheels"
THEME_PACKS_PATH = THEME_SAVE_PATH / "theme_packs"
FROG_THEME_PATH = THEME_PACKS_PATH / "frog"
THEME_SIZES_PATH = THEME_SAVE_PATH / "theme_sizes.json"

BACKUP_PATH = Path("/cache/on_backup")
FROGPILOT_BACKUPS = Path("/data/backups")
PARAMS_CACHE_PATH = Path("/cache/params")
RESTORE_TEMP_PATH = Path("/data/restore_temp")
TOGGLE_BACKUPS = Path("/data/toggle_backups")

BOOT_LOGO_PATH = Path("/usr/comma/bg.jpg")
FROGPILOT_BOOT_LOGO_PATH = Path(BASEDIR) / "frogpilot/assets/other_images/frogpilot_boot_logo.jpg"
STOCK_BOOT_LOGO_PATH = Path(BASEDIR) / "frogpilot/assets/other_images/stock_bg.jpg"

BORDERS_PATH = Path(BASEDIR) / "frogpilot/assets/borders.json"
# City data from GeoNames (https://www.geonames.org/), CC BY 4.0
CITIES_PATH = Path(BASEDIR) / "frogpilot/assets/cities.json"

ERROR_LOGS_PATH = Path("/data/error_logs")
ERROR_LOG_PATH = ERROR_LOGS_PATH / "error.txt"
SCREEN_RECORDINGS_PATH = Path("/data/media/screen_recordings")
SENTRY_QUEUE_PATH = Path("/data/sentry_queue")
SPEED_LIMIT_CAPTURES_PATH = Path("/data/media/speed_limit_captures")
TELEMETRY_SCHEMAS_PATH = Path("/data/telemetry_schemas")
TMUX_LOGS_PATH = Path("/data/tmux_logs")

FROGPILOT_PANELS_PATH = Path(BASEDIR) / "frogpilot/ui/qt/offroad"
FROGPILOT_UI_DIR = Path(BASEDIR) / "frogpilot/ui"
STOCK_PANELS_PATH = Path(BASEDIR) / "selfdrive/ui/qt"

FROGS_GO_MOO_PATH = Path("/persist/frogsgomoo.py")

HD_LOGS_PATH = Path("/data/media/0/realdata_HD")
HD_PATH = Path("/cache/use_HD")

KONIK_LOGS_PATH = Path("/data/media/0/realdata_konik")
KONIK_PATH = Path("/cache/use_konik")

LOG_ROOTS = (Path("/data/media/0/realdata"), HD_LOGS_PATH, KONIK_LOGS_PATH)

MAPD_DOWNLOAD_MENU_PATH = Path(BASEDIR) / "mapd_download_menu.json"
MAPS_PATH = Path("/data/media/0/osm/offline")

NNFF_MODELS_PATH = Path(BASEDIR) / "frogpilot/assets/nnff_models"

NOT_VETTED_PATH = Path("/data/openpilot/not_vetted")

OLD_OPENPILOT = Path("/data/safe_staging/old_openpilot")

SPEED_LIMIT_FILLER_IMAGES_PATH = Path("/data/speed_limit_filler_images")
SPEED_LIMIT_FILLER_PATH = Path("/data/speed_limit_filler.json")
SPEED_LIMIT_FILLER_PENDING_IMAGES_PATH = SPEED_LIMIT_FILLER_IMAGES_PATH / "pending"

SPEED_LIMIT_READER_BUNDLED_PATH = Path(BASEDIR) / "frogpilot/assets/models"
SPEED_LIMIT_READER_MODEL_PATH = Path("/data/speed_limit_reader_model")
SPEED_LIMIT_READER_MANIFEST_PATH = SPEED_LIMIT_READER_MODEL_PATH / "manifest.json"

TAILSCALE_PATH = Path("/data/tailscale")
TAILSCALE_SOCKET_PATH = TAILSCALE_PATH / "tailscaled.sock"
TAILSCALE_STATE_PATH = TAILSCALE_PATH / "state"

THE_POND_ASSETS_PATH = Path(BASEDIR) / "frogpilot/system/the_pond/assets"
THE_POND_PATH = Path("/data/the_pond")
THE_POND_DRIVES_PATH = THE_POND_PATH / "drives"
THE_POND_THUMBNAILS_PATH = THE_POND_PATH / "thumbnails"
THE_POND_VIDEOS_PATH = THE_POND_PATH / "videos"
