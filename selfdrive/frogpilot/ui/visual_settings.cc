#include "selfdrive/frogpilot/ui/visual_settings.h"

FrogPilotVisualsPanel::FrogPilotVisualsPanel(SettingsWindow *parent) : FrogPilotListWidget(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> visualToggles {
    {"CustomTheme", "Custom Themes", "Enable the ability to use custom themes.", "../frogpilot/assets/wheel_images/frog.png"},
    {"HolidayThemes", "Holiday Themes", "The openpilot theme changes according to the current/upcoming holiday. Minor holidays last a day, major holidays (Easter, Christmas, Halloween, etc.) last a week.", ""},
    {"CustomColors", "Color Theme", "Switch out the standard openpilot color scheme with a custom color scheme.\n\nWant to submit your own color scheme? Post it in the 'feature-request' channel in the FrogPilot Discord!", ""},
    {"CustomIcons", "Icon Pack", "Switch out the standard openpilot icons with a set of custom icons.\n\nWant to submit your own icon pack? Post it in the 'feature-request' channel in the FrogPilot Discord!", ""},
    {"CustomSignals", "Turn Signals", "Add custom animations for your turn signals for a personal touch!\n\nWant to submit your own turn signal animation? Post it in the 'feature-request' channel in the FrogPilot Discord!", ""},
    {"CustomSounds", "Sound Pack", "Switch out the standard openpilot sounds with a set of custom sounds.\n\nWant to submit your own sound pack? Post it in the 'feature-request' channel in the FrogPilot Discord!", ""},

    {"AlertVolumeControl", "Alert Volume Control", "Control the volume level for each individual sound in openpilot.", "../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"DisengageVolume", "Disengage Volume", "Related alerts:\n\nAdaptive Cruise Disabled\nParking Brake Engaged\nPedal Pressed\nSpeed too Low", ""},
    {"EngageVolume", "Engage Volume", "Related alerts:\n\nNNFF Torque Controller loaded", ""},
    {"PromptVolume", "Prompt Volume", "Related alerts:\n\nCar Detected in Blindspot\nLight turned green\nSpeed too Low\nSteer Unavailable Below 'X'\nTake Control, Turn Exceeds Steering Limit", ""},
    {"PromptDistractedVolume", "Prompt Distracted Volume", "Related alerts:\n\nPay Attention, Driver Distracted\nTouch Steering Wheel, Driver Unresponsive", ""},
    {"RefuseVolume", "Refuse Volume", "Related alerts:\n\nopenpilot Unavailable", ""},
    {"WarningSoftVolume", "Warning Soft Volume", "Related alerts:\n\nBRAKE!, Risk of Collision\nTAKE CONTROL IMMEDIATELY", ""},
    {"WarningImmediateVolume", "Warning Immediate Volume", "Related alerts:\n\nDISENGAGE IMMEDIATELY, Driver Distracted\nDISENGAGE IMMEDIATELY, Driver Unresponsive", ""},

    {"CameraView", "Camera View", "Choose your preferred camera view for the onroad UI. This is a visual change only and doesn't impact openpilot.", "../frogpilot/assets/toggle_icons/icon_camera.png"},
    {"Compass", "Compass", "Add a compass to your onroad UI.", "../frogpilot/assets/toggle_icons/icon_compass.png"},

    {"CustomAlerts", "Custom Alerts", "Enable custom alerts for various logic or situational changes.", "../frogpilot/assets/toggle_icons/icon_green_light.png"},
    {"GreenLightAlert", "Green Light Alert", "Get an alert when a traffic light changes from red to green.", ""},
    {"LeadDepartingAlert", "Lead Departing Alert", "Get an alert when your lead vehicle starts departing when you're at a standstill.", ""},
    {"LoudBlindspotAlert", "Loud Blindspot Alert", "Enable a louder alert for when a vehicle is detected in your blindspot when attempting to change lanes.", ""},

    {"CustomUI", "Custom Onroad UI", "Customize the Onroad UI with some additional visual functions.", "../assets/offroad/icon_road.png"},
    {"AccelerationPath", "Acceleration Path", "Visualize the car's intended acceleration or deceleration with a color-coded path.", ""},
    {"AdjacentPath", "Adjacent Paths", "Display paths to the left and right of your car, visualizing where the model detects lanes.", ""},
    {"BlindSpotPath", "Blind Spot Path", "Visualize your blind spots with a red path when another vehicle is detected nearby.", ""},
    {"FPSCounter", "FPS Counter", "Display the Frames Per Second (FPS) of your onroad UI for monitoring system performance.", ""},
    {"LeadInfo", "Lead Info and Logics", "Get detailed information about the vehicle ahead, including speed and distance, and the logic behind your following distance.", ""},
    {"PedalsOnUI", "Pedals Being Pressed", "Display which pedals are being pressed on the onroad UI below the steering wheel icon.", ""},
    {"RoadNameUI", "Road Name", "See the name of the road you're on at the bottom of your screen. Sourced from OpenStreetMap.", ""},

    {"DriverCamera", "Driver Camera On Reverse", "Show the driver's camera feed when you shift to reverse.", "../assets/img_driver_face_static.png"},

    {"ModelUI", "Model UI", "Personalize how the model's visualizations appear on your screen.", "../assets/offroad/icon_calibration.png"},
    {"DynamicPathWidth", "Dynamic Path Width", "Have the path width dynamically adjust based on the current engagement state of openpilot.", ""},
    {"LaneLinesWidth", "Lane Lines", "Adjust the visual thickness of lane lines on your display.\n\nDefault matches the MUTCD average of 4 inches.", ""},
    {"PathEdgeWidth", "Path Edges", "Adjust the width of the path edges shown on your UI to represent different driving modes and statuses.\n\nDefault is 20% of the total path.\n\nBlue = Navigation\nLight Blue = Always On Lateral\nGreen = Default with 'FrogPilot Colors'\nLight Green = Default with stock colors\nOrange = Experimental Mode Active\nYellow = Conditional Overriden", ""},
    {"PathWidth", "Path Width", "Customize the width of the driving path shown on your UI.\n\nDefault matches the width of a 2019 Lexus ES 350.", ""},
    {"RoadEdgesWidth", "Road Edges", "Adjust the visual thickness of road edges on your display.\n\nDefault is 1/2 of the MUTCD average lane line width of 4 inches.", ""},
    {"UnlimitedLength", "'Unlimited' Road UI Length", "Extend the display of the path, lane lines, and road edges as far as the system can detect, providing a more expansive view of the road ahead.", ""},

    {"NumericalTemp", "Numerical Temperature Gauge", "Replace the 'GOOD', 'OK', and 'HIGH' temperature statuses with a numerical temperature gauge based on the highest temperature between the memory, CPU, and GPU.", "../frogpilot/assets/toggle_icons/icon_temperature.png"},

    {"QOLVisuals", "Quality of Life", "Miscellaneous quality of life changes to improve your overall openpilot experience.", "../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"DriveStats", "Drive Stats In Home Screen", "Display your device's drive stats in the home screen.", ""},
    {"FullMap", "Full Sized Map", "Maximize the size of the map in the onroad UI.", ""},
    {"HideSpeed", "Hide Speed", "Hide the speed indicator in the onroad UI. Additional toggle allows it to be hidden/shown via tapping the speed itself.", ""},

    {"ScreenBrightness", "Screen Brightness", "Customize your screen brightness.", "../frogpilot/assets/toggle_icons/icon_light.png"},
  };

  for (const auto &[param, title, desc, icon] : visualToggles) {
    ParamControl *toggle;

    if (param == "AlertVolumeControl") {
      FrogPilotParamManageControl *alertVolumeControlToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(alertVolumeControlToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        parentToggleClicked();
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

    } else if (param == "CameraView") {
      std::vector<QString> cameraOptions{tr("Auto"), tr("Standard"), tr("Wide"), tr("Driver")};
      FrogPilotButtonParamControl *preferredCamera = new FrogPilotButtonParamControl(param, title, desc, icon, cameraOptions);
      toggle = preferredCamera;

    } else if (param == "CustomAlerts") {
      FrogPilotParamManageControl *customAlertsToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(customAlertsToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        parentToggleClicked();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(customAlertsKeys .find(key.c_str()) != customAlertsKeys .end());
        }
      });
      toggle = customAlertsToggle;

    } else if (param == "CustomTheme") {
      FrogPilotParamManageControl *customThemeToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(customThemeToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        parentToggleClicked();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(customThemeKeys.find(key.c_str()) != customThemeKeys.end());
        }
      });
      toggle = customThemeToggle;
    } else if (customThemeKeys.find(param) != customThemeKeys.end() && param != "HolidayThemes") {
      std::vector<QString> themeOptions{tr("Stock"), tr("Frog"), tr("Tesla"), tr("Stalin")};
      FrogPilotButtonParamControl *themeSelection = new FrogPilotButtonParamControl(param, title, desc, icon, themeOptions);
      toggle = themeSelection;

      if (param == "CustomSounds") {
        QObject::connect(static_cast<FrogPilotButtonParamControl*>(toggle), &FrogPilotButtonParamControl::buttonClicked, [this](int id) {
          if (id == 1) {
            if (FrogPilotConfirmationDialog::yesorno("Do you want to enable the bonus 'Goat' sound effect?", this)) {
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
        parentToggleClicked();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(customOnroadUIKeys.find(key.c_str()) != customOnroadUIKeys.end());
        }
      });
      toggle = customUIToggle;
    } else if (param == "LeadInfo") {
      std::vector<QString> leadInfoToggles{"UseSI"};
      std::vector<QString> leadInfoToggleNames{tr("Use SI Values")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, leadInfoToggles, leadInfoToggleNames);
    } else if (param == "AdjacentPath") {
      std::vector<QString> adjacentPathToggles{"AdjacentPathMetrics"};
      std::vector<QString> adjacentPathToggleNames{tr("Display Metrics")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, adjacentPathToggles, adjacentPathToggleNames);

    } else if (param == "ModelUI") {
      FrogPilotParamManageControl *modelUIToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(modelUIToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        parentToggleClicked();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(modelUIKeys.find(key.c_str()) != modelUIKeys.end());
        }
      });
      toggle = modelUIToggle;
    } else if (param == "LaneLinesWidth" || param == "RoadEdgesWidth") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 24, std::map<int, QString>(), this, false, " inches");
    } else if (param == "PathEdgeWidth") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, std::map<int, QString>(), this, false, "%");
    } else if (param == "PathWidth") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, std::map<int, QString>(), this, false, " feet", 10);

    } else if (param == "NumericalTemp") {
      std::vector<QString> temperatureToggles{"Fahrenheit"};
      std::vector<QString> temperatureToggleNames{tr("Fahrenheit")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, temperatureToggles, temperatureToggleNames);

    } else if (param == "QOLVisuals") {
      FrogPilotParamManageControl *qolToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(qolToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        parentToggleClicked();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(qolKeys.find(key.c_str()) != qolKeys.end());
        }
      });
      toggle = qolToggle;
    } else if (param == "HideSpeed") {
      std::vector<QString> hideSpeedToggles{"HideSpeedUI"};
      std::vector<QString> hideSpeedToggleNames{tr("Control Via UI")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, hideSpeedToggles, hideSpeedToggleNames);

    } else if (param == "ScreenBrightness") {
      std::map<int, QString> brightnessLabels;
      for (int i = 0; i <= 101; ++i) {
        brightnessLabels[i] = i == 0 ? "Screen Off" : i == 101 ? "Auto" : QString::number(i) + "%";
      }
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 101, brightnessLabels, this, false);

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

  std::set<std::string> rebootKeys = {"DriveStats"};
  for (const std::string &key : rebootKeys) {
    QObject::connect(toggles[key], &ToggleControl::toggleFlipped, [this, key]() {
      if (started || key == "DriveStats") {
        if (FrogPilotConfirmationDialog::toggle("Reboot required to take effect.", "Reboot Now", this)) {
          Hardware::soft_reboot();
        }
      }
    });
  }

  QObject::connect(device(), &Device::interactiveTimeout, this, &FrogPilotVisualsPanel::hideSubToggles);
  QObject::connect(parent, &SettingsWindow::closeParentToggle, this, &FrogPilotVisualsPanel::hideSubToggles);
  QObject::connect(parent, &SettingsWindow::closeSubParentToggle, this, &FrogPilotVisualsPanel::hideSubSubToggles);
  QObject::connect(parent, &SettingsWindow::updateMetric, this, &FrogPilotVisualsPanel::updateMetric);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotVisualsPanel::updateState);

  hideSubToggles();
  updateMetric();
}

void FrogPilotVisualsPanel::updateState(const UIState &s) {
  started = s.scene.started;
}

void FrogPilotVisualsPanel::updateToggles() {
  std::thread([this]() {
    paramsMemory.putBool("FrogPilotTogglesUpdated", true);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    paramsMemory.putBool("FrogPilotTogglesUpdated", false);
  }).detach();
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
    laneLinesWidthToggle->setDescription("Customize the lane line width.\n\nDefault matches the Vienna average of 10 centimeters.");
    roadEdgesWidthToggle->setDescription("Customize the road edges width.\n\nDefault is 1/2 of the Vienna average lane line width of 10 centimeters.");

    laneLinesWidthToggle->updateControl(0, 60, " centimeters");
    roadEdgesWidthToggle->updateControl(0, 60, " centimeters");
    pathWidthToggle->updateControl(0, 30, " meters", 10);
  } else {
    laneLinesWidthToggle->setDescription("Customize the lane line width.\n\nDefault matches the MUTCD average of 4 inches.");
    roadEdgesWidthToggle->setDescription("Customize the road edges width.\n\nDefault is 1/2 of the MUTCD average lane line width of 4 inches.");

    laneLinesWidthToggle->updateControl(0, 24, " inches");
    roadEdgesWidthToggle->updateControl(0, 24, " inches");
    pathWidthToggle->updateControl(0, 100, " feet", 10);
  }

  laneLinesWidthToggle->refresh();
  roadEdgesWidthToggle->refresh();

  previousIsMetric = isMetric;
}

void FrogPilotVisualsPanel::parentToggleClicked() {
  openParentToggle();
}

void FrogPilotVisualsPanel::subParentToggleClicked() {
  openSubParentToggle();
}

void FrogPilotVisualsPanel::hideSubToggles() {
  for (auto &[key, toggle] : toggles) {
    bool subToggles = alertVolumeControlKeys.find(key.c_str()) != alertVolumeControlKeys.end() ||
                      customAlertsKeys.find(key.c_str()) != customAlertsKeys.end() ||
                      customOnroadUIKeys.find(key.c_str()) != customOnroadUIKeys.end() ||
                      customThemeKeys.find(key.c_str()) != customThemeKeys.end() ||
                      modelUIKeys.find(key.c_str()) != modelUIKeys.end() ||
                      qolKeys.find(key.c_str()) != qolKeys.end();
    toggle->setVisible(!subToggles);
  }

  closeParentToggle();
}

void FrogPilotVisualsPanel::hideSubSubToggles() {
  for (auto &[key, toggle] : toggles) {
    bool isVisible = false;
    toggle->setVisible(isVisible);
  }

  closeSubParentToggle();
  update();
}

void FrogPilotVisualsPanel::hideEvent(QHideEvent *event) {
  hideSubToggles();
}
