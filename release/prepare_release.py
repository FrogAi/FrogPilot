#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys

from pathlib import Path


GENERATED_FILES = {
  "cereal/messaging/bridge",
  "common/params_pyx.so",
  "common/transformations/transformations.so",
  "msgq_repo/msgq/ipc_pyx.so",
  "msgq_repo/msgq/visionipc/visionipc_pyx.so",
  "panda/board/obj/bootstub.panda.bin",
  "panda/board/obj/bootstub.panda_h7.bin",
  "panda/board/obj/panda.bin.signed",
  "panda/board/obj/panda_h7.bin.signed",
  "rednose_repo/rednose/helpers/ekf_sym_pyx.so",
  "selfdrive/controls/lib/longitudinal_mpc_lib/c_generated_code/acados_ocp_solver_pyx.so",
  "selfdrive/controls/lib/longitudinal_mpc_lib/c_generated_code/libacados_ocp_solver_long.so",
  "selfdrive/modeld/models/commonmodel_pyx.so",
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
  "selfdrive/locationd/models/generated/*.so",
  "selfdrive/modeld/models/*.pkl",
)

KEEP_FILES = {
  ".github/workflows/compile_frogpilot.yaml",
  ".github/workflows/review_pull_request.yaml",
  ".github/workflows/schedule_update.yaml",
  ".github/workflows/update_pr_branch.yaml",
  ".github/workflows/update_release_branch.yaml",
  "LICENSE",
  "README.md",
  "RELEASES.md",
  "common/version.h",
  "launch_chffrplus.sh",
  "launch_env.sh",
  "launch_openpilot.sh",
  "msgq",
  "opendbc",
  "opendbc_repo/LICENSE",
  "panda/LICENSE",
  "panda/__init__.py",
  "panda/board/body/__init__.py",
  "panda/board/jungle/__init__.py",
  "rednose",
  "rednose_repo/LICENSE",
  "selfdrive/ui/translations/languages.json",
  "teleoprtc",
  "teleoprtc_repo/LICENSE",
  "third_party/libyuv/LICENSE",
  "tinygrad",
  "tinygrad_repo/LICENSE",
  "tools/lib/kbhit.py",
}

RUNTIME_DIRECTORIES = (
  "cereal/",
  "common/",
  "frogpilot/",
  "msgq_repo/msgq/",
  "opendbc_repo/opendbc/",
  "openpilot/",
  "panda/python/",
  "rednose_repo/rednose/",
  "selfdrive/",
  "system/",
  "teleoprtc_repo/teleoprtc/",
  "third_party/acados/larch64/lib/",
  "tinygrad_repo/tinygrad/",
  "tools/bodyteleop/",
  "tools/joystick/",
  "tools/longitudinal_maneuvers/",
  "tools/webcam/",
)

EXCLUDED_PATHS = (
  "opendbc_repo/opendbc/dbc/generator/",
  "selfdrive/assets/fonts/unifont.otf",
  "selfdrive/controls/lib/lateral_mpc_lib/",
  "selfdrive/debug/",
  "selfdrive/ui/translations/",
  "tinygrad_repo/tinygrad/runtime/autogen/am/",
  "tinygrad_repo/tinygrad/runtime/autogen/amd_gpu.py",
  "tinygrad_repo/tinygrad/runtime/autogen/nv/",
  "tinygrad_repo/tinygrad/runtime/autogen/nv_gpu.py",
  "tinygrad_repo/tinygrad/viz/",
)

SOURCE_SUFFIXES = {".c", ".cc", ".h", ".hpp", ".ksy", ".md", ".onnx", ".pxd", ".pyx", ".qrc"}

ENTRYPOINTS = {
  "launch_chffrplus.sh",
  "launch_openpilot.sh",
  "system/hardware/tici/agnos.py",
  "system/hardware/tici/updater",
  "system/hardware/tici/updater_magic",
  "system/hardware/tici/updater_weston",
  "system/manager/manager.py",
}

IMPORT_CHECK = """
import glob
import importlib
import os
import pickle
import subprocess

from opendbc.car.values import PLATFORMS
from openpilot.system.manager.process import NativeProcess
from openpilot.system.manager.process_config import managed_processes


for process in managed_processes.values():
  if isinstance(process, NativeProcess):
    command = next(argument for argument in process.cmdline if argument.startswith("./"))
    executable = os.path.join(process.cwd, command)
    assert os.access(executable, os.X_OK), process.name

    dynamic = subprocess.run(["readelf", "--dynamic", executable], check=True, stdout=subprocess.PIPE, text=True).stdout
    if "(NEEDED)" in dynamic:
      libraries = subprocess.run(["ldd", executable], check=True, stdout=subprocess.PIPE, text=True).stdout
      assert "not found" not in libraries, (process.name, libraries)
  elif process.enabled:
    importlib.import_module(process.module)

importlib.import_module("openpilot.system.webrtc.device.video")

for filename in glob.glob("frogpilot/assets/models/*.pkl") + glob.glob("selfdrive/modeld/models/*_tinygrad.pkl"):
  with open(filename, "rb") as file:
    pickle.load(file)

for platform in PLATFORMS.values():
  for name in platform.config.dbc_dict.values():
    assert os.path.isfile(f"opendbc/dbc/{name}.dbc"), (platform, name)
"""


def is_runtime_file(relative):
  path = Path(relative)

  if relative.startswith(EXCLUDED_PATHS):
    return False

  if any(part.startswith(".") or part in {"test", "tests"} for part in path.parts):
    return False

  if path.name.startswith("test_") or path.name == "SConscript" or path.suffix in SOURCE_SUFFIXES:
    return False

  return relative.startswith(RUNTIME_DIRECTORIES)


def is_elf(path):
  with path.open("rb") as file:
    return file.read(4) == b"\x7fELF"


def verify(root, files):
  broken = [relative for relative in files if not (root / relative).exists()]

  if broken:
    raise RuntimeError(f"broken release symlinks: {broken}")

  for relative in ENTRYPOINTS:
    assert os.access(root / relative, os.X_OK), relative

  for relative in ("launch_chffrplus.sh", "launch_env.sh", "launch_openpilot.sh", "system/hardware/tici/updater"):
    subprocess.run(["bash", "-n", root / relative], check=True)

  environment = os.environ | {"PYTHONPATH": f"{root}/frogpilot/third_party:{root}"}
  subprocess.run([sys.executable, "-B", "-c", IMPORT_CHECK], cwd=root, env=environment, check=True)


def main(source, destination):
  tracked = subprocess.run(["git", "-C", destination, "ls-files", "-z"], check=True, stdout=subprocess.PIPE, text=True).stdout.split("\0")

  files = {relative for relative in tracked if is_runtime_file(relative)} | KEEP_FILES | GENERATED_FILES
  for pattern in GENERATED_GLOBS:
    files |= {path.relative_to(source).as_posix() for path in source.glob(pattern)}

  for relative in files:
    target = destination / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / relative, target, follow_symlinks=False)

    if not target.is_symlink() and is_elf(target):
      subprocess.run(["llvm-strip", "--strip-debug", target], check=True)

  (destination / "prebuilt").touch()

  verify(destination, files)


if __name__ == "__main__":
  main(Path(sys.argv[1]), Path(sys.argv[2]))
