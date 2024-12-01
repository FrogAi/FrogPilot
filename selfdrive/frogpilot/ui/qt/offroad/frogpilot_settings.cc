#include <filesystem>

#include "selfdrive/ui/qt/widgets/scrollview.h"

#include "selfdrive/frogpilot/navigation/ui/maps_settings.h"
#include "selfdrive/frogpilot/navigation/ui/primeless_settings.h"
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

bool checkNNFFLogFileExists(const std::string &carFingerprint) {
  static std::vector<std::string> files;
  if (files.empty()) {
    for (std::filesystem::directory_entry entry : std::filesystem::directory_iterator("../car/torque_data/lat_models")) {
      files.emplace_back(entry.path().filename().stem().string());
    }
  }

  for (const std::string &file : files) {
    if (file.rfind(carFingerprint, 0) == 0) {
      std::cout << "NNFF supports fingerprint: " << file << std::endl;
      return true;
    }
  }

  return false;
}

FrogPilotSettingsWindow::FrogPilotSettingsWindow(SettingsWindow *parent) : QFrame(parent) {
  mainLayout = new QStackedLayout(this);

  frogpilotSettingsWidget = new QWidget(this);
  QVBoxLayout *frogpilotSettingsLayout = new QVBoxLayout(frogpilotSettingsWidget);
  frogpilotSettingsLayout->setContentsMargins(50, 25, 50, 25);

  FrogPilotListWidget *list = new FrogPilotListWidget(frogpilotSettingsWidget);

  std::vector<QString> togglePresets{tr("Minimal"), tr("Standard"), tr("Advanced"), tr("Developer")};
  ButtonParamControl *togglePreset = new ButtonParamControl("TuningLevel", tr("Tuning Level"),
                                        tr("Select the tuning level that best suits your needs. 'Minimal' is ideal for those who prefer simplicity and ease of use, "
                                        "'Standard' is recommended for most users, offering a balanced experience, "
                                        "'Advanced' provides more control for experienced users, "
                                        "while 'Developer' unlocks highly customizable settings designed for seasoned enthusiasts."),
                                        "../frogpilot/assets/toggle_icons/icon_customization.png",
                                        togglePresets);
  int timeTo100FPHours = 100 - (paramsTracking.getInt("FrogPilotMinutes") / 60);
  int timeTo250OPHours = 250 - (params.getInt("openpilotMinutes") / 60);
  togglePreset->setEnabledButtons(3, timeTo100FPHours <= 0 || timeTo250OPHours <= 0);
  QObject::connect(togglePreset, &ButtonParamControl::buttonClicked, [=](int id) {
    tuningLevel = id;

    if (id == 3) {
      FrogPilotConfirmationDialog::toggleAlert(
        tr("WARNING: This unlocks some potentially dangerous settings that can DRASTICALLY alter your driving experience!"),
        tr("I understand the risks."), this
      );
    }

    updateVariables();
  });
  QObject::connect(togglePreset, &ButtonParamControl::disabledButtonClicked, [=](int id) {
    if (id == 3) {
      FrogPilotConfirmationDialog::toggleAlert(
        tr("The 'Developer' preset is only available for users with either over 100 hours on FrogPilot, or 250 hours with openpilot."),
        tr("Okay"), this
      );
    }
  });
  list->addItem(togglePreset);

  FrogPilotDevicePanel *frogpilotDevicePanel = new FrogPilotDevicePanel(this);
  QObject::connect(frogpilotDevicePanel, &FrogPilotDevicePanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);

  FrogPilotLateralPanel *frogpilotLateralPanel = new FrogPilotLateralPanel(this);
  QObject::connect(frogpilotLateralPanel, &FrogPilotLateralPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);

  FrogPilotLongitudinalPanel *frogpilotLongitudinalPanel = new FrogPilotLongitudinalPanel(this);
  QObject::connect(frogpilotLongitudinalPanel, &FrogPilotLongitudinalPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);
  QObject::connect(frogpilotLongitudinalPanel, &FrogPilotLongitudinalPanel::openSubParentToggle, this, &FrogPilotSettingsWindow::openSubParentToggle);

  FrogPilotModelPanel *frogpilotModelPanel = new FrogPilotModelPanel(this);
  QObject::connect(frogpilotModelPanel, &FrogPilotModelPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);
  QObject::connect(frogpilotModelPanel, &FrogPilotModelPanel::openSubParentToggle, this, &FrogPilotSettingsWindow::openSubParentToggle);

  FrogPilotMapsPanel *frogpilotMapsPanel = new FrogPilotMapsPanel(this);
  QObject::connect(frogpilotMapsPanel, &FrogPilotMapsPanel::openMapSelection, this, &FrogPilotSettingsWindow::openMapSelection);

  FrogPilotPrimelessPanel *frogpilotPrimelessPanel = new FrogPilotPrimelessPanel(this);
  QObject::connect(frogpilotPrimelessPanel, &FrogPilotPrimelessPanel::closeMapBoxInstructions, this, &FrogPilotSettingsWindow::closeMapBoxInstructions);
  QObject::connect(frogpilotPrimelessPanel, &FrogPilotPrimelessPanel::openMapBoxInstructions, this, &FrogPilotSettingsWindow::openMapBoxInstructions);

  FrogPilotSoundsPanel *frogpilotSoundsPanel = new FrogPilotSoundsPanel(this);
  QObject::connect(frogpilotSoundsPanel, &FrogPilotSoundsPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);

  FrogPilotThemesPanel *frogpilotThemesPanel = new FrogPilotThemesPanel(this);
  QObject::connect(frogpilotThemesPanel, &FrogPilotThemesPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);

  FrogPilotVisualsPanel *frogpilotVisualsPanel = new FrogPilotVisualsPanel(this);
  QObject::connect(frogpilotVisualsPanel, &FrogPilotVisualsPanel::openParentToggle, this, &FrogPilotSettingsWindow::openParentToggle);

  std::vector<std::pair<QString, std::vector<QWidget*>>> panels = {
    {tr("Alerts and Sounds"), {frogpilotSoundsPanel}},
    {tr("Driving Controls"), {frogpilotModelPanel, frogpilotLongitudinalPanel, frogpilotLateralPanel}},
    {tr("Navigation"), {frogpilotMapsPanel, frogpilotPrimelessPanel}},
    {tr("System Management"), {new FrogPilotDataPanel(this), frogpilotDevicePanel, new UtilitiesPanel(this)}},
    {tr("Theme and Appearance"), {frogpilotVisualsPanel, frogpilotThemesPanel}},
    {tr("Vehicle Controls"), {new FrogPilotVehiclesPanel(this)}}
  };

  std::vector<QString> icons = {
    "../frogpilot/assets/toggle_icons/icon_sound.png",
    "../frogpilot/assets/toggle_icons/icon_steering.png",
    "../frogpilot/assets/toggle_icons/icon_map.png",
    "../frogpilot/assets/toggle_icons/icon_system.png",
    "../frogpilot/assets/toggle_icons/icon_display.png",
    "../frogpilot/assets/toggle_icons/icon_vehicle.png",
  };

  std::vector<QString> descriptions = {
    tr("Options to customize FrogPilot's sound alerts and notifications."),
    tr("FrogPilot features that impact acceleration, braking, and steering."),
    tr("Map data downloader and 'Navigate On openpilot (NOO)' settings."),
    tr("Tools and system utilities used to maintain and troubleshoot FrogPilot."),
    tr("Options for customizing FrogPilot's themes, UI appearance, and onroad widgets."),
    tr("Vehicle-specific settings and configurations for supported makes and models.")
  };

  std::vector<std::vector<QString>> buttonLabels = {
    {tr("MANAGE")},
    {tr("DRIVING MODEL"), tr("GAS / BRAKE"), tr("STEERING")},
    {tr("MAP DATA"), tr("PRIMELESS NAVIGATION")},
    {tr("DATA"), tr("DEVICE"), tr("UTILITIES")},
    {tr("APPEARANCE"), tr("THEME")},
    {tr("MANAGE")}
  };

  for (size_t i = 0; i < panels.size(); ++i) {
    addPanelControl(list, panels[i].first, descriptions[i], buttonLabels[i], icons[i], panels[i].second, panels[i].first);
  }

  frogpilotSettingsLayout->addWidget(new ScrollView(list, frogpilotSettingsWidget));
  frogpilotSettingsLayout->addStretch(1);

  mainLayout->addWidget(frogpilotSettingsWidget);
  mainLayout->setCurrentWidget(frogpilotSettingsWidget);

  QObject::connect(parent, &SettingsWindow::closeMapBoxInstructions, this, &FrogPilotSettingsWindow::closeMapBoxInstructions);
  QObject::connect(parent, &SettingsWindow::closeMapSelection, this, &FrogPilotSettingsWindow::closeMapSelection);
  QObject::connect(parent, &SettingsWindow::closePanel, this, &FrogPilotSettingsWindow::closePanel);
  QObject::connect(parent, &SettingsWindow::closePanel, this, &updateFrogPilotToggles);
  QObject::connect(parent, &SettingsWindow::closeParentToggle, this, &FrogPilotSettingsWindow::closeParentToggle);
  QObject::connect(parent, &SettingsWindow::closeSubParentToggle, this, &FrogPilotSettingsWindow::closeSubParentToggle);
  QObject::connect(parent, &SettingsWindow::updateMetric, this, &FrogPilotSettingsWindow::updateMetric);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &FrogPilotSettingsWindow::updateVariables);

  frogpilotToggleLevels = QJsonDocument::fromJson(QString::fromStdString(params_memory.get("FrogPilotTuningLevels", true)).toUtf8()).object();
  tuningLevel = params.getInt("TuningLevel");

  closeParentToggle();
}

void FrogPilotSettingsWindow::showEvent(QShowEvent *event) {
  updateVariables();
  mainLayout->setCurrentWidget(frogpilotSettingsWidget);
}

void FrogPilotSettingsWindow::closePanel() {
  QWidget *currentWidget = mainLayout->currentWidget();
  if (currentWidget != frogpilotSettingsWidget) {
    mainLayout->removeWidget(currentWidget);
  }
}

void FrogPilotSettingsWindow::updateVariables() {
  std::string carParams = params.get("CarParamsPersistent");
  if (!carParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(carParams.data(), carParams.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();
    cereal::CarParams::SafetyModel safetyModel = CP.getSafetyConfigs()[0].getSafetyModel();

    std::string carFingerprint = CP.getCarFingerprint();
    std::string carModel = CP.getCarName();

    hasAutoTune = (carModel == "hyundai" || carModel == "toyota") && CP.getLateralTuning().which() == cereal::CarParams::LateralTuning::TORQUE;
    hasBSM = CP.getEnableBsm();
    hasDashSpeedLimits = carModel == "hyundai" || carModel == "toyota";
    hasExperimentalOpenpilotLongitudinal = CP.getExperimentalLongitudinalAvailable();
    hasNNFFLog = checkNNFFLogFileExists(carFingerprint);
    hasOpenpilotLongitudinal = hasLongitudinalControl(CP);
    hasPCMCruise = CP.getPcmCruise();
    hasRadar = !CP.getRadarUnavailable();
    hasSNG = CP.getMinEnableSpeed() <= 0;
    isBolt = carFingerprint == "CHEVROLET_BOLT_CC" || carFingerprint == "CHEVROLET_BOLT_EUV";
    isGM = carModel == "gm";
    isHKGCanFd = carModel == "hyundai" && safetyModel == cereal::CarParams::SafetyModel::HYUNDAI_CANFD;
    isImpreza = carFingerprint == "SUBARU_IMPREZA";
    isPIDCar = CP.getLateralTuning().which() == cereal::CarParams::LateralTuning::PID;
    isSubaru = carModel == "subaru";
    isToyota = carModel == "toyota";
    isVolt = carFingerprint == "CHEVROLET_VOLT";
    forcingAutoTune = params.getBool("AdvancedLateralTune") && params.getBool("ForceAutoTune");
    steerFrictionStock = CP.getLateralTuning().getTorque().getFriction();
    steerKPStock = CP.getLateralTuning().getTorque().getKp();
    steerLatAccelStock = CP.getLateralTuning().getTorque().getLatAccelFactor();
    steerRatioStock = CP.getSteerRatio();

    float currentFrictionStock = params.getFloat("SteerFrictionStock");
    float currentKPStock = params.getFloat("SteerKPStock");
    float currentLatAccelStock = params.getFloat("SteerLatAccelStock");
    float currentRatioStock = params.getFloat("SteerRatioStock");

    if (currentFrictionStock != steerFrictionStock && steerFrictionStock != 0) {
      if (params.getFloat("SteerFriction") == currentFrictionStock || currentFrictionStock == 0) {
        params.putFloat("SteerFriction", steerFrictionStock);
      }
      params.putFloat("SteerFrictionStock", steerFrictionStock);
    }

    if (currentKPStock != steerKPStock && currentKPStock != 0) {
      if (params.getFloat("SteerKP") == currentKPStock || currentKPStock == 0) {
        params.putFloat("SteerKP", steerKPStock);
      }
      params.putFloat("SteerKPStock", steerKPStock);
    }

    if (currentLatAccelStock != steerLatAccelStock && steerLatAccelStock != 0) {
      if (params.getFloat("SteerLatAccel") == steerLatAccelStock || steerLatAccelStock == 0) {
        params.putFloat("SteerLatAccel", steerLatAccelStock);
      }
      params.putFloat("SteerLatAccelStock", steerLatAccelStock);
    }

    if (currentRatioStock != steerRatioStock && steerRatioStock != 0) {
      if (params.getFloat("SteerRatio") == steerRatioStock || steerRatioStock == 0) {
        params.putFloat("SteerRatio", steerRatioStock);
      }
      params.putFloat("SteerRatioStock", steerRatioStock);
    }

    if (params.checkKey("LiveTorqueParameters")) {
      std::string torqueParams = params.get("LiveTorqueParameters");
      if (!torqueParams.empty()) {
        capnp::FlatArrayMessageReader cmsgtp(aligned_buf.align(torqueParams.data(), torqueParams.size()));
        cereal::Event::Reader LTP = cmsgtp.getRoot<cereal::Event>();

        cereal::LiveTorqueParametersData::Reader liveTorqueParams = LTP.getLiveTorqueParameters();

        liveValid = liveTorqueParams.getLiveValid();
      } else {
        liveValid = false;
      }
    } else {
      liveValid = false;
    }
  }

  drivingButton->setVisible(hasOpenpilotLongitudinal || tuningLevel >= frogpilotToggleLevels.value("Model").toDouble());
  drivingButton->setVisibleButton(0, tuningLevel >= frogpilotToggleLevels.value("Model").toDouble());
  drivingButton->setVisibleButton(1, hasOpenpilotLongitudinal);

  navigationButton->setVisibleButton(1, !uiState()->hasPrime());

  systemButton->setVisibleButton(1, tuningLevel >= frogpilotToggleLevels.value("DeviceManagement").toDouble() || tuningLevel >= frogpilotToggleLevels.value("ScreenManagement").toDouble());

  update();
}

void FrogPilotSettingsWindow::addPanelControl(FrogPilotListWidget *list, QString &title, QString &desc, std::vector<QString> &button_labels, QString &icon, std::vector<QWidget*> &panels, QString &currentPanel) {
  std::vector<QWidget*> panelContainers;
  panelContainers.reserve(panels.size());

  for (QWidget *panel : panels) {
    QWidget *panelContainer = new QWidget(this);
    QVBoxLayout *panelLayout = new QVBoxLayout(panelContainer);
    panelLayout->setContentsMargins(50, 25, 50, 25);
    panelLayout->addWidget(panel);
    panelContainers.push_back(panelContainer);
  }

  FrogPilotButtonsControl *button;
  if (currentPanel == tr("Driving Controls")) {
    drivingButton = new FrogPilotButtonsControl(title, desc, button_labels, false, true, icon);
    button = drivingButton;
  } else if (currentPanel == tr("Navigation")) {
    navigationButton = new FrogPilotButtonsControl(title, desc, button_labels, false, true, icon);
    button = navigationButton;
  } else if (currentPanel == tr("System Management")) {
    systemButton = new FrogPilotButtonsControl(title, desc, button_labels, false, true, icon);
    button = systemButton;
  } else {
    button = new FrogPilotButtonsControl(title, desc, button_labels, false, true, icon);
  }

  QObject::connect(button, &FrogPilotButtonsControl::buttonClicked, [this, panelContainers](int buttonId) {
    if (buttonId < panelContainers.size()) {
      QWidget *selectedPanel = panelContainers[buttonId];
      if (mainLayout->indexOf(selectedPanel) == -1) {
        mainLayout->addWidget(selectedPanel);
      }
      mainLayout->setCurrentWidget(selectedPanel);
    }
    openPanel();
  });

  list->addItem(button);
}
