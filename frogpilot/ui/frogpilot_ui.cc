#include "frogpilot/ui/frogpilot_ui.h"

#include "selfdrive/ui/ui.h"

static void update_state(FrogPilotUIState *fs) {
  FrogPilotUIScene &frogpilot_scene = fs->frogpilot_scene;
}

FrogPilotUIState::FrogPilotUIState(QObject *parent) : QObject(parent) {
  wifi = new WifiManager(this);
}

FrogPilotUIState *frogpilotUIState() {
  static FrogPilotUIState frogpilot_ui_state;
  return &frogpilot_ui_state;
}

void FrogPilotUIState::update() {
  update_state(this);

  if (!uiState()->scene.started) {
    frogpilot_scene.reverse = false;
    frogpilot_scene.standstill = false;
  }
}
