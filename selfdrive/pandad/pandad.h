#pragma once

#include <string>
#include <vector>

#include "selfdrive/pandad/panda.h"

bool safety_setter_thread(std::vector<Panda *> pandas);
void pandad_main_thread(std::vector<std::string> serials);

// FrogPilot variables
inline bool heartbeat_engaged(bool engaged, bool always_on_lateral, bool controls_allowed) {
  return engaged || (always_on_lateral && !controls_allowed);
}
