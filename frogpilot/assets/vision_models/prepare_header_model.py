"""Prepare PaddleOCR's pinned English recognizer for OpenCV DNN.

Development-only dependencies: onnx==1.22.0, onnxruntime==1.24.4, numpy.
Download inference.onnx from the revision linked in README.md, then run:
  python prepare_header_model.py /path/to/inference.onnx /path/to/output.onnx
The deployed worker does not import this script or require ONNX Runtime.
"""
import argparse
import hashlib
import tempfile
from pathlib import Path

import numpy as np
import onnx
import onnxruntime as ort

SOURCE_HASH = "b5f833dfc5d0eb71da397b4efa06ebeee9b431b690a47d6af40d77d8eabc557f"


def prepare(source, destination):
  if hashlib.sha256(source.read_bytes()).hexdigest() != SOURCE_HASH:
    raise ValueError("Expected the pinned PaddleOCR inference.onnx")
  model = onnx.load(source)
  # IR 3 requires initializers to be graph inputs. Use IR 7 so constant folding
  # can remove inputs and introduce initializers without leaving an invalid graph.
  model.ir_version = 7
  for dimension, size in zip(model.graph.input[0].type.tensor_type.shape.dim, (1, 3, 48, 160), strict=True):
    dimension.ClearField("dim_param")
    dimension.dim_value = size
  with tempfile.TemporaryDirectory() as directory:
    fixed = Path(directory) / "fixed.onnx"
    optimized = Path(directory) / "optimized.onnx"
    onnx.save(model, fixed)
    options = ort.SessionOptions()
    options.log_severity_level = 3
    options.intra_op_num_threads = options.inter_op_num_threads = 1
    options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_BASIC
    options.optimized_model_filepath = str(optimized)
    ort.InferenceSession(str(fixed), options, providers=["CPUExecutionProvider"])
    onnx.checker.check_model(onnx.load(optimized))
    options.optimized_model_filepath = ""
    sessions = [ort.InferenceSession(str(path), options, providers=["CPUExecutionProvider"]) for path in (source, optimized)]
    rng = np.random.default_rng(6)
    for value in (np.zeros((1, 3, 48, 160), np.float32), np.ones((1, 3, 48, 160), np.float32),
                  rng.uniform(-1, 1, (1, 3, 48, 160)).astype(np.float32)):
      outputs = [session.run(None, {"x": value})[0] for session in sessions]
      np.testing.assert_allclose(outputs[0], outputs[1], rtol=1e-4, atol=1e-5)
    destination.write_bytes(optimized.read_bytes())
  print(hashlib.sha256(destination.read_bytes()).hexdigest(), destination)


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("source", type=Path)
  parser.add_argument("destination", type=Path)
  arguments = parser.parse_args()
  prepare(arguments.source, arguments.destination)
