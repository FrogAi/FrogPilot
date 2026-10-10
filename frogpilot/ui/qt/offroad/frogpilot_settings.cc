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
    {{tr("NAVIGATION"), frogpilotNavigationPanel}, {tr("SPEED LIMIT MAPS"), frogpilotMapsPanel}},
    {{tr("DATA"), frogpilotDataPanel}, {tr("DEVICE / SCREEN"), frogpilotDevicePanel}, {tr("UTILITIES"), frogpilotUtilitiesPanel}},
    {{tr("DRIVING VIEW"), frogpilotVisualsPanel}, {tr("THEME"), frogpilotThemesPanel}},
    {{tr("VEHICLE SETTINGS"), frogpilotVehiclesPanel}, {tr("WHEEL BUTTONS"), frogpilotWheelPanel}}
  };

  std::vector<std::tuple<QString, QString, QString>> panelInfo = {
    {tr("Alerts and Sounds"), tr("<b>Set the volume for each of openpilot's alerts, and add extra alerts stock openpilot doesn't have.</b> Extra alerts include a chime when the light turns green or when the car ahead starts moving."), "../../frogpilot/assets/toggle_icons/icon_sound.svg"},
    {tr("Driving Controls"), tr("<b>Adjust how openpilot accelerates, brakes, steers, and changes lanes.</b><br><br>\"GAS / BRAKE\" only appears on cars where openpilot handles the gas and brake."), "../../frogpilot/assets/toggle_icons/icon_steering.svg"},
    {tr("Maps and Navigation"), tr("<b>Download the speed limit data openpilot uses, and set up Mapbox as a fallback speed limit source.</b> Speed limits come from offline map data for the states or countries you pick, so they work without cell signal."), "../../frogpilot/assets/toggle_icons/icon_navigate.svg"},
    {tr("System Settings"), tr("<b>Manage your saved data, how the device and screen behave, and tools for fixing problems.</b> This is also where your drive stats and backups of your settings live."), "../../frogpilot/assets/toggle_icons/icon_system.svg"},
    {tr("Theme and Appearance"), tr("<b>Change what appears on the driving screen, and how openpilot looks and sounds.</b> Anything from hiding on-screen icons to full theme packs with new colors, sounds, and turn signal animations."), "../../frogpilot/assets/toggle_icons/icon_display.svg"},
    {tr("Vehicle Settings"), tr("<b>Tell openpilot what car you drive, turn on features made for your brand, and change what your steering wheel buttons do.</b><br><br>Brand features include things like smoother stop-and-go and automatic door locks. \"WHEEL BUTTONS\" only appears once your \"Tuning Level\" is \"Advanced\" or higher."), "../../frogpilot/assets/toggle_icons/icon_vehicle.svg"}
  };

  FrogPilotButtonsControl **panelMembers[] = {&soundPanelButtons, &drivingPanelButtons, &navigationPanelButtons, &systemPanelButtons, &themePanelButtons, &vehiclePanelButtons};

  for (size_t i = 0; i < panelInfo.size(); ++i) {
    const QString &title = std::get<0>(panelInfo[i]);
    const QString &description = std::get<1>(panelInfo[i]);
    const QString &icon = std::get<2>(panelInfo[i]);

    const std::vector<std::tuple<QString, QWidget*>> &widgetLabels = panelButtons[i];

    std::vector<QString> labels;
    std::vector<ScrollView*> widgets;

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

    QObject::connect(panelButton, &FrogPilotButtonsControl::buttonClicked, [widgets, this](int id) {
      mainLayout->setCurrentWidget(widgets[id]);

      panelOpen = true;

      openPanel();

      QWidget *panel = widgets[id]->widget();
      QString className = panel->metaObject()->className();

      if (!shownDescriptions.value(className).toBool(false)) {
        shownDescriptions = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("ShownToggleDescriptions"))).object();
        shownDescriptions.insert(className, true);
        params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
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
}

FrogPilotSettingsWindow::FrogPilotSettingsWindow(SettingsWindow *parent) : QFrame(parent) {
  shownDescriptions = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("ShownToggleDescriptions"))).object();

  QString className = this->metaObject()->className();
  forceOpenDescriptions = !shownDescriptions.value(className).toBool(false);

  mainLayout = new QStackedLayout(this);

  QWidget *frogpilotWidget = new QWidget(this);
  QVBoxLayout *frogpilotLayout = new QVBoxLayout(frogpilotWidget);
  frogpilotLayout->setContentsMargins(50, 25, 50, 25);

  frogpilotPanel = new ScrollView(frogpilotWidget, this);
  mainLayout->addWidget(frogpilotPanel);

  FrogPilotListWidget *list = new FrogPilotListWidget(this);
  frogpilotLayout->addWidget(list);

  for (const std::string &key : params.allKeys()) {
    frogpilotToggleLevels[QString::fromStdString(key)] = params.getTuningLevel(key);
  }
  tuningLevel = params.getInt("TuningLevel");

  std::vector<QString> togglePresets{tr("Minimal"), tr("Standard"), tr("Advanced"), tr("Developer")};
  togglePreset = new FrogPilotButtonsControl(tr("Tuning Level"),
                                             tr("<b>Choose how much control you want over FrogPilot's settings.</b> Anything above your level is hidden and uses FrogPilot's recommended setting instead. Nothing you've set is lost, and it comes back when you move up.<br><br>"
                                                "Minimal - FrogPilot decides nearly everything for you<br>"
                                                "Standard - Recommended for most drivers<br>"
                                                "Advanced - Extra fine-tuning once you know how your car drives<br>"
                                                "Developer - Everything, including settings that can drastically change how openpilot drives"),
                                              "../../frogpilot/assets/toggle_icons/icon_tuning.svg", togglePresets, true);
  QObject::connect(togglePreset, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 3 && !ConfirmationDialog::confirm(tr("\"Developer\" unlocks settings that can drastically change how openpilot drives, and any you changed before will start being used again.\n\nOnly continue if you know what they do."), tr("Continue"), this)) {
      togglePreset->setCheckedButton(tuningLevel);
      return;
    }

    bool rebootRequired = false;
    for (const QString &key : QStringList{"ForceTorqueController", "LateralTune", "NNFF", "NNFFLite"}) {
      double keyLevel = frogpilotToggleLevels.value(key).toDouble();
      bool crossesLevel = (tuningLevel >= keyLevel) != (id >= keyLevel);
      bool differsFromDefault = params.getBool(key.toStdString()) != (params.getKeyDefaultValue(key.toStdString()) == "1");
      rebootRequired |= crossesLevel && differsFromDefault;
    }

    tuningLevel = id;

    params.putIntNonBlocking("TuningLevel", tuningLevel);
    params.putBoolNonBlocking("TuningLevelConfirmed", true);

    updateVariables();

    emit tuningLevelChanged(tuningLevel);

    if (uiState()->scene.started && !isAngleCar && rebootRequired && FrogPilotConfirmationDialog::toggleReboot(this)) {
      FrogPilotConfirmationDialog::softReboot(this);
    }
  });
  togglePreset->setCheckedButton(tuningLevel);
  list->addItem(togglePreset, true);

  createPanelButtons(list);

  QObject::connect(parent, &SettingsWindow::closePanel, this, &FrogPilotSettingsWindow::closePanel);
  QObject::connect(parent, &SettingsWindow::closeSubPanel, this, &FrogPilotSettingsWindow::closeSubPanel);
  QObject::connect(parent, &SettingsWindow::closeSubSubPanel, this, &FrogPilotSettingsWindow::closeSubSubPanel);
  QObject::connect(parent, &SettingsWindow::closeSubSubSubPanel, this, &FrogPilotSettingsWindow::closeSubSubSubPanel);
  carParamsWatcher = new ParamWatcher(this);
  QObject::connect(carParamsWatcher, &ParamWatcher::paramChanged, this, &FrogPilotSettingsWindow::updateVariables);

  QObject::connect(uiState(), &UIState::offroadTransition, this, &FrogPilotSettingsWindow::updateVariables);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotSettingsWindow::updateState);

  closeSubPanel();
  updateMetric(params.getBool("IsMetric"), true);
  updateVariables();
}

void FrogPilotSettingsWindow::confirmTuningLevel(QWidget *dialogParent) {
  int frogpilotHours = QJsonDocument::fromJson(QString::fromStdString(params.get("FrogPilotStats")).toUtf8()).object().value("FrogPilotSeconds").toDouble() / (60 * 60);
  int openpilotHours = params.getInt("KonikMinutes") / 60 + params.getInt("openpilotMinutes") / 60;

  QString message;
  int newTuningLevel;
  if (frogpilotHours < 1 && openpilotHours < 10) {
    message = SettingsWindow::tr("Welcome to FrogPilot! Since you're new to openpilot, the \"Minimal\" toggle preset has been applied, but you can change this at any time via the \"Tuning Level\" button!");
    newTuningLevel = 0;
  } else if (frogpilotHours < 1 && openpilotHours < 100) {
    message = SettingsWindow::tr("Welcome to FrogPilot! Since you're new to FrogPilot, the \"Minimal\" toggle preset has been applied, but you can change this at any time via the \"Tuning Level\" button!");
    newTuningLevel = 0;
  } else if (frogpilotHours < 50 && openpilotHours < 100) {
    message = SettingsWindow::tr("Since you're fairly new to FrogPilot, the \"Minimal\" toggle preset has been applied, but you can change this at any time via the \"Tuning Level\" button!");
    newTuningLevel = 0;
  } else if (frogpilotHours < 100 && openpilotHours >= 100) {
    message = SettingsWindow::tr("Since you're experienced with openpilot, the \"Standard\" toggle preset has been applied, but you can change this at any time via the \"Tuning Level\" button!");
    newTuningLevel = 1;
  } else if (frogpilotHours < 100) {
    message = SettingsWindow::tr("Since you're experienced with FrogPilot, the \"Standard\" toggle preset has been applied, but you can change this at any time via the \"Tuning Level\" button!");
    newTuningLevel = 1;
  } else {
    message = SettingsWindow::tr("Since you're very experienced with FrogPilot, the \"Advanced\" toggle preset has been applied, but you can change this at any time via the \"Tuning Level\" button!");
    newTuningLevel = 2;
  }

  if (ConfirmationDialog::alert(message, dialogParent, true)) {
    params.putBool("TuningLevelConfirmed", true);
    params.putInt("TuningLevel", newTuningLevel);
  }
  updateTuningLevel();
}

void FrogPilotSettingsWindow::updateTuningLevel() {
  tuningLevel = params.getInt("TuningLevel");
  togglePreset->setCheckedButton(tuningLevel);

  updateVariables();

  emit tuningLevelChanged(tuningLevel);
}

void FrogPilotSettingsWindow::showEvent(QShowEvent *event) {
  updateTuningLevel();

  static bool alertShown = false;

  QString className = this->metaObject()->className();
  if (!shownDescriptions.value(className).toBool(false)) {
    shownDescriptions = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("ShownToggleDescriptions"))).object();
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
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

  frogpilot_scene.frogpilot_panel_active = panelOpen && (keepScreenOn || activeOperations > 0);
}

void FrogPilotSettingsWindow::updateVariables() {
  FrogPilotUIState &fs = *frogpilotUIState();
  FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;
  QJsonObject &frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  bool migratedStockValues = false;

  carParamsWatcher->addParam("CarParamsPersistent");

  std::string carParams = params.get("CarParamsPersistent");
  if (!carParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(carParams.data(), carParams.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();
    capnp::List<cereal::CarParams::SafetyConfig>::Reader safetyConfigs = CP.getSafetyConfigs();

    carDetected = true;

    carFingerprint = CP.getCarFingerprint();
    std::string carMake = CP.getBrand();
    isTorqueCar = CP.getLateralTuning().which() == cereal::CarParams::LateralTuning::TORQUE;

    canDisableOpenpilotLong = frogpilot_toggles.value("can_disable_openpilot_long").toBool();
    canUseDSUBypass = frogpilot_toggles.value("can_use_dsu_bypass").toBool();
    friction = isTorqueCar ? CP.getLateralTuning().getTorque().getFriction() : 0.0f;
    hasAlphaLongitudinal = CP.getAlphaLongitudinalAvailable();
    hasBSM = CP.getEnableBsm();
    hasDashSpeedLimits = false;
    hasLKASButton = frogpilot_toggles.value("has_lkas_button").toBool();
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
    isGMCCOnly = frogpilot_toggles.value("is_gm_cc_only").toBool();
    isHKG = carMake == "hyundai";
    isHKGCanFd = isHKG && safetyConfigs.size() > 0 && safetyConfigs[safetyConfigs.size() - 1].getSafetyModel() == cereal::CarParams::SafetyModel::HYUNDAI_CANFD;
    isHondaNidec = frogpilot_toggles.value("is_honda_nidec").toBool();
    isSubaru = carMake == "subaru";
    isToyota = carMake == "toyota";
    isTSK = CP.getSecOcRequired();
    isVolt = carFingerprint == "CHEVROLET_VOLT";
    latAccelFactor = isTorqueCar ? CP.getLateralTuning().getTorque().getLatAccelFactor() : 0.0f;
    lkasAllowedForAOL = frogpilot_toggles.value("lkas_allowed_for_aol").toBool();
    longitudinalActuatorDelay = CP.getLongitudinalActuatorDelay();
    startAccel = CP.getStartAccel();
    steerActuatorDelay = CP.getSteerActuatorDelay();
    steerKp = isTorqueCar ? 1.0f : 0.0f;
    steerRatio = CP.getSteerRatio();
    stopAccel = CP.getStopAccel();
    stoppingDecelRate = CP.getStoppingDecelRate();
    vEgoStarting = CP.getVEgoStarting();
    vEgoStopping = CP.getVEgoStopping();

    std::vector<std::pair<std::string, float>> stockValues = {
      {"SteerDelay", steerActuatorDelay},
      {"SteerFriction", friction},
      {"SteerKP", steerKp},
      {"SteerLatAccel", latAccelFactor},
      {"LongitudinalActuatorDelay", longitudinalActuatorDelay},
      {"StartAccel", startAccel},
      {"SteerRatio", steerRatio},
      {"StopAccel", stopAccel},
      {"StoppingDecelRate", stoppingDecelRate},
      {"VEgoStarting", vEgoStarting},
      {"VEgoStopping", vEgoStopping}
    };
    for (const auto &[key, value] : stockValues) {
      float currentStock = params.getFloat(key + "Stock");
      float storedValue = std::stof(std::to_string(value));

      if (currentStock != storedValue && (value != 0 || key == "StartAccel")) {
        if (params.getFloat(key) == currentStock || currentStock == 0) {
          params.putFloat(key, value);
        }
        params.putFloat(key + "Stock", value);
        migratedStockValues = true;
      }
    }
  }

  if (migratedStockValues) {
    frogpilotUIState()->updateToggles();
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
  } else {
    hasAutoTune = false;
  }

  drivingPanelButtons->setVisibleButton(0, tuningLevel >= frogpilotToggleLevels.value("DrivingModel").toDouble());
  drivingPanelButtons->setVisibleButton(1, hasOpenpilotLongitudinal);

  systemPanelButtons->setVisibleButton(1, tuningLevel >= frogpilotToggleLevels.value("DeviceManagement").toDouble() || tuningLevel >= frogpilotToggleLevels.value("ScreenManagement").toDouble());

  themePanelButtons->setVisibleButton(0, hasOpenpilotLongitudinal || tuningLevel >= frogpilotToggleLevels.value("Compass").toDouble());

  vehiclePanelButtons->setVisibleButton(1, tuningLevel >= frogpilotToggleLevels.value("WheelControls").toDouble());

  update();
}
