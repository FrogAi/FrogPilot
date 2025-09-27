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

bool nnffLogFileExists(const QString &carFingerprint) {
  static QStringList files;
  if (files.isEmpty()) {
    QFileInfoList fileInfoList = QDir(QStringLiteral("../../frogpilot/assets/nnff_models")).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fileInfo : fileInfoList) {
      files.append(fileInfo.completeBaseName());
    }
  }

  for (const QString &file : files) {
    if (file.startsWith(carFingerprint)) {
      std::cout << "NNFF supports fingerprint: " << file.toStdString() << std::endl;
      return true;
    }
  }

  return false;
}

void FrogPilotSettingsWindow::createPanelButtons(FrogPilotListWidget *list) {
  FrogPilotDevicePanel *frogpilotDevicePanel = new FrogPilotDevicePanel(this);
  FrogPilotLateralPanel *frogpilotLateralPanel = new FrogPilotLateralPanel(this);
  FrogPilotLongitudinalPanel *frogpilotLongitudinalPanel = new FrogPilotLongitudinalPanel(this);
  FrogPilotMapsPanel *frogpilotMapsPanel = new FrogPilotMapsPanel(this);
  FrogPilotModelPanel *frogpilotModelPanel = new FrogPilotModelPanel(this);
  FrogPilotNavigationPanel *frogpilotNavigationPanel = new FrogPilotNavigationPanel(this);
  FrogPilotSoundsPanel *frogpilotSoundsPanel = new FrogPilotSoundsPanel(this);
  FrogPilotThemesPanel *frogpilotThemesPanel = new FrogPilotThemesPanel(this);
  FrogPilotVehiclesPanel *frogpilotVehiclesPanel = new FrogPilotVehiclesPanel(this);
  FrogPilotVisualsPanel *frogpilotVisualsPanel = new FrogPilotVisualsPanel(this);
  FrogPilotWheelPanel *frogpilotWheelPanel = new FrogPilotWheelPanel(this);

  std::vector<std::vector<std::tuple<QString, QWidget*>>> panelButtons = {
    {{tr("MANAGE"), frogpilotSoundsPanel}},
    {{tr("DRIVING MODEL"), frogpilotModelPanel}, {tr("GAS / BRAKE"), frogpilotLongitudinalPanel}, {tr("STEERING"), frogpilotLateralPanel}},
    {{tr("MAP DATA"), frogpilotMapsPanel}, {tr("NAVIGATION"), frogpilotNavigationPanel}},
    {{tr("DATA"), new FrogPilotDataPanel(this)}, {tr("DEVICE CONTROLS"), frogpilotDevicePanel}, {tr("UTILITIES"), new FrogPilotUtilitiesPanel(this)}},
    {{tr("APPEARANCE"), frogpilotVisualsPanel}, {tr("THEME"), frogpilotThemesPanel}},
    {{tr("VEHICLE SETTINGS"), frogpilotVehiclesPanel}, {tr("WHEEL CONTROLS"), frogpilotWheelPanel}}
  };

  std::vector<std::tuple<QString, QString, QString>> panelInfo = {
    {tr("Alerts and Sounds"), tr("<b>Adjust alert volumes and enable custom notifications.</b>"), "../../frogpilot/assets/toggle_icons/icon_sound.png"},
    {tr("Driving Controls"), tr("<b>Fine-tune custom FrogPilot acceleration, braking, and steering controls.</b>"), "../../frogpilot/assets/toggle_icons/icon_steering.png"},
    {tr("Navigation"), tr("<b>Download map data for the \"Speed Limit Controller\" and configure \"Navigate on openpilot\" (NOO).</b>"), "../../frogpilot/assets/toggle_icons/icon_map.png"},
    {tr("System Settings"), tr("<b>Manage backups, device settings, screen options, storage, and tools to keep FrogPilot running smoothly.</b>"), "../../frogpilot/assets/toggle_icons/icon_system.png"},
    {tr("Theme and Appearance"), tr("<b>Customize the look of the driving screen and interface, including themes!</b>"), "../../frogpilot/assets/toggle_icons/icon_display.png"},
    {tr("Vehicle Settings"), tr("<b>Configure car-specific options and steering wheel button mappings.</b>"), "../../frogpilot/assets/toggle_icons/icon_vehicle.png"}
  };

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
    if (title == tr("Alerts and Sounds")) soundPanelButtons = panelButton;
    if (title == tr("Driving Controls")) drivingPanelButtons = panelButton;
    if (title == tr("Navigation")) navigationPanelButtons = panelButton;
    if (title == tr("System Settings")) systemPanelButtons = panelButton;
    if (title == tr("Theme and Appearance")) themePanelButtons = panelButton;
    if (title == tr("Vehicle Settings")) vehiclePanelButtons = panelButton;

    if (forceOpenDescriptions) {
      panelButton->showDescription();
    }

    QObject::connect(panelButton, &FrogPilotButtonsControl::buttonClicked, [widgets, this](int id) {
      mainLayout->setCurrentWidget(widgets[id]);

      panelOpen = true;

      openPanel();
    });

    list->addItem(panelButton);
  }

  QObject::connect(frogpilotDevicePanel, &FrogPilotDevicePanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotLateralPanel, &FrogPilotLateralPanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotLongitudinalPanel, &FrogPilotLongitudinalPanel::openSubPanel, this, &FrogPilotSettingsWindow::openSubPanel);
  QObject::connect(frogpilotLongitudinalPanel, &FrogPilotLongitudinalPanel::openSubSubPanel, this, &FrogPilotSettingsWindow::openSubSubPanel);
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
  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

  mainLayout = new QStackedLayout(this);

  QWidget *frogpilotWidget = new QWidget(this);
  QVBoxLayout *frogpilotLayout = new QVBoxLayout(frogpilotWidget);
  frogpilotLayout->setContentsMargins(50, 25, 50, 25);
  frogpilotWidget->setLayout(frogpilotLayout);

  frogpilotPanel = new ScrollView(frogpilotWidget, this);
  mainLayout->addWidget(frogpilotPanel);
  frogpilotPanel->setWidget(frogpilotWidget);

  FrogPilotListWidget *list = new FrogPilotListWidget(this);
  frogpilotLayout->addWidget(list);

  std::vector<QString> togglePresets{tr("Minimal"), tr("Standard"), tr("Advanced"), tr("Developer")};
  togglePreset = new FrogPilotButtonsControl(tr("Tuning Level"),
                                             tr("Choose your tuning level. Lower levels keep it simple; higher levels unlock more toggles for finer control.\n\n"
                                                "Minimal - Ideal for those who prefer simplicity or ease of use\n"
                                                "Standard - Recommended for most users for a balanced experience\n"
                                                "Advanced - Fine-tuning for experienced users\n"
                                                "Developer - Highly customizable settings for seasoned enthusiasts"),
                                              "../../frogpilot/assets/toggle_icons/icon_tuning.png", togglePresets, true);
  QObject::connect(togglePreset, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    tuningLevel = id;

    params.putInt("TuningLevel", tuningLevel);

    updateVariables();

    if (id == 3) {
      ConfirmationDialog::alert(tr("WARNING: These settings are risky and can drastically change how openpilot drives. Only change if you fully understand what they do!"), this);
    }
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
  QObject::connect(parent, &SettingsWindow::updateMetric, this, &FrogPilotSettingsWindow::updateMetric);
  QObject::connect(parent, &SettingsWindow::updateTuningLevel, this, &FrogPilotSettingsWindow::updateTuningLevel);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &FrogPilotSettingsWindow::updateVariables);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotSettingsWindow::updateState);

  frogpilotToggleLevels = QJsonDocument::fromJson(params_memory.get("FrogPilotTuningLevels", true).c_str()).object();
  tuningLevel = params.getInt("TuningLevel");

  closeSubPanel();
  updateMetric(params.getBool("IsMetric"), true);
  updateVariables();
}

void FrogPilotSettingsWindow::updateTuningLevel() {
  tuningLevel = params.getInt("TuningLevel");
  togglePreset->setCheckedButton(params.getInt("TuningLevel"));

  updateVariables();
}

void FrogPilotSettingsWindow::showEvent(QShowEvent *event) {
  static bool alertShown = false;

  if (forceOpenDescriptions) {
    togglePreset->showDescription();

    drivingPanelButtons->showDescription();
    navigationPanelButtons->showDescription();
    soundPanelButtons->showDescription();
    systemPanelButtons->showDescription();
    themePanelButtons->showDescription();
    vehiclePanelButtons->showDescription();

    if (!alertShown) {
      ConfirmationDialog::alert(tr("All toggle descriptions are currently expanded. You can tap a toggle's name to open or close its description at any time!"), this);
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

  panelOpen = false;

  updateFrogPilotToggles();
}

void FrogPilotSettingsWindow::updateState() {
  FrogPilotUIState &fs = *frogpilotUIState();
  FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;

  frogpilot_scene.frogpilot_panel_active = panelOpen && keepScreenOn;
}

void FrogPilotSettingsWindow::updateVariables() {
  FrogPilotUIState &fs = *frogpilotUIState();
  QJsonObject &frogpilot_toggles = fs.frogpilot_toggles;

  std::string carParams = params.get("CarParamsPersistent");
  if (!carParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(carParams.data(), carParams.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();
    cereal::CarParams::SafetyModel safetyModel = CP.getSafetyConfigs()[0].getSafetyModel();

    std::string carFingerprint = CP.getCarFingerprint();
    carMake = CP.getCarName();

    friction = CP.getLateralTuning().getTorque().getFriction();
    hasBSM = CP.getEnableBsm();
    hasDashSpeedLimits = carMake == "ford" || carMake == "hyundai" || carMake == "toyota";
    hasExperimentalOpenpilotLongitudinal = CP.getExperimentalLongitudinalAvailable();
    hasNNFFLog = nnffLogFileExists(QString::fromStdString(carFingerprint));
    hasOpenpilotLongitudinal = hasLongitudinalControl(CP);
    hasPCMCruise = CP.getPcmCruise();
    hasPedal = CP.getEnableGasInterceptor();
    hasRadar = !CP.getRadarUnavailable();
    hasSDSU = frogpilot_toggles.value("has_sdsu").toBool();
    hasSNG = hasOpenpilotLongitudinal && CP.getAutoResumeSng();
    hasZSS = frogpilot_toggles.value("has_zss").toBool();
    isAngleCar = CP.getSteerControlType() == cereal::CarParams::SteerControlType::ANGLE;
    isBolt = carFingerprint == "CHEVROLET_BOLT_CC" || carFingerprint == "CHEVROLET_BOLT_EUV";
    isGM = carMake == "gm";
    isHKG = carMake == "hyundai";
    isHKGCanFd = isHKG && safetyModel == cereal::CarParams::SafetyModel::HYUNDAI_CANFD;
    isSubaru = carMake == "subaru";
    isTorqueCar = CP.getLateralTuning().which() == cereal::CarParams::LateralTuning::TORQUE;
    isToyota = carMake == "toyota";
    isTSK = CP.getSecOcRequired();
    isVolt = carFingerprint == "CHEVROLET_VOLT";
    latAccelFactor = CP.getLateralTuning().getTorque().getLatAccelFactor();
    longitudinalActuatorDelay = CP.getLongitudinalActuatorDelay();
    startAccel = CP.getStartAccel();
    steerActuatorDelay = CP.getSteerActuatorDelay();
    steerKp = CP.getLateralTuning().getTorque().getKp();
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
        params.putFloat("SteerDelay", steerActuatorDelay);
      }
      params.putFloat("SteerDelayStock", steerActuatorDelay);
    }

    if (currentFrictionStock != friction && friction != 0) {
      if (params.getFloat("SteerFriction") == currentFrictionStock || currentFrictionStock == 0) {
        params.putFloat("SteerFriction", friction);
      }
      params.putFloat("SteerFrictionStock", friction);
    }

    if (currentKPStock != steerKp && steerKp != 0) {
      if (params.getFloat("SteerKP") == currentKPStock || currentKPStock == 0) {
        params.putFloat("SteerKP", steerKp);
      }
      params.putFloat("SteerKPStock", steerKp);
    }

    if (currentLatAccelStock != latAccelFactor && latAccelFactor != 0) {
      if (params.getFloat("SteerLatAccel") == currentLatAccelStock || currentLatAccelStock == 0) {
        params.putFloat("SteerLatAccel", latAccelFactor);
      }
      params.putFloat("SteerLatAccelStock", latAccelFactor);
    }

    if (currentLongDelayStock != longitudinalActuatorDelay && longitudinalActuatorDelay != 0) {
      if (params.getFloat("LongitudinalActuatorDelay") == currentLongDelayStock || currentLongDelayStock == 0) {
        params.putFloat("LongitudinalActuatorDelay", longitudinalActuatorDelay);
      }
      params.putFloat("LongitudinalActuatorDelayStock", longitudinalActuatorDelay);
    }

    if (currentStartAccelStock != startAccel && startAccel != 0) {
      if (params.getFloat("StartAccel") == currentStartAccelStock || currentStartAccelStock == 0) {
        params.putFloat("StartAccel", startAccel);
      }
      params.putFloat("StartAccelStock", startAccel);
    }

    if (currentSteerRatioStock != steerRatio && steerRatio != 0) {
      if (params.getFloat("SteerRatio") == currentSteerRatioStock || currentSteerRatioStock == 0) {
        params.putFloat("SteerRatio", steerRatio);
      }
      params.putFloat("SteerRatioStock", steerRatio);
    }

    if (currentStopAccelStock != stopAccel && stopAccel != 0) {
      if (params.getFloat("StopAccel") == currentStopAccelStock || currentStopAccelStock == 0) {
        params.putFloat("StopAccel", stopAccel);
      }
      params.putFloat("StopAccelStock", stopAccel);
    }

    if (currentStoppingDecelRateStock != stoppingDecelRate && stoppingDecelRate != 0) {
      if (params.getFloat("StoppingDecelRate") == currentStoppingDecelRateStock || currentStoppingDecelRateStock == 0) {
        params.putFloat("StoppingDecelRate", stoppingDecelRate);
      }
      params.putFloat("StoppingDecelRateStock", stoppingDecelRate);
    }

    if (currentVEgoStartingStock != vEgoStarting && vEgoStarting != 0) {
      if (params.getFloat("VEgoStarting") == currentVEgoStartingStock || currentVEgoStartingStock == 0) {
        params.putFloat("VEgoStarting", vEgoStarting);
      }
      params.putFloat("VEgoStartingStock", vEgoStarting);
    }

    if (currentVEgoStoppingStock != vEgoStopping && vEgoStopping != 0) {
      if (params.getFloat("VEgoStopping") == currentVEgoStoppingStock || currentVEgoStoppingStock == 0) {
        params.putFloat("VEgoStopping", vEgoStopping);
      }
      params.putFloat("VEgoStoppingStock", vEgoStopping);
    }
  }

  std::string frogpilotCarParams = params.get("FrogPilotCarParamsPersistent");
  if (!frogpilotCarParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader fpcmsg(aligned_buf.align(frogpilotCarParams.data(), frogpilotCarParams.size()));
    cereal::FrogPilotCarParams::Reader FPCP = fpcmsg.getRoot<cereal::FrogPilotCarParams>();

    canUsePedal = FPCP.getCanUsePedal();
    canUseSDSU = FPCP.getCanUseSDSU();
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

  isC3 = util::read_file("/sys/firmware/devicetree/base/model").find("tici") != std::string::npos;

  drivingPanelButtons->setVisibleButton(0, tuningLevel >= frogpilotToggleLevels.value("Model").toDouble());
  drivingPanelButtons->setVisibleButton(1, hasOpenpilotLongitudinal);

  systemPanelButtons->setVisibleButton(1, tuningLevel >= frogpilotToggleLevels.value("DeviceManagement").toDouble() || tuningLevel >= frogpilotToggleLevels.value("ScreenManagement").toDouble());

  vehiclePanelButtons->setVisibleButton(1, tuningLevel >= frogpilotToggleLevels.value("WheelControls").toDouble());

  update();
}
