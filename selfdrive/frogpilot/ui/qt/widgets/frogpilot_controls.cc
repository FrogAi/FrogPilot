#include "selfdrive/ui/ui.h"

Params paramsMemory{"/dev/shm/params"};

std::atomic<int> callCounter(0);

void updateFrogPilotToggles() {
  int currentCall = ++callCounter;
  std::thread([currentCall]() {
    paramsMemory.putBool("FrogPilotTogglesUpdated", true);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (currentCall == callCounter) {
      paramsMemory.putBool("FrogPilotTogglesUpdated", false);
    }
  }).detach();
}
