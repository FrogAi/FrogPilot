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
  if (sm.updated("frogpilotCarState")) {
    const cereal::FrogPilotCarState::Reader &frogpilotCarState = sm["frogpilotCarState"].getFrogpilotCarState();
  }
  if (sm.updated("frogpilotPlan")) {
    const cereal::FrogPilotPlan::Reader &frogpilotPlan = sm["frogpilotPlan"].getFrogpilotPlan();
    if (frogpilotPlan.getTogglesUpdated()) {
      frogpilot_scene.frogpilot_toggles = QJsonDocument::fromJson(fs->params_memory.get("FrogPilotToggles").c_str()).object();
    }
  }
  if (sm.updated("selfdriveState")) {
    const cereal::SelfdriveState::Reader &selfdriveState = sm["selfdriveState"].getSelfdriveState();
  }
}

FrogPilotUIState::FrogPilotUIState(QObject *parent) : QObject(parent) {
  sm = std::make_unique<SubMaster, const std::initializer_list<const char *>>({
    "carControl", "carState", "controlsState", "deviceState", "frogpilotCarState", "frogpilotSelfdriveState",
    "frogpilotDeviceState", "frogpilotPlan", "frogpilotRadarState", "liveDelay",
    "liveParameters", "liveTorqueParameters", "liveTracks", "navInstruction", "selfdriveState"
  });

  frogpilot_scene.frogpilot_toggles = QJsonDocument::fromJson(QString::fromStdString(params_memory.get("FrogPilotToggles", true)).toUtf8()).object();
}

FrogPilotUIState *frogpilotUIState() {
  static FrogPilotUIState frogpilot_ui_state;
  return &frogpilot_ui_state;
}

void FrogPilotUIState::update() {
  update_state(this);
}
