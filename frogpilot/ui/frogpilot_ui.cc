#include "frogpilot/ui/frogpilot_ui.h"

static void update_state(FrogPilotUIState *fs) {
  FrogPilotUIScene &frogpilot_scene = fs->frogpilot_scene;

  SubMaster &sm = *(fs->sm);
  sm.update(0);

  if (sm.updated("carState")) {
    const cereal::CarState::Reader &carState = sm["carState"].getCarState();
    frogpilot_scene.parked = carState.getGearShifter() == cereal::CarState::GearShifter::PARK;
  }
  if (sm.updated("deviceState")) {
    const cereal::DeviceState::Reader &deviceState = sm["deviceState"].getDeviceState();
    frogpilot_scene.online = deviceState.getNetworkType() != cereal::DeviceState::NetworkType::NONE;
  }
}

FrogPilotUIState::FrogPilotUIState(QObject *parent) : QObject(parent) {
  sm = std::make_unique<SubMaster, const std::initializer_list<const char *>>({
    "carControl", "carState", "controlsState", "deviceState", "frogpilotCarState", "frogpilotSelfdriveState",
    "frogpilotDeviceState", "frogpilotPlan", "frogpilotRadarState", "liveDelay",
    "liveParameters", "liveTorqueParameters", "liveTracks", "navInstruction", "selfdriveState"
  });
}

FrogPilotUIState *frogpilotUIState() {
  static FrogPilotUIState frogpilot_ui_state;
  return &frogpilot_ui_state;
}

void FrogPilotUIState::update() {
  update_state(this);
}
