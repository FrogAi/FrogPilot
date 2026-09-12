#!/usr/bin/env python3
import argparse
import shutil
import subprocess

from pathlib import Path

# Files outside the runtime packages, plus runtime exceptions to the source-file filter.
KEEP_FILES = {
  ".github/workflows/compile_frogpilot.yaml",
  ".github/workflows/review_pull_request.yaml",
  ".github/workflows/schedule_update.yaml",
  ".github/workflows/update_pr_branch.yaml",
  ".github/workflows/update_release_branch.yaml",
  "LICENSE",
  "README.md",
  "RELEASES.md",
  "body/LICENSE",
  "common/version.h",
  "launch_chffrplus.sh",
  "launch_env.sh",
  "launch_openpilot.sh",
  "msgq",
  "panda/board/jungle/__init__.py",
  "rednose",
  "rednose_repo/LICENSE",
  "system/camerad/sensors/ar0231_cl.h",
  "system/camerad/sensors/os04c10_cl.h",
  "system/camerad/sensors/ox03c10_cl.h",
  "teleoprtc",
  "teleoprtc_repo/LICENSE",
  "third_party/libyuv/LICENSE",
  "third_party/snpe/larch64",
  "tinygrad",
  "tinygrad_repo/LICENSE",
}

RUNTIME_DIRECTORIES = (
  "cereal/",
  "common/",
  "frogpilot/",
  "msgq_repo/msgq/",
  "opendbc/",
  "openpilot/",
  "panda/",
  "rednose_repo/rednose/",
  "selfdrive/",
  "system/",
  "teleoprtc_repo/teleoprtc/",
  "tinygrad_repo/tinygrad/",
  "tools/bodyteleop/",
  "tools/lib/",
)

EXCLUDED_DIRECTORIES = (
  "common/mock/",
  "frogpilot/third_party/reactivex/",
  "frogpilot/tools/",
  "opendbc/generator/",
  "panda/board/",
  "panda/crypto/",
  "selfdrive/controls/lib/lateral_mpc_lib/",
  "selfdrive/debug/",
  "selfdrive/ui/translations/",
  "tinygrad_repo/tinygrad/frontend/",
  "tinygrad_repo/tinygrad/viz/",
  "tinygrad_repo/tinygrad/runtime/autogen/nv/",
)

# Large unused models, obsolete binaries, and unsupported GPU backends.
EXCLUDED_FILES = {
  "frogpilot/classic_modeld/models/dmonitoring_model_q.dlc",
  "frogpilot/tinygrad_modeld/models/dmonitoring_model_tinygrad.pkl",
  "rednose_repo/rednose/helpers/chi2_lookup_table.npy",
  "selfdrive/ui/qt/spinner_larch64",
  "selfdrive/ui/qt/text_larch64",
  "tinygrad_repo/tinygrad/runtime/autogen/amd_gpu.py",
  "tinygrad_repo/tinygrad/runtime/autogen/cuda.py",
  "tinygrad_repo/tinygrad/runtime/autogen/hip.py",
  "tinygrad_repo/tinygrad/runtime/autogen/ib.py",
  "tinygrad_repo/tinygrad/runtime/autogen/io_uring.py",
  "tinygrad_repo/tinygrad/runtime/autogen/nv_gpu.py",
  "tinygrad_repo/tinygrad/runtime/autogen/nvrtc.py",
  "tinygrad_repo/tinygrad/runtime/autogen/qcom_dsp.py",
  "tinygrad_repo/tinygrad/runtime/autogen/webgpu.py",
  "tinygrad_repo/tinygrad/runtime/ops_cuda.py",
  "tinygrad_repo/tinygrad/runtime/ops_dsp.py",
  "tinygrad_repo/tinygrad/runtime/ops_metal.py",
  "tinygrad_repo/tinygrad/runtime/ops_nv.py",
  "tinygrad_repo/tinygrad/runtime/ops_webgpu.py",
}

BUILD_SUFFIXES = {
  ".a", ".c", ".cc", ".cpp", ".current", ".h", ".hpp", ".o", ".onnx", ".os", ".pxd", ".pyc", ".pyo", ".pyx", ".qrc", ".ts",
}


def is_runtime_file(path, root):
  relative = path.relative_to(root)
  name = relative.as_posix()

  if name in KEEP_FILES:
    return True

  if name.startswith(("body/board/obj/", "panda/board/obj/")):
    return path.name.endswith(".bin.signed") or (path.name.startswith("bootstub.") and path.suffix == ".bin")

  if name.startswith(EXCLUDED_DIRECTORIES) or name in EXCLUDED_FILES:
    return False
  if any(part.startswith(".") or part in {"test", "tests", "docs", "examples", "site_scons", "__pycache__"} for part in relative.parts):
    return False
  if path.name.startswith("test_") or path.name.endswith("_test.py") or path.name in {"conftest.py", "SConstruct", "SConscript"}:
    return False
  if path.suffix.lower() in BUILD_SUFFIXES:
    return False

  if name.startswith("third_party/"):
    return name.startswith((
      "third_party/acados/larch64/lib/",
      "third_party/maplibre-native-qt/larch64/lib/",
      "third_party/snpe/aarch64-ubuntu-gcc7.5/",
      "third_party/snpe/dsp/",
    )) and (path.name.endswith(".so") or ".so." in path.name)

  return name.startswith(RUNTIME_DIRECTORIES)


def prepare_release(root):
  root = root.resolve()
  if not (root / ".git").is_dir() or (root / ".git").is_symlink():
    raise RuntimeError("release input must be a built Git checkout")

  maplibre = root / "third_party/maplibre-native-qt/larch64/lib/libQMapLibre.so"
  for path in (root / "README.md", root / "launch_openpilot.sh", root / "selfdrive/ui/ui", maplibre.with_name("libQMapLibre.so.3.0.0")):
    if not path.is_file() or path.stat().st_size == 0:
      raise RuntimeError(f"required release input is missing or empty: {path.relative_to(root)}")

  strip = shutil.which("llvm-strip")
  if strip is None:
    raise RuntimeError("llvm-strip is required to prepare a release")

  links = []

  def trim(directory):
    for path in directory.iterdir():
      if path == root / ".git":
        continue

      if path.is_dir() and not path.is_symlink():
        trim(path)

        if not any(path.iterdir()):
          path.rmdir()

        continue

      if not is_runtime_file(path, root):
        path.unlink()
        continue

      if path.is_symlink():
        links.append(path)
        continue

      if path == maplibre or path.relative_to(root).as_posix().startswith("third_party/snpe/dsp/"):
        continue

      with path.open("rb") as file:
        is_elf = file.read(4) == b"\x7fELF"

      if is_elf:
        subprocess.run([strip, "--strip-debug", str(path)], check=True)

  trim(root)

  maplibre.unlink(missing_ok=True)
  maplibre.symlink_to("libQMapLibre.so.3.0.0")

  for path in links:
    if not path.exists() or not path.resolve().is_relative_to(root):
      raise RuntimeError(f"broken or escaping release symlink: {path.relative_to(root)}")

  (root / "prebuilt").touch()
  print(f"Prepared release at {root}")


def main():
  parser = argparse.ArgumentParser(description="Trim a clean, already-built checkout in place for release")
  parser.add_argument("root", type=Path)
  args = parser.parse_args()

  prepare_release(args.root)


if __name__ == "__main__":
  main()
