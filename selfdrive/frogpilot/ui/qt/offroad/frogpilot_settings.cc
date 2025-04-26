#include "selfdrive/frogpilot/navigation/ui/maps_settings.h"
#include "selfdrive/frogpilot/navigation/ui/navigation_settings.h"
#include "selfdrive/frogpilot/ui/qt/offroad/data_settings.h"
#include "selfdrive/frogpilot/ui/qt/offroad/device_settings.h"
#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"
#include "selfdrive/frogpilot/ui/qt/offroad/lateral_settings.h"
#include "selfdrive/frogpilot/ui/qt/offroad/longitudinal_settings.h"
#include "selfdrive/frogpilot/ui/qt/offroad/model_settings.h"
#include "selfdrive/frogpilot/ui/qt/offroad/sounds_settings.h"
#include "selfdrive/frogpilot/ui/qt/offroad/theme_settings.h"
#include "selfdrive/frogpilot/ui/qt/offroad/utilities.h"
#include "selfdrive/frogpilot/ui/qt/offroad/vehicle_settings.h"
#include "selfdrive/frogpilot/ui/qt/offroad/visual_settings.h"
#include "selfdrive/frogpilot/ui/qt/offroad/wheel_settings.h"

bool nnffLogFileExists(const QString &carFingerprint) {
  static QStringList files;
  if (files.isEmpty()) {
    QFileInfoList fileInfoList = QDir(QStringLiteral("../car/torque_data/lat_models")).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
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
    {tr("Alerts and Sounds"), tr("FrogPilot settings for alert volumes and custom notifications to stay informed about important driving events."), "../frogpilot/assets/toggle_icons/icon_sound.png"},
    {tr("Driving Controls"), tr("FrogPilot settings for acceleration, braking, and steering."), "../frogpilot/assets/toggle_icons/icon_steering.png"},
    {tr("Navigation"), tr("Download map data for <b>Curve Speed Control</b>, <b>Speed Limit Controller</b>, and set up <b>Navigate on openpilot (NOO)</b>."), "../frogpilot/assets/toggle_icons/icon_map.png"},
    {tr("System Management"), tr("Data storage management, debugging tools, device settings, screen behavior settings, system backups, and utilities to maintain, optimize, and troubleshoot FrogPilot."), "../frogpilot/assets/toggle_icons/icon_system.png"},
    {tr("Theme and Appearance"), tr("FrogPilot settings for the current theme, driving-screen, and the overall user interface."), "../frogpilot/assets/toggle_icons/icon_display.png"},
    {tr("Vehicle Controls"), tr("Options unique to supported manufacturers and vehicle fingerprint management."), "../frogpilot/assets/toggle_icons/icon_vehicle.png"}
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
    if (title == tr("Driving Controls")) drivingPanelButtons = panelButton;
    if (title == tr("System Management")) systemPanelButtons = panelButton;
    if (title == tr("Vehicle Controls")) vehiclePanelButtons = panelButton;

    QObject::connect(panelButton, &FrogPilotButtonsControl::buttonClicked, [this, widgets](int id) {
      mainLayout->setCurrentWidget(widgets[id]);

      panelOpen = true;

      openPanel();
    });

    list->addItem(panelButton);
  }

  QObject::connect(frogpilotDevicePanel, &FrogPilotDevicePanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);
  QObject::connect(frogpilotLateralPanel, &FrogPilotLateralPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);
  QObject::connect(frogpilotLongitudinalPanel, &FrogPilotLongitudinalPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);
  QObject::connect(frogpilotLongitudinalPanel, &FrogPilotLongitudinalPanel::openSubParentToggle, this, &FrogPilotSettingsWindow::openSubParentToggle);
  QObject::connect(frogpilotMapsPanel, &FrogPilotMapsPanel::openMapSelection, this, &FrogPilotSettingsWindow::openMapSelection);
  QObject::connect(frogpilotModelPanel, &FrogPilotModelPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);
  QObject::connect(frogpilotNavigationPanel, &FrogPilotNavigationPanel::closeMapBoxInstructions, this, &FrogPilotSettingsWindow::closeMapBoxInstructions);
  QObject::connect(frogpilotNavigationPanel, &FrogPilotNavigationPanel::openMapBoxInstructions, this, &FrogPilotSettingsWindow::openMapBoxInstructions);
  QObject::connect(frogpilotSoundsPanel, &FrogPilotSoundsPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);
  QObject::connect(frogpilotThemesPanel, &FrogPilotThemesPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);
  QObject::connect(frogpilotVehiclesPanel, &FrogPilotVehiclesPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);
  QObject::connect(frogpilotVisualsPanel, &FrogPilotVisualsPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);
  QObject::connect(frogpilotVisualsPanel, &FrogPilotVisualsPanel::openSubParentToggle, this, &FrogPilotSettingsWindow::openSubParentToggle);
}

FrogPilotSettingsWindow::FrogPilotSettingsWindow(SettingsWindow *parent) : QFrame(parent) {
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
  FrogPilotButtonsControl *togglePreset = new FrogPilotButtonsControl(tr("Tuning Level"),
                                          tr("The visibility and complexity of tuning settings. Lower levels simplify the interface by hiding advanced options, while higher levels unlock detailed customization.\n\n"
                                          "Minimal - Ideal for those who prefer simplicity or ease of use\n"
                                          "Standard - Recommended for most users for a balanced experience\n"
                                          "Advanced - Unlocks fine-tuning controls for more experienced users\n"
                                          "Developer - Unlocks highly customizable settings for seasoned enthusiasts"),
                                          "../frogpilot/assets/toggle_icons/icon_customization.png",
                                          togglePresets, true);

  int timeTo100FPHours = 100 - (paramsTracking.getInt("FrogPilotMinutes") / 60);
  int timeTo250OPHours = 250 - (params.getInt("openpilotMinutes") / 60);
  togglePreset->setEnabledButtons(3, timeTo100FPHours <= 0 || timeTo250OPHours <= 0);

  QObject::connect(togglePreset, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    tuningLevel = id;

    params.putInt("TuningLevel", tuningLevel);

    updateVariables();

    if (id == 3) {
      ConfirmationDialog::alert(tr("WARNING: This unlocks some potentially dangerous settings that can DRASTICALLY alter your driving experience!"), this);
    }
  });

  QObject::connect(togglePreset, &FrogPilotButtonsControl::disabledButtonClicked, [this](int id) {
    if (id == 3) {
      ConfirmationDialog::alert(tr("The <b>Developer</b> preset is only available for users with either over 100 hours on FrogPilot, or 250 hours with openpilot."), this);
    }
  });
  togglePreset->setCheckedButton(params.getInt("TuningLevel"));
  list->addItem(togglePreset, true);

  createPanelButtons(list);

  QObject::connect(parent, &SettingsWindow::closeMapBoxInstructions, this, &FrogPilotSettingsWindow::closeMapBoxInstructions);
  QObject::connect(parent, &SettingsWindow::closeMapSelection, this, &FrogPilotSettingsWindow::closeMapSelection);
  QObject::connect(parent, &SettingsWindow::closePanel, this, &FrogPilotSettingsWindow::closePanel);
  QObject::connect(parent, &SettingsWindow::closeParentToggle, this, &FrogPilotSettingsWindow::closeParentToggle);
  QObject::connect(parent, &SettingsWindow::closeSubParentToggle, this, &FrogPilotSettingsWindow::closeSubParentToggle);
  QObject::connect(parent, &SettingsWindow::updateMetric, this, &FrogPilotSettingsWindow::updateMetric);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &FrogPilotSettingsWindow::updateVariables);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotSettingsWindow::updateState);

  frogpilotToggleLevels = QJsonDocument::fromJson(params_memory.get("FrogPilotTuningLevels", true).c_str()).object();
  tuningLevel = params.getInt("TuningLevel");

  closeParentToggle();
  updateMetric(params.getBool("IsMetric"), true);
  updateVariables();
}

void FrogPilotSettingsWindow::hideEvent(QHideEvent *event) {
  closePanel();
}

void FrogPilotSettingsWindow::closePanel() {
  mainLayout->setCurrentWidget(frogpilotPanel);

  panelOpen = false;

  updateFrogPilotToggles();
}

void FrogPilotSettingsWindow::updateState() {
  UIState *s = uiState();
  UIScene &scene = s->scene;

  scene.frogpilot_panel_active = panelOpen && keepScreenOn;
}

void FrogPilotSettingsWindow::updateVariables() {
  std::string carParams = params.get("CarParamsPersistent");
  if (!carParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(carParams.data(), carParams.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();
    cereal::CarParams::SafetyModel safetyModel = CP.getSafetyConfigs()[0].getSafetyModel();

    std::string carFingerprint = CP.getCarFingerprint();
    std::string carMake = CP.getCarName();

    hasAutoTune = (carMake == "hyundai" || carMake == "toyota") && CP.getLateralTuning().which() == cereal::CarParams::LateralTuning::TORQUE;
    hasBSM = CP.getEnableBsm();
    hasDashSpeedLimits = carMake == "ford" || carMake == "hyundai" || carMake == "toyota";
    hasExperimentalOpenpilotLongitudinal = CP.getExperimentalLongitudinalAvailable();
    hasNNFFLog = nnffLogFileExists(QString::fromStdString(carFingerprint));
    hasOpenpilotLongitudinal = hasLongitudinalControl(CP);
    hasPCMCruise = CP.getPcmCruise();
    hasRadar = !CP.getRadarUnavailable();
    hasSNG = CP.getMinEnableSpeed() <= 0;
    isBolt = carFingerprint == "CHEVROLET_BOLT_CC" || carFingerprint == "CHEVROLET_BOLT_EUV";
    isGM = carMake == "gm";
    isHKG = carMake == "hyundai";
    isHKGCanFd = isHKG && safetyModel == cereal::CarParams::SafetyModel::HYUNDAI_CANFD;
    isSubaru = carMake == "subaru";
    isTorqueCar = CP.getLateralTuning().which() == cereal::CarParams::LateralTuning::TORQUE;
    isToyota = carMake == "toyota";
    isTSK = CP.getSecOcRequired();
    isVolt = carFingerprint == "CHEVROLET_VOLT";
    frictionStock = CP.getLateralTuning().getTorque().getFriction();
    kpStock = CP.getLateralTuning().getTorque().getKp();
    latAccelStock = CP.getLateralTuning().getTorque().getLatAccelFactor();
    steerRatioStock = CP.getSteerRatio();

    float currentFrictionStock = params.getFloat("SteerFrictionStock");
    float currentKPStock = params.getFloat("SteerKPStock");
    float currentLatAccelStock = params.getFloat("SteerLatAccelStock");
    float currentSteerRatioStock = params.getFloat("SteerRatioStock");

    if (currentFrictionStock != frictionStock && frictionStock != 0) {
      if (params.getFloat("SteerFriction") == currentFrictionStock || currentFrictionStock == 0) {
        params.putFloat("SteerFriction", frictionStock);
      }
      params.putFloat("SteerFrictionStock", frictionStock);
    }

    if (currentKPStock != kpStock && kpStock != 0) {
      if (params.getFloat("SteerKP") == currentKPStock || currentKPStock == 0) {
        params.putFloat("SteerKP", kpStock);
      }
      params.putFloat("SteerKPStock", kpStock);
    }

    if (currentLatAccelStock != latAccelStock && latAccelStock != 0) {
      if (params.getFloat("SteerLatAccel") == currentLatAccelStock || currentLatAccelStock == 0) {
        params.putFloat("SteerLatAccel", latAccelStock);
      }
      params.putFloat("SteerLatAccelStock", latAccelStock);
    }

    if (currentSteerRatioStock != steerRatioStock && steerRatioStock != 0) {
      if (params.getFloat("SteerRatio") == currentSteerRatioStock || currentSteerRatioStock == 0) {
        params.putFloat("SteerRatio", steerRatioStock);
      }
      params.putFloat("SteerRatioStock", steerRatioStock);
    }
  }

  isC3 = util::read_file("/sys/firmware/devicetree/base/model").find("tici") != std::string::npos;

  drivingPanelButtons->setVisibleButton(0, tuningLevel >= frogpilotToggleLevels.value("Model").toDouble());
  drivingPanelButtons->setVisibleButton(1, hasOpenpilotLongitudinal);

  systemPanelButtons->setVisibleButton(1, tuningLevel >= frogpilotToggleLevels.value("DeviceManagement").toDouble() || tuningLevel >= frogpilotToggleLevels.value("ScreenManagement").toDouble());

  vehiclePanelButtons->setVisibleButton(1, tuningLevel >= frogpilotToggleLevels.value("WheelControls").toDouble());

  update();
}
