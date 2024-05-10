#include "selfdrive/frogpilot/ui/qt/offroad/visual_settings.h"

FrogPilotVisualsPanel::FrogPilotVisualsPanel(SettingsWindow *parent) : FrogPilotListWidget(parent) {
  std::string branch = params.get("GitBranch");
  isRelease = branch == "FrogPilot";

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
    {"GreenLightAlert", tr("Green Light Alert"), tr("Get an alert when a traffic light changes from red to green."), ""},
    {"LeadDepartingAlert", tr("Lead Departing Alert"), tr("Get an alert when the lead vehicle starts departing when at a standstill."), ""},
    {"LoudBlindspotAlert", tr("Loud Blindspot Alert"), tr("Enable a louder alert for when a vehicle is detected in the blindspot when attempting to change lanes."), ""},

    {"CustomUI", tr("Custom Onroad UI"), tr("Customize the Onroad UI."), "../assets/offroad/icon_road.png"},
    {"Compass", tr("Compass"), tr("Add a compass to the onroad UI."), ""},
    {"CustomPaths", tr("Paths"), tr("Show your projected acceleration on the driving path, detected adjacent lanes, or when a vehicle is detected in your blindspot."), ""},
    {"PedalsOnUI", tr("Pedals Being Pressed"), tr("Display the brake and gas pedals on the onroad UI below the steering wheel icon."), ""},
    {"RoadNameUI", tr("Road Name"), tr("Display the current road's name at the bottom of the screen. Sourced from OpenStreetMap."), ""},
    {"WheelIcon", tr("Steering Wheel Icon"), tr("Replace the default steering wheel icon with a custom icon."), ""},

    {"CustomTheme", tr("Custom Themes"), tr("Enable the ability to use custom themes."), "../frogpilot/assets/wheel_images/frog.png"},
    {"CustomColors", tr("Color Theme"), tr("Switch out the standard openpilot color scheme with themed colors.\n\nWant to submit your own color scheme? Post it in the 'feature-request' channel in the FrogPilot Discord!"), ""},
    {"CustomIcons", tr("Icon Pack"), tr("Switch out the standard openpilot icons with a set of themed icons.\n\nWant to submit your own icon pack? Post it in the 'feature-request' channel in the FrogPilot Discord!"), ""},
    {"CustomSounds", tr("Sound Pack"), tr("Switch out the standard openpilot sounds with a set of themed sounds.\n\nWant to submit your own sound pack? Post it in the 'feature-request' channel in the FrogPilot Discord!"), ""},
    {"CustomSignals", tr("Turn Signals"), tr("Add themed animation for your turn signals.\n\nWant to submit your own turn signal animation? Post it in the 'feature-request' channel in the FrogPilot Discord!"), ""},
    {"HolidayThemes", tr("Holiday Themes"), tr("The openpilot theme changes according to the current/upcoming holiday. Minor holidays last a day, while major holidays (Easter, Christmas, Halloween, etc.) last a week."), ""},
    {"RandomEvents", tr("Random Events"), tr("Enjoy a bit of unpredictability with random events that can occur during certain driving conditions. This is purely cosmetic and has no impact on driving controls!"), ""},

    {"DeveloperUI", tr("Developer UI"), tr("Get various detailed information of what openpilot is doing behind the scenes."), "../frogpilot/assets/toggle_icons/icon_device.png"},
    {"BorderMetrics", tr("Border Metrics"), tr("Display metrics in onroad UI border."), ""},
    {"FPSCounter", tr("FPS Counter"), tr("Display the 'Frames Per Second' (FPS) of your onroad UI for monitoring system performance."), ""},
    {"LateralMetrics", tr("Lateral Metrics"), tr("Display various metrics related to the lateral performance of openpilot."), ""},
    {"LongitudinalMetrics", tr("Longitudinal Metrics"), tr("Display various metrics related to the longitudinal performance of openpilot."), ""},
    {"NumericalTemp", tr("Numerical Temperature Gauge"), tr("Replace the 'GOOD', 'OK', and 'HIGH' temperature statuses with a numerical temperature gauge based on the highest temperature between the memory, CPU, and GPU."), ""},
    {"SidebarMetrics", tr("Sidebar"), tr("Display various custom metrics on the sidebar for the CPU, GPU, RAM, IP, and storage used/left."), ""},
    {"UseSI", tr("Use International System of Units"), tr("Display relevant metrics in the SI format."), ""},

    {"ModelUI", tr("Model UI"), tr("Customize the model visualizations on the screen."), "../assets/offroad/icon_calibration.png"},
    {"DynamicPathWidth", tr("Dynamic Path Width"), tr("Have the path width dynamically adjust based on the current engagement state of openpilot."), ""},
    {"HideLeadMarker", tr("Hide Lead Marker"), tr("Hide the lead marker from the onroad UI."), ""},
    {"LaneLinesWidth", tr("Lane Lines"), tr("Adjust the visual thickness of lane lines on your display.\n\nDefault matches the MUTCD average of 4 inches."), ""},
    {"PathEdgeWidth", tr("Path Edges"), tr("Adjust the width of the path edges shown on your UI to represent different driving modes and statuses.\n\nDefault is 20% of the total path.\n\nBlue = Navigation\nLight Blue = 'Always On Lateral'\nGreen = Default\nOrange = 'Experimental Mode'\nRed = 'Traffic Mode'\nYellow = 'Conditional Experimental Mode' Overriden"), ""},
    {"PathWidth", tr("Path Width"), tr("Customize the width of the driving path shown on your UI.\n\nDefault matches the width of a 2019 Lexus ES 350."), ""},
    {"RoadEdgesWidth", tr("Road Edges"), tr("Adjust the visual thickness of road edges on your display.\n\nDefault is 1/2 of the MUTCD average lane line width of 4 inches."), ""},
    {"UnlimitedLength", tr("'Unlimited' Road UI Length"), tr("Extend the display of the path, lane lines, and road edges out as far as the model can see."), ""},

    {"QOLVisuals", tr("Quality of Life"), tr("Miscellaneous quality of life changes to improve your overall openpilot experience."), "../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"BigMap", tr("Big Map"), tr("Increase the size of the map in the onroad UI."), ""},
    {"CameraView", tr("Camera View"), tr("Choose your preferred camera view for the onroad UI. This is purely a visual change and doesn't impact how openpilot drives."), ""},
    {"DriverCamera", tr("Driver Camera On Reverse"), tr("Show the driver camera feed when in reverse."), ""},
    {"HideSpeed", tr("Hide Speed"), tr("Hide the speed indicator in the onroad UI. Additional toggle allows it to be hidden/shown via tapping the speed itself."), ""},
    {"MapStyle", tr("Map Style"), tr("Select a map style to use with navigation."), ""},
    {"WheelSpeed", tr("Use Wheel Speed"), tr("Use the wheel speed instead of the cluster speed in the onroad UI."), ""},

    {"ScreenManagement", tr("Screen Management"), tr("Manage your screen's brightness, timeout settings, and hide onroad UI elements."), "../frogpilot/assets/toggle_icons/icon_light.png"},
    {"HideUIElements", tr("Hide UI Elements"), tr("Hide the selected UI elements from the onroad screen."), ""},
    {"ScreenBrightness", tr("Screen Brightness"), tr("Customize your screen brightness when offroad."), ""},
    {"ScreenBrightnessOnroad", tr("Screen Brightness (Onroad)"), tr("Customize your screen brightness when onroad."), ""},
    {"ScreenRecorder", tr("Screen Recorder"), tr("Enable the ability to record the screen while onroad."), ""},
    {"ScreenTimeout", tr("Screen Timeout"), tr("Customize how long it takes for your screen to turn off."), ""},
    {"ScreenTimeoutOnroad", tr("Screen Timeout (Onroad)"), tr("Customize how long it takes for your screen to turn off when onroad."), ""},
    {"StandbyMode", tr("Standby Mode"), tr("Turn the screen off after your screen times out when onroad, but wake it back up when engagement state changes or important alerts are triggered."), ""},
  };

  for (const auto &[param, title, desc, icon] : visualToggles) {
    AbstractControl *toggle;

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

          if (!hasBSM) {
            modifiedCustomAlertsKeys.erase("LoudBlindspotAlert");
          }

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

          if (!hasOpenpilotLongitudinal && !hasAutoTune) {
            modifiedCustomOnroadUIKeys.erase("DeveloperUI");
          }

          toggle->setVisible(modifiedCustomOnroadUIKeys.find(key.c_str()) != modifiedCustomOnroadUIKeys.end());
        }
      });
      toggle = customUIToggle;
    } else if (param == "CustomPaths") {
      std::vector<QString> pathToggles{"AccelerationPath", "AdjacentPath", "BlindSpotPath", "AdjacentPathMetrics"};
      std::vector<QString> pathToggleNames{tr("Acceleration"), tr("Adjacent"), tr("Blind Spot"), tr("Metrics")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, pathToggles, pathToggleNames);
    } else if (param == "PedalsOnUI") {
      std::vector<QString> pedalsToggles{"DynamicPedalsOnUI", "StaticPedalsOnUI"};
      std::vector<QString> pedalsToggleNames{tr("Dynamic"), tr("Static")};
      FrogPilotParamToggleControl *pedalsToggle = new FrogPilotParamToggleControl(param, title, desc, icon, pedalsToggles, pedalsToggleNames);
      QObject::connect(pedalsToggle, &FrogPilotParamToggleControl::buttonTypeClicked, this, [this, pedalsToggle](int index) {
        if (index == 0) {
          params.putBool("StaticPedalsOnUI", false);
        } else if (index == 1) {
          params.putBool("DynamicPedalsOnUI", false);
        }

        pedalsToggle->updateButtonStates();
      });
      toggle = pedalsToggle;

    } else if (param == "WheelIcon") {
      std::vector<QString> wheelToggles{"RotatingWheel"};
      std::vector<QString> wheelToggleNames{"Live Rotation"};
      std::map<int, QString> steeringWheelLabels = {{-1, tr("None")}, {0, tr("Stock")}, {1, tr("Lexus")}, {2, tr("Toyota")}, {3, tr("Frog")}, {4, tr("Rocket")}, {5, tr("Hyundai")}, {6, tr("Stalin")}};
      toggle = new FrogPilotParamValueToggleControl(param, title, desc, icon, -1, 6, steeringWheelLabels, this, true, "", 1, 1, wheelToggles, wheelToggleNames);

    } else if (param == "DeveloperUI") {
      FrogPilotParamManageControl *developerUIToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(developerUIToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedDeveloperUIKeys  = developerUIKeys ;

          toggle->setVisible(modifiedDeveloperUIKeys.find(key.c_str()) != modifiedDeveloperUIKeys.end());
        }
      });
      toggle = developerUIToggle;
    } else if (param == "BorderMetrics") {
      std::vector<QString> borderToggles{"BlindSpotMetrics", "ShowSteering", "SignalMetrics"};
      std::vector<QString> borderToggleNames{tr("Blind Spot"), tr("Steering Torque"), tr("Turn Signal"), };
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, borderToggles, borderToggleNames);
    } else if (param == "NumericalTemp") {
      std::vector<QString> temperatureToggles{"Fahrenheit"};
      std::vector<QString> temperatureToggleNames{tr("Fahrenheit")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, temperatureToggles, temperatureToggleNames);
    } else if (param == "SidebarMetrics") {
      std::vector<QString> sidebarMetricsToggles{"ShowCPU", "ShowGPU", "ShowIP", "ShowMemoryUsage", "ShowStorageLeft", "ShowStorageUsed"};
      std::vector<QString> sidebarMetricsToggleNames{tr("CPU"), tr("GPU"), tr("IP"), tr("RAM"), tr("SSD Left"), tr("SSD Used")};
      FrogPilotParamToggleControl *sidebarMetricsToggle = new FrogPilotParamToggleControl(param, title, desc, icon, sidebarMetricsToggles, sidebarMetricsToggleNames, this, 125);
      QObject::connect(sidebarMetricsToggle, &FrogPilotParamToggleControl::buttonTypeClicked, this, [this, sidebarMetricsToggle](int index) {
        if (index == 0) {
          params.putBool("ShowGPU", false);
        } else if (index == 1) {
          params.putBool("ShowCPU", false);
        } else if (index == 3) {
          params.putBool("ShowStorageLeft", false);
          params.putBool("ShowStorageUsed", false);
        } else if (index == 4) {
          params.putBool("ShowMemoryUsage", false);
          params.putBool("ShowStorageUsed", false);
        } else if (index == 5) {
          params.putBool("ShowMemoryUsage", false);
          params.putBool("ShowStorageLeft", false);
        }

        sidebarMetricsToggle->updateButtonStates();
      });
      toggle = sidebarMetricsToggle;

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
    } else if (param == "BigMap") {
      std::vector<QString> mapToggles{"FullMap"};
      std::vector<QString> mapToggleNames{tr("Full Map")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, mapToggles, mapToggleNames);
    } else if (param == "HideSpeed") {
      std::vector<QString> hideSpeedToggles{"HideSpeedUI"};
      std::vector<QString> hideSpeedToggleNames{tr("Control Via UI")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, hideSpeedToggles, hideSpeedToggleNames);
    } else if (param == "MapStyle") {
      QMap<int, QString> styleMap = {
        {0, tr("Stock openpilot")},
        {1, tr("Mapbox Streets")},
        {2, tr("Mapbox Outdoors")},
        {3, tr("Mapbox Light")},
        {4, tr("Mapbox Dark")},
        {5, tr("Mapbox Satellite")},
        {6, tr("Mapbox Satellite Streets")},
        {7, tr("Mapbox Navigation Day")},
        {8, tr("Mapbox Navigation Night")},
        {9, tr("Mapbox Traffic Night")},
        {10, tr("mike854's (Satellite hybrid)")},
      };

      QStringList styles = styleMap.values();
      ButtonControl *mapStyleButton = new ButtonControl(title, tr("SELECT"), desc);
      QObject::connect(mapStyleButton, &ButtonControl::clicked, [=]() {
        QStringList styles = styleMap.values();
        QString selection = MultiOptionDialog::getSelection(tr("Select a map style"), styles, "", this);
        if (!selection.isEmpty()) {
          int selectedStyle = styleMap.key(selection);
          params.putIntNonBlocking("MapStyle", selectedStyle);
          mapStyleButton->setValue(selection);
          updateFrogPilotToggles();
        }
      });

      int currentStyle = params.getInt("MapStyle");
      mapStyleButton->setValue(styleMap[currentStyle]);

      toggle = mapStyleButton;

    } else if (param == "ScreenManagement") {
      FrogPilotParamManageControl *screenToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(screenToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(screenKeys.find(key.c_str()) != screenKeys.end());
        }
      });
      toggle = screenToggle;
    } else if (param == "HideUIElements") {
      std::vector<QString> uiElementsToggles{"HideAlerts", "HideMapIcon", "HideMaxSpeed"};
      std::vector<QString> uiElementsToggleNames{tr("Alerts"), tr("Map Icon"), tr("Max Speed")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, uiElementsToggles, uiElementsToggleNames);
    } else if (param == "ScreenBrightness" || param == "ScreenBrightnessOnroad") {
      std::map<int, QString> brightnessLabels;
      if (param == "ScreenBrightnessOnroad") {
        for (int i = 0; i <= 101; i++) {
          brightnessLabels[i] = (i == 0) ? tr("Screen Off") : (i == 101) ? tr("Auto") : QString::number(i) + "%";
        }
        toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 101, brightnessLabels, this, false);
      } else {
        for (int i = 1; i <= 101; i++) {
          brightnessLabels[i] = (i == 101) ? tr("Auto") : QString::number(i) + "%";
        }
        toggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 101, brightnessLabels, this, false);
      }
    } else if (param == "ScreenTimeout" || param == "ScreenTimeoutOnroad") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 5, 60, std::map<int, QString>(), this, false, tr(" seconds"));

    } else {
      toggle = new ParamControl(param, title, desc, icon, this);
    }

    addItem(toggle);
    toggles[param.toStdString()] = toggle;

    QObject::connect(static_cast<ToggleControl*>(toggle), &ToggleControl::toggleFlipped, &updateFrogPilotToggles);
    QObject::connect(static_cast<FrogPilotParamValueControl*>(toggle), &FrogPilotParamValueControl::valueChanged, &updateFrogPilotToggles);

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

void FrogPilotVisualsPanel::showEvent(QShowEvent *event) {
  hasOpenpilotLongitudinal = hasOpenpilotLongitudinal && !params.getBool("DisableOpenpilotLongitudinal");
}

void FrogPilotVisualsPanel::updateState(const UIState &s) {
  if (!isVisible()) return;

  started = s.scene.started;
}

void FrogPilotVisualsPanel::updateCarToggles() {
  auto carParams = params.get("CarParamsPersistent");
  if (!carParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(carParams.data(), carParams.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();
    auto carName = CP.getCarName();

    hasAutoTune = (carName == "hyundai" || carName == "toyota") && CP.getLateralTuning().which() == cereal::CarParams::LateralTuning::TORQUE;
    hasBSM = CP.getEnableBsm();
    hasOpenpilotLongitudinal = CP.getOpenpilotLongitudinalControl() && !params.getBool("DisableOpenpilotLongitudinal");
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
                      developerUIKeys.find(key.c_str()) != developerUIKeys.end() ||
                      modelUIKeys.find(key.c_str()) != modelUIKeys.end() ||
                      qolKeys.find(key.c_str()) != qolKeys.end() ||
                      screenKeys.find(key.c_str()) != screenKeys.end();
    toggle->setVisible(!subToggles);
  }

  update();
}
