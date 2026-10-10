#include "frogpilot/ui/frogpilot_ui.h"

#include "selfdrive/ui/ui.h"

static void update_state(FrogPilotUIState *fs) {
  FrogPilotUIScene &frogpilot_scene = fs->frogpilot_scene;

  SubMaster &fpsm = *(fs->sm);
  fpsm.update(0);

  SubMaster &sm = *(uiState()->sm);

  if (sm.updated("carState")) {
    const cereal::CarState::Reader &carState = sm["carState"].getCarState();
    frogpilot_scene.parked = carState.getGearShifter() == cereal::CarState::GearShifter::PARK;
    frogpilot_scene.reverse = carState.getGearShifter() == cereal::CarState::GearShifter::REVERSE;
    frogpilot_scene.standstill = carState.getStandstill() && !frogpilot_scene.reverse;
  }
  if (sm.updated("deviceState")) {
    const cereal::DeviceState::Reader &deviceState = sm["deviceState"].getDeviceState();
    frogpilot_scene.online = deviceState.getNetworkType() != cereal::DeviceState::NetworkType::NONE;
  }
  if (fpsm.updated("frogpilotCarState")) {
    const cereal::FrogPilotCarState::Reader &frogpilotCarState = fpsm["frogpilotCarState"].getFrogpilotCarState();
    frogpilot_scene.traffic_mode_enabled = frogpilotCarState.getTrafficModeEnabled();
  }
  if (fpsm.updated("frogpilotPlan")) {
    const cereal::FrogPilotPlan::Reader &frogpilotPlan = fpsm["frogpilotPlan"].getFrogpilotPlan();
    capnp::Text::Reader toggles = frogpilotPlan.getFrogpilotToggles();
    QByteArray current_toggles = QByteArray::fromRawData(toggles.cStr(), toggles.size());
    static QByteArray previous_toggles;
    if (previous_toggles != current_toggles) {
      frogpilot_scene.frogpilot_toggles = QJsonDocument::fromJson(current_toggles).object();
      previous_toggles = QByteArray(toggles.cStr(), toggles.size());
      emit fs->togglesUpdated();
    }
    static uint64_t previous_theme_update_count = 0;
    if (previous_theme_update_count != frogpilotPlan.getThemeUpdateCount()) {
      previous_theme_update_count = frogpilotPlan.getThemeUpdateCount();
      emit fs->themeUpdated();
    }
  }
  if (fpsm.updated("frogpilotProcessState")) {
    const cereal::FrogPilotProcessState::Reader &frogpilotProcessState = fpsm["frogpilotProcessState"].getFrogpilotProcessState();
    static uint64_t previous_stats_saved_count = 0;
    if (previous_stats_saved_count != frogpilotProcessState.getStatsSavedCount()) {
      previous_stats_saved_count = frogpilotProcessState.getStatsSavedCount();
      emit fs->statsSaved();
    }
  }
}

FrogPilotUIState::FrogPilotUIState(QObject *parent) : QObject(parent) {
  pm = std::make_unique<PubMaster>(std::vector<const char*>{"frogpilotUIEvent", "frogpilotUIRequest"});
  sm = std::make_unique<SubMaster>(std::vector<const char*>{
    "carControl", "frogpilotCarState", "frogpilotDeviceState",
    "frogpilotPlan", "frogpilotProcessState", "frogpilotRadarState", "frogpilotSelfdriveState", "frogpilotSignReading", "liveDelay",
    "liveParameters", "liveTorqueParameters", "liveTracks", "mapdExtendedOut", "mapdOut"
  });

  wifi = new WifiManager(this);

  tethering_mode = Params().getInt("TetheringEnabled");
  if (tethering_mode == 1) {
    wifi->setTetheringEnabled(true);
  } else if (tethering_mode == 0) {
    wifi->setTetheringEnabled(false);
  }

  QObject::connect(uiState(), &UIState::offroadTransition, this, [this](bool offroad) {
    frogpilot_scene.wake_up_screen = false;

    if (Params().getInt("TetheringEnabled") == 2) {
      wifi->setTetheringEnabled(!offroad);
    }
  });
  QObject::connect(this, &FrogPilotUIState::togglesUpdated, this, [this]() {
    int mode = Params().getInt("TetheringEnabled");
    if (mode != tethering_mode) {
      setTethering(mode);
    }
  });
}

FrogPilotUIState *frogpilotUIState() {
  static FrogPilotUIState frogpilot_ui_state;
  return &frogpilot_ui_state;
}

void FrogPilotUIState::cancelMapsDownload() {
  download_maps_request_time = 0;

  MessageBuilder msg;
  msg.initEvent().initFrogpilotUIRequest().setCancelMapsDownload();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::cancelModelDownload() {
  MessageBuilder msg;
  msg.initEvent().initFrogpilotUIRequest().setCancelModelDownload();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::cancelThemeDownload() {
  MessageBuilder msg;
  msg.initEvent().initFrogpilotUIRequest().setCancelThemeDownload();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::downloadAllModels() {
  MessageBuilder msg;
  cereal::Event::Builder event = msg.initEvent();
  event.initFrogpilotUIRequest().setDownloadAllModels();
  download_model_request_time = event.getLogMonoTime();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::downloadMaps() {
  MessageBuilder msg;
  cereal::Event::Builder event = msg.initEvent();
  event.initFrogpilotUIRequest().setDownloadMaps();
  download_maps_request_time = event.getLogMonoTime();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::downloadModels(const QStringList &models) {
  MessageBuilder msg;
  cereal::Event::Builder event = msg.initEvent();
  capnp::List<capnp::Text>::Builder modelDownloads = event.initFrogpilotUIRequest().initDownloadModels(models.size());
  for (int i = 0; i < models.size(); ++i) {
    modelDownloads.set(i, models[i].toStdString());
  }
  download_model_request_time = event.getLogMonoTime();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::downloadTheme(const QString &component, const QStringList &themes) {
  MessageBuilder msg;
  cereal::Event::Builder event = msg.initEvent();
  capnp::List<cereal::FrogPilotUIRequest::ThemeDownload>::Builder themeDownloads = event.initFrogpilotUIRequest().initDownloadTheme(themes.size());
  for (int i = 0; i < themes.size(); ++i) {
    themeDownloads[i].setComponent(component.toStdString());
    themeDownloads[i].setTheme(themes[i].toStdString());
  }
  download_theme_request_time = event.getLogMonoTime();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::experimentalModePressed() {
  MessageBuilder msg;
  msg.initEvent().initFrogpilotUIEvent().setExperimentalModePressed();
  pm->send("frogpilotUIEvent", msg);
}

void FrogPilotUIState::flashPanda() {
  MessageBuilder msg;
  cereal::Event::Builder event = msg.initEvent();
  event.initFrogpilotUIRequest().setFlashPanda();
  flash_panda_request_time = event.getLogMonoTime();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::reportIssue(const QString &report) {
  MessageBuilder msg;
  cereal::Event::Builder event = msg.initEvent();
  event.initFrogpilotUIRequest().setIssueReport(report.toStdString());
  issue_report_request_time = event.getLogMonoTime();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::runUpdateChecks() {
  MessageBuilder msg;
  msg.initEvent().initFrogpilotUIRequest().setUpdateChecks();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::screenRecorderEvent(cereal::FrogPilotOnroadEvent::EventName event) {
  MessageBuilder msg;
  msg.initEvent().initFrogpilotUIEvent().setScreenRecorderEvent(event);
  pm->send("frogpilotUIEvent", msg);
}

void FrogPilotUIState::setDistanceButtonPressed(bool pressed) {
  if (pressed != distance_button_pressed) {
    distance_button_pressed = pressed;

    MessageBuilder msg;
    msg.initEvent().initFrogpilotUIEvent().setDistanceButtonPressed(pressed);
    pm->send("frogpilotUIEvent", msg);
  }
}

void FrogPilotUIState::setTethering(int mode) {
  tethering_mode = mode;
  wifi->setTetheringEnabled(mode == 1 || (mode == 2 && uiState()->scene.started) || mode == 3);
}

void FrogPilotUIState::speedLimitAccepted() {
  MessageBuilder msg;
  msg.initEvent().initFrogpilotUIEvent().setSpeedLimitAccepted();
  pm->send("frogpilotUIEvent", msg);
}

void FrogPilotUIState::testAlert(const QString &alert) {
  MessageBuilder msg;
  msg.initEvent().initFrogpilotUIRequest().setTestAlert(alert.toStdString());
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::update() {
  update_state(this);

  if (!uiState()->scene.started) {
    frogpilot_scene.parked = false;
    frogpilot_scene.reverse = false;
    frogpilot_scene.standstill = false;
  }

  const bool enabled = (*uiState()->sm)["selfdriveState"].getSelfdriveState().getEnabled();
  frogpilot_scene.always_on_lateral_active = !enabled && (*sm)["frogpilotCarState"].getFrogpilotCarState().getAlwaysOnLateralEnabled();
  frogpilot_scene.conditional_status = enabled ? (*sm)["frogpilotPlan"].getFrogpilotPlan().getCeStatus() : 0;
  frogpilot_scene.driver_camera_timer = frogpilot_scene.reverse && frogpilot_scene.frogpilot_toggles.value(QLatin1String("driver_camera_in_reverse")).toBool() ? frogpilot_scene.driver_camera_timer + 1 : 0;
}

void FrogPilotUIState::updateToggles() {
  MessageBuilder msg;
  msg.initEvent().initFrogpilotUIRequest().setUpdateToggles();
  pm->send("frogpilotUIRequest", msg);
}
