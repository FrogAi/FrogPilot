import os
import pickle
import sys

if "JIT_BATCH_SIZE" not in os.environ:
  os.environ["JIT_BATCH_SIZE"] = "0"

import numpy as np

from tinygrad import Device, Tensor, TinyJit
from tinygrad.nn.onnx import OnnxRunner


def compile_model(onnx_path, output_path):
  runner = OnnxRunner(onnx_path)

  input_shapes = {name: spec.shape for name, spec in runner.graph_inputs.items()}
  state_names = [name for name in input_shapes if f"next_{name}" in runner.graph_outputs]
  host_names = [name for name in input_shapes if name != "new_img" and name not in state_names]
  img_shape = (1, 2 * input_shapes["new_img"][1], *input_shapes["new_img"][2:])

  def run(img, big_img, **inputs):
    new_img = Tensor.cat(img[:, img_shape[1] // 2:], big_img[:, img_shape[1] // 2:], dim=0).contiguous()
    outputs = runner({"new_img": new_img, **{name: value.to(Device.DEFAULT) for name, value in inputs.items()}})
    for name in state_names:
      inputs[name].assign(outputs[f"next_{name}"].contiguous())

    return outputs["outputs"].cast("float32").contiguous().realize(*[inputs[name] for name in state_names])

  model_run = TinyJit(run, prune=True)

  rng = np.random.default_rng(100)
  imgs = {name: rng.integers(0, 256, img_shape, dtype=np.uint8) for name in ("big_img", "img")}
  host_inputs = {name: rng.standard_normal(input_shapes[name]).astype(np.float32) for name in host_names}

  results = []
  for _ in range(3):
    states = {name: Tensor.zeros(*input_shapes[name], dtype=runner.graph_inputs[name].dtype).contiguous().realize() for name in state_names}
    device_imgs = {name: Tensor(value, device=Device.DEFAULT).realize() for name, value in imgs.items()}
    npy_inputs = {name: Tensor(value.copy(), device="NPY").realize() for name, value in host_inputs.items()}

    outputs = model_run(**device_imgs, **npy_inputs, **states).numpy()
    results.append((outputs, {name: state.numpy() for name, state in states.items()}))

  (captured, captured_states), (replayed, replayed_states) = results[1], results[2]
  np.testing.assert_equal(captured, replayed)
  for name in state_names:
    np.testing.assert_equal(captured_states[name], replayed_states[name])
    assert np.any(replayed_states[name]), f"{name} was not updated"

  print(f"captured {len(model_run.captured.jit_cache)} kernels, jit run validated")

  with open(output_path, "wb") as f:
    pickle.dump(model_run, f)


if __name__ == "__main__":
  compile_model(sys.argv[1], sys.argv[2])
