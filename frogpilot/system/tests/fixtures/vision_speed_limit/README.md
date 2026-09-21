# Speed sign regression frames

`sign_20_frame_140.png` and `sign_20_frame_146.png` are lossless decodes of zero-based
frames 140 and 146 from StarPilot's 30 FPS
[`speed-limit-vision-demo.mp4`](https://github.com/firestar5683/StarPilot/blob/b990a776b2fceefaca4d678cf87067874f4b1672/docs/assets/speed-limit-vision-demo.mp4),
at commit `b990a776b2fceefaca4d678cf87067874f4b1672`.

They visibly contain a 20 mph school-zone sign with a yellow header. The text
recognizer can read the printed number, but the full detector must reject this
conditional layout. The native NV12/Params/controller test checks that it cannot
supply a limit. The worker does not determine flashing-light status. These are
compressed screen-recording frames with UI overlays, not raw camera data or an
accuracy benchmark. See the retained
[StarPilot MIT notice](../../../../assets/vision_models/STARPILOT-LICENSE).
