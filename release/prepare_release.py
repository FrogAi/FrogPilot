#!/usr/bin/env python3
import argparse
import fnmatch
import os
import shutil
import subprocess
import sys

from pathlib import Path


# Manifest inclusions
FIRMWARE_FILES = {
  "panda/board/obj/bootstub.panda.bin",
  "panda/board/obj/bootstub.panda_h7.bin",
  "panda/board/obj/panda.bin.signed",
  "panda/board/obj/panda_h7.bin.signed",
}

GENERATED_FILES = FIRMWARE_FILES | {
  "cereal/messaging/bridge",
  "common/params_pyx.so",
  "common/transformations/transformations.so",
  "msgq_repo/msgq/ipc_pyx.so",
  "msgq_repo/msgq/visionipc/visionipc_pyx.so",
  "rednose_repo/rednose/helpers/ekf_sym_pyx.so",
  "selfdrive/controls/lib/lateral_mpc_lib/c_generated_code/acados_ocp_solver_pyx.so",
  "selfdrive/controls/lib/lateral_mpc_lib/c_generated_code/libacados_ocp_solver_lat.so",
  "selfdrive/controls/lib/longitudinal_mpc_lib/c_generated_code/acados_ocp_solver_pyx.so",
  "selfdrive/controls/lib/longitudinal_mpc_lib/c_generated_code/libacados_ocp_solver_long.so",
  "selfdrive/locationd/models/generated/libcar.so",
  "selfdrive/locationd/models/generated/libpose.so",
  "selfdrive/modeld/models/commonmodel_pyx.so",
  "selfdrive/modeld/models/dmonitoring_model_metadata.pkl",
  "selfdrive/modeld/models/dmonitoring_model_tinygrad.pkl",
  "selfdrive/modeld/models/driving_policy_metadata.pkl",
  "selfdrive/modeld/models/driving_policy_tinygrad.pkl",
  "selfdrive/modeld/models/driving_vision_metadata.pkl",
  "selfdrive/modeld/models/driving_vision_tinygrad.pkl",
  "selfdrive/pandad/pandad",
  "selfdrive/pandad/pandad_api_impl.so",
  "selfdrive/ui/ui",
  "system/camerad/camerad",
  "system/loggerd/bootlog",
  "system/loggerd/encoderd",
  "system/loggerd/loggerd",
}

GENERATED_GLOBS = (
  "opendbc_repo/opendbc/dbc/*_generated.dbc",
)

LEGAL_FILES = {
  "LICENSE",
  "opendbc_repo/LICENSE",
  "panda/LICENSE",
  "rednose_repo/LICENSE",
  "teleoprtc_repo/LICENSE",
  "third_party/libyuv/LICENSE",
  "tinygrad_repo/LICENSE",
}

REPOSITORY_WORKFLOWS = {
  ".github/workflows/compile_frogpilot.yaml",
  ".github/workflows/review_pull_request.yaml",
  ".github/workflows/schedule_update.yaml",
  ".github/workflows/update_pr_branch.yaml",
  ".github/workflows/update_release_branch.yaml",
}

CAMERAD_OPENCL_PROGRAM = "system/camerad/cameras/process_raw.cl"

CAMERAD_OPENCL_HEADERS = {
  "system/camerad/sensors/ar0231_cl.h",
  "system/camerad/sensors/os04c10_cl.h",
  "system/camerad/sensors/ox03c10_cl.h",
}

REQUIRED_FILES = GENERATED_FILES | LEGAL_FILES | REPOSITORY_WORKFLOWS | {
  "RELEASES.md",
  "common/version.h",
  "frogpilot/assets/city_lookup.sqlite",
  "frogpilot/navigation/mapd",
  "frogpilot/third_party/timezonefinder/data/boundaries/coordinates.fbs",
  "frogpilot/third_party/timezonefinder/data/holes/coordinates.fbs",
  "frogpilot/third_party/timezonefinder/data/hybrid_shortcuts_uint16.fbs",
  "frogpilot/third_party/timezonefinder/data/timezone_names.txt",
  "launch_chffrplus.sh",
  "launch_env.sh",
  "launch_openpilot.sh",
  "opendbc_repo/opendbc/car/torque_data/override.toml",
  "opendbc_repo/opendbc/car/torque_data/params.toml",
  "opendbc_repo/opendbc/car/torque_data/substitute.toml",
  "panda/board/body/__init__.py",
  "panda/board/jungle/__init__.py",
  "selfdrive/assets/offroad/fcc.html",
  "selfdrive/modeld/transforms/loadyuv.cl",
  "selfdrive/modeld/transforms/transform.cl",
  "selfdrive/ui/translations/languages.json",
  "system/hardware/tici/agnos.json",
  "system/hardware/tici/updater",
  "system/hardware/tici/updater_magic",
  "system/hardware/tici/updater_weston",
}

RUNTIME_SUFFIXES = {
  ".capnp",
  ".cl",
  ".css",
  ".db",
  ".dbc",
  ".gif",
  ".gz",
  ".html",
  ".ico",
  ".jpeg",
  ".jpg",
  ".js",
  ".json",
  ".mjs",
  ".nmconnection",
  ".npy",
  ".pem",
  ".pkl",
  ".png",
  ".py",
  ".qml",
  ".signed",
  ".so",
  ".sqlite",
  ".svg",
  ".thneed",
  ".ttf",
  ".wav",
  ".woff",
  ".woff2",
}

SIGNAL_MARKER_GLOB = "frogpilot/assets/holiday_themes/*/signals/traditional_125"

SOURCE_SYMLINKS = {
  "cereal/car.capnp": "../opendbc_repo/opendbc/car/car.capnp",
  "frogpilot/assets/stock_theme/icons": "../../../selfdrive/assets/images",
  "frogpilot/assets/stock_theme/sounds": "../../../selfdrive/assets/sounds",
  "frogpilot/assets/stock_theme/steering_wheel/wheel.png": "../../../../selfdrive/assets/icons/chffr_wheel.png",
  "msgq": "msgq_repo/msgq",
  "opendbc": "opendbc_repo/opendbc",
  "openpilot/common": "../common",
  "openpilot/frogpilot": "../frogpilot",
  "openpilot/selfdrive": "../selfdrive/",
  "openpilot/system": "../system/",
  "openpilot/third_party": "../third_party",
  "openpilot/tools": "../tools",
  "rednose": "rednose_repo/rednose",
  "teleoprtc": "teleoprtc_repo/teleoprtc",
  "third_party/acados/larch64/lib/libqpOASES_e.so": "libqpOASES_e.so.3.1",
  "tinygrad": "tinygrad_repo/tinygrad",
}

TOOLS_FILES = {
  "tools/joystick/joystick_control.py",
  "tools/joystick/joystickd.py",
  "tools/lib/__init__.py",
  "tools/lib/helpers.py",
  "tools/lib/kbhit.py",
  "tools/longitudinal_maneuvers/maneuversd.py",
  "tools/webcam/camera.py",
  "tools/webcam/camerad.py",
}

TINYGRAD_FILES = {
  "tinygrad_repo/LICENSE",
  "tinygrad_repo/tinygrad/__init__.py",
  "tinygrad_repo/tinygrad/codegen/__init__.py",
  "tinygrad_repo/tinygrad/codegen/gpudims.py",
  "tinygrad_repo/tinygrad/codegen/late/devectorizer.py",
  "tinygrad_repo/tinygrad/codegen/late/expander.py",
  "tinygrad_repo/tinygrad/codegen/late/linearizer.py",
  "tinygrad_repo/tinygrad/codegen/opt/__init__.py",
  "tinygrad_repo/tinygrad/codegen/opt/heuristic.py",
  "tinygrad_repo/tinygrad/codegen/opt/postrange.py",
  "tinygrad_repo/tinygrad/codegen/opt/search.py",
  "tinygrad_repo/tinygrad/codegen/opt/tc.py",
  "tinygrad_repo/tinygrad/codegen/simplify.py",
  "tinygrad_repo/tinygrad/device.py",
  "tinygrad_repo/tinygrad/dtype.py",
  "tinygrad_repo/tinygrad/engine/__init__.py",
  "tinygrad_repo/tinygrad/engine/jit.py",
  "tinygrad_repo/tinygrad/engine/memory.py",
  "tinygrad_repo/tinygrad/engine/realize.py",
  "tinygrad_repo/tinygrad/engine/schedule.py",
  "tinygrad_repo/tinygrad/gradient.py",
  "tinygrad_repo/tinygrad/helpers.py",
  "tinygrad_repo/tinygrad/mixin/__init__.py",
  "tinygrad_repo/tinygrad/mixin/math.py",
  "tinygrad_repo/tinygrad/mixin/movement.py",
  "tinygrad_repo/tinygrad/nn/state.py",
  "tinygrad_repo/tinygrad/renderer/__init__.py",
  "tinygrad_repo/tinygrad/renderer/cstyle.py",
  "tinygrad_repo/tinygrad/runtime/__init__.py",
  "tinygrad_repo/tinygrad/runtime/autogen/__init__.py",
  "tinygrad_repo/tinygrad/runtime/autogen/adreno.py",
  "tinygrad_repo/tinygrad/runtime/autogen/kgsl.py",
  "tinygrad_repo/tinygrad/runtime/autogen/libc.py",
  "tinygrad_repo/tinygrad/runtime/autogen/libusb.py",
  "tinygrad_repo/tinygrad/runtime/autogen/opencl.py",
  "tinygrad_repo/tinygrad/runtime/autogen/pci.py",
  "tinygrad_repo/tinygrad/runtime/autogen/vfio.py",
  "tinygrad_repo/tinygrad/runtime/graph/__init__.py",
  "tinygrad_repo/tinygrad/runtime/graph/hcq.py",
  "tinygrad_repo/tinygrad/runtime/ops_cl.py",
  "tinygrad_repo/tinygrad/runtime/ops_npy.py",
  "tinygrad_repo/tinygrad/runtime/ops_qcom.py",
  "tinygrad_repo/tinygrad/runtime/support/__init__.py",
  "tinygrad_repo/tinygrad/runtime/support/c.py",
  "tinygrad_repo/tinygrad/runtime/support/hcq.py",
  "tinygrad_repo/tinygrad/runtime/support/memory.py",
  "tinygrad_repo/tinygrad/runtime/support/system.py",
  "tinygrad_repo/tinygrad/runtime/support/usb.py",
  "tinygrad_repo/tinygrad/schedule/__init__.py",
  "tinygrad_repo/tinygrad/schedule/indexing.py",
  "tinygrad_repo/tinygrad/schedule/multi.py",
  "tinygrad_repo/tinygrad/schedule/rangeify.py",
  "tinygrad_repo/tinygrad/tensor.py",
  "tinygrad_repo/tinygrad/uop/__init__.py",
  "tinygrad_repo/tinygrad/uop/decompositions.py",
  "tinygrad_repo/tinygrad/uop/ops.py",
  "tinygrad_repo/tinygrad/uop/spec.py",
  "tinygrad_repo/tinygrad/uop/symbolic.py",
  "tinygrad_repo/tinygrad/uop/upat.py",
  "tinygrad_repo/tinygrad/uop/validate.py",
}


# Manifest exclusions
DEVELOPMENT_DIRECTORIES = {"docs", "examples", "site_scons"}

EXCLUDED_FILES = {
  ".gitattributes",
  ".gitmodules",
  ".lfsconfig",
  ".overlay_init",
  "Jenkinsfile",
  "common/prefix.py",
  "common/timeout.py",
  "common/transformations/coordinates.py",
  "panda/crypto/sign.py",
  "panda/setup.py",
  "rednose_repo/rednose/helpers/chi2_lookup.py",
  "rednose_repo/rednose/helpers/chi2_lookup_table.npy",
  "rednose_repo/rednose/helpers/ekf_sym.py",
  "rednose_repo/rednose/helpers/sympy_helpers.py",
  "rednose_repo/setup.py",
  "selfdrive/car/docs.py",
  "selfdrive/modeld/get_model_metadata.py",
  "selfdrive/modeld/models/big_driving_policy.onnx",
  "selfdrive/modeld/models/big_driving_vision.onnx",
  "selfdrive/ui/update_translations.py",
  "system/hardware/tici/precise_power_measure.py",
  "system/manager/build.py",
  "system/qcomgpsd/nmeaport.py",
}

EXCLUDED_PREFIXES = (
  ".devcontainer/",
  ".git/",
  ".github/",
  ".vscode/",
  "common/mock/",
  "frogpilot/tools/",
  "opendbc_repo/opendbc/dbc/generator/",
  "release/",
  "scripts/",
  "selfdrive/debug/",
)


# Verification contracts
EXPECTED_SYMLINKS = SOURCE_SYMLINKS

NON_MANAGER_ELF_ENTRYPOINTS = {
  "selfdrive/pandad/pandad",
  "system/hardware/tici/updater_weston",
  "system/loggerd/bootlog",
}

REQUIRED_EXECUTABLES = NON_MANAGER_ELF_ENTRYPOINTS | {
  "frogpilot/navigation/mapd",
  "launch_chffrplus.sh",
  "launch_env.sh",
  "launch_openpilot.sh",
  "system/hardware/tici/updater",
  "system/hardware/tici/updater_magic",
}


# Manifest construction
def relative_path(path: Path, root: Path) -> str:
  return path.relative_to(root).as_posix()


def is_shared_library(relative: str) -> bool:
  name = Path(relative).name
  return ".so." in name or name.endswith(".so")


def is_test_path(relative: str) -> bool:
  path = Path(relative)
  if any(part in {"test", "tests"} for part in path.parts):
    return True
  return path.name == "conftest.py" or path.name.startswith("test_") or path.name.endswith("_test.py")


def is_third_party_runtime(relative: str) -> bool:
  if relative == "third_party/libyuv/LICENSE":
    return True
  if relative.startswith("third_party/acados/larch64/lib/"):
    return is_shared_library(relative)
  return False


def is_tinygrad_runtime(relative: str) -> bool:
  return relative in TINYGRAD_FILES


def is_runtime_file(path: Path, root: Path) -> bool:
  relative = relative_path(path, root)
  parts = Path(relative).parts

  if relative in CAMERAD_OPENCL_HEADERS:
    return (root / CAMERAD_OPENCL_PROGRAM).is_file()
  if relative in REQUIRED_FILES or relative in SOURCE_SYMLINKS or relative in TOOLS_FILES:
    return True
  if fnmatch.fnmatchcase(relative, SIGNAL_MARKER_GLOB):
    return True
  if relative in EXCLUDED_FILES or relative.startswith(EXCLUDED_PREFIXES) or is_test_path(relative):
    return False
  if any(part in DEVELOPMENT_DIRECTORIES for part in parts) or path.name.startswith(".") or path.name in {"Jenkinsfile", "SConstruct", "SConscript"}:
    return False
  if relative.startswith("panda/board/"):
    return relative in FIRMWARE_FILES
  if relative.startswith("third_party/"):
    return is_third_party_runtime(relative)
  if relative.startswith("tinygrad_repo/"):
    return is_tinygrad_runtime(relative)
  if relative.startswith("tools/"):
    return relative.startswith("tools/bodyteleop/") or relative in TOOLS_FILES
  if relative.startswith("selfdrive/ui/translations/"):
    return False
  if path.is_symlink():
    return True
  if is_shared_library(relative):
    return True
  if path.suffix.lower() in RUNTIME_SUFFIXES:
    return True
  return False


def tracked_files(source: Path, git_directory: Path) -> list[Path]:
  command = ["git", f"--git-dir={git_directory}", f"--work-tree={source}", "ls-files", "-z"]
  output = subprocess.run(command, check=True, capture_output=True).stdout
  return [source / os.fsdecode(relative) for relative in output.split(b"\0") if relative]


def generated_files(source: Path) -> list[Path]:
  paths = [source / relative for relative in GENERATED_FILES]
  for pattern in GENERATED_GLOBS:
    paths.extend(source.glob(pattern))
  return paths


def manifest(source: Path, git_directory: Path) -> list[Path]:
  candidates = set(tracked_files(source, git_directory) + generated_files(source))
  files = [path for path in candidates if (path.is_file() or path.is_symlink()) and is_runtime_file(path, source)]
  return sorted(files, key=lambda path: relative_path(path, source))


# Release operations
def release_tree(root: Path):
  for path in root.rglob("*"):
    relative = relative_path(path, root)
    if relative == ".git" or relative.startswith(".git/"):
      continue
    yield path


def copy_file(source: Path, destination: Path) -> None:
  destination.parent.mkdir(parents=True, exist_ok=True)
  if source.is_symlink():
    destination.symlink_to(os.readlink(source), target_is_directory=source.is_dir())
  else:
    shutil.copy2(source, destination)


def runtime_dbc_files(root: Path) -> set[str]:
  code = """
from opendbc.car.values import PLATFORMS

names = set()
for platform in PLATFORMS.values():
  names.update(name for name in platform.config.dbc_dict.values() if name is not None)
print("\\n".join(sorted(names)))
"""
  environment = os.environ.copy()
  environment["PYTHONPATH"] = str(root)
  result = subprocess.run([sys.executable, "-B", "-c", code], cwd=root, env=environment, check=True, stdout=subprocess.PIPE, text=True)
  return {f"opendbc/dbc/{name}.dbc" for name in result.stdout.splitlines()}


def prune_dbcs(root: Path) -> int:
  required = runtime_dbc_files(root)
  removed = 0
  for path in (root / "opendbc/dbc").glob("*.dbc"):
    if relative_path(path, root) not in required:
      path.unlink()
      removed += 1
  return removed


def strip_debug_sections(root: Path) -> tuple[int, int]:
  strip = shutil.which("llvm-strip")
  if strip is None:
    raise RuntimeError("llvm-strip is required to prepare a production release")

  stripped = 0
  bytes_removed = 0
  for path in sorted(release_tree(root)):
    if not path.is_file() or path.is_symlink():
      continue
    # llvm-strip breaks Qualcomm's DSP-loadable binaries.
    if relative_path(path, root).startswith("third_party/snpe/dsp/"):
      continue
    with path.open("rb") as file:
      if file.read(4) != b"\x7fELF":
        continue
    before = path.stat().st_size
    subprocess.run([strip, "--strip-debug", str(path)], check=True)
    bytes_removed += before - path.stat().st_size
    stripped += 1
  return stripped, bytes_removed


# Release verification
def verify(root: Path) -> None:
  runtime_opencl_headers = CAMERAD_OPENCL_HEADERS if (root / CAMERAD_OPENCL_PROGRAM).is_file() else set()
  required_files = REQUIRED_FILES | runtime_opencl_headers

  missing = [relative for relative in sorted(required_files) if not (root / relative).is_file()]
  if missing:
    raise RuntimeError(f"required release file is missing: {missing[0]}")
  if not (root / "prebuilt").is_file():
    raise RuntimeError("prebuilt release marker is missing")

  for relative in sorted(required_files):
    path = root / relative
    if path.stat().st_size == 0:
      raise RuntimeError(f"required release file is empty: {relative}")

  broken_links = [path for path in release_tree(root) if path.is_symlink() and not path.exists()]
  if broken_links:
    raise RuntimeError(f"broken release symlink: {relative_path(broken_links[0], root)}")
  symlinks = {relative_path(path, root): os.readlink(path) for path in release_tree(root) if path.is_symlink()}
  if symlinks != EXPECTED_SYMLINKS:
    difference = sorted(symlinks.keys() ^ EXPECTED_SYMLINKS.keys())
    if not difference:
      difference = sorted(relative for relative, target in symlinks.items() if target != EXPECTED_SYMLINKS[relative])
    raise RuntimeError(f"unexpected release symlink set: {difference[0]}")
  release_root = root.resolve()
  for relative in sorted(EXPECTED_SYMLINKS):
    target = (root / relative).resolve()
    if not target.exists() or not target.is_relative_to(release_root):
      raise RuntimeError(f"release symlink escapes or has no target: {relative}")

  for relative in sorted(REQUIRED_EXECUTABLES):
    path = root / relative
    if not path.is_file() or not os.access(path, os.X_OK):
      raise RuntimeError(f"required executable is not executable: {relative}")

  import_check = """
import glob
import importlib
import os
import panda
import pickle
import shutil
import subprocess
import teleoprtc
import teleoprtc.info
from openpilot.system.manager.process import DaemonProcess, NativeProcess, PythonProcess
from openpilot.system.manager.process_config import managed_processes

for process in managed_processes.values():
  if isinstance(process, NativeProcess) and process.enabled:
    command = process.cmdline[0]
    if command == "env":
      command = next(argument for argument in process.cmdline[1:] if "=" not in argument)
    path = os.path.join(os.getcwd(), process.cwd, command) if command.startswith("./") else shutil.which(command)
    assert path is not None and os.path.isfile(path) and os.access(path, os.X_OK), (process.name, path)
    with open(path, "rb") as file:
      is_elf = file.read(4) == b"\\x7fELF"
    if is_elf:
      dynamic = subprocess.run(["readelf", "--dynamic", "--wide", path], check=True, capture_output=True, text=True).stdout
      if "(NEEDED)" in dynamic:
        dependencies = subprocess.run(["ldd", path], capture_output=True, text=True)
        resolved = dependencies.returncode == 0 and "not found" not in dependencies.stdout and "not found" not in dependencies.stderr
        assert resolved, (process.name, dependencies.stdout, dependencies.stderr)
  elif isinstance(process, PythonProcess) and process.enabled:
    importlib.import_module(process.module)
  elif isinstance(process, DaemonProcess) and process.enabled:
    importlib.import_module(process.module)

importlib.import_module("openpilot.system.webrtc.device.video")

for filename in glob.glob("selfdrive/modeld/models/*_tinygrad.pkl"):
  with open(filename, "rb") as file:
    pickle.load(file)
"""
  environment = os.environ.copy()
  environment["PYTHONPATH"] = os.pathsep.join((str(root / "frogpilot/third_party"), str(root)))
  subprocess.run([sys.executable, "-B", "-c", import_check], cwd=root, env=environment, check=True)

  missing_dbcs = [relative for relative in sorted(runtime_dbc_files(root)) if not (root / relative).is_file()]
  if missing_dbcs:
    raise RuntimeError(f"runtime DBC is missing: {missing_dbcs[0]}")

  sqlite_check = """
import sqlite3
connection = sqlite3.connect("frogpilot/assets/city_lookup.sqlite")
assert connection.execute("PRAGMA quick_check").fetchone() == ("ok",)
"""
  subprocess.run([sys.executable, "-B", "-c", sqlite_check], cwd=root, check=True)

  forbidden_suffixes = {".a", ".c", ".cc", ".cpp", ".h", ".hpp", ".o", ".onnx", ".os", ".pxd", ".pyx", ".qrc", ".ts"}
  for path in release_tree(root):
    if not path.is_file() or path.is_symlink():
      continue
    if path.suffix in forbidden_suffixes and relative_path(path, root) not in (runtime_opencl_headers | {"common/version.h"}):
      raise RuntimeError(f"build input entered release: {relative_path(path, root)}")
    with path.open("rb") as file:
      if file.read(4) != b"\x7fELF":
        continue
    header = subprocess.run(["readelf", "--file-header", str(path)], check=True, capture_output=True, text=True).stdout
    if "AArch64" not in header and "QUALCOMM DSP" not in header.upper():
      raise RuntimeError(f"wrong ELF architecture in {relative_path(path, root)}")
    sections = subprocess.run(["readelf", "--sections", "--wide", str(path)], check=True, capture_output=True, text=True).stdout
    if ".debug_" in sections or ".zdebug_" in sections:
      raise RuntimeError(f"ELF debug sections remain in {relative_path(path, root)}")
    relative = relative_path(path, root)
    if relative in NON_MANAGER_ELF_ENTRYPOINTS:
      dynamic = subprocess.run(["readelf", "--dynamic", "--wide", str(path)], check=True, capture_output=True, text=True).stdout
      if "(NEEDED)" in dynamic:
        dependencies = subprocess.run(["ldd", str(path)], cwd=path.parent, capture_output=True, text=True)
        if dependencies.returncode != 0 or "not found" in dependencies.stdout or "not found" in dependencies.stderr:
          raise RuntimeError(f"unresolved ELF dependency in {relative}:\n{dependencies.stdout}{dependencies.stderr}")

  for relative in ("launch_chffrplus.sh", "launch_env.sh", "launch_openpilot.sh", "system/hardware/tici/updater"):
    subprocess.run(["bash", "-n", str(root / relative)], check=True)

  forbidden = [
    path for path in release_tree(root)
    if path.name == ".overlay_init" or path.name == "__pycache__" or is_test_path(relative_path(path, root))
  ]
  if forbidden:
    raise RuntimeError(f"development file entered release: {relative_path(forbidden[0], root)}")


# Release orchestration
def materialize(source: Path, destination: Path) -> None:
  source = source.resolve()
  destination = destination.resolve()
  unexpected = [path for path in destination.iterdir() if path.name != ".git"]
  if unexpected:
    raise RuntimeError(f"release destination is not empty: {unexpected[0]}")

  files = manifest(source, destination / ".git")
  for path in files:
    copy_file(path, destination / relative_path(path, source))

  (destination / "prebuilt").touch()
  removed_dbcs = prune_dbcs(destination)
  stripped, bytes_removed = strip_debug_sections(destination)
  verify(destination)
  print(f"Prepared {len(files) + 1 - removed_dbcs} files; removed {removed_dbcs} unused DBCs; stripped {stripped} ELFs and removed {bytes_removed} debug bytes")


# Command-line entry point
def main() -> None:
  parser = argparse.ArgumentParser(description="Materialize and verify a minimal prebuilt FrogPilot release")
  parser.add_argument("source", type=Path)
  parser.add_argument("destination", type=Path)
  args = parser.parse_args()
  materialize(args.source, args.destination)


if __name__ == "__main__":
  main()
