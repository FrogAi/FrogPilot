#include "frogpilot/ui/qt/offroad/visual_settings.h"

FrogPilotVisualsPanel::FrogPilotVisualsPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QStackedLayout *visualsLayout = new QStackedLayout();
  addItem(visualsLayout);

  FrogPilotListWidget *visualsList = new FrogPilotListWidget(this);

  ScrollView *visualsPanel = new ScrollView(visualsList, this);

  visualsLayout->addWidget(visualsPanel);

  FrogPilotListWidget *advancedCustomList = new FrogPilotListWidget(this);
  FrogPilotListWidget *customUIList = new FrogPilotListWidget(this);
  FrogPilotListWidget *developerMetricList = new FrogPilotListWidget(this);
  FrogPilotListWidget *developerSidebarList = new FrogPilotListWidget(this);
  FrogPilotListWidget *developerUIList = new FrogPilotListWidget(this);
  FrogPilotListWidget *developerWidgetList = new FrogPilotListWidget(this);
  FrogPilotListWidget *modelUIList = new FrogPilotListWidget(this);
  FrogPilotListWidget *navigationUIList = new FrogPilotListWidget(this);
  FrogPilotListWidget *qualityOfLifeList = new FrogPilotListWidget(this);

  ScrollView *advancedCustomPanel = new ScrollView(advancedCustomList, this);
  ScrollView *customUIPanel = new ScrollView(customUIList, this);
  ScrollView *developerMetricPanel = new ScrollView(developerMetricList, this);
  ScrollView *developerSidebarPanel = new ScrollView(developerSidebarList, this);
  ScrollView *developerUIPanel = new ScrollView(developerUIList, this);
  ScrollView *developerWidgetPanel = new ScrollView(developerWidgetList, this);
  ScrollView *modelUIPanel = new ScrollView(modelUIList, this);
  ScrollView *navigationUIPanel = new ScrollView(navigationUIList, this);
  ScrollView *qualityOfLifePanel = new ScrollView(qualityOfLifeList, this);

  visualsLayout->addWidget(advancedCustomPanel);
  visualsLayout->addWidget(customUIPanel);
  visualsLayout->addWidget(developerMetricPanel);
  visualsLayout->addWidget(developerSidebarPanel);
  visualsLayout->addWidget(developerUIPanel);
  visualsLayout->addWidget(developerWidgetPanel);
  visualsLayout->addWidget(modelUIPanel);
  visualsLayout->addWidget(navigationUIPanel);
  visualsLayout->addWidget(qualityOfLifePanel);

  const std::vector<std::tuple<QString, QString, QString, QString>> visualToggles {
    {"AdvancedCustomUI", tr("Advanced UI Controls"), tr("Advanced settings for fine-tuning openpilot's driving screen."), "../../frogpilot/assets/toggle_icons/icon_advanced_device.png"},
    {"HideSpeed", tr("Hide Current Speed"), tr("Hide the current speed from the driving screen."), ""},
    {"HideLeadMarker", tr("Hide Lead Marker"), tr("Hide the marker for lead vehicles from the driving screen."), ""},
    {"HideMapIcon", tr("Hide Map Settings Button"), tr("Hide the map settings button from the driving screen."), ""},
    {"HideMaxSpeed", tr("Hide Max Speed"), tr("Hide the max speed from the driving screen."), ""},
    {"HideAlerts", tr("Hide Non-Critical Alerts"), tr("Hide non-critical alerts from the driving screen."), ""},
    {"HideSpeedLimit", tr("Hide Speed Limits"), tr("Hide the speed limits from the driving screen."), ""},
    {"WheelSpeed", tr("Use Wheel Speed"), tr("Use the vehicle's wheel speed instead of the cluster speed. This is purely a visual change and doesn't impact how openpilot drives."), ""},

    {"DeveloperUI", tr("Developer UI"), tr("Detailed information about openpilot's internal operations."), "../assets/offroad/icon_shell.png"},
    {"AdjacentPathMetrics", tr("Adjacent Path Metrics"), tr("Metrics displayed on top of the adjacent lanes measuring their current width."), ""},
    {"DeveloperMetrics", tr("Developer Metrics"), tr("Performance data, sensor readings, and system metrics for debugging and optimizing openpilot."), ""},
    {"BorderMetrics", tr("Border Metrics"), tr("Metrics displayed around the border of the driving screen.<br><br><b>Blind Spot</b>: Turn the border red when a vehicle is detected in a blind spot<br><b>Steering Torque</b>: Highlight the border green to red in accordance to the amount of steering torque being used<br><b>Turn Signal</b>: Flash the border yellow when a turn signal is active"), ""},
    {"LeadInfo", tr("Lead Info"), tr("Metrics displayed under vehicle markers listing their distance and current speed."), ""},
    {"FPSCounter", tr("FPS Display"), tr("Display the <b>Frames Per Second (FPS)</b> at the bottom of the driving screen."), ""},
    {"NumericalTemp", tr("Numerical Temperature Gauge"), tr("Use numerical temperature readings instead of status labels in the sidebar."), ""},
    {"SidebarMetrics", tr("Sidebar"), tr("Display system information (<b>CPU</b>, <b>GPU</b>, <b>RAM usage</b>, <b>IP address</b>, <b>device storage</b>) in the sidebar."), ""},
    {"UseSI", tr("Use International System of Units"), tr("Display measurements using the <b>International System of Units (SI)</b> standard."), ""},
    {"DeveloperSidebar", tr("Developer Sidebar"), tr("Display debugging info and metrics in a dedicated sidebar on the right side of the screen."), ""},
    {"DeveloperSidebarMetric1", tr("Metric #1"), tr("Metric to display in the first metric in the \"Developer Sidebar\"."), ""},
    {"DeveloperSidebarMetric2", tr("Metric #2"), tr("Metric to display in the second metric in the \"Developer Sidebar\"."), ""},
    {"DeveloperSidebarMetric3", tr("Metric #3"), tr("Metric to display in the third metric in the \"Developer Sidebar\"."), ""},
    {"DeveloperSidebarMetric4", tr("Metric #4"), tr("Metric to display in the fourth metric in the \"Developer Sidebar\"."), ""},
    {"DeveloperSidebarMetric5", tr("Metric #5"), tr("Metric to display in the fifth metric in the \"Developer Sidebar\"."), ""},
    {"DeveloperSidebarMetric6", tr("Metric #6"), tr("Metric to display in the sixth metric in the \"Developer Sidebar\"."), ""},
    {"DeveloperSidebarMetric7", tr("Metric #7"), tr("Metric to display in the seventh metric in the \"Developer Sidebar\"."), ""},
    {"DeveloperWidgets", tr("Developer Widgets"), tr("Overlays displaying debugging visuals, internal states, and model predictions on the driving screen."), ""},
    {"AdjacentLeadsUI", tr("Adjacent Leads Tracking"), tr("Adjacent leads detected by the car's radar to the left and right of the current driving path."), ""},
    {"ShowStoppingPoint", tr("Model Stopping Point"), tr("Display an image on the screen where openpilot is wanting to stop."), ""},
    {"RadarTracksUI", tr("Radar Tracks"), tr("Display all of the radar points produced by the car's radar."), ""},

    {"CustomUI", tr("Driving Screen Widgets"), tr("Custom FrogPilot widgets for the driving screen."), "../assets/offroad/icon_road.png"},
    {"AccelerationPath", tr("Acceleration Path"), tr("Colorize the driving path based on openpilot's current desired acceleration and deceleration rate."), ""},
    {"AdjacentPath", tr("Adjacent Lanes"), tr("Driving paths for the left and right adjacent lanes."), ""},
    {"BlindSpotPath", tr("Blind Spot Path"), tr("Display a red driving path for detected vehicles in the corresponding lane's blind spot."), ""},
    {"Compass", tr("Compass"), tr("A compass to show the current driving direction."), ""},
    {"OnroadDistanceButton", tr("Driving Personality Button"), tr("Display the current driving personality on the screen. Tap to switch personalities, or long press for 0.5 seconds to change the current state of <b>Experimental Mode</b>, or 2.5 seconds for <b>Traffic Mode</b>."), ""},
    {"PedalsOnUI", tr("Gas / Brake Pedal Indicators"), tr("Pedals to indicate when either of the pedals are currently being used.<br><br><b>Dynamic</b>: The pedals change in opacity in accordance to how much openpilot is accelerating or decelerating<br><b>Static</b>: The pedals are displayed with full opacity when active, and dimmed when not in use"), ""},
    {"RotatingWheel", tr("Rotating Steering Wheel"), tr("Rotate the steering wheel alongside the vehicle's physical steering wheel."), ""},

    {"ModelUI", tr("Model UI"), tr("Model visualizations on the driving screen for the driving path, lane lines, path edges, and road edges."), "../../frogpilot/assets/toggle_icons/icon_vtc.png"},
    {"DynamicPathWidth", tr("Dynamic Path Width"), tr("Adjust the width of the driving path based on the current engagement state.<br><br><b>Fully engaged</b>: 100%<br><b>Always On Lateral</b>: 75%<br><b>Fully disengaged</b>: 50%"), ""},
    {"LaneLinesWidth", tr("Lane Lines Width"), tr("The thickness of the lane lines on the driving screen.<br><br><b>Default matches the <b>MUTCD</b> lane line width standard of 4 inches."), ""},
    {"PathEdgeWidth", tr("Path Edges Width"), tr("The width of the edges of the driving path that represent different driving modes and statuses.<br><br>Default is <b>20%</b> of the total path width.<br><br>Color Guide:<br><br>- <b>Blue</b>: Navigation<br>- <b>Light Blue</b>: Always On Lateral<br>- <b>Green</b>: Default<br>- <b>Orange</b>: Experimental Mode<br>- <b>Red</b>: Traffic Mode<br>- <b>Yellow</b>: Conditional Experimental Mode overridden"), ""},
    {"PathWidth", tr("Path Width"), tr("The width of the driving path on the driving screen.<br><br>Default <b>(6.1 feet / 1.9 meters)</b> matches the width of a <b>2019 Lexus ES 350</b>."), ""},
    {"RoadEdgesWidth", tr("Road Edges Width"), tr("The thickness of the road edges on the driving screen.<br><br><b>Default matches half of the <b>MUTCD</b> lane line width standard of 4 inches."), ""},
    {"UnlimitedLength", tr("\"Unlimited\" Road UI"), tr("Extend the display of the driving path, lane lines, and road edges as far as the model can see."), ""},

    {"NavigationUI", tr("Navigation Widgets"), tr("Map style tweaks, speed limits, and other navigation related widgets."), "../../frogpilot/assets/toggle_icons/icon_map.png"},
    {"BigMap", tr("Larger Map Display"), tr("Increase the size of the map for easier navigation readings."), ""},
    {"MapStyle", tr("Map Style"), tr("The map style used for <b>Navigate on openpilot (NOO)</b>:<br><br><b>Stock</b>: Default comma.ai style<br><b>Mapbox Streets</b>: Standard street-focused view<br><b>Mapbox Outdoors</b>: Emphasizes outdoor and terrain features<br><b>Mapbox Light</b>: Minimalist, bright theme<br><b>Mapbox Dark</b>: Minimalist, dark theme<br><b>Mapbox Navigation Day</b>: Optimized for daytime navigation<br><b>Mapbox Navigation Night</b>: Optimized for nighttime navigation<br><b>Mapbox Satellite</b>: Satellite imagery only<br><b>Mapbox Satellite Streets</b>: Hybrid satellite imagery with street labels<br><b>Mapbox Traffic Night</b>: Dark theme emphasizing traffic conditions<br><b>mike854's (Satellite hybrid)</b>: Customized hybrid satellite view"), ""},
    {"RoadNameUI", tr("Road Name"), tr("Display the road name at the bottom of the driving screen using data from <b>OpenStreetMap</b>."), ""},
    {"ShowSpeedLimits", tr("Show Speed Limits"), tr("Display speed limits in the top left corner of the driving screen. Uses data from your car's dashboard (if supported) and data from <b>OpenStreetMaps</b>."), ""},
    {"SLCMapboxFiller", tr("Show Speed Limits from Mapbox"), tr("Use <b>Mapbox</b> speed limit data when no other sources are available."), ""},
    {"UseVienna", tr("Use Vienna-Style Speed Signs"), tr("Force <b>Vienna-style (EU)</b> speed limit signs instead of <b>MUTCD (US)</b>."), ""},

    {"QOLVisuals", tr("Quality of Life"), tr("Visual features to improve your overall openpilot experience."), "../../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"CameraView", tr("Camera View"), tr("The active camera view display. This is purely a visual change and doesn't impact how openpilot drives!"), ""},
    {"DriverCamera", tr("Show Driver Camera When In Reverse"), tr("Display the driver camera feed when the vehicle is in reverse."), ""},
    {"StandbyMode", tr("Standby Mode"), tr("Turn the screen off when driving and automatically wake it up if engagement state changes or important alerts occur."), ""},
    {"StoppedTimer", tr("Stopped Timer"), tr("Replace the current speed with a timer when stopped to indicate how long the vehicle has been stopped for."), ""}
  };

  for (const auto &[param, title, desc, icon] : visualToggles) {
    AbstractControl *visualToggle;

    if (param == "AdvancedCustomUI") {
      FrogPilotManageControl *advancedCustomUIToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(advancedCustomUIToggle, &FrogPilotManageControl::manageButtonClicked, [visualsLayout, advancedCustomPanel]() {
        visualsLayout->setCurrentWidget(advancedCustomPanel);
      });
      visualToggle = advancedCustomUIToggle;

    } else if (param == "DeveloperUI") {
      FrogPilotManageControl *developerUIToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(developerUIToggle, &FrogPilotManageControl::manageButtonClicked, [visualsLayout, developerUIPanel]() {
        visualsLayout->setCurrentWidget(developerUIPanel);
      });
      visualToggle = developerUIToggle;
    } else if (param == "DeveloperMetrics") {
      FrogPilotManageControl *developerMetricsToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(developerMetricsToggle, &FrogPilotManageControl::manageButtonClicked, [this, visualsLayout, developerMetricPanel]() {
        openSubSubPanel();

        visualsLayout->setCurrentWidget(developerMetricPanel);

        developerUIOpen = true;
      });
      visualToggle = developerMetricsToggle;
    } else if (param == "BorderMetrics") {
      std::vector<QString> borderToggles{"BlindSpotMetrics", "ShowSteering", "SignalMetrics"};
      std::vector<QString> borderToggleNames{tr("Blind Spot"), tr("Steering Torque"), tr("Turn Signal")};
      borderMetricsBtn = new FrogPilotButtonToggleControl(param, title, desc, icon, borderToggles, borderToggleNames);
      visualToggle = borderMetricsBtn;
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
    } else if (param == "DeveloperSidebar") {
      FrogPilotManageControl *developerSidebarToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(developerSidebarToggle, &FrogPilotManageControl::manageButtonClicked, [this, visualsLayout, developerSidebarPanel]() {
        openSubSubPanel();

        visualsLayout->setCurrentWidget(developerSidebarPanel);

        developerUIOpen = true;
      });
      visualToggle = developerSidebarToggle;
    } else if (developerSidebarKeys.find(param) != developerSidebarKeys.end()) {
      QMap<int, QString> developerSidebarMetricOptions {
        {0, tr("None")},
        {1, tr("Acceleration: Current")},
        {2, tr("Acceleration: Max")},
        {3, tr("Auto Tune: Actuator Delay")},
        {4, tr("Auto Tune: Friction")},
        {5, tr("Auto Tune: Lateral Acceleration")},
        {6, tr("Auto Tune: Steer Ratio")},
        {7, tr("Auto Tune: Stiffness Factor")},
        {8, tr("Engagement %: Lateral")},
        {9, tr("Engagement %: Longitudinal")},
        {10, tr("Lateral Control: Steering Angle")},
        {11, tr("Lateral Control: Torque % Used")},
        {12, tr("Longitudinal Control: Actuator Acceleration Output")},
        {13, tr("Longitudinal MPC Jerk: Acceleration")},
        {14, tr("Longitudinal MPC Jerk: Danger Zone")},
        {15, tr("Longitudinal MPC Jerk: Speed Control")},
      };

      ButtonControl *metricToggle = new ButtonControl(title, tr("SELECT"), desc);
      QObject::connect(metricToggle, &ButtonControl::clicked, [this, metricToggle, key = param, developerSidebarMetricOptions]() mutable {
        QString current = developerSidebarMetricOptions.value(params.getInt(key.toStdString()), tr("None"));
        QString selection = MultiOptionDialog::getSelection(tr("Select a metric to display"), developerSidebarMetricOptions.values(), current, this);

        if (!selection.isEmpty()) {
          int selectedMetric = developerSidebarMetricOptions.key(selection);

          params.putInt(key.toStdString(), selectedMetric);

          metricToggle->setValue(selection);
        }
      });
      metricToggle->setValue(developerSidebarMetricOptions.value(params.getInt(param.toStdString()), tr("None")));
      visualToggle = metricToggle;
    } else if (param == "DeveloperWidgets") {
      FrogPilotManageControl *developerWidgetsToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(developerWidgetsToggle, &FrogPilotManageControl::manageButtonClicked, [this, visualsLayout, developerWidgetPanel]() {
        openSubSubPanel();

        visualsLayout->setCurrentWidget(developerWidgetPanel);

        developerUIOpen = true;
      });
      visualToggle = developerWidgetsToggle;
    } else if (param == "ShowStoppingPoint") {
      std::vector<QString> stoppingPointToggles{"ShowStoppingPointMetrics"};
      std::vector<QString> stoppingPointToggleNames{tr("Show Distance")};
      visualToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, stoppingPointToggles, stoppingPointToggleNames);

    } else if (param == "CustomUI") {
      FrogPilotManageControl *customUIToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(customUIToggle, &FrogPilotManageControl::manageButtonClicked, [visualsLayout, customUIPanel]() {
        visualsLayout->setCurrentWidget(customUIPanel);
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

    } else if (param == "ModelUI") {
      FrogPilotManageControl *modelUIToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(modelUIToggle, &FrogPilotManageControl::manageButtonClicked, [visualsLayout, modelUIPanel]() {
        visualsLayout->setCurrentWidget(modelUIPanel);
      });
      visualToggle = modelUIToggle;
    } else if (param == "LaneLinesWidth" || param == "RoadEdgesWidth") {
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 24, tr(" inches"));
    } else if (param == "PathEdgeWidth") {
      std::map<float, QString> pathEdgeLabels;
      for (int i = 0; i <= 100; ++i) {
        pathEdgeLabels[i] = i == 0 ? tr("Off") : QString::number(i) + "%";
      }
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, QString(), pathEdgeLabels);
    } else if (param == "PathWidth") {
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, tr(" feet"), std::map<float, QString>(), 0.1);

    } else if (param == "NavigationUI") {
      FrogPilotManageControl *navigationUIToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(navigationUIToggle, &FrogPilotManageControl::manageButtonClicked, [visualsLayout, navigationUIPanel]() {
        visualsLayout->setCurrentWidget(navigationUIPanel);
      });
      visualToggle = navigationUIToggle;
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
        {7, tr("Mapbox Navigation Day")},
        {8, tr("Mapbox Navigation Night")},
        {5, tr("Mapbox Satellite")},
        {6, tr("Mapbox Satellite Streets")},
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

    } else if (param == "QOLVisuals") {
      FrogPilotManageControl *qolToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(qolToggle, &FrogPilotManageControl::manageButtonClicked, [visualsLayout, qualityOfLifePanel]() {
        visualsLayout->setCurrentWidget(qualityOfLifePanel);
      });
      visualToggle = qolToggle;
    } else if (param == "CameraView") {
      std::vector<QString> cameraOptions{tr("Auto"), tr("Driver"), tr("Standard"), tr("Wide")};
      ButtonParamControl *cameraSelection = new ButtonParamControl(param, title, desc, icon, cameraOptions);
      visualToggle = cameraSelection;

    } else {
      visualToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = visualToggle;

    if (advancedCustomOnroadUIKeys.find(param) != advancedCustomOnroadUIKeys.end()) {
      advancedCustomList->addItem(visualToggle);
    } else if (customOnroadUIKeys.find(param) != customOnroadUIKeys.end()) {
      customUIList->addItem(visualToggle);
    } else if (developerMetricKeys.find(param) != developerMetricKeys.end()) {
      developerMetricList->addItem(visualToggle);
    } else if (developerSidebarKeys.find(param) != developerSidebarKeys.end()) {
      developerSidebarList->addItem(visualToggle);
    } else if (developerUIKeys.find(param) != developerUIKeys.end()) {
      developerUIList->addItem(visualToggle);
    } else if (developerWidgetKeys.find(param) != developerWidgetKeys.end()) {
      developerWidgetList->addItem(visualToggle);
    } else if (modelUIKeys.find(param) != modelUIKeys.end()) {
      modelUIList->addItem(visualToggle);
    } else if (navigationUIKeys.find(param) != navigationUIKeys.end()) {
      navigationUIList->addItem(visualToggle);
    } else if (qualityOfLifeKeys.find(param) != qualityOfLifeKeys.end()) {
      qualityOfLifeList->addItem(visualToggle);
    } else {
      visualsList->addItem(visualToggle);

      parentKeys.insert(param);
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(visualToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, this, &FrogPilotVisualsPanel::openSubPanel);
    }

    QObject::connect(visualToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  std::set<QString> forceUpdateKeys = {"ShowSpeedLimits"};
  for (const QString &key : forceUpdateKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, this, &FrogPilotVisualsPanel::updateToggles);
  }

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [visualsLayout, visualsPanel] {visualsLayout->setCurrentWidget(visualsPanel);});
  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubSubPanel, [this, visualsLayout, developerUIPanel]() {
    if (developerUIOpen) {
      visualsLayout->setCurrentWidget(developerUIPanel);

      developerUIOpen = false;
    }
  });
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, this, &FrogPilotVisualsPanel::updateMetric);
}

void FrogPilotVisualsPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  hasAutoTune = parent->hasAutoTune;
  hasBSM = parent->hasBSM;
  hasOpenpilotLongitudinal = parent->hasOpenpilotLongitudinal;
  hasRadar = parent->hasRadar;
  tuningLevel = parent->tuningLevel;

  updateToggles();
}

void FrogPilotVisualsPanel::updateMetric(bool metric, bool bootRun) {
  static bool previousMetric;
  if (metric != previousMetric && !bootRun) {
    double distanceConversion = metric ? FOOT_TO_METER : METER_TO_FOOT;
    double smallDistanceConversion = metric ? INCH_TO_CM : CM_TO_INCH;

    params.putIntNonBlocking("LaneLinesWidth", params.getInt("LaneLinesWidth") * smallDistanceConversion);
    params.putIntNonBlocking("RoadEdgesWidth", params.getInt("RoadEdgesWidth") * smallDistanceConversion);

    params.putFloatNonBlocking("PathWidth", params.getFloat("PathWidth") * distanceConversion);
  }
  previousMetric = metric;

  static std::map<float, QString> imperialDistanceLabels;
  static std::map<float, QString> imperialSmallDistanceLabels;
  static std::map<float, QString> metricDistanceLabels;
  static std::map<float, QString> metricSmallDistanceLabels;

  static bool labelsInitialized = false;
  if (!labelsInitialized) {
    for (int i = 0; i <= 10; ++i) {
      imperialDistanceLabels[i] = i == 0 ? tr("Off") : i == 1 ? QString::number(i) + tr(" foot") : QString::number(i) + tr(" feet");
    }

    for (int i = 0; i <= 24; ++i) {
      imperialSmallDistanceLabels[i] = i == 0 ? tr("Off") : i == 1 ? QString::number(i) + tr(" inch") : QString::number(i) + tr(" inches");
    }

    for (int i = 0; i <= 3; ++i) {
      metricDistanceLabels[i] = i == 0 ? tr("Off") : i == 1 ? QString::number(i) + tr(" meter") : QString::number(i) + tr(" meters");
    }

    for (int i = 0; i <= 60; ++i) {
      metricSmallDistanceLabels[i] = i == 0 ? tr("Off") : i == 1 ? QString::number(i) + tr(" centimeter") : QString::number(i) + tr(" centimeters");
    }

    labelsInitialized = true;
  }

  FrogPilotParamValueControl *laneLinesWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["LaneLinesWidth"]);
  FrogPilotParamValueControl *pathWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["PathWidth"]);
  FrogPilotParamValueControl *roadEdgesWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["RoadEdgesWidth"]);

  if (metric) {
    laneLinesWidthToggle->setDescription(tr("The thickness of the lane lines on the driving screen.<br><br><b>Default matches the <b>MUTCD</b> lane line width standard of 10 centimeters."));
    roadEdgesWidthToggle->setDescription(tr("The thickness of the road edges on the driving screen.<br><br><b>Default matches half of the <b>MUTCD</b> lane line width standard of 10 centimeters."));

    laneLinesWidthToggle->updateControl(0, 60, metricSmallDistanceLabels);
    roadEdgesWidthToggle->updateControl(0, 60, metricSmallDistanceLabels);

    pathWidthToggle->updateControl(0, 3, metricDistanceLabels);
  } else {
    laneLinesWidthToggle->setDescription(tr("The thickness of the lane lines on the driving screen.<br><br><b>Default matches the <b>MUTCD</b> lane line width standard of 4 inches."));
    roadEdgesWidthToggle->setDescription(tr("The thickness of the road edges on the driving screen.<br><br><b>Default matches half of the <b>MUTCD</b> lane line width standard of 4 inches."));

    laneLinesWidthToggle->updateControl(0, 24, imperialSmallDistanceLabels);
    roadEdgesWidthToggle->updateControl(0, 24, imperialSmallDistanceLabels);

    pathWidthToggle->updateControl(0, 10, imperialDistanceLabels);
  }
}

void FrogPilotVisualsPanel::updateToggles() {
  for (auto &[key, toggle] : toggles) {
    if (parentKeys.find(key) != parentKeys.end()) {
      toggle->setVisible(false);
    }
  }

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.find(key) != parentKeys.end()) {
      continue;
    }

    bool setVisible = tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (key == "AccelerationPath") {
      setVisible &= hasOpenpilotLongitudinal;
    }

    if (key == "AdjacentLeadsUI") {
      setVisible &= hasRadar && !(params.getBool("AdvancedCustomUI") && params.getBool("HideLeadMarker"));
    }

    if (key == "BlindSpotPath") {
      setVisible &= hasBSM;
    }

    if (key == "HideLeadMarker") {
      setVisible &= hasOpenpilotLongitudinal;
    }

    if (key == "LeadInfo") {
      setVisible &= hasOpenpilotLongitudinal;
    }

    if (key == "OnroadDistanceButton") {
      setVisible &= hasOpenpilotLongitudinal;
    }

    if (key == "PedalsOnUI") {
      setVisible &= hasOpenpilotLongitudinal;
    }

    if (key == "RadarTracksUI") {
      setVisible &= hasRadar;
    }

    if (key == "ShowSpeedLimits") {
      setVisible &= !params.getBool("SpeedLimitController");
    }

    if (key == "ShowStoppingPoint") {
      setVisible &= hasOpenpilotLongitudinal;
    }

    if (key == "SLCMapboxFiller") {
      setVisible &= params.getBool("ShowSpeedLimits") && !(hasOpenpilotLongitudinal && params.getBool("SpeedLimitController"));
      setVisible &= !params_cache.get("MapboxSecretKey").empty();
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (advancedCustomOnroadUIKeys.find(key) != advancedCustomOnroadUIKeys.end()) {
        toggles["AdvancedCustomUI"]->setVisible(true);
      } else if (customOnroadUIKeys.find(key) != customOnroadUIKeys.end()) {
        toggles["CustomUI"]->setVisible(true);
      } else if (developerMetricKeys.find(key) != developerMetricKeys.end()) {
        toggles["DeveloperMetrics"]->setVisible(true);
      } else if (developerUIKeys.find(key) != developerUIKeys.end()) {
        toggles["DeveloperUI"]->setVisible(true);
      } else if (developerWidgetKeys.find(key) != developerWidgetKeys.end()) {
        toggles["DeveloperWidgets"]->setVisible(true);
      } else if (modelUIKeys.find(key) != modelUIKeys.end()) {
        toggles["ModelUI"]->setVisible(true);
      } else if (navigationUIKeys.find(key) != navigationUIKeys.end()) {
        toggles["NavigationUI"]->setVisible(true);
      } else if (qualityOfLifeKeys.find(key) != qualityOfLifeKeys.end()) {
        toggles["QOLVisuals"]->setVisible(true);
      }
    }
  }

  borderMetricsBtn->setVisibleButton(0, hasBSM);

  update();
}
