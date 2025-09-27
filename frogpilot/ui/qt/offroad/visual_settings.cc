#include "frogpilot/ui/qt/offroad/visual_settings.h"

FrogPilotVisualsPanel::FrogPilotVisualsPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

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
    {"AdvancedCustomUI", tr("Advanced UI Controls"), tr("<b>Advanced visual changes</b> to fine-tune how the driving screen looks."), "../../frogpilot/assets/toggle_icons/icon_advanced_device.png"},
    {"HideSpeed", tr("Hide Current Speed"), tr("<b>Hide the current speed</b> from the driving screen."), ""},
    {"HideLeadMarker", tr("Hide Lead Marker"), tr("<b>Hide the lead-vehicle marker</b> from the driving screen."), ""},
    {"HideMapIcon", tr("Hide Map Settings Button"), tr("<b>Hide the map settings button or map</b> from the driving screen."), ""},
    {"HideMaxSpeed", tr("Hide Max Speed"), tr("<b>Hide the max speed</b> from the driving screen."), ""},
    {"HideAlerts", tr("Hide Non-Critical Alerts"), tr("<b>Hide non-critical alerts</b> from the driving screen."), ""},
    {"HideSpeedLimit", tr("Hide Speed Limits"), tr("<b>Hide posted speed limits</b> from the driving screen."), ""},
    {"WheelSpeed", tr("Use Wheel Speed"), tr("<b>Use the vehicle's wheel speed</b> instead of the cluster speed. This is purely a visual change and doesn't impact how openpilot drives!"), ""},

    {"DeveloperUI", tr("Developer UI"), tr("<b>Detailed information about openpilot's internal operations.</b>"), "../assets/offroad/icon_shell.png"},
    {"AdjacentPathMetrics", tr("Adjacent Path Metrics"), tr("<b>Show the width of the adjacent lanes.</b>"), ""},
    {"DeveloperMetrics", tr("Developer Metrics"), tr("<b>Performance data, sensor readings, and system metrics</b> for debugging and optimizing openpilot."), ""},
    {"BorderMetrics", tr("Border Metrics"), tr("<b>Show statuses along the border of the driving screen.</b><br><br><b>Blind Spot</b>: The border turns red when a vehicle is in a blind spot<br><b>Steering Torque</b>: The border goes from green to red according to how much steering torque is being used<br><b>Turn Signal</b>: The border flashes yellow when a turn signal is on"), ""},
    {"LeadInfo", tr("Lead Info"), tr("<b>Show each tracked vehicle's distance and speed</b> below its marker."), ""},
    {"FPSCounter", tr("FPS Display"), tr("<b>Show the frames per second (FPS)</b> at the bottom of the driving screen."), ""},
    {"NumericalTemp", tr("Numerical Temperature Gauge"), tr("<b>Show a numerical temperature in the sidebar</b> instead of the status labels."), ""},
    {"SidebarMetrics", tr("Sidebar Metrics"), tr("<b>Display system information</b> (CPU, GPU, RAM usage, IP address, device storage) in the sidebar."), ""},
    {"UseSI", tr("Use International System of Units"), tr("<b>Display measurements using the \"International System of Units\" (SI)</b> standard."), ""},
    {"DeveloperSidebar", tr("Developer Sidebar"), tr("<b>Display debugging info and metrics</b> in a dedicated sidebar on the right side of the screen."), ""},
    {"DeveloperSidebarMetric1", tr("Metric #1"), tr("<b>Select the metric shown in the first \"Developer Sidebar\" widget.</b>"), ""},
    {"DeveloperSidebarMetric2", tr("Metric #2"), tr("<b>Select the metric shown in the second \"Developer Sidebar\" widget.</b>"), ""},
    {"DeveloperSidebarMetric3", tr("Metric #3"), tr("<b>Select the metric shown in the third \"Developer Sidebar\" widget.</b>"), ""},
    {"DeveloperSidebarMetric4", tr("Metric #4"), tr("<b>Select the metric shown in the fourth \"Developer Sidebar\" widget.</b>"), ""},
    {"DeveloperSidebarMetric5", tr("Metric #5"), tr("<b>Select the metric shown in the fifth \"Developer Sidebar\" widget.</b>"), ""},
    {"DeveloperSidebarMetric6", tr("Metric #6"), tr("<b>Select the metric shown in the sixth \"Developer Sidebar\" widget.</b>"), ""},
    {"DeveloperSidebarMetric7", tr("Metric #7"), tr("<b>Select the metric shown in the seventh \"Developer Sidebar\" widget.</b>"), ""},
    {"DeveloperWidgets", tr("Developer Widgets"), tr("<b>Overlays for debugging visuals, internal states, and model predictions</b> on the driving screen."), ""},
    {"AdjacentLeadsUI", tr("Adjacent Leads Tracking"), tr("<b>Display adjacent leads detected by the car's radar</b> to the left and right of the current driving path."), ""},
    {"ShowStoppingPoint", tr("Model Stopping Point"), tr("<b>Show a stop-sign marker where the model intends to stop.</b>"), ""},
    {"RadarTracksUI", tr("Radar Tracks"), tr("<b>Display all radar points</b> produced by the car's radar."), ""},

    {"CustomUI", tr("Driving Screen Widgets"), tr("<b>Custom FrogPilot widgets</b> for the driving screen."), "../assets/offroad/icon_road.png"},
    {"AccelerationPath", tr("Acceleration Path"), tr("<b>Color the driving path by planned acceleration and braking.</b>"), ""},
    {"AdjacentPath", tr("Adjacent Lanes"), tr("<b>Show the driving paths for the left and right lanes.</b>"), ""},
    {"BlindSpotPath", tr("Blind Spot Path"), tr("<b>Show a red path when a vehicle is in that lane's blind spot.</b>"), ""},
    {"Compass", tr("Compass"), tr("<b>Show the current driving direction</b> with a simple on-screen compass."), ""},
    {"OnroadDistanceButton", tr("Driving Personality Button"), tr("<b>Control and view the current driving personality</b> via a driving screen widget."), ""},
    {"PedalsOnUI", tr("Gas / Brake Pedal Indicators"), tr("<b>On-screen gas and brake indicators.</b><br><br><b>Dynamic</b>: Opacity changes according to how much openpilot is accelerating or braking<br><b>Static</b>: Full when active, dim when not"), ""},
    {"RotatingWheel", tr("Rotating Steering Wheel"), tr("<b>Rotate the driving screen wheel</b> with the physical steering wheel."), ""},

    {"ModelUI", tr("Model UI"), tr("<b>Model visualizations</b> for the driving path, lane lines, path edges, and road edges."), "../../frogpilot/assets/toggle_icons/icon_vtc.png"},
    {"DynamicPathWidth", tr("Dynamic Path Width"), tr("<b>Change the path width based on engagement.</b><br><br><b>Fully Engaged</b>: 100%<br><b>Always On Lateral</b>: 75%<br><b>Disengaged</b>: 50%"), ""},
    {"LaneLinesWidth", tr("Lane Lines Width"), tr("<b>Set the lane-line thickness.</b><br><br>Default matches the MUTCD lane-line width standard of 4 inches."), ""},
    {"PathEdgeWidth", tr("Path Edges Width"), tr("<b>Set the driving-path edge width</b> that represents different driving modes and statuses.<br><br>Default is 20% of the total path width.<br><br>Color Guide:<br><br>- <b>Blue</b>: Navigation<br>- <b>Light Blue</b>: Always On Lateral<br>- <b>Green</b>: Default<br>- <b>Orange</b>: Experimental Mode<br>- <b>Red</b>: Traffic Mode<br>- <b>Yellow</b>: Conditional Experimental Mode overridden"), ""},
    {"PathWidth", tr("Path Width"), tr("<b>Set the driving-path width.</b><br><br>Default (6.1 feet) matches the width of a 2019 Lexus ES 350."), ""},
    {"RoadEdgesWidth", tr("Road Edges Width"), tr("<b>Set the road-edge thickness.</b><br><br>Default matches half of the MUTCD lane-line width standard of 4 inches."), ""},
    {"UnlimitedLength", tr("\"Unlimited\" Road UI"), tr("<b>Extend the length of the driving path, lane lines, and road edges</b> for as far as the model can see."), ""},

    {"NavigationUI", tr("Navigation Widgets"), tr("<b>Map style, speed limits, and other navigation widgets.</b>"), "../../frogpilot/assets/toggle_icons/icon_map.png"},
    {"BigMap", tr("Larger Map Display"), tr("<b>Increase the map size</b> for easier navigation readings."), ""},
    {"MapStyle", tr("Map Style"), tr("<b>Select the map style</b> for \"Navigate on openpilot\" (NOO):<br><br><b>Stock openpilot</b>: Default comma.ai style<br><b>FrogPilot</b>: Official FrogPilot map style<br><b>Mapbox Streets</b>: Standard street-focused view<br><b>Mapbox Outdoors</b>: Emphasizes outdoor and terrain features<br><b>Mapbox Light</b>: Minimalist, bright theme<br><b>Mapbox Dark</b>: Minimalist, dark theme<br><b>Mapbox Navigation Day</b>: Optimized for daytime navigation<br><b>Mapbox Navigation Night</b>: Optimized for nighttime navigation<br><b>Mapbox Satellite</b>: Satellite imagery only<br><b>Mapbox Satellite Streets</b>: Hybrid satellite imagery with street labels<br><b>Mapbox Traffic Night</b>: Dark theme emphasizing traffic conditions<br><b>Mike's Personalized Style</b>: Customized hybrid satellite view"), ""},
    {"RoadNameUI", tr("Road Name"), tr("<b>Display the road name at the bottom of the driving screen</b> using data from \"OpenStreetMap (OSM)\"."), ""},
    {"ShowSpeedLimits", tr("Show Speed Limits"), tr("<b>Show speed limits</b> in the top-left corner of the driving screen. Uses data from the car's dashboard (if supported) and \"OpenStreetMap (OSM)\"."), ""},
    {"SLCMapboxFiller", tr("Show Speed Limits from Mapbox"), tr("<b>Use Mapbox speed-limit data when no other source is available.</b>"), ""},
    {"UseVienna", tr("Use Vienna-Style Speed Signs"), tr("<b>Show Vienna-style (EU) speed-limit signs</b> instead of MUTCD (US)."), ""},

    {"QOLVisuals", tr("Quality of Life"), tr("<b>Miscellaneous visual changes</b> to fine-tune how the driving screen looks."), "../../frogpilot/assets/toggle_icons/icon_quality_of_life.png"},
    {"CameraView", tr("Camera View"), tr("<b>Select the active camera view.</b> This is purely a visual change and doesn't impact how openpilot drives!"), ""},
    {"DriverCamera", tr("Show Driver Camera When In Reverse"), tr("<b>Show the driver camera feed</b> when the vehicle is in reverse."), ""},
    {"StoppedTimer", tr("Stopped Timer"), tr("<b>Show a timer when stopped</b> in place of the current speed to indicate how long the vehicle has been stopped."), ""}
  };

  for (const auto &[param, title, desc, icon] : visualToggles) {
    AbstractControl *visualToggle;

    if (param == "AdvancedCustomUI") {
      FrogPilotManageControl *advancedCustomUIToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(advancedCustomUIToggle, &FrogPilotManageControl::manageButtonClicked, [visualsLayout, advancedCustomPanel]() {
        visualsLayout->setCurrentWidget(advancedCustomPanel);
      });
      visualToggle = advancedCustomUIToggle;
    } else if (param == "HideMapIcon") {
      std::vector<QString> mapIconToggles{"HideMap"};
      std::vector<QString> mapIconToggleNames{tr("Hide Map")};
      visualToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, mapIconToggles, mapIconToggleNames);

    } else if (param == "DeveloperUI") {
      FrogPilotManageControl *developerUIToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(developerUIToggle, &FrogPilotManageControl::manageButtonClicked, [visualsLayout, developerUIPanel]() {
        visualsLayout->setCurrentWidget(developerUIPanel);
      });
      visualToggle = developerUIToggle;
    } else if (param == "DeveloperMetrics") {
      FrogPilotManageControl *developerMetricsToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(developerMetricsToggle, &FrogPilotManageControl::manageButtonClicked, [visualsLayout, developerMetricPanel, this]() {
        openSubSubPanel();

        visualsLayout->setCurrentWidget(developerMetricPanel);

        developerUIOpen = true;
      });
      visualToggle = developerMetricsToggle;
    } else if (param == "BorderMetrics") {
      std::vector<QString> borderToggles{"BlindSpotMetrics", "ShowSteering", "SignalMetrics"};
      std::vector<QString> borderToggleNames{tr("Blind Spot"), tr("Steering Torque"), tr("Turn Signal")};
      borderMetricsButton = new FrogPilotButtonToggleControl(param, title, desc, icon, borderToggles, borderToggleNames);
      visualToggle = borderMetricsButton;
    } else if (param == "NumericalTemp") {
      std::vector<QString> temperatureToggles{"Fahrenheit"};
      std::vector<QString> temperatureToggleNames{tr("Fahrenheit")};
      visualToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, temperatureToggles, temperatureToggleNames);
    } else if (param == "SidebarMetrics") {
      sidebarMetricsToggles = {"ShowCPU", "ShowGPU", "ShowIP", "ShowMemoryUsage", "ShowStorageLeft", "ShowStorageUsed"};
      std::vector<QString> sidebarMetricsToggleNames{tr("CPU"), tr("GPU"), tr("IP"), tr("RAM"), tr("SSD Left"), tr("SSD Used")};
      sidebarMetricsToggle = new FrogPilotButtonsControl(title, desc, icon, sidebarMetricsToggleNames, true, false, 150);
      for (int i = 0; i < sidebarMetricsToggles.size(); ++i) {
        if (params.getBool(sidebarMetricsToggles[i].toStdString())) {
          sidebarMetricsToggle->setCheckedButton(i);
        }
      }
      QObject::connect(sidebarMetricsToggle, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        params.putBool(sidebarMetricsToggles[id].toStdString(), !params.getBool(sidebarMetricsToggles[id].toStdString()));

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

        sidebarMetricsToggle->clearCheckedButtons();
        for (int i = 0; i < sidebarMetricsToggles.size(); ++i) {
          if (params.getBool(sidebarMetricsToggles[i].toStdString())) {
            sidebarMetricsToggle->setCheckedButton(i);
          }
        }
      });
      visualToggle = sidebarMetricsToggle;
    } else if (param == "DeveloperSidebar") {
      FrogPilotManageControl *developerSidebarToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(developerSidebarToggle, &FrogPilotManageControl::manageButtonClicked, [visualsLayout, developerSidebarPanel, this]() {
        openSubSubPanel();

        visualsLayout->setCurrentWidget(developerSidebarPanel);

        developerUIOpen = true;
      });
      visualToggle = developerSidebarToggle;
    } else if (developerSidebarKeys.contains(param)) {
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
      QObject::connect(metricToggle, &ButtonControl::clicked, [metricToggle, key = param, developerSidebarMetricOptions, this]() mutable {
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
      QObject::connect(developerWidgetsToggle, &FrogPilotManageControl::manageButtonClicked, [visualsLayout, developerWidgetPanel, this]() {
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
        {0, tr("Stock openpilot")},
        {1, tr("FrogPilot")},
        {2, tr("Mapbox Streets")},
        {3, tr("Mapbox Outdoors")},
        {4, tr("Mapbox Light")},
        {5, tr("Mapbox Dark")},
        {6, tr("Mapbox Navigation Day")},
        {7, tr("Mapbox Navigation Night")},
        {8, tr("Mapbox Satellite")},
        {9, tr("Mapbox Satellite Streets")},
        {10, tr("Mapbox Traffic Night")},
        {11, tr("Mike's Personalized Style")}
      };

      ButtonControl *mapStyleButton = new ButtonControl(title, tr("SELECT"), desc);
      QObject::connect(mapStyleButton, &ButtonControl::clicked, [mapStyleButton, styleMap, this]() {
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

    if (advancedCustomOnroadUIKeys.contains(param)) {
      advancedCustomList->addItem(visualToggle);
    } else if (customOnroadUIKeys.contains(param)) {
      customUIList->addItem(visualToggle);
    } else if (developerMetricKeys.contains(param)) {
      developerMetricList->addItem(visualToggle);
    } else if (developerSidebarKeys.contains(param)) {
      developerSidebarList->addItem(visualToggle);
    } else if (developerUIKeys.contains(param)) {
      developerUIList->addItem(visualToggle);
    } else if (developerWidgetKeys.contains(param)) {
      developerWidgetList->addItem(visualToggle);
    } else if (modelUIKeys.contains(param)) {
      modelUIList->addItem(visualToggle);
    } else if (navigationUIKeys.contains(param)) {
      navigationUIList->addItem(visualToggle);
    } else if (qualityOfLifeKeys.contains(param)) {
      qualityOfLifeList->addItem(visualToggle);
    } else {
      visualsList->addItem(visualToggle);

      parentKeys.insert(param);
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(visualToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        emit openSubPanel();
        openDescriptions(forceOpenDescriptions, toggles);
      });
    }

    QObject::connect(visualToggle, &AbstractControl::hideDescriptionEvent, [this]() {
      update();
    });
    QObject::connect(visualToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  QSet<QString> forceUpdateKeys = {"HideLeadMarker", "ShowSpeedLimits"};
  for (const QString &key : forceUpdateKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, this, &FrogPilotVisualsPanel::updateToggles);
  }

  openDescriptions(forceOpenDescriptions, toggles);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [visualsLayout, visualsPanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    visualsLayout->setCurrentWidget(visualsPanel);
  });
  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubSubPanel, [visualsLayout, developerUIPanel, this]() {
    openDescriptions(forceOpenDescriptions, toggles);

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

  for (int i = 0; i < sidebarMetricsToggles.size(); ++i) {
    if (params.getBool(sidebarMetricsToggles[i].toStdString())) {
      sidebarMetricsToggle->setCheckedButton(i);
    }
  }

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

    for (float i = 0.0f; i <= 3.0f; i += 0.1f) {
      metricDistanceLabels[i] = i == 0.0f ? tr("Off") : i == 1.0 ? QString::number(i) + tr(" meter") : QString::number(i, 'f', 1) + tr(" meters");
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
    laneLinesWidthToggle->setDescription(tr("<b>Set the lane-line thickness.</b><br><br>Default matches the MUTCD lane-line width standard of 10 centimeters."));
    pathWidthToggle->setDescription(tr("<b>Set the driving-path width.</b><br><br>Default (1.9 meters) matches the width of a 2019 Lexus ES 350."));
    roadEdgesWidthToggle->setDescription(tr("<b>Set the road-edge thickness.</b><br><br>Default matches half of the MUTCD lane-line width standard of 10 centimeters."));

    laneLinesWidthToggle->updateControl(0, 60, metricSmallDistanceLabels);
    roadEdgesWidthToggle->updateControl(0, 60, metricSmallDistanceLabels);

    pathWidthToggle->updateControl(0, 3, metricDistanceLabels);
  } else {
    laneLinesWidthToggle->setDescription(tr("<b>Set the lane-line thickness.</b><br><br>Default matches the MUTCD lane-line width standard of 4 inches."));
    pathWidthToggle->setDescription(tr("<b>Set the driving-path width.</b><br><br>Default (6.1 feet) matches the width of a 2019 Lexus ES 350."));
    roadEdgesWidthToggle->setDescription(tr("<b>Set the road-edge thickness.</b><br><br>Default matches half of the MUTCD lane-line width standard of 4 inches."));

    laneLinesWidthToggle->updateControl(0, 24, imperialSmallDistanceLabels);
    roadEdgesWidthToggle->updateControl(0, 24, imperialSmallDistanceLabels);

    pathWidthToggle->updateControl(0, 10, imperialDistanceLabels);
  }
}

void FrogPilotVisualsPanel::updateToggles() {
  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      toggle->setVisible(false);
    }
  }

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      continue;
    }

    bool setVisible = tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (key == "AccelerationPath") {
      setVisible &= hasOpenpilotLongitudinal;
    }

    else if (key == "AdjacentLeadsUI") {
      setVisible &= hasRadar && !(params.getBool("AdvancedCustomUI") && params.getBool("HideLeadMarker"));
    }

    else if (key == "BlindSpotPath") {
      setVisible &= hasBSM;
    }

    else if (key == "HideLeadMarker") {
      setVisible &= hasOpenpilotLongitudinal;
    }

    else if (key == "LeadInfo") {
      setVisible &= hasOpenpilotLongitudinal;
    }

    else if (key == "OnroadDistanceButton") {
      setVisible &= hasOpenpilotLongitudinal;
    }

    else if (key == "PedalsOnUI") {
      setVisible &= hasOpenpilotLongitudinal;
    }

    else if (key == "RadarTracksUI") {
      setVisible &= hasRadar;
    }

    else if (key == "ShowSpeedLimits") {
      setVisible &= !params.getBool("SpeedLimitController") || !hasOpenpilotLongitudinal;
    }

    else if (key == "ShowStoppingPoint") {
      setVisible &= hasOpenpilotLongitudinal;
    }

    else if (key == "SLCMapboxFiller") {
      setVisible &= params.getBool("ShowSpeedLimits") && !(hasOpenpilotLongitudinal && params.getBool("SpeedLimitController"));
      setVisible &= !params.get("MapboxSecretKey").empty();
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (advancedCustomOnroadUIKeys.contains(key)) {
        toggles["AdvancedCustomUI"]->setVisible(true);
      } else if (customOnroadUIKeys.contains(key)) {
        toggles["CustomUI"]->setVisible(true);
      } else if (developerMetricKeys.contains(key)) {
        toggles["DeveloperMetrics"]->setVisible(true);
      } else if (developerUIKeys.contains(key)) {
        toggles["DeveloperUI"]->setVisible(true);
      } else if (developerWidgetKeys.contains(key)) {
        toggles["DeveloperWidgets"]->setVisible(true);
      } else if (modelUIKeys.contains(key)) {
        toggles["ModelUI"]->setVisible(true);
      } else if (navigationUIKeys.contains(key)) {
        toggles["NavigationUI"]->setVisible(true);
      } else if (qualityOfLifeKeys.contains(key)) {
        toggles["QOLVisuals"]->setVisible(true);
      }
    }
  }

  borderMetricsButton->setVisibleButton(0, hasBSM);

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
