#include "frogpilot/ui/frogpilot_ui.h"

#include "selfdrive/ui/ui.h"

static void update_state(FrogPilotUIState *fs) {
  FrogPilotUIScene &frogpilot_scene = fs->frogpilot_scene;

  SubMaster &fpsm = *(fs->sm);
  fpsm.update(0);

  SubMaster &sm = *(uiState()->sm);

  if (sm.updated("carState")) {
    const cereal::CarState::Reader &carState = sm["carState"].getCarState();
    bool manual_transmission = sm["carParams"].getCarParams().getTransmissionType() == cereal::CarParams::TransmissionType::MANUAL;

    frogpilot_scene.parked = carState.getGearShifter() == cereal::CarState::GearShifter::PARK || (manual_transmission && carState.getStandstill());
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
  if (fpsm.updated("frogpilotCarState")) {
    const cereal::FrogPilotCarState::Reader &frogpilotCarState = fpsm["frogpilotCarState"].getFrogpilotCarState();
    frogpilot_scene.always_on_lateral_active = !frogpilot_scene.enabled && frogpilotCarState.getAlwaysOnLateralEnabled();
    frogpilot_scene.traffic_mode_enabled = frogpilotCarState.getTrafficModeEnabled();
  }
  if (fpsm.updated("frogpilotPlan")) {
    const cereal::FrogPilotPlan::Reader &frogpilotPlan = fpsm["frogpilotPlan"].getFrogpilotPlan();
    frogpilot_scene.lane_width_left = frogpilotPlan.getLaneWidthLeft();
    frogpilot_scene.lane_width_right = frogpilotPlan.getLaneWidthRight();
    static uint64_t previous_toggles_update_time = 0;
    if (previous_toggles_update_time != frogpilotPlan.getTogglesUpdateTime()) {
      previous_toggles_update_time = frogpilotPlan.getTogglesUpdateTime();
      frogpilot_scene.frogpilot_toggles = QJsonDocument::fromJson(QByteArray::fromStdString(fs->params_memory.get("FrogPilotToggles"))).object();
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
  pm = std::make_unique<PubMaster, const std::initializer_list<const char *>>({"frogpilotUIEvent", "frogpilotUIRequest"});
  sm = std::make_unique<SubMaster, const std::initializer_list<const char *>>({
    "carControl", "frogpilotAccountState", "frogpilotCarState", "frogpilotControlsState",
    "frogpilotDeviceState", "frogpilotNavigation", "frogpilotPlan", "frogpilotProcessState", "frogpilotRadarState", "frogpilotSignReading", "liveDelay",
    "liveParameters", "liveTorqueParameters", "liveTracks"
  });

  wifi = new WifiManager(this);

  frogpilot_scene.frogpilot_toggles = QJsonDocument::fromJson(QByteArray::fromStdString(params_memory.get("FrogPilotToggles", true))).object();

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

void FrogPilotUIState::downloadModels(const QStringList &models) {
  MessageBuilder msg;
  cereal::Event::Builder event = msg.initEvent();
  capnp::List<capnp::Text>::Builder modelIds = event.initFrogpilotUIRequest().initDownloadModels(models.size());
  for (int i = 0; i < models.size(); ++i) {
    modelIds.set(i, models[i].toStdString());
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

void FrogPilotUIState::requestAccountLink() {
  MessageBuilder msg;
  cereal::Event::Builder event = msg.initEvent();
  event.initFrogpilotUIRequest().setAccountLink();
  account_link_request_time = event.getLogMonoTime();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::requestAccountUrl(const QString &purpose) {
  MessageBuilder msg;
  cereal::Event::Builder event = msg.initEvent();
  event.initFrogpilotUIRequest().setAccountUrl(purpose.toStdString());
  account_url_request_time = event.getLogMonoTime();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::runUpdateChecks() {
  MessageBuilder msg;
  msg.initEvent().initFrogpilotUIRequest().setUpdateChecks();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::screenRecorderEvent(cereal::FrogPilotCarEvent::EventName event) {
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

void FrogPilotUIState::testAlert(const QString &alert, float volume) {
  MessageBuilder msg;
  cereal::FrogPilotUIRequest::Builder request = msg.initEvent().initFrogpilotUIRequest();
  request.setTestAlert(alert.toStdString());
  request.setTestAlertVolume(volume);
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::unlinkAccount() {
  MessageBuilder msg;
  cereal::Event::Builder event = msg.initEvent();
  event.initFrogpilotUIRequest().setUnlinkAccount();
  account_link_request_time = event.getLogMonoTime();
  pm->send("frogpilotUIRequest", msg);
}

void FrogPilotUIState::update() {
  update_state(this);

  if (!uiState()->scene.started) {
    frogpilot_scene.reverse = false;
    frogpilot_scene.standstill = false;
  }

  frogpilot_scene.conditional_status = frogpilot_scene.enabled ? (*sm)["frogpilotPlan"].getFrogpilotPlan().getCeStatus() : 0;
  driver_camera_timer = frogpilot_scene.reverse && frogpilot_toggles.value(QLatin1String("driver_camera_in_reverse")).toBool() ? driver_camera_timer + 1 : 0;
  frogpilot_scene.driver_camera_active = driver_camera_timer >= UI_FREQ / 2;
}

void FrogPilotUIState::updateToggles() {
  MessageBuilder msg;
  msg.initEvent().initFrogpilotUIRequest().setUpdateToggles();
  pm->send("frogpilotUIRequest", msg);
}

bool FrogPilotConfirmationDialog::toggleReboot(QWidget *parent) {
  if (isOpenpilotSteering()) {
    ConfirmationDialog::alert(tr("Disengage to Reboot"), parent);
    return false;
  }

  ConfirmationDialog d(tr("Reboot required to take effect."), tr("Reboot Now"), tr("Reboot Later"), false, parent);
  return d.exec() && !isOpenpilotSteering();
}

bool isOpenpilotSteering() {
  return uiState()->engaged() || (uiState()->scene.started && frogpilotUIState()->frogpilot_scene.always_on_lateral_active);
}

QString formatShortTime(const QTime &time) {
  QLocale uiLocale(uiState()->language.mid(5));
  if (uiLocale.language() == QLocale::C) {
    uiLocale = QLocale(QLocale::English, QLocale::UnitedStates);
  }
  return uiLocale.toString(time, QLocale::ShortFormat);
}
