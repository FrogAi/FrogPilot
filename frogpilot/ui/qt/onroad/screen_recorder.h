#pragma once

class ScreenRecorder {
public:
  static void attach();  // call after the EGL display exists and before the first frame is swapped, when the swapchain is created
  static void start();
  static void stop();
  static bool active();
};
