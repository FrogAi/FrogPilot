import asyncio
import io
import json
import shutil
import tarfile
import zipfile
import zlib
import zstandard

from aiohttp import web
from pathlib import Path, PurePosixPath

from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params
from openpilot.system.hardware import HARDWARE
from openpilot.system.updated.common import set_consistent_flag
from openpilot.system.updated.updated import FINALIZED

from openpilot.frogpilot.common import frogpilot_backups, frogpilot_variables
from openpilot.frogpilot.system.the_pond import tailscale, utilities

BACKUP_EXTENSION = ".tar.zst"

MAX_SETTING_SIZE = 1024 * 1024

OPENPILOT_FILES = ("launch_chffrplus.sh", "launch_openpilot.sh")
OPENPILOT_FOLDERS = ("selfdrive", "system")

routes = web.RouteTableDef()

def get_backup_name(name):
  if not isinstance(name, str):
    raise web.HTTPBadRequest(text="invalid_name")

  name = name.strip().replace(" ", "_")
  if not utilities.is_valid_name(name):
    raise web.HTTPBadRequest(text="invalid_name")
  if frogpilot_variables.AUTOMATIC_BACKUP_SUFFIX in name or frogpilot_variables.IN_PROGRESS_BACKUP_SUFFIX in name:
    raise web.HTTPBadRequest(text="invalid_name")
  return name


def get_settings_backup(name):
  path = frogpilot_variables.TOGGLE_BACKUPS / name
  if name in ("", ".", "..") or path.parent != frogpilot_variables.TOGGLE_BACKUPS or not path.is_dir():
    raise web.HTTPNotFound(text="not_found")
  if frogpilot_variables.IN_PROGRESS_BACKUP_SUFFIX in name:
    raise web.HTTPNotFound(text="not_found")
  return path


def list_backups(directory, pattern):
  backups = []
  for path in directory.glob(pattern):
    if frogpilot_variables.IN_PROGRESS_BACKUP_SUFFIX not in path.name:
      backups.append({
        "automatic": frogpilot_variables.AUTOMATIC_BACKUP_SUFFIX in path.name,
        "modified": int(path.stat().st_mtime),
        "name": path.name.removesuffix(BACKUP_EXTENSION),
        "size": path.stat().st_size if path.is_file() else None,
      })
  return sorted(backups, key=lambda backup: backup["modified"], reverse=True)


def get_setting_label(label, metric):
  if isinstance(label, tuple):
    return label[1] if metric else label[0]
  return label


def format_setting(key, value, metric):
  labels = frogpilot_variables.SETTING_LABELS.get(key, {})
  value_labels = get_setting_label(labels.get("values", {}), metric)

  if value is None:
    return "Not set"
  if isinstance(value, bool):
    return "On" if value else "Off"
  if isinstance(value, str):
    return value_labels.get(value, f"“{value}”")
  if isinstance(value, (dict, list)):
    return json.dumps(value)
  if not labels:
    return str(value)

  decimals = labels.get("decimals", 0)
  for label_value, label in value_labels.items():
    if round(label_value * 10 ** decimals) == round(value * 10 ** decimals):
      return label
  return f"{value:.{decimals}f}{get_setting_label(labels.get('unit', ''), metric)}"


def get_changes(params, backup_path):
  metric = params.get_bool("IsMetric")
  backup_metric = metric
  metric_path = backup_path / "IsMetric"
  if metric_path.is_file() and metric_path.stat().st_size > 0:
    backup_metric = params.cpp2python("IsMetric", metric_path.read_bytes())

  changes = []
  for path in sorted(backup_path.iterdir()):
    key = path.name
    if not path.is_file() or key not in frogpilot_variables.SETTINGS_BACKUP_KEYS:
      continue

    current = params.get(key)
    value = params.cpp2python(key, path.read_bytes()) if path.stat().st_size > 0 else params.get_default_value(key)
    if value != current:
      name = get_setting_label(frogpilot_variables.SETTING_LABELS.get(key, {}).get("name", key), metric)
      if key in frogpilot_variables.SECRET_KEYS:
        changes.append({"current": None, "key": key, "name": name, "secret": True, "value": None})
      else:
        changes.append({
          "current": format_setting(key, current, metric),
          "key": key,
          "name": name,
          "secret": False,
          "value": format_setting(key, value, backup_metric),
        })
  return sorted(changes, key=lambda change: change["name"].lower())


def read_settings_archive(params, file):
  try:
    backup_zip = zipfile.ZipFile(file)
  except zipfile.BadZipFile:
    raise web.HTTPBadRequest(text="invalid_file") from None

  settings = {}
  with backup_zip:
    for member in backup_zip.infolist():
      if member.file_size > MAX_SETTING_SIZE:
        raise web.HTTPBadRequest(text="invalid_file")
      if member.is_dir() or member.filename not in frogpilot_variables.SETTINGS_BACKUP_KEYS:
        continue

      try:
        value = backup_zip.read(member)
      except (NotImplementedError, RuntimeError, zipfile.BadZipFile, zlib.error):
        raise web.HTTPBadRequest(text="invalid_file") from None
      if not value or params.cpp2python(member.filename, value) is not None:
        settings[member.filename] = value
  return settings


def reset_settings(stock):
  params = Params()
  params_cache = Params(str(frogpilot_variables.PARAMS_CACHE_PATH))

  for key in params.all_keys():
    key = key.decode()
    if key in frogpilot_variables.EXCLUDED_KEYS:
      continue

    value = params.get_stock_value(key) if stock else params.get_default_value(key)
    if value == "":
      params.remove(key)
      params_cache.remove(key)
    elif value is not None and value != params.get(key):
      params.put(key, value)

  for key in frogpilot_variables.RESET_REMOVED_KEYS:
    params.remove(key)
    params_cache.remove(key)

  if params.get_bool("ThePondPairing"):
    params.put("ThePondPairingCode", utilities.new_pairing_code())

  if stock:
    params.put("TuningLevel", frogpilot_variables.TUNING_LEVELS["DEVELOPER"])
    params.put_bool("TuningLevelConfirmed", True)

  if params.get_bool("IsMetric"):
    for key in frogpilot_variables.METRIC_CONVERSIONS:
      params.put(key, utilities.convert_metric(key, params.get(key), True))


async def sign_out_of_tailscale(the_pond):
  if not tailscale.is_installed():
    return True

  await tailscale.cancel_login(the_pond)
  process = await asyncio.create_subprocess_exec(*tailscale.get_command("logout"))
  return await process.wait() == 0


def find_openpilot_root(extract_path):
  candidates = [extract_path / "data" / "openpilot", *sorted(path for path in extract_path.iterdir() if path.is_dir()), extract_path]
  for candidate in candidates:
    if all((candidate / file).is_file() for file in OPENPILOT_FILES) and all((candidate / folder).is_dir() for folder in OPENPILOT_FOLDERS):
      return candidate
  return None


def is_frogpilot_backup(path):
  files = set()
  folders = set()
  try:
    with open(path, "rb") as file:
      stream = zstandard.ZstdDecompressor().stream_reader(file)
      with tarfile.open(fileobj=stream, mode="r|") as archive:
        for member in archive:
          member_path = PurePosixPath(member.name)
          if member_path.is_absolute() or ".." in member_path.parts:
            return False

          if member.isfile():
            files.add(member_path)
          elif member.isdir():
            folders.add(member_path)
          elif not member.issym():
            return False

          folders.update(member_path.parents)
  except (tarfile.TarError, zstandard.ZstdError):
    return False

  top_folders = [folder for folder in folders if len(folder.parts) == 1]
  for candidate in [PurePosixPath("data/openpilot"), *top_folders, PurePosixPath()]:
    has_files = all(candidate / file in files for file in OPENPILOT_FILES)
    has_folders = all(candidate / folder in folders for folder in OPENPILOT_FOLDERS)
    if has_files and has_folders:
      return True
  return False


async def create_frogpilot_backup(the_pond, name):
  destination = frogpilot_variables.FROGPILOT_BACKUPS / f"{name}{BACKUP_EXTENSION}"
  in_progress_destination = frogpilot_variables.FROGPILOT_BACKUPS / f"{name}{frogpilot_variables.IN_PROGRESS_BACKUP_SUFFIX}{BACKUP_EXTENSION}"

  try:
    await utilities.run_heavy_process(the_pond, ["tar", "--use-compress-program=zstd", "-cf", str(in_progress_destination),
                                                 "-C", str(Path(BASEDIR).parent), Path(BASEDIR).name])
    if in_progress_destination.stat().st_size == 0:
      raise RuntimeError("Empty backup")
    in_progress_destination.rename(destination)
  finally:
    in_progress_destination.unlink(missing_ok=True)


async def restore_frogpilot_backup(the_pond, backup_path):
  cancelled = False
  extract_path = frogpilot_variables.RESTORE_TEMP_PATH
  try:
    shutil.rmtree(extract_path, ignore_errors=True)
    extract_path.mkdir(parents=True)

    if shutil.disk_usage(extract_path.parent).free / 4 <= backup_path.stat().st_size:
      raise RuntimeError("Not enough space to restore")

    await utilities.run_heavy_process(the_pond, ["tar", "--use-compress-program=zstd", "-xf", str(backup_path), "-C", str(extract_path)])

    source_root = await asyncio.to_thread(find_openpilot_root, extract_path)
    if source_root is None:
      raise RuntimeError("Not a FrogPilot backup")

    install = asyncio.create_task(install_frogpilot_backup(the_pond, source_root))
    while not install.done():
      try:
        await asyncio.shield(install)
      except asyncio.CancelledError:
        cancelled = True
    install.result()
  finally:
    await asyncio.to_thread(shutil.rmtree, extract_path, True)

  if not cancelled:
    HARDWARE.reboot()


async def install_frogpilot_backup(the_pond, source_root):
  await utilities.run_heavy_process(the_pond, ["rsync", "-a", "--delete", f"{source_root}/", f"{BASEDIR}/"])
  set_consistent_flag(FINALIZED, False)
  frogpilot_variables.BACKUP_PATH.touch()


async def receive_frogpilot_backup(the_pond, reader):
  field = await reader.next()
  if field is None or field.name != "name":
    raise web.HTTPBadRequest(text="invalid_name")

  name = get_backup_name(await field.text())
  destination = frogpilot_variables.FROGPILOT_BACKUPS / f"{name}{BACKUP_EXTENSION}"
  if destination.exists():
    raise web.HTTPConflict(text="name_in_use")

  field = await reader.next()
  if field is None or field.name != "file":
    raise web.HTTPBadRequest(text="invalid_file")

  frogpilot_variables.FROGPILOT_BACKUPS.mkdir(parents=True, exist_ok=True)
  in_progress_destination = frogpilot_variables.FROGPILOT_BACKUPS / f"{name}{frogpilot_variables.IN_PROGRESS_BACKUP_SUFFIX}{BACKUP_EXTENSION}"
  try:
    with open(in_progress_destination, "wb") as file:
      while chunk := await field.read_chunk(utilities.STREAM_CHUNK_SIZE):
        await asyncio.to_thread(file.write, chunk)

    async with the_pond.heavy_work:
      valid = await asyncio.to_thread(is_frogpilot_backup, in_progress_destination)
    if not valid:
      raise web.HTTPBadRequest(text="invalid_file")

    in_progress_destination.rename(destination)
  finally:
    in_progress_destination.unlink(missing_ok=True)


@routes.post("/api/frogpilot_backups")
async def create_frogpilot_backup_route(request):
  the_pond = request.app[utilities.THE_POND]
  name = get_backup_name((await utilities.read_json(request)).get("name"))
  if (frogpilot_variables.FROGPILOT_BACKUPS / f"{name}{BACKUP_EXTENSION}").exists():
    raise web.HTTPConflict(text="name_in_use")

  frogpilot_variables.FROGPILOT_BACKUPS.mkdir(parents=True, exist_ok=True)
  utilities.start_job(the_pond, "frogpilot_backup", create_frogpilot_backup(the_pond, name), {"kind": "create", "name": name})
  return web.Response(status=202)


@routes.delete("/api/frogpilot_backups")
async def delete_frogpilot_backups(request):
  job = utilities.get_job(request.app[utilities.THE_POND], "frogpilot_backup")
  if job is not None and job["state"] == "running":
    raise web.HTTPConflict(text="busy")
  for path in frogpilot_variables.FROGPILOT_BACKUPS.glob("*"):
    if path.is_file():
      path.unlink()
  return web.Response(status=204)


@routes.delete("/api/frogpilot_backups/{name}")
async def delete_frogpilot_backup(request):
  utilities.get_file(frogpilot_variables.FROGPILOT_BACKUPS, f"{request.match_info['name']}{BACKUP_EXTENSION}").unlink()
  return web.Response(status=204)


@routes.get("/api/frogpilot_backups")
async def get_frogpilot_backups(request):
  backups = await asyncio.to_thread(list_backups, frogpilot_variables.FROGPILOT_BACKUPS, f"*{BACKUP_EXTENSION}")
  return web.json_response({"backups": backups, "job": utilities.get_job(request.app[utilities.THE_POND], "frogpilot_backup")})


@routes.get("/api/frogpilot_backups/{name}/download")
async def download_frogpilot_backup(request):
  path = utilities.get_file(frogpilot_variables.FROGPILOT_BACKUPS, f"{request.match_info['name']}{BACKUP_EXTENSION}")
  return web.FileResponse(path, headers={"Content-Disposition": f'attachment; filename="{path.name}"'})


@routes.post("/api/frogpilot_backups/import")
async def import_frogpilot_backup(request):
  try:
    await receive_frogpilot_backup(request.app[utilities.THE_POND], await request.multipart())
  except web.HTTPException:
    await request.release()
    raise
  return web.Response(status=204)


@routes.post("/api/frogpilot_backups/{name}/restore")
async def restore_frogpilot_backup_route(request):
  the_pond = request.app[utilities.THE_POND]
  path = utilities.get_file(frogpilot_variables.FROGPILOT_BACKUPS, f"{request.match_info['name']}{BACKUP_EXTENSION}")
  if the_pond.sm["deviceState"].started:
    raise web.HTTPConflict(text="car_on")

  utilities.start_job(the_pond, "frogpilot_backup", restore_frogpilot_backup(the_pond, path), {"kind": "restore", "name": request.match_info["name"]})
  return web.Response(status=202)


@routes.post("/api/settings_backups")
async def create_settings_backup_route(request):
  params = request.app[utilities.THE_POND].params
  name = get_backup_name((await utilities.read_json(request)).get("name"))
  if (frogpilot_variables.TOGGLE_BACKUPS / name).exists():
    raise web.HTTPConflict(text="name_in_use")

  frogpilot_variables.TOGGLE_BACKUPS.mkdir(parents=True, exist_ok=True)
  await asyncio.to_thread(frogpilot_backups.create_settings_backup, params, name)
  return web.Response(status=204)


@routes.delete("/api/settings_backups")
async def delete_settings_backups(request):
  await asyncio.to_thread(shutil.rmtree, frogpilot_variables.TOGGLE_BACKUPS, True)
  frogpilot_variables.TOGGLE_BACKUPS.mkdir(parents=True, exist_ok=True)
  return web.Response(status=204)


@routes.delete("/api/settings_backups/{name}")
async def delete_settings_backup(request):
  await asyncio.to_thread(shutil.rmtree, get_settings_backup(request.match_info["name"]))
  return web.Response(status=204)


@routes.get("/api/settings_backups")
async def get_settings_backups(request):
  return web.json_response(await asyncio.to_thread(list_backups, frogpilot_variables.TOGGLE_BACKUPS, "*"))


@routes.get("/api/settings_backups/{name}/changes")
async def get_settings_backup_changes(request):
  params = request.app[utilities.THE_POND].params
  path = get_settings_backup(request.match_info["name"])
  return web.json_response(await asyncio.to_thread(get_changes, params, path))


@routes.get("/api/settings_backups/{name}/download")
async def download_settings_backup(request):
  path = get_settings_backup(request.match_info["name"])

  archive = io.BytesIO()
  with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as backup_zip:
    for file in sorted(path.iterdir()):
      if file.is_file() and file.name in frogpilot_variables.SETTINGS_BACKUP_KEYS and file.name not in frogpilot_variables.SECRET_KEYS:
        backup_zip.write(file, file.name)

  return web.Response(body=archive.getvalue(), content_type="application/zip",
                      headers={"Content-Disposition": f'attachment; filename="{path.name}.zip"'})


@routes.post("/api/settings_backups/import")
async def import_settings_backup(request):
  params = request.app[utilities.THE_POND].params
  form = await request.post()

  name = get_backup_name(form.get("name"))
  if (frogpilot_variables.TOGGLE_BACKUPS / name).exists():
    raise web.HTTPConflict(text="name_in_use")

  upload = form.get("file")
  if not isinstance(upload, web.FileField):
    raise web.HTTPBadRequest(text="invalid_file")

  settings = await asyncio.to_thread(read_settings_archive, params, upload.file)
  if not settings:
    raise web.HTTPBadRequest(text="invalid_file")

  in_progress_destination = frogpilot_variables.TOGGLE_BACKUPS / f"{name}{frogpilot_variables.IN_PROGRESS_BACKUP_SUFFIX}"
  shutil.rmtree(in_progress_destination, ignore_errors=True)
  in_progress_destination.mkdir(parents=True)
  for key, value in settings.items():
    (in_progress_destination / key).write_bytes(value)
  in_progress_destination.rename(frogpilot_variables.TOGGLE_BACKUPS / name)
  return web.Response(status=204)


@routes.post("/api/settings_backups/{name}/restore")
async def restore_settings_backup_route(request):
  the_pond = request.app[utilities.THE_POND]
  path = get_settings_backup(request.match_info["name"])

  await asyncio.to_thread(frogpilot_backups.restore_settings_backup, the_pond.params, path)
  the_pond.send_request("updateToggles")
  return web.Response(status=204)


@routes.post("/api/settings/reset")
async def reset_settings_route(request):
  the_pond = request.app[utilities.THE_POND]
  stock = (await utilities.read_json(request)).get("stock")
  if not isinstance(stock, bool):
    raise web.HTTPBadRequest(text="invalid_stock")
  if the_pond.sm["deviceState"].started:
    raise web.HTTPConflict(text="car_on")

  await asyncio.to_thread(reset_settings, stock)
  the_pond.send_request("updateToggles")

  if not await sign_out_of_tailscale(the_pond):
    raise web.HTTPInternalServerError(text="tailscale_sign_out_failed")
  return web.Response(status=204)
