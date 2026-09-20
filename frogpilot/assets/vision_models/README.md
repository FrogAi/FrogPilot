# Vision speed limit models

The detector and number classifier are unmodified model files from [StarPilot `Dom`, commit
b990a776b2fceefaca4d678cf87067874f4b1672](https://github.com/firestar5683/StarPilot/tree/b990a776b2fceefaca4d678cf87067874f4b1672/starpilot/assets/vision_models).

| File | Input | Output | SHA-256 |
| --- | --- | --- | --- |
| speed_limit_us_detector.onnx | RGB float32, 1x3x256x256 | 1x5x1344, xywh and one sign confidence | 82408b68c79c269296f0af942130c5383cace4ee06c78e2a4690e8488720116a |
| speed_limit_us_value_classifier.onnx | RGB float32, 1x3x128x128 | 19 probabilities, including reject | 07c6696e530eb940d2757d5849b4bc0f1d785cda704e5296e18c0a94959f30a5 |
| speed_limit_header.onnx | BGR float32, 1x3x48x160, normalized to [-1, 1] | 1x20x438 CTC probabilities | c76ae166149da213cc9268b0c73381925a825fe2737a23d41b4c8dc1221431f0 |

The classifier class order is `10, 100, 15, 20, 25, 30, 35, 40, 45, 5, 50, 55,
60, 65, 70, 75, 80, 90, reject`. The port accepts U.S. limits from 5 through
80 mph in increments of 5; it does not reinterpret the number as km/h when the
display is metric. The detector has **one** class. StarPilot's legacy three-class
detector branches do not describe these weights.

## Licensing and attribution

Both StarPilot ONNX files explicitly declare `AGPL-3.0 License
(https://ultralytics.com/license)` in their metadata. The license text is retained
in [AGPL-3.0.txt](AGPL-3.0.txt). They are not covered by an assumption that all
StarPilot assets are MIT-licensed. The upstream training/export instructions are
[here](https://github.com/firestar5683/StarPilot/blob/b990a776b2fceefaca4d678cf87067874f4b1672/docs/how-to/train-speed-limit-vision.md).

**Before upstream distribution**, resolve whether these weights and their
corresponding-source obligations are acceptable to FrogPilot, obtain an appropriate
license, or replace them with compatible weights and revalidate the integration.
Adding the license text does not itself settle those obligations. No relicensing
of FrogPilot's existing code is intended by this port.

The inference pipeline's preprocessing, classifier ordering, confidence weighting,
and regulatory-panel heuristic are adapted from StarPilot's
[`speed_limit_vision.py`](https://github.com/firestar5683/StarPilot/blob/b990a776b2fceefaca4d678cf87067874f4b1672/starpilot/system/speed_limit_vision.py).
Its MIT notice, including firestar5683 and StarPilot contributors, is preserved
in [STARPILOT-LICENSE](STARPILOT-LICENSE). The worker, confirmation state, freshness
checks, and FrogPilot integration were implemented separately for this port.

## Heading recognizer

The additional heading recognizer comes from PaddlePaddle's
[`en_PP-OCRv5_mobile_rec_onnx`, revision
3fafbc3b5dcf93dd72add9f48368be8a3a2cd33b](https://huggingface.co/PaddlePaddle/en_PP-OCRv5_mobile_rec_onnx/tree/3fafbc3b5dcf93dd72add9f48368be8a3a2cd33b).
The publisher identifies its license as Apache-2.0. The PaddleOCR project's
license text is retained in [PADDLEOCR-LICENSE](PADDLEOCR-LICENSE).

| Original publisher file | SHA-256 |
| --- | --- |
| inference.onnx | b5f833dfc5d0eb71da397b4efa06ebeee9b431b690a47d6af40d77d8eabc557f |
| inference.yml | 27e91d0582f40168aa218303c76e184bc78fa7a5d105aad0cfbad8458b441067 |

The shipped graph fixes the input to batch 1 and width 160, uses ONNX IR 7, and
folds constants with ONNX Runtime's basic optimizer so existing OpenCV DNN can
load it. It is not retrained or quantized. [prepare_header_model.py](prepare_header_model.py)
checks the original checksum, performs the conversion, validates the graph, and
compares its output against the original model on three deterministic inputs.
The recorded conversion uses `onnx==1.22.0` and `onnxruntime==1.24.4`; these are
development dependencies only. The deployed worker still uses OpenCV DNN on CPU.

The recognizer sees two cropped heading rows placed side by side. Only the exact
CTC-decoded words `SPEED LIMIT` (ignoring case and spaces) can pass. Its dictionary
indices are pinned from `inference.yml`; all other emitted characters reject the
heading. OCR never supplies a numeric speed. This fallback requires a strong sign
proposal and number classification, and is used only when the panel color checks
reject a crop. It has at most two alignments per proposal and cannot establish
lane applicability or whether a conditional limit is active.

Model replacement is deliberate: update the checksums, class mapping, shape
checks, and regression evidence together. Missing, modified, unsupported, or
non-finite model outputs make the Vision source unavailable.
