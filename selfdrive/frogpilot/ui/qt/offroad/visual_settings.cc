#include "selfdrive/frogpilot/ui/qt/offroad/visual_settings.h"

FrogPilotVisualsPanel::FrogPilotVisualsPanel(SettingsWindow *parent) : FrogPilotListWidget(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> visualToggles {
    {"AlertVolumeControl", tr("Alert Volume Controller"), tr("Control the volume level for each individual sound in openpilot."), "../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"DisengageVolume", tr("Disengage Volume"), tr("Related alerts:\n\nAdaptive Cruise Disabled\nParking Brake Engaged\nBrake Pedal Pressed\nSpeed too Low"), ""},
    {"EngageVolume", tr("Engage Volume"), tr("Related alerts:\n\nNNFF Torque Controller loaded\nopenpilot engaged"), ""},
    {"PromptVolume", tr("Prompt Volume"), tr("Related alerts:\n\nCar Detected in Blindspot\nSpeed too Low\nSteer Unavailable Below 'X'\nTake Control, Turn Exceeds Steering Limit"), ""},
    {"PromptDistractedVolume", tr("Prompt Distracted Volume"), tr("Related alerts:\n\nPay Attention, Driver Distracted\nTouch Steering Wheel, Driver Unresponsive"), ""},
    {"RefuseVolume", tr("Refuse Volume"), tr("Related alerts:\n\nopenpilot Unavailable"), ""},
    {"WarningSoftVolume", tr("Warning Soft Volume"), tr("Related alerts:\n\nBRAKE!, Risk of Collision\nTAKE CONTROL IMMEDIATELY"), ""},
    {"WarningImmediateVolume", tr("Warning Immediate Volume"), tr("Related alerts:\n\nDISENGAGE IMMEDIATELY, Driver Distracted\nDISENGAGE IMMEDIATELY, Driver Unresponsive"), ""},

    {"CustomAlerts", tr("Custom Alerts"), tr("Enable custom alerts for openpilot events."), "../frogpilot/assets/toggle_icons/icon_green_light.png"},

    {"CustomUI", tr("Custom Onroad UI"), tr("Customize the Onroad UI."), "../assets/offroad/icon_road.png"},
    {"CustomPaths", tr("Paths"), tr("Show your projected acceleration on the driving path, detected adjacent lanes, or when a vehicle is detected in your blindspot."), ""},

    {"CustomTheme", tr("Custom Themes"), tr("Enable the ability to use custom themes."), "../frogpilot/assets/wheel_images/frog.png"},
    {"CustomColors", tr("Color Theme"), tr("Switch out the standard openpilot color scheme with themed colors.\n\nWant to submit your own color scheme? Post it in the 'feature-request' channel in the FrogPilot Discord!"), ""},
    {"CustomIcons", tr("Icon Pack"), tr("Switch out the standard openpilot icons with a set of themed icons.\n\nWant to submit your own icon pack? Post it in the 'feature-request' channel in the FrogPilot Discord!"), ""},
    {"CustomSignals", tr("Turn Signals"), tr("Add themed animation for your turn signals.\n\nWant to submit your own turn signal animation? Post it in the 'feature-request' channel in the FrogPilot Discord!"), ""},
    {"CustomSounds", tr("Sound Pack"), tr("Switch out the standard openpilot sounds with a set of themed sounds.\n\nWant to submit your own sound pack? Post it in the 'feature-request' channel in the FrogPilot Discord!"), ""},

    {"ModelUI", tr("Model UI"), tr("Customize the model visualizations on the screen."), "../assets/offroad/icon_calibration.png"},
    {"DynamicPathWidth", tr("Dynamic Path Width"), tr("Have the path width dynamically adjust based on the current engagement state of openpilot."), ""},
    {"HideLeadMarker", tr("Hide Lead Marker"), tr("Hide the lead marker from the onroad UI."), ""},
    {"LaneLinesWidth", tr("Lane Lines"), tr("Adjust the visual thickness of lane lines on your display.\n\nDefault matches the MUTCD average of 4 inches."), ""},
    {"PathEdgeWidth", tr("Path Edges"), tr("Adjust the width of the path edges shown on your UI to represent different driving modes and statuses.\n\nDefault is 20% of the total path.\n\nBlue = Navigation\nLight Blue = 'Always On Lateral'\nGreen = Default\nOrange = 'Experimental Mode'\nRed = 'Traffic Mode'\nYellow = 'Conditional Experimental Mode' Overriden"), ""},
    {"PathWidth", tr("Path Width"), tr("Customize the width of the driving path shown on your UI.\n\nDefault matches the width of a 2019 Lexus ES 350."), ""},
    {"RoadEdgesWidth", tr("Road Edges"), tr("Adjust the visual thickness of road edges on your display.\n\nDefault is 1/2 of the MUTCD average lane line width of 4 inches."), ""},
    {"UnlimitedLength", tr("'Unlimited' Road UI Length"), tr("Extend the display of the path, lane lines, and road edges out as far as the model can see."), ""},

    {"QOLVisuals", tr("Quality of Life"), tr("Miscellaneous quality of life changes to improve your overall openpilot experience."), "../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"CameraView", tr("Camera View"), tr("Choose your preferred camera view for the onroad UI. This is purely a visual change and doesn't impact how openpilot drives."), ""},
  };

  for (const auto &[param, title, desc, icon] : visualToggles) {
    ParamControl *toggle;

    if (param == "AlertVolumeControl") {
      FrogPilotParamManageControl *alertVolumeControlToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(alertVolumeControlToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(alertVolumeControlKeys.find(key.c_str()) != alertVolumeControlKeys.end());
        }
      });
      toggle = alertVolumeControlToggle;
    } else if (alertVolumeControlKeys.find(param) != alertVolumeControlKeys.end()) {
      if (param == "WarningImmediateVolume") {
        toggle = new FrogPilotParamValueControl(param, title, desc, icon, 25, 100, std::map<int, QString>(), this, false, "%");
      } else {
        toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, std::map<int, QString>(), this, false, "%");
      }

    } else if (param == "CustomAlerts") {
      FrogPilotParamManageControl *customAlertsToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(customAlertsToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedCustomAlertsKeys = customAlertsKeys;

          toggle->setVisible(modifiedCustomAlertsKeys.find(key.c_str()) != modifiedCustomAlertsKeys.end());
        }
      });
      toggle = customAlertsToggle;

    } else if (param == "CustomTheme") {
      FrogPilotParamManageControl *customThemeToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(customThemeToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(customThemeKeys.find(key.c_str()) != customThemeKeys.end());
        }
      });
      toggle = customThemeToggle;
    } else if (param == "CustomColors" || param == "CustomIcons" || param == "CustomSignals" || param == "CustomSounds") {
      std::vector<QString> themeOptions{tr("Stock"), tr("Frog"), tr("Tesla"), tr("Stalin")};
      FrogPilotButtonParamControl *themeSelection = new FrogPilotButtonParamControl(param, title, desc, icon, themeOptions);
      toggle = themeSelection;

      if (param == "CustomSounds") {
        QObject::connect(static_cast<FrogPilotButtonParamControl*>(toggle), &FrogPilotButtonParamControl::buttonClicked, [this](int id) {
          if (id == 1) {
            if (FrogPilotConfirmationDialog::yesorno(tr("Do you want to enable the bonus 'Goat' sound effect?"), this)) {
              params.putBoolNonBlocking("GoatScream", true);
            } else {
              params.putBoolNonBlocking("GoatScream", false);
            }
          }
        });
      }

    } else if (param == "CustomUI") {
      FrogPilotParamManageControl *customUIToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(customUIToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedCustomOnroadUIKeys = customOnroadUIKeys;

          toggle->setVisible(modifiedCustomOnroadUIKeys.find(key.c_str()) != modifiedCustomOnroadUIKeys.end());
        }
      });
      toggle = customUIToggle;
    } else if (param == "CustomPaths") {
      std::vector<QString> pathToggles{"AccelerationPath", "BlindSpotPath"};
      std::vector<QString> pathToggleNames{tr("Acceleration"), tr("Blind Spot")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, pathToggles, pathToggleNames);

    } else if (param == "ModelUI") {
      FrogPilotParamManageControl *modelUIToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(modelUIToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedModelUIKeysKeys = modelUIKeys;

          if (!hasOpenpilotLongitudinal) {
            modifiedModelUIKeysKeys.erase("HideLeadMarker");
          }

          toggle->setVisible(modifiedModelUIKeysKeys.find(key.c_str()) != modifiedModelUIKeysKeys.end());
        }
      });
      toggle = modelUIToggle;
    } else if (param == "LaneLinesWidth" || param == "RoadEdgesWidth") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 24, std::map<int, QString>(), this, false, tr(" inches"));
    } else if (param == "PathEdgeWidth") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, std::map<int, QString>(), this, false, tr("%"));
    } else if (param == "PathWidth") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, std::map<int, QString>(), this, false, tr(" feet"), 10);

    } else if (param == "QOLVisuals") {
      FrogPilotParamManageControl *qolToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(qolToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(qolKeys.find(key.c_str()) != qolKeys.end());
        }
      });
      toggle = qolToggle;
    } else if (param == "CameraView") {
      std::vector<QString> cameraOptions{tr("Auto"), tr("Driver"), tr("Standard"), tr("Wide")};
      FrogPilotButtonParamControl *preferredCamera = new FrogPilotButtonParamControl(param, title, desc, icon, cameraOptions);
      toggle = preferredCamera;

    } else {
      toggle = new ParamControl(param, title, desc, icon, this);
    }

    addItem(toggle);
    toggles[param.toStdString()] = toggle;

    QObject::connect(toggle, &ToggleControl::toggleFlipped, [this]() {
      updateToggles();
    });

    QObject::connect(static_cast<FrogPilotButtonParamControl*>(toggle), &FrogPilotButtonParamControl::buttonClicked, [this]() {
      updateToggles();
    });

    QObject::connect(static_cast<FrogPilotParamValueControl*>(toggle), &FrogPilotParamValueControl::valueChanged, [this]() {
      updateToggles();
    });

    QObject::connect(toggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });

    QObject::connect(static_cast<FrogPilotParamManageControl*>(toggle), &FrogPilotParamManageControl::manageButtonClicked, [this]() {
      update();
    });
  }

  QObject::connect(parent, &SettingsWindow::closeParentToggle, this, &FrogPilotVisualsPanel::hideToggles);
  QObject::connect(parent, &SettingsWindow::updateMetric, this, &FrogPilotVisualsPanel::updateMetric);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &FrogPilotVisualsPanel::updateCarToggles);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotVisualsPanel::updateState);

  updateMetric();
}

void FrogPilotVisualsPanel::updateState(const UIState &s) {
  if (!isVisible()) return;

  started = s.scene.started;
}

void FrogPilotVisualsPanel::updateToggles() {
  std::thread([this]() {
    paramsMemory.putBool("FrogPilotTogglesUpdated", true);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    paramsMemory.putBool("FrogPilotTogglesUpdated", false);
  }).detach();
}

void FrogPilotVisualsPanel::updateCarToggles() {
  auto carParams = params.get("CarParamsPersistent");
  if (!carParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(carParams.data(), carParams.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();

    hasBSM = CP.getEnableBsm();
    hasOpenpilotLongitudinal = CP.getOpenpilotLongitudinalControl();
  } else {
    hasBSM = true;
    hasOpenpilotLongitudinal = true;
  }

  hideToggles();
}

void FrogPilotVisualsPanel::updateMetric() {
  bool previousIsMetric = isMetric;
  isMetric = params.getBool("IsMetric");

  if (isMetric != previousIsMetric) {
    double distanceConversion = isMetric ? INCH_TO_CM : CM_TO_INCH;
    double speedConversion = isMetric ? FOOT_TO_METER : METER_TO_FOOT;
    params.putIntNonBlocking("LaneLinesWidth", std::nearbyint(params.getInt("LaneLinesWidth") * distanceConversion));
    params.putIntNonBlocking("RoadEdgesWidth", std::nearbyint(params.getInt("RoadEdgesWidth") * distanceConversion));
    params.putIntNonBlocking("PathWidth", std::nearbyint(params.getInt("PathWidth") * speedConversion));
  }

  FrogPilotParamValueControl *laneLinesWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["LaneLinesWidth"]);
  FrogPilotParamValueControl *roadEdgesWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["RoadEdgesWidth"]);
  FrogPilotParamValueControl *pathWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["PathWidth"]);

  if (isMetric) {
    laneLinesWidthToggle->setDescription(tr("Customize the lane line width.\n\nDefault matches the Vienna average of 10 centimeters."));
    roadEdgesWidthToggle->setDescription(tr("Customize the road edges width.\n\nDefault is 1/2 of the Vienna average lane line width of 10 centimeters."));

    laneLinesWidthToggle->updateControl(0, 60, tr(" centimeters"));
    roadEdgesWidthToggle->updateControl(0, 60, tr(" centimeters"));
    pathWidthToggle->updateControl(0, 30, tr(" meters"), 10);
  } else {
    laneLinesWidthToggle->setDescription(tr("Customize the lane line width.\n\nDefault matches the MUTCD average of 4 inches."));
    roadEdgesWidthToggle->setDescription(tr("Customize the road edges width.\n\nDefault is 1/2 of the MUTCD average lane line width of 4 inches."));

    laneLinesWidthToggle->updateControl(0, 24, tr(" inches"));
    roadEdgesWidthToggle->updateControl(0, 24, tr(" inches"));
    pathWidthToggle->updateControl(0, 100, tr(" feet"), 10);
  }

  laneLinesWidthToggle->refresh();
  roadEdgesWidthToggle->refresh();
}

void FrogPilotVisualsPanel::hideToggles() {
  for (auto &[key, toggle] : toggles) {
    bool subToggles = alertVolumeControlKeys.find(key.c_str()) != alertVolumeControlKeys.end() ||
                      customAlertsKeys.find(key.c_str()) != customAlertsKeys.end() ||
                      customOnroadUIKeys.find(key.c_str()) != customOnroadUIKeys.end() ||
                      customThemeKeys.find(key.c_str()) != customThemeKeys.end() ||
                      modelUIKeys.find(key.c_str()) != modelUIKeys.end() ||
                      qolKeys.find(key.c_str()) != qolKeys.end() ||
                      screenKeys.find(key.c_str()) != screenKeys.end();
    toggle->setVisible(!subToggles);
  }

  update();
}
