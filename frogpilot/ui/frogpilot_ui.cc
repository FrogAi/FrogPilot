#include "frogpilot/ui/frogpilot_ui.h"

static void update_state(FrogPilotUIState *fs) {
  FrogPilotUIScene &frogpilot_scene = fs->frogpilot_scene;

  SubMaster &sm = *(fs->sm);
  sm.update(0);

  if (sm.updated("carState")) {
    const cereal::CarState::Reader &carState = sm["carState"].getCarState();
    frogpilot_scene.parked = carState.getGearShifter() == cereal::CarState::GearShifter::PARK;
    frogpilot_scene.reverse = carState.getGearShifter() == cereal::CarState::GearShifter::REVERSE;
    frogpilot_scene.standstill = carState.getStandstill() && !frogpilot_scene.reverse;
  }
  if (sm.updated("controlsState")) {
    const cereal::ControlsState::Reader &controlsState = sm["controlsState"].getControlsState();
    frogpilot_scene.enabled = controlsState.getEnabled();
  }
  if (sm.updated("deviceState")) {
    const cereal::DeviceState::Reader &deviceState = sm["deviceState"].getDeviceState();
    frogpilot_scene.online = deviceState.getNetworkType() != cereal::DeviceState::NetworkType::NONE;
  }
  if (sm.updated("frogpilotCarState")) {
    const cereal::FrogPilotCarState::Reader &frogpilotCarState = sm["frogpilotCarState"].getFrogpilotCarState();
    frogpilot_scene.always_on_lateral_active = !frogpilot_scene.enabled && frogpilotCarState.getAlwaysOnLateralEnabled();
    frogpilot_scene.traffic_mode_enabled = frogpilotCarState.getTrafficModeEnabled();
  }
  if (sm.updated("frogpilotPlan")) {
    const cereal::FrogPilotPlan::Reader &frogpilotPlan = sm["frogpilotPlan"].getFrogpilotPlan();
    frogpilot_scene.lane_width_left = frogpilotPlan.getLaneWidthLeft();
    frogpilot_scene.lane_width_right = frogpilotPlan.getLaneWidthRight();
    if (frogpilotPlan.getThemeUpdated()) {
      emit fs->themeUpdated();
    }
    if (frogpilotPlan.getTogglesUpdated()) {
      frogpilot_scene.frogpilot_toggles = QJsonDocument::fromJson(fs->params_memory.get("FrogPilotToggles").c_str()).object();
    }
  }
}

void update_theme(FrogPilotUIState *fs) {
  FrogPilotUIScene &frogpilot_scene = fs->frogpilot_scene;

  frogpilot_scene.use_stock_colors = frogpilot_scene.frogpilot_toggles.value("color_scheme").toString() == "stock";

  if (!frogpilot_scene.use_stock_colors) {
    frogpilot_scene.use_stock_colors |= !loadThemeColors("", true).isValid();

    frogpilot_scene.lane_lines_color = loadThemeColors("LaneLines");
    frogpilot_scene.lead_marker_color = loadThemeColors("LeadMarker");
    frogpilot_scene.path_color = loadThemeColors("Path");
    frogpilot_scene.path_edges_color = loadThemeColors("PathEdge");
    frogpilot_scene.sidebar_color1 = loadThemeColors("Sidebar1");
    frogpilot_scene.sidebar_color2 = loadThemeColors("Sidebar2");
    frogpilot_scene.sidebar_color3 = loadThemeColors("Sidebar3");
  }
}

FrogPilotUIState::FrogPilotUIState(QObject *parent) : QObject(parent) {
  sm = std::make_unique<SubMaster, const std::initializer_list<const char *>>({
    "carControl", "carState", "controlsState", "deviceState", "frogpilotCarState", "frogpilotDeviceState",
    "frogpilotNavigation", "frogpilotPlan", "liveDelay", "liveParameters", "liveTorqueParameters", "liveTracks",
    "navInstruction"
  });

  wifi = new WifiManager(this);

  frogpilot_scene.frogpilot_toggles = QJsonDocument::fromJson(QString::fromStdString(params_memory.get("FrogPilotToggles", true)).toUtf8()).object();
}

FrogPilotUIState *frogpilotUIState() {
  static FrogPilotUIState frogpilot_ui_state;
  return &frogpilot_ui_state;
}

void FrogPilotUIState::update() {
  update_state(this);

  frogpilot_scene.conditional_status = frogpilot_scene.enabled ? params_memory.getInt("CEStatus") : 0;
  frogpilot_scene.driver_camera_timer = frogpilot_scene.reverse && frogpilot_toggles.value("driver_camera_in_reverse").toBool() ? frogpilot_scene.driver_camera_timer + 1 : 0;
}
