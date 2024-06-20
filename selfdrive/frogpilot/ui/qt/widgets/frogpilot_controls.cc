#include "selfdrive/ui/ui.h"

Params paramsMemory{"/dev/shm/params"};

void updateFrogPilotToggles() {
  std::thread([]() {
    paramsMemory.putBool("FrogPilotTogglesUpdated", true);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    paramsMemory.putBool("FrogPilotTogglesUpdated", false);
  }).detach();
}
