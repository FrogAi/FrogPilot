#include "selfdrive/frogpilot/ui/qt/offroad/visual_settings.h"

FrogPilotVisualsPanel::FrogPilotVisualsPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> visualToggles {
    {"QOLVisuals", tr("Accessibility"), tr("Visual features to improve your overall openpilot experience."), "../frogpilot/assets/toggle_icons/icon_accessibility.png"},
    {"CameraView", tr("Camera View"), tr("Changes the camera view display. This is purely a visual change and doesn't impact how openpilot drives."), ""},
    {"OnroadDistanceButton", tr("On Screen Personality Button"), tr("Displays the current driving personality on the screen. Tap to switch personalities, or long press for 2.5 seconds to activate 'Traffic' mode."), ""},
    {"DriverCamera", tr("Show Driver Camera When In Reverse"), tr("Displays the driver camera feed when the vehicle is in reverse."), ""},
    {"StandbyMode", tr("Standby Mode"), tr("Turns the screen off when driving and automatically wakes it up if engagement state changes or important alerts occur."), ""},
    {"StoppedTimer", tr("Stopped Timer"), tr("Activates a timer when stopped to indicate how long the vehicle has been stopped for."), ""},

    {"AdvancedCustomUI", tr("Advanced UI Controls"), tr("Advanced features to fine tune your personalized UI."), "../frogpilot/assets/toggle_icons/icon_advanced_device.png"},
    {"HideSpeed", tr("Hide Current Speed"), tr("Hides the current speed."), ""},
    {"HideLeadMarker", tr("Hide Lead Marker"), tr("Hides the marker for the vehicle ahead."), ""},
    {"HideMapIcon", tr("Hide Map Icon"), tr("Hides the map icon."), ""},
    {"HideMaxSpeed", tr("Hide Max Speed"), tr("Hides the max speed."), ""},
    {"HideAlerts", tr("Hide Non-Critical Alerts"), tr("Hides non-critical alerts."), ""},
    {"HideSpeedLimit", tr("Hide Speed Limits"), tr("Hides the speed limits."), ""},
    {"WheelSpeed", tr("Use Wheel Speed"), tr("Uses the wheel speed instead of the cluster speed. This is purely a visual change and doesn't impact how openpilot drives."), ""},

    {"DeveloperUI", tr("Developer UI"), tr("Show detailed information about openpilot's internal operations."), "../assets/offroad/icon_shell.png"},
    {"DeveloperMetrics", tr("Developer Metrics"), tr("Show detailed information about openpilot's internal operations."), ""},
    {"BorderMetrics", tr("Border Metrics"), tr("Displays performance metrics around the edge of the screen while driving."), ""},
    {"FPSCounter", tr("FPS Display"), tr("Displays the 'Frames Per Second' (FPS) at the bottom of the screen while driving."), ""},
    {"LateralMetrics", tr("Lateral Metrics"), tr("Displays metrics related to steering control at the top of the screen while driving."), ""},
    {"LongitudinalMetrics", tr("Longitudinal Metrics"), tr("Displays metrics related to acceleration, speed, and desired following distance at the top of the screen while driving."), ""},
    {"NumericalTemp", tr("Numerical Temperature Gauge"), tr("Shows exact temperature readings instead of status labels like 'GOOD', 'OK', or 'HIGH' in the sidebar."), ""},
    {"SidebarMetrics", tr("Sidebar"), tr("Displays system information like CPU, GPU, RAM usage, IP address, and storage space in the sidebar."), ""},
    {"UseSI", tr("Use International System of Units"), tr("Displays measurements using the 'International System of Units' (SI)."), ""},
    {"DeveloperWidgets", tr("Developer Widgets"), tr("Show detailed information about openpilot's internal operations."), ""},
    {"AdjacentLeadsUI", tr("Adjacent Leads Tracking"), tr("Show adjacent leads being detected by the car's radar."), ""},
    {"ShowStoppingPoint", tr("Model Stopping Point"), tr("Displays an image on the screen where openpilot is wanting to stop."), ""},
    {"RadarTracksUI", tr("Radar Tracks"), tr("Show all of the radar points being tracked by the car's radar."), ""},

    {"ModelUI", tr("Model UI"), tr("Customize the model visualizations on the screen."), "../frogpilot/assets/toggle_icons/icon_vtc.png"},
    {"DynamicPathWidth", tr("Dynamic Path Width"), tr("Automatically adjusts the width of the driving path display based on the current engagement state:\n\nFully engaged = 100%\nAlways On Lateral Active = 75%\nFully disengaged = 50%"), ""},
    {"LaneLinesWidth", tr("Lane Lines Width"), tr("Controls the thickness the lane lines appear on the display.\n\nDefault matches the MUTCD standard of 4 inches."), ""},
    {"PathEdgeWidth", tr("Path Edges Width"), tr("Controls the width of the edges of the driving path to represent different modes and statuses.\n\nDefault is 20% of the total path width.\n\nColor Guide:\n\n- Blue: Navigation\n- Light Blue: 'Always On Lateral'\n- Green: Default\n- Orange: 'Experimental Mode'\n- Red: 'Traffic Mode'\n- Yellow: 'Conditional Experimental Mode' Overridden"), ""},
    {"PathWidth", tr("Path Width"), tr("Controls how wide the driving path appears on your screen.\n\nDefault (6.1 feet / 1.9 meters) matches the width of a 2019 Lexus ES 350."), ""},
    {"RoadEdgesWidth", tr("Road Edges Width"), tr("Controls how thick the road edges appear on the display.\n\nDefault matches half of the MUTCD standard lane line width of 4 inches."), ""},
    {"UnlimitedLength", tr("'Unlimited' Road UI"), tr("Extends the display of the path, lane lines, and road edges as far as the model can see."), ""},

    {"NavigationUI", tr("Navigation Widgets"), tr("Wwidgets focused around navigation."), "../frogpilot/assets/toggle_icons/icon_map.png"},
    {"BigMap", tr("Larger Map Display"), tr("Increases the size of the map for easier navigation readings."), ""},
    {"MapStyle", tr("Map Style"), tr("Swaps out the stock map style for community created ones."), ""},
    {"RoadNameUI", tr("Road Name"), tr("Displays the current road name at the bottom of the screen using data from 'OpenStreetMap'."), ""},
    {"ShowSpeedLimits", tr("Show Speed Limits"), tr("Displays the currently detected speed limit in the top left corner of the onroad UI. Uses data from your car's dashboard (if supported) and data from 'OpenStreetMaps'."), ""},
    {"UseVienna", tr("Use Vienna-Style Speed Signs"), tr("Forces Vienna-style (EU) speed limit signs instead of MUTCD (US)."), ""},

    {"CustomUI", tr("Onroad Screen Widgets"), tr("Custom FrogPilot widgets used in the onroad user interface."), "../assets/offroad/icon_road.png"},
    {"AccelerationPath", tr("Acceleration Path"), tr("Projects a path based on openpilot's current desired acceleration or deceleration."), ""},
    {"AdjacentPath", tr("Adjacent Lanes"), tr("Projects paths for the adjascent lanes."), ""},
    {"BlindSpotPath", tr("Blind Spot Path"), tr("Projects a red path when vehicles are detected in the blind spot for the respective lane."), ""},
    {"Compass", tr("Compass"), tr("Displays a compass to show the current driving direction."), ""},
    {"PedalsOnUI", tr("Gas / Brake Pedal Indicators"), tr("Displays pedal indicators to indicate when either of the pedals are currently being used."), ""},
    {"RotatingWheel", tr("Rotating Steering Wheel"), tr("Rotates the steering wheel in the onroad UI rotates along with your steering wheel movements."), ""}
  };

  for (const auto &[param, title, desc, icon] : visualToggles) {
    AbstractControl *visualToggle;

    if (param == "QOLVisuals") {
      FrogPilotManageControl *qolToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(qolToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedAccessibilityKeys = accessibilityKeys;

        if (!hasOpenpilotLongitudinal) {
          modifiedAccessibilityKeys.erase("OnroadDistanceButton");
        }

        showToggles(modifiedAccessibilityKeys);
      });
      visualToggle = qolToggle;
    } else if (param == "CameraView") {
      std::vector<QString> cameraOptions{tr("Auto"), tr("Driver"), tr("Standard"), tr("Wide")};
      ButtonParamControl *preferredCamera = new ButtonParamControl(param, title, desc, icon, cameraOptions);
      visualToggle = preferredCamera;

    } else if (param == "AdvancedCustomUI") {
      FrogPilotManageControl *advancedCustomUIToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(advancedCustomUIToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedAdvancedCustomOnroadUIKeys = advancedCustomOnroadUIKeys;

        if (!hasOpenpilotLongitudinal) {
          modifiedAdvancedCustomOnroadUIKeys.erase("HideLeadMarker");
        }

        showToggles(modifiedAdvancedCustomOnroadUIKeys);
      });
      visualToggle = advancedCustomUIToggle;

    } else if (param == "DeveloperUI") {
      FrogPilotManageControl *developerUIToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(developerUIToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedDeveloperUIKeys = developerUIKeys;

        if (!hasOpenpilotLongitudinal) {
          modifiedDeveloperUIKeys.erase("DeveloperWidgets");
        }

        showToggles(modifiedDeveloperUIKeys);
      });
      visualToggle = developerUIToggle;
    } else if (param == "DeveloperMetrics") {
      ButtonControl *developerMetricsToggle = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(developerMetricsToggle, &ButtonControl::clicked, [this]() {
        developerUIOpen = true;

        openSubParentToggle();

        borderMetricsBtn->setVisibleButton(0, hasBSM);
        lateralMetricsBtn->setVisibleButton(1, hasAutoTune);

        std::set<QString> modifiedDeveloperMetricKeys = developerMetricKeys;

        if (!hasOpenpilotLongitudinal) {
          modifiedDeveloperMetricKeys.erase("LongitudinalMetrics");
        }

        showToggles(modifiedDeveloperMetricKeys);
      });
      visualToggle = developerMetricsToggle;
    } else if (param == "BorderMetrics") {
      std::vector<QString> borderToggles{"BlindSpotMetrics", "ShowSteering", "SignalMetrics"};
      std::vector<QString> borderToggleNames{tr("Blind Spot"), tr("Steering Torque"), tr("Turn Signal")};
      borderMetricsBtn = new FrogPilotButtonToggleControl(param, title, desc, icon, borderToggles, borderToggleNames);
      visualToggle = borderMetricsBtn;
    } else if (param == "LateralMetrics") {
      std::vector<QString> lateralToggles{"AdjacentPathMetrics", "TuningInfo"};
      std::vector<QString> lateralToggleNames{tr("Adjacent Path Metrics"), tr("Auto Tune")};
      lateralMetricsBtn = new FrogPilotButtonToggleControl(param, title, desc, icon, lateralToggles, lateralToggleNames);
      visualToggle = lateralMetricsBtn;
    } else if (param == "LongitudinalMetrics") {
      std::vector<QString> longitudinalToggles{"LeadInfo", "JerkInfo"};
      std::vector<QString> longitudinalToggleNames{tr("Lead Info"), tr("Jerk Values")};
      visualToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, longitudinalToggles, longitudinalToggleNames);
    } else if (param == "NumericalTemp") {
      std::vector<QString> temperatureToggles{"Fahrenheit"};
      std::vector<QString> temperatureToggleNames{tr("Fahrenheit")};
      visualToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, temperatureToggles, temperatureToggleNames);
    } else if (param == "SidebarMetrics") {
      std::vector<QString> sidebarMetricsToggles{"ShowCPU", "ShowGPU", "ShowIP", "ShowMemoryUsage", "ShowStorageLeft", "ShowStorageUsed"};
      std::vector<QString> sidebarMetricsToggleNames{tr("CPU"), tr("GPU"), tr("IP"), tr("RAM"), tr("SSD Left"), tr("SSD Used")};
      FrogPilotButtonToggleControl *sidebarMetricsToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, sidebarMetricsToggles, sidebarMetricsToggleNames, false, 150);
      QObject::connect(sidebarMetricsToggle, &FrogPilotButtonToggleControl::buttonClicked, [this, sidebarMetricsToggle](int id) {
        if (id == 0) {
          params.putBool("ShowGPU", false);
        } else if (id == 1) {
          params.putBool("ShowCPU", false);
        } else if (id == 3) {
          params.putBool("ShowStorageLeft", false);
          params.putBool("ShowStorageUsed", false);
        } else if (id == 4) {
          params.putBool("ShowMemoryUsage", false);
          params.putBool("ShowStorageUsed", false);
        } else if (id == 5) {
          params.putBool("ShowMemoryUsage", false);
          params.putBool("ShowStorageLeft", false);
        }
        sidebarMetricsToggle->refresh();
      });
      visualToggle = sidebarMetricsToggle;
    } else if (param == "DeveloperWidgets") {
      ButtonControl *developerWidgetsToggle = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(developerWidgetsToggle, &ButtonControl::clicked, [this]() {
        developerUIOpen = true;

        openSubParentToggle();

        std::set<QString> modifiedDeveloperWidgetKeys = developerWidgetKeys;

        if (!hasOpenpilotLongitudinal) {
          modifiedDeveloperWidgetKeys.erase("ShowStoppingPoint");
        }

        if (!hasRadar) {
          modifiedDeveloperWidgetKeys.erase("AdjacentLeadsUI");
          modifiedDeveloperWidgetKeys.erase("RadarTracksUI");
        }

        showToggles(modifiedDeveloperWidgetKeys);
      });
      visualToggle = developerWidgetsToggle;
    } else if (param == "ShowStoppingPoint") {
      std::vector<QString> stoppingPointToggles{"ShowStoppingPointMetrics"};
      std::vector<QString> stoppingPointToggleNames{tr("Show Distance")};
      visualToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, stoppingPointToggles, stoppingPointToggleNames);

    } else if (param == "ModelUI") {
      FrogPilotManageControl *modelUIToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(modelUIToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        showToggles(modelUIKeys);
      });
      visualToggle = modelUIToggle;
    } else if (param == "LaneLinesWidth" || param == "RoadEdgesWidth") {
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 24, tr(" inches"));
    } else if (param == "PathEdgeWidth") {
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, tr("%"));
    } else if (param == "PathWidth") {
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, tr(" feet"), std::map<float, QString>(), 0.1);

    } else if (param == "NavigationUI") {
      FrogPilotManageControl *customUIToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(customUIToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedNavigationUIKeys = navigationUIKeys;

        if (params.getBool("SpeedLimitController")) {
          modifiedNavigationUIKeys.erase("ShowSpeedLimits");
        }

        showToggles(modifiedNavigationUIKeys);
      });
      visualToggle = customUIToggle;
    } else if (param == "BigMap") {
      std::vector<QString> mapToggles{"FullMap"};
      std::vector<QString> mapToggleNames{tr("Full Map")};
      visualToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, mapToggles, mapToggleNames);
    } else if (param == "MapStyle") {
      QMap<int, QString> styleMap {
        {0, tr("Stock")},
        {1, tr("Mapbox Streets")},
        {2, tr("Mapbox Outdoors")},
        {3, tr("Mapbox Light")},
        {4, tr("Mapbox Dark")},
        {5, tr("Mapbox Satellite")},
        {6, tr("Mapbox Satellite Streets")},
        {7, tr("Mapbox Navigation Day")},
        {8, tr("Mapbox Navigation Night")},
        {9, tr("Mapbox Traffic Night")},
        {10, tr("mike854's (Satellite hybrid)")}
      };

      ButtonControl *mapStyleButton = new ButtonControl(title, tr("SELECT"), desc);
      QObject::connect(mapStyleButton, &ButtonControl::clicked, [this, mapStyleButton, styleMap]() {
        QString selection = MultiOptionDialog::getSelection(tr("Select a map style"), styleMap.values(), "", this);
        if (!selection.isEmpty()) {
          int selectedStyle = styleMap.key(selection);
          params.putInt("MapStyle", selectedStyle);
          mapStyleButton->setValue(selection);
        }
      });
      int currentStyle = params.getInt("MapStyle");
      mapStyleButton->setValue(styleMap[currentStyle]);

      visualToggle = mapStyleButton;

    } else if (param == "CustomUI") {
      FrogPilotManageControl *customUIToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(customUIToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedCustomOnroadUIKeys = customOnroadUIKeys;

        if (!hasBSM) {
          modifiedCustomOnroadUIKeys.erase("BlindSpotPath");
        }

        if (!hasOpenpilotLongitudinal) {
          modifiedCustomOnroadUIKeys.erase("AccelerationPath");
          modifiedCustomOnroadUIKeys.erase("PedalsOnUI");
        }

        showToggles(modifiedCustomOnroadUIKeys);
      });
      visualToggle = customUIToggle;
    } else if (param == "PedalsOnUI") {
      std::vector<QString> pedalsToggles{"DynamicPedalsOnUI", "StaticPedalsOnUI"};
      std::vector<QString> pedalsToggleNames{tr("Dynamic"), tr("Static")};
      FrogPilotButtonToggleControl *pedalsToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, pedalsToggles, pedalsToggleNames, true);
      QObject::connect(pedalsToggle, &FrogPilotButtonToggleControl::buttonClicked, [this](int id) {
        if (id == 0) {
          params.putBool("StaticPedalsOnUI", false);
        } else if (id == 1) {
          params.putBool("DynamicPedalsOnUI", false);
        }
      });
      visualToggle = pedalsToggle;

    } else {
      visualToggle = new ParamControl(param, title, desc, icon);
    }

    addItem(visualToggle);
    toggles[param] = visualToggle;

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(visualToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, this, &FrogPilotVisualsPanel::openParentToggle);
    }
  }

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, this, &FrogPilotVisualsPanel::hideToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubParentToggle, this, &FrogPilotVisualsPanel::hideSubToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, this, &FrogPilotVisualsPanel::updateMetric);
}

void FrogPilotVisualsPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  hasAutoTune = parent->hasAutoTune;
  hasBSM = parent->hasBSM;
  hasOpenpilotLongitudinal = parent->hasOpenpilotLongitudinal;
  hasRadar = parent->hasRadar;
  tuningLevel = parent->tuningLevel;

  hideToggles();
}

void FrogPilotVisualsPanel::updateMetric(bool metric, bool bootRun) {
  static bool previousMetric;
  if (metric != previousMetric && !bootRun) {
    double smallDistanceConversion = metric ? INCH_TO_CM : CM_TO_INCH;
    double distanceConversion = metric ? FOOT_TO_METER : METER_TO_FOOT;

    params.putFloatNonBlocking("LaneLinesWidth", params.getFloat("LaneLinesWidth") * smallDistanceConversion);
    params.putFloatNonBlocking("RoadEdgesWidth", params.getFloat("RoadEdgesWidth") * smallDistanceConversion);

    params.putFloatNonBlocking("PathWidth", params.getFloat("PathWidth") * distanceConversion);
  }
  previousMetric = metric;

  FrogPilotParamValueControl *laneLinesWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["LaneLinesWidth"]);
  FrogPilotParamValueControl *pathWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["PathWidth"]);
  FrogPilotParamValueControl *roadEdgesWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["RoadEdgesWidth"]);

  if (metric) {
    laneLinesWidthToggle->setDescription(tr("Adjust how thick the lane lines appear on the display.\n\nDefault matches the Vienna standard of 10 centimeters."));
    roadEdgesWidthToggle->setDescription(tr("Adjust how thick the road edges appear on the display.\n\nDefault matches half of the Vienna standard of 10 centimeters."));

    laneLinesWidthToggle->updateControl(0, 60, tr(" centimeters"));
    roadEdgesWidthToggle->updateControl(0, 60, tr(" centimeters"));

    pathWidthToggle->updateControl(0, 3, tr(" meters"));
  } else {
    laneLinesWidthToggle->setDescription(tr("Adjust how thick the lane lines appear on the display.\n\nDefault matches the MUTCD standard of 4 inches."));
    roadEdgesWidthToggle->setDescription(tr("Adjust how thick the road edges appear on the display.\n\nDefault matches half of the MUTCD standard of 4 inches."));

    laneLinesWidthToggle->updateControl(0, 24, tr(" inches"));
    roadEdgesWidthToggle->updateControl(0, 24, tr(" inches"));

    pathWidthToggle->updateControl(0, 10, tr(" feet"));
  }
}

void FrogPilotVisualsPanel::showToggles(const std::set<QString> &keys) {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(keys.find(key) != keys.end() && tuningLevel >= frogpilotToggleLevels[key].toDouble());
  }

  setUpdatesEnabled(true);

  update();
}

void FrogPilotVisualsPanel::hideToggles() {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    bool subToggles = accessibilityKeys.find(key) != accessibilityKeys.end() ||
                      advancedCustomOnroadUIKeys.find(key) != advancedCustomOnroadUIKeys.end() ||
                      customOnroadUIKeys.find(key) != customOnroadUIKeys.end() ||
                      developerMetricKeys.find(key) != developerMetricKeys.end() ||
                      developerUIKeys.find(key) != developerUIKeys.end() ||
                      developerWidgetKeys.find(key) != developerWidgetKeys.end() ||
                      modelUIKeys.find(key) != modelUIKeys.end() ||
                      navigationUIKeys.find(key) != navigationUIKeys.end();

    toggle->setVisible(!subToggles && tuningLevel >= frogpilotToggleLevels[key].toDouble());
  }

  toggles["QOLVisuals"]->setVisible(toggles["QOLVisuals"]->isVisible() || hasOpenpilotLongitudinal);

  setUpdatesEnabled(true);

  update();
}

void FrogPilotVisualsPanel::hideSubToggles() {
  if (developerUIOpen) {
    developerUIOpen = false;
    showToggles(developerUIKeys);
  }
}
