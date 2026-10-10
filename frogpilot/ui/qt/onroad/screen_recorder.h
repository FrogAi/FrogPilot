#pragma once

class ScreenRecorder {
public:
  static bool active();
  static void attach();  // call after the EGL display exists and before the first frame is swapped, when the swapchain is created
  static bool replayReady();
  static bool replaySaving();
  static void saveReplay();
  static void setReplayDuration(int seconds);
  static void start();
  static void stop();
};
