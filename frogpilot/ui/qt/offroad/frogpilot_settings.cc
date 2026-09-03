#include "frogpilot/ui/qt/offroad/data_settings.h"
#include "frogpilot/ui/qt/offroad/device_settings.h"
#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"
#include "frogpilot/ui/qt/offroad/lateral_settings.h"
#include "frogpilot/ui/qt/offroad/longitudinal_settings.h"
#include "frogpilot/ui/qt/offroad/maps_settings.h"
#include "frogpilot/ui/qt/offroad/model_settings.h"
#include "frogpilot/ui/qt/offroad/navigation_settings.h"
#include "frogpilot/ui/qt/offroad/sounds_settings.h"
#include "frogpilot/ui/qt/offroad/theme_settings.h"
#include "frogpilot/ui/qt/offroad/utilities.h"
#include "frogpilot/ui/qt/offroad/vehicle_settings.h"
#include "frogpilot/ui/qt/offroad/visual_settings.h"
#include "frogpilot/ui/qt/offroad/wheel_settings.h"

void FrogPilotSettingsWindow::createPanelButtons(FrogPilotListWidget *list) {
  FrogPilotDataPanel *frogpilotDataPanel = new FrogPilotDataPanel(this, !shownDescriptions.value("FrogPilotDataPanel").toBool(false));
  FrogPilotDevicePanel *frogpilotDevicePanel = new FrogPilotDevicePanel(this, !shownDescriptions.value("FrogPilotDevicePanel").toBool(false));
  FrogPilotLateralPanel *frogpilotLateralPanel = new FrogPilotLateralPanel(this, !shownDescriptions.value("FrogPilotLateralPanel").toBool(false));
  FrogPilotLongitudinalPanel *frogpilotLongitudinalPanel = new FrogPilotLongitudinalPanel(this, !shownDescriptions.value("FrogPilotLongitudinalPanel").toBool(false));
  FrogPilotMapsPanel *frogpilotMapsPanel = new FrogPilotMapsPanel(this, !shownDescriptions.value("FrogPilotMapsPanel").toBool(false));
  FrogPilotModelPanel *frogpilotModelPanel = new FrogPilotModelPanel(this, !shownDescriptions.value("FrogPilotModelPanel").toBool(false));
  FrogPilotNavigationPanel *frogpilotNavigationPanel = new FrogPilotNavigationPanel(this, !shownDescriptions.value("FrogPilotNavigationPanel").toBool(false));
  FrogPilotSoundsPanel *frogpilotSoundsPanel = new FrogPilotSoundsPanel(this, !shownDescriptions.value("FrogPilotSoundsPanel").toBool(false));
  FrogPilotThemesPanel *frogpilotThemesPanel = new FrogPilotThemesPanel(this, !shownDescriptions.value("FrogPilotThemesPanel").toBool(false));
  FrogPilotUtilitiesPanel *frogpilotUtilitiesPanel = new FrogPilotUtilitiesPanel(this, !shownDescriptions.value("FrogPilotUtilitiesPanel").toBool(false));
  FrogPilotVehiclesPanel *frogpilotVehiclesPanel = new FrogPilotVehiclesPanel(this, !shownDescriptions.value("FrogPilotVehiclesPanel").toBool(false));
  FrogPilotVisualsPanel *frogpilotVisualsPanel = new FrogPilotVisualsPanel(this, !shownDescriptions.value("FrogPilotVisualsPanel").toBool(false));
  FrogPilotWheelPanel *frogpilotWheelPanel = new FrogPilotWheelPanel(this, !shownDescriptions.value("FrogPilotWheelPanel").toBool(false));

  std::vector<std::vector<std::tuple<QString, QWidget*>>> panelButtons = {
    {{tr("MANAGE"), frogpilotSoundsPanel}},
    {{tr("DRIVING MODEL"), frogpilotModelPanel}, {tr("GAS / BRAKE"), frogpilotLongitudinalPanel}, {tr("STEERING"), frogpilotLateralPanel}},
    {{tr("MAP DATA"), frogpilotMapsPanel}, {tr("NAVIGATION"), frogpilotNavigationPanel}},
    {{tr("DATA"), frogpilotDataPanel}, {tr("DEVICE / SCREEN"), frogpilotDevicePanel}, {tr("UTILITIES"), frogpilotUtilitiesPanel}},
    {{tr("APPEARANCE"), frogpilotVisualsPanel}, {tr("THEME"), frogpilotThemesPanel}},
    {{tr("VEHICLE SETTINGS"), frogpilotVehiclesPanel}, {tr("WHEEL BUTTONS"), frogpilotWheelPanel}}
  };

  std::vector<std::tuple<QString, QString, QString>> panelInfo = {
    {tr("Alerts and Sounds"), tr("<b>Set the volume for each of openpilot's alerts, and add extra alerts stock openpilot doesn't have.</b> Extra alerts include a chime when the light turns green or when the car ahead starts moving."), "../../frogpilot/assets/toggle_icons/icon_sound.png"},
    {tr("Driving Controls"), tr("<b>Adjust how openpilot accelerates, brakes, steers, and changes lanes, and switch between driving models.</b><br><br>\"DRIVING MODEL\" only appears once your \"Tuning Level\" is \"Standard\" or higher, and \"GAS / BRAKE\" only appears on cars where openpilot handles the gas and brake."), "../../frogpilot/assets/toggle_icons/icon_steering.png"},
    {tr("Maps and Navigation"), tr("<b>Download the speed limit data openpilot uses, and set up turn-by-turn navigation.</b> Speed limits come from offline map data for the states or countries you pick, so they work without cell signal."), "../../frogpilot/assets/toggle_icons/icon_navigate.png"},
    {tr("System Settings"), tr("<b>Manage your saved data, how the device and screen behave, and tools for fixing problems.</b> This is also where your drive stats and backups of your settings live."), "../../frogpilot/assets/toggle_icons/icon_system.png"},
    {tr("Theme and Appearance"), tr("<b>Change what appears on the driving screen, and how openpilot looks and sounds.</b> Anything from hiding on-screen icons to full theme packs with new colors, sounds, and turn signal animations."), "../../frogpilot/assets/toggle_icons/icon_display.png"},
    {tr("Vehicle Settings"), tr("<b>Tell openpilot what car you drive, turn on features made for your brand, and change what your steering wheel buttons do.</b><br><br>Brand features include things like smoother stop-and-go and automatic door locks. \"WHEEL BUTTONS\" only appears once your \"Tuning Level\" is \"Advanced\" or higher."), "../../frogpilot/assets/toggle_icons/icon_vehicle.png"}
  };

  FrogPilotButtonsControl **panelMembers[] = {&soundPanelButtons, &drivingPanelButtons, &navigationPanelButtons, &systemPanelButtons, &themePanelButtons, &vehiclePanelButtons};

  for (size_t i = 0; i < panelInfo.size(); ++i) {
    const QString &title = std::get<0>(panelInfo[i]);
    const QString &description = std::get<1>(panelInfo[i]);
    const QString &icon = std::get<2>(panelInfo[i]);

    const std::vector<std::tuple<QString, QWidget*>> &widgetLabels = panelButtons[i];

    std::vector<QString> labels;
    std::vector<QWidget*> widgets;

    for (size_t j = 0; j < widgetLabels.size(); ++j) {
      labels.push_back(std::get<0>(widgetLabels[j]));

      QWidget *panel = std::get<1>(widgetLabels[j]);
      panel->setContentsMargins(50, 25, 50, 25);

      ScrollView *panelFrame = new ScrollView(panel, this);
      mainLayout->addWidget(panelFrame);
      widgets.push_back(panelFrame);
    }

    FrogPilotButtonsControl *panelButton = new FrogPilotButtonsControl(title, description, icon, labels);
    *panelMembers[i] = panelButton;

    if (forceOpenDescriptions) {
      panelButton->showDescription();
    }

    QObject::connect(panelButton, &FrogPilotButtonsControl::buttonClicked, [widgets, this](int id) {
      mainLayout->setCurrentWidget(widgets[id]);

      panelOpen = true;

      openPanel();

      ScrollView *panelFrame = qobject_cast<ScrollView*>(widgets[id]);
      if (panelFrame) {
        QWidget *panel = panelFrame->widget();
        QString className = panel->metaObject()->className();

        if (!shownDescriptions.value(className).toBool(false)) {
          shownDescriptions.insert(className, true);
          params.putNonBlocking("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
        }
      }
    });

    list->addItem(panelButton);
  }

  QObject::connect(frogpilotDataPanel, &FrogPilotDataPanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotDevicePanel, &FrogPilotDevicePanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotLateralPanel, &FrogPilotLateralPanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotLongitudinalPanel, &FrogPilotLongitudinalPanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotLongitudinalPanel, &FrogPilotLongitudinalPanel::openSubSubPanel, this, &FrogPilotSettingsWindow::openSubSubPanel);
  QObject::connect(frogpilotLongitudinalPanel, &FrogPilotLongitudinalPanel::openSubSubSubPanel, this, &FrogPilotSettingsWindow::openSubSubSubPanel);
  QObject::connect(frogpilotMapsPanel, &FrogPilotMapsPanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotModelPanel, &FrogPilotModelPanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotNavigationPanel, &FrogPilotNavigationPanel::closeSubPanel, this, &FrogPilotSettingsWindow::closeSubPanel);
  QObject::connect(frogpilotNavigationPanel, &FrogPilotNavigationPanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotSoundsPanel, &FrogPilotSoundsPanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotThemesPanel, &FrogPilotThemesPanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotVehiclesPanel, &FrogPilotVehiclesPanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotVisualsPanel, &FrogPilotVisualsPanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotVisualsPanel, &FrogPilotVisualsPanel::openSubSubPanel, this, &FrogPilotSettingsWindow::openSubSubPanel);
}

FrogPilotSettingsWindow::FrogPilotSettingsWindow(SettingsWindow *parent) : QFrame(parent) {
  shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();

  QString className = this->metaObject()->className();
  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
  }

  mainLayout = new QStackedLayout(this);

  QWidget *frogpilotWidget = new QWidget(this);
  QVBoxLayout *frogpilotLayout = new QVBoxLayout(frogpilotWidget);
  frogpilotLayout->setContentsMargins(50, 25, 50, 25);

  frogpilotPanel = new ScrollView(frogpilotWidget, this);
  mainLayout->addWidget(frogpilotPanel);

  FrogPilotListWidget *list = new FrogPilotListWidget(this);
  frogpilotLayout->addWidget(list);

  std::vector<QString> togglePresets{tr("Minimal"), tr("Standard"), tr("Advanced"), tr("Developer")};
  togglePreset = new FrogPilotButtonsControl(tr("Tuning Level"),
                                             tr("<b>Choose how much control you want over FrogPilot's settings.</b> Anything above your level is hidden and uses FrogPilot's recommended setting instead. Nothing you've set is lost, and it comes back when you move up.<br><br>"
                                                "Minimal - FrogPilot decides nearly everything for you<br>"
                                                "Standard - Recommended for most drivers<br>"
                                                "Advanced - Extra fine-tuning once you know how your car drives<br>"
                                                "Developer - Everything, including settings that can drastically change how openpilot drives"),
                                              "../../frogpilot/assets/toggle_icons/icon_tuning.png", togglePresets, true);
  QObject::connect(togglePreset, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 3 && !ConfirmationDialog::confirm(tr("\"Developer\" unlocks settings that can drastically change how openpilot drives, and any you changed before will start being used again.\n\nOnly continue if you know what they do."), tr("Continue"), this)) {
      togglePreset->setCheckedButton(params.getInt("TuningLevel"));
      return;
    }

    tuningLevel = id;

    params.putIntNonBlocking("TuningLevel", tuningLevel);

    updateVariables();

    emit tuningLevelChanged(tuningLevel);
  });
  togglePreset->setCheckedButton(params.getInt("TuningLevel"));
  if (forceOpenDescriptions) {
    togglePreset->showDescription();
  }
  list->addItem(togglePreset, true);

  createPanelButtons(list);

  QObject::connect(parent, &SettingsWindow::closePanel, this, &FrogPilotSettingsWindow::closePanel);
  QObject::connect(parent, &SettingsWindow::closeSubPanel, this, &FrogPilotSettingsWindow::closeSubPanel);
  QObject::connect(parent, &SettingsWindow::closeSubSubPanel, this, &FrogPilotSettingsWindow::closeSubSubPanel);
  QObject::connect(parent, &SettingsWindow::closeSubSubSubPanel, this, &FrogPilotSettingsWindow::closeSubSubSubPanel);
  QObject::connect(parent, &SettingsWindow::updateMetric, this, &FrogPilotSettingsWindow::updateMetric);
  QObject::connect(parent, &SettingsWindow::updateTuningLevel, this, &FrogPilotSettingsWindow::updateTuningLevel);
  carParamsWatcher = new ParamWatcher(this);
  QObject::connect(carParamsWatcher, &ParamWatcher::paramChanged, this, [this]() {
    updateVariables();
  });

  QObject::connect(uiState(), &UIState::offroadTransition, this, &FrogPilotSettingsWindow::updateVariables);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotSettingsWindow::updateState);

  std::vector<std::string> keys = params.allKeys();
  for (std::vector<std::string>::const_iterator it = keys.begin(); it != keys.end(); ++it) {
    const std::string &key = *it;
    frogpilotToggleLevels[QString::fromStdString(key)] = params.getTuningLevel(key);
  }
  tuningLevel = params.getInt("TuningLevel");

  closeSubPanel();
  updateMetric(params.getBool("IsMetric"), true);
  updateVariables();
}

void FrogPilotSettingsWindow::updateTuningLevel() {
  tuningLevel = params.getInt("TuningLevel");
  togglePreset->setCheckedButton(params.getInt("TuningLevel"));

  updateVariables();

  emit tuningLevelChanged(tuningLevel);
}

void FrogPilotSettingsWindow::showEvent(QShowEvent *event) {
  updateTuningLevel();

  static bool alertShown = false;

  QString className = this->metaObject()->className();
  if (!shownDescriptions.value(className).toBool(false)) {
    shownDescriptions.insert(className, true);
    params.putNonBlocking("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

  if (forceOpenDescriptions) {
    togglePreset->showDescription();

    drivingPanelButtons->showDescription();
    navigationPanelButtons->showDescription();
    soundPanelButtons->showDescription();
    systemPanelButtons->showDescription();
    themePanelButtons->showDescription();
    vehiclePanelButtons->showDescription();

    if (!alertShown) {
      ConfirmationDialog::alert(tr("All descriptions are currently expanded. You can tap any setting's name to open or close its description at any time!"), this);
      alertShown = true;
    }
  }
}

void FrogPilotSettingsWindow::hideEvent(QHideEvent *event) {
  closePanel();
}

void FrogPilotSettingsWindow::closePanel() {
  if (forceOpenDescriptions) {
    togglePreset->showDescription();

    drivingPanelButtons->showDescription();
    navigationPanelButtons->showDescription();
    soundPanelButtons->showDescription();
    systemPanelButtons->showDescription();
    themePanelButtons->showDescription();
    vehiclePanelButtons->showDescription();
  }

  mainLayout->setCurrentWidget(frogpilotPanel);

  keepScreenOn = false;
  panelOpen = false;
}

void FrogPilotSettingsWindow::updateState() {
  FrogPilotUIState &fs = *frogpilotUIState();
  FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;

  frogpilot_scene.frogpilot_panel_active = panelOpen && keepScreenOn;
}

void FrogPilotSettingsWindow::updateVariables() {
  FrogPilotUIState &fs = *frogpilotUIState();
  FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;
  QJsonObject &frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  isFrogsGoMoo = ::isFrogsGoMoo();

  bool migratedStockValues = false;

  // Re-add every pass since the watch is dropped when the param is rewritten
  carParamsWatcher->addParam("CarParamsPersistent");

  std::string carParams = params.get("CarParamsPersistent");
  if (!carParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(carParams.data(), carParams.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();
    capnp::List<cereal::CarParams::SafetyConfig>::Reader safetyConfigs = CP.getSafetyConfigs();
    cereal::CarParams::SafetyModel lastSafetyModel = safetyConfigs[safetyConfigs.size() - 1].getSafetyModel();

    carDetected = true;

    carFingerprint = CP.getCarFingerprint();
    carMake = CP.getBrand();

    friction = CP.getLateralTuning().getTorque().getFriction();
    hasAlphaLongitudinal = CP.getAlphaLongitudinalAvailable();
    hasBSM = CP.getEnableBsm();
    hasDashSpeedLimits = false;
    hasNNFFLog = frogpilot_toggles.value("has_nnff").toBool();
    hasOpenpilotLongitudinal = hasLongitudinalControl(CP);
    hasPCMCruise = CP.getPcmCruise();
    hasPedal = CP.getEnableGasInterceptorDEPRECATED();
    hasRadar = !CP.getRadarUnavailable();
    hasSDSU = frogpilot_toggles.value("has_sdsu").toBool();
    hasSNG = CP.getAutoResumeSng();
    hasZSS = frogpilot_toggles.value("has_zss").toBool();
    isAngleCar = CP.getSteerControlType() == cereal::CarParams::SteerControlType::ANGLE;
    isGM = carMake == "gm";
    isHKG = carMake == "hyundai";
    isHKGCanFd = isHKG && lastSafetyModel == cereal::CarParams::SafetyModel::HYUNDAI_CANFD;
    isSubaru = carMake == "subaru";
    isTorqueCar = CP.getLateralTuning().which() == cereal::CarParams::LateralTuning::TORQUE;
    isToyota = carMake == "toyota";
    isTSK = CP.getSecOcRequired();
    isVolt = carFingerprint == "CHEVROLET_VOLT";
    latAccelFactor = CP.getLateralTuning().getTorque().getLatAccelFactor();
    lkasAllowedForAOL = frogpilot_toggles.value("lkas_allowed_for_aol").toBool();
    longitudinalActuatorDelay = CP.getLongitudinalActuatorDelay();
    maxLateralAccel = CP.getMaxLateralAccel();
    startAccel = CP.getStartAccel();
    steerActuatorDelay = CP.getSteerActuatorDelay();
    steerKp = isTorqueCar ? 1.0f : 0.0f;
    steerRatio = CP.getSteerRatio();
    stopAccel = CP.getStopAccel();
    stoppingDecelRate = CP.getStoppingDecelRate();
    vEgoStarting = CP.getVEgoStarting();
    vEgoStopping = CP.getVEgoStopping();

    float currentDelayStock = params.getFloat("SteerDelayStock");
    float currentFrictionStock = params.getFloat("SteerFrictionStock");
    float currentKPStock = params.getFloat("SteerKPStock");
    float currentLatAccelStock = params.getFloat("SteerLatAccelStock");
    float currentLongDelayStock = params.getFloat("LongitudinalActuatorDelayStock");
    float currentStartAccelStock = params.getFloat("StartAccelStock");
    float currentSteerRatioStock = params.getFloat("SteerRatioStock");
    float currentStopAccelStock = params.getFloat("StopAccelStock");
    float currentStoppingDecelRateStock = params.getFloat("StoppingDecelRateStock");
    float currentVEgoStartingStock = params.getFloat("VEgoStartingStock");
    float currentVEgoStoppingStock = params.getFloat("VEgoStoppingStock");

    if (currentDelayStock != steerActuatorDelay && steerActuatorDelay != 0) {
      if (params.getFloat("SteerDelay") == currentDelayStock || currentDelayStock == 0) {
        params.putFloatNonBlocking("SteerDelay", steerActuatorDelay);
      }
      params.putFloatNonBlocking("SteerDelayStock", steerActuatorDelay);
      migratedStockValues = true;
    }

    if (currentFrictionStock != friction && friction != 0) {
      if (params.getFloat("SteerFriction") == currentFrictionStock || currentFrictionStock == 0) {
        params.putFloatNonBlocking("SteerFriction", friction);
      }
      params.putFloatNonBlocking("SteerFrictionStock", friction);
      migratedStockValues = true;
    }

    if (currentKPStock != steerKp && steerKp != 0) {
      if (params.getFloat("SteerKP") == currentKPStock || currentKPStock == 0) {
        params.putFloatNonBlocking("SteerKP", steerKp);
      }
      params.putFloatNonBlocking("SteerKPStock", steerKp);
      migratedStockValues = true;
    }

    if (currentLatAccelStock != latAccelFactor && latAccelFactor != 0) {
      if (params.getFloat("SteerLatAccel") == currentLatAccelStock || currentLatAccelStock == 0) {
        params.putFloatNonBlocking("SteerLatAccel", latAccelFactor);
      }
      params.putFloatNonBlocking("SteerLatAccelStock", latAccelFactor);
      migratedStockValues = true;
    }

    if (currentLongDelayStock != longitudinalActuatorDelay && longitudinalActuatorDelay != 0) {
      if (params.getFloat("LongitudinalActuatorDelay") == currentLongDelayStock || currentLongDelayStock == 0) {
        params.putFloatNonBlocking("LongitudinalActuatorDelay", longitudinalActuatorDelay);
      }
      params.putFloatNonBlocking("LongitudinalActuatorDelayStock", longitudinalActuatorDelay);
      migratedStockValues = true;
    }

    if (currentStartAccelStock != startAccel) {
      if (params.getFloat("StartAccel") == currentStartAccelStock || currentStartAccelStock == 0) {
        params.putFloatNonBlocking("StartAccel", startAccel);
      }
      params.putFloatNonBlocking("StartAccelStock", startAccel);
      migratedStockValues = true;
    }

    if (currentSteerRatioStock != steerRatio && steerRatio != 0) {
      if (params.getFloat("SteerRatio") == currentSteerRatioStock || currentSteerRatioStock == 0) {
        params.putFloatNonBlocking("SteerRatio", steerRatio);
      }
      params.putFloatNonBlocking("SteerRatioStock", steerRatio);
      migratedStockValues = true;
    }

    if (currentStopAccelStock != stopAccel && stopAccel != 0) {
      if (params.getFloat("StopAccel") == currentStopAccelStock || currentStopAccelStock == 0) {
        params.putFloatNonBlocking("StopAccel", stopAccel);
      }
      params.putFloatNonBlocking("StopAccelStock", stopAccel);
      migratedStockValues = true;
    }

    if (currentStoppingDecelRateStock != stoppingDecelRate && stoppingDecelRate != 0) {
      if (params.getFloat("StoppingDecelRate") == currentStoppingDecelRateStock || currentStoppingDecelRateStock == 0) {
        params.putFloatNonBlocking("StoppingDecelRate", stoppingDecelRate);
      }
      params.putFloatNonBlocking("StoppingDecelRateStock", stoppingDecelRate);
      migratedStockValues = true;
    }

    if (currentVEgoStartingStock != vEgoStarting && vEgoStarting != 0) {
      if (params.getFloat("VEgoStarting") == currentVEgoStartingStock || currentVEgoStartingStock == 0) {
        params.putFloatNonBlocking("VEgoStarting", vEgoStarting);
      }
      params.putFloatNonBlocking("VEgoStartingStock", vEgoStarting);
      migratedStockValues = true;
    }

    if (currentVEgoStoppingStock != vEgoStopping && vEgoStopping != 0) {
      if (params.getFloat("VEgoStopping") == currentVEgoStoppingStock || currentVEgoStoppingStock == 0) {
        params.putFloatNonBlocking("VEgoStopping", vEgoStopping);
      }
      params.putFloatNonBlocking("VEgoStoppingStock", vEgoStopping);
      migratedStockValues = true;
    }
  }

  if (migratedStockValues) {
    updateFrogPilotToggles();
  }

  std::string frogpilotCarParams = params.get("FrogPilotCarParamsPersistent");
  if (!frogpilotCarParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader fpcmsg(aligned_buf.align(frogpilotCarParams.data(), frogpilotCarParams.size()));
    cereal::FrogPilotCarParams::Reader FPCP = fpcmsg.getRoot<cereal::FrogPilotCarParams>();

    canUsePedal = FPCP.getCanUsePedal();
    canUseSDSU = FPCP.getCanUseSDSU();
    hasDashSpeedLimits = FPCP.getHasDashboardSpeedLimit();
    openpilotLongitudinalControlDisabled = FPCP.getOpenpilotLongitudinalControlDisabled();
  }

  std::string liveTorqueParameters = params.get("LiveTorqueParameters");
  if (!liveTorqueParameters.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader reader(aligned_buf.align(liveTorqueParameters.data(), liveTorqueParameters.size()));
    cereal::Event::Reader event = reader.getRoot<cereal::Event>();
    cereal::LiveTorqueParametersData::Reader LTP = event.getLiveTorqueParameters();

    hasAutoTune = LTP.getUseParams();
  }

  drivingPanelButtons->setVisibleButton(0, false);
  drivingPanelButtons->setVisibleButton(1, hasOpenpilotLongitudinal);

  systemPanelButtons->setVisibleButton(1, tuningLevel >= frogpilotToggleLevels.value("DeviceManagement").toDouble() || tuningLevel >= frogpilotToggleLevels.value("ScreenManagement").toDouble());

  vehiclePanelButtons->setVisibleButton(1, tuningLevel >= frogpilotToggleLevels.value("WheelControls").toDouble());

  update();
}
