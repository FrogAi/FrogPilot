#include "frogpilot/ui/qt/offroad/visual_settings.h"

QMap<int, QString> FrogPilotVisualsPanel::developerMetricOptions() {
  return {
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
}

QMap<int, QString> FrogPilotVisualsPanel::mapStyleOptions() {
  return {
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
}

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
    {"AdvancedCustomUI", tr("Advanced UI Controls"), tr("<b>Fine-tune how the driving screen looks, beyond what the everyday settings cover.</b><br><br>None of these change how the car drives."), "../../frogpilot/assets/toggle_icons/icon_advanced_device.png"},
    {"HideSpeed", tr("Hide Current Speed"), tr("<b>Take your current speed off the driving screen.</b><br><br>Your dashboard still shows it."), ""},
    {"HideLeadMarker", tr("Hide Lead Marker"), tr("<b>Take the marker off the car in front of you on the driving screen.</b><br><br>openpilot still tracks that car and still follows it.<br><br>\"Lead Info\" and \"Adjacent Leads Tracking\" have nothing left to attach to, so they disappear too."), ""},
    {"HideMapIcon", tr("Hide Map Settings Button"), tr("<b>Take the map button off the driving screen.</b><br><br>Tapping the driving screen still opens the map. Switching on \"Hide Map\" as well stops that too, but that also takes away your turn-by-turn directions."), ""},
    {"HideMaxSpeed", tr("Hide Max Speed"), tr("<b>Take the speed you have set off the driving screen.</b><br><br>openpilot still holds that speed."), ""},
    {"HideAlerts", tr("Hide Non-Critical Alerts"), tr("<b>Take the everyday informational messages off the driving screen.</b><br><br>Anything that actually needs you, like a warning or a takeover request, still comes through."), ""},
    {"HideSpeedLimit", tr("Hide Speed Limits"), tr("<b>Take the posted speed limit sign off the driving screen.</b><br><br>If you use \"Speed Limit Controller\" it keeps working on the limit it reads, so your speed can still change for a sign you can no longer see. The sign does come back when it asks you to confirm a new limit."), ""},
    {"WheelSpeed", tr("Use Wheel Speed"), tr("<b>Show the speed your wheels are actually turning at instead of the slightly optimistic number your dashboard shows.</b><br><br>Most cars read a little high on purpose, so this usually reads one or two lower. It only changes the number on screen, never how openpilot drives."), ""},

    {"DeveloperUI", tr("Developer UI"), tr("<b>Put openpilot's internal numbers on screen, so you can see what it is thinking while it drives.</b><br><br>None of it changes how the car behaves. It is for working out why openpilot did something, and it is worth turning back off once you are done."), "../assets/offroad/icon_shell.png"},
    {"DeveloperMetrics", tr("Developer Metrics"), tr("<b>Show readouts of what openpilot's sensors and hardware are doing while you drive.</b>"), ""},
    {"AdjacentPathMetrics", tr("Adjacent Path Metrics"), tr("<b>Print how wide the lanes either side of you are, right on their paths.</b><br><br>It draws the adjacent lane paths itself, so there is something to print onto, even with \"Adjacent Lanes\" switched off."), ""},
    {"BorderMetrics", tr("Border Metrics"), tr("<b>Colour the edge of the driving screen to show what openpilot is reacting to.</b><br><br>You can switch on the blind spot, steering effort and turn signal borders separately. The blind spot one needs a car with factory blind spot monitoring, and stays dark without it."), ""},
    {"FPSCounter", tr("FPS Display"), tr("<b>Show how many frames a second the driving screen is drawing, along the bottom.</b><br><br>It also tracks the lowest and highest it has seen."), ""},
    {"LeadInfo", tr("Lead Info"), tr("<b>Print how far away the car ahead is and how fast it is going, underneath its marker.</b><br><br>It also shows the gap openpilot is aiming for, so you can see it closing in on the distance you asked for."), ""},
    {"NumericalTemp", tr("Numerical Temperature Gauge"), tr("<b>Show the device temperature as a number in the sidebar instead of a word like \"GOOD\".</b><br><br>Useful for spotting the device getting hot before it slows itself down."), ""},
    {"SidebarMetrics", tr("Sidebar Metrics"), tr("<b>Fill the sidebar with how hard the device is working, how much storage is left and its IP address.</b><br><br>They take the place of the vehicle, connection and signal indicators normally shown there.<br><br>CPU and GPU cannot both be on, and RAM, SSD Left and SSD Used are one at a time, so picking one clears the other."), ""},
    {"UseSI", tr("Use International System of Units"), tr("<b>Show the developer readouts in plain scientific units instead of the friendlier ones.</b><br><br>This only affects the developer numbers, not the speed or distances you drive by."), ""},
    {"DeveloperSidebar", tr("Developer Sidebar"), tr("<b>Add a second sidebar down the right-hand side, holding up to seven readouts you pick yourself.</b><br><br>It sits over part of the camera view, so it is worth turning off when you are not using it. Setting all seven to \"None\" leaves the bar there but empty."), ""},
    {"DeveloperSidebarMetric1", tr("Metric #1"), tr("<b>Choose what the first slot in the developer sidebar shows.</b><br><br>The list covers steering, acceleration, engagement and auto-tune readouts. Every slot starts already filled in, and picking \"None\" closes the gap rather than leaving a hole."), ""},
    {"DeveloperSidebarMetric2", tr("Metric #2"), tr("<b>Choose what the second slot in the developer sidebar shows.</b>"), ""},
    {"DeveloperSidebarMetric3", tr("Metric #3"), tr("<b>Choose what the third slot in the developer sidebar shows.</b>"), ""},
    {"DeveloperSidebarMetric4", tr("Metric #4"), tr("<b>Choose what the fourth slot in the developer sidebar shows.</b>"), ""},
    {"DeveloperSidebarMetric5", tr("Metric #5"), tr("<b>Choose what the fifth slot in the developer sidebar shows.</b>"), ""},
    {"DeveloperSidebarMetric6", tr("Metric #6"), tr("<b>Choose what the sixth slot in the developer sidebar shows.</b>"), ""},
    {"DeveloperSidebarMetric7", tr("Metric #7"), tr("<b>Choose what the seventh slot in the developer sidebar shows.</b>"), ""},
    {"DeveloperWidgets", tr("Developer Widgets"), tr("<b>Draw what openpilot's model and radar are seeing straight onto the driving screen.</b><br><br>Your car needs radar for two of these, and openpilot handling the gas and brake for the third, so this menu can come up almost empty."), ""},
    {"AdjacentLeadsUI", tr("Adjacent Leads Tracking"), tr("<b>Mark the cars your radar sees in the lanes either side of you.</b><br><br>Your car needs radar for these to appear, and they hang off the same marker \"Hide Lead Marker\" removes."), ""},
    {"ShowStoppingPoint", tr("Model Stopping Point"), tr("<b>Drop a stop sign on the road where openpilot is planning to come to a stop.</b><br><br>It only marks stops openpilot works out for itself, so it stays away when openpilot is slowing for a car ahead, and \"Traffic Mode\" switches it off entirely."), ""},
    {"RadarTracksUI", tr("Radar Tracks"), tr("<b>Show every object your car's radar is returning, not just the car you are following.</b><br><br>Expect a lot of dots, including signs, barriers and parked cars."), ""},

    {"CustomUI", tr("Driving Screen Widgets"), tr("<b>Add extra things to the driving screen that stock openpilot does not show.</b><br><br>One of these does change how the car drives: the \"Driving Personality Button\" switches your following distance."), "../assets/offroad/icon_road.png"},
    {"AccelerationPath", tr("Acceleration Path"), tr("<b>Colour the driving path green when openpilot is speeding up and red when it is slowing down.</b><br><br>Handy for seeing a slowdown coming before you feel it."), ""},
    {"AdjacentPath", tr("Adjacent Lanes"), tr("<b>Draw the paths of the lanes either side of you, so you can see where openpilot thinks they run.</b><br><br>They only appear above about 20 mph, and only where the lane beside you measures wide enough to be a real lane."), ""},
    {"BlindSpotPath", tr("Blind Spot Path"), tr("<b>Turn the lane beside you red whenever your car's sensors see something in that blind spot.</b><br><br>It only shows up above about 20 mph, so it stays away in slow traffic and car parks. Keep checking your mirrors regardless."), ""},
    {"Compass", tr("Compass"), tr("<b>Add a compass to the driving screen showing which way you are heading.</b><br><br>Without a GPS fix it freezes pointing north rather than disappearing, so treat a compass that never moves as no reading at all."), ""},
    {"OnroadDistanceButton", tr("Driving Personality Button"), tr("<b>Add a button to the driving screen that switches your following distance between Aggressive, Standard and Relaxed without going into the menus.</b><br><br>This changes how the car actually drives, not just what you see. It also shows which one is active."), ""},
    {"PedalsOnUI", tr("Gas / Brake Pedal Indicators"), tr("<b>Show gas and brake indicators on the driving screen so you can see what openpilot is doing with the pedals.</b><br><br>\"Dynamic\" fades them in and out with how hard it is pressing. \"Static\" shows them fully lit when active and dim when not."), ""},
    {"RotatingWheel", tr("Rotating Steering Wheel"), tr("<b>Turn the steering wheel picture on screen in time with your real steering wheel.</b>"), ""},

    {"ModelUI", tr("Model UI"), tr("<b>Change how openpilot draws the road ahead, including the driving path, the lane lines and the road edges.</b><br><br>Turning this off falls back to stock openpilot's sizes, though the coloured path edges go away with it."), "../../frogpilot/assets/toggle_icons/icon_road.png"},
    {"DynamicPathWidth", tr("Dynamic Path Width"), tr("<b>Make the driving path narrower when openpilot is doing less of the driving, so you can tell at a glance how much control it has.</b><br><br>Full width while openpilot is driving, three quarters while it is only steering for you, and half the rest of the time."), ""},
    {"LaneLinesWidth", tr("Lane Lines Width"), tr("<b>Set how thick the lane lines are drawn on the driving screen.</b><br><br>The default matches the 4 inch lines actually painted on US roads. This only changes the picture, never where openpilot steers."), ""},
    {"PathEdgeWidth", tr("Path Edges Width"), tr("<b>Set how thick the coloured stripe down each side of the driving path is, which is what tells you which mode openpilot is in.</b><br><br>The default is a fifth of the path width. Set it to zero to hide the stripe entirely.<br><br>Blue means navigation is steering, light blue means openpilot is only steering for you, green is normal driving, orange is Experimental Mode, red is Traffic Mode, and yellow means you have overridden Conditional Experimental Mode."), ""},
    {"PathWidth", tr("Path Width"), tr("<b>Set how wide the driving path is drawn ahead of your car.</b><br><br>The default of 6.1 feet is roughly the width of a real car."), ""},
    {"RoadEdgesWidth", tr("Road Edges Width"), tr("<b>Set how thick the road edges are drawn on the driving screen.</b><br><br>The default is half a lane line."), ""},
    {"UnlimitedLength", tr("\"Unlimited\" Road UI"), tr("<b>Draw the path, lane lines and road edges as far ahead as the model can see, instead of stopping short.</b><br><br>The far end is the model's best guess, so it wanders more the further out it goes."), ""},

    {"NavigationUI", tr("Navigation Widgets"), tr("<b>Change what navigation shows on the driving screen, from the map itself to speed limit signs.</b><br><br>Some of these need map data downloaded under \"Maps and Navigation\" before they show anything."), "../../frogpilot/assets/toggle_icons/icon_map.png"},
    {"BigMap", tr("Larger Map Display"), tr("<b>Make the map on the driving screen bigger so it is easier to read at a glance.</b>"), ""},
    {"MapStyle", tr("Map Style"), tr("<b>Choose how the navigation map looks, from plain streets to satellite imagery.</b><br><br><b>Stock openpilot</b>: Default comma.ai style<br><b>FrogPilot</b>: Official FrogPilot map style<br><b>Mapbox Streets</b>: Standard street-focused view<br><b>Mapbox Outdoors</b>: Emphasizes outdoor and terrain features<br><b>Mapbox Light</b>: Minimalist, bright theme<br><b>Mapbox Dark</b>: Minimalist, dark theme<br><b>Mapbox Navigation Day</b>: Optimized for daytime navigation<br><b>Mapbox Navigation Night</b>: Optimized for nighttime navigation<br><b>Mapbox Satellite</b>: Satellite imagery only<br><b>Mapbox Satellite Streets</b>: Hybrid satellite imagery with street labels<br><b>Mapbox Traffic Night</b>: Dark theme emphasizing traffic conditions<br><b>Mike's Personalized Style</b>: Customized hybrid satellite view"), ""},
    {"RoadNameUI", tr("Road Name"), tr("<b>Show the name of the road you are on along the bottom of the driving screen.</b><br><br>It comes from downloaded map data, so it stays blank on roads you have not downloaded."), ""},
    {"ShowSpeedLimits", tr("Show Speed Limits"), tr("<b>Show the posted speed limit as a sign in the top-left corner of the driving screen.</b><br><br>The limit comes from your car's dashboard where it can read one, and from your downloaded map data otherwise."), ""},
    {"SLCMapboxFiller", tr("Show Speed Limits from Mapbox"), tr("<b>Fall back to Mapbox for the speed limit when neither your dashboard nor your downloaded maps know one.</b><br><br>Needs your Public Mapbox Key set up under \"Maps and Navigation\" and a working internet connection."), ""},
    {"UseVienna", tr("Use Vienna-Style Speed Signs"), tr("<b>Draw speed limit signs in the round European style instead of the rectangular American one.</b><br><br>Needs \"Show Speed Limits\" or \"Speed Limit Controller\" switched on, or the sign disappears instead of changing shape. It never changes the limit openpilot reads."), ""},

    {"QOLVisuals", tr("Quality of Life"), tr("<b>Pick which camera you watch, and a couple of smaller driving screen touches that did not fit anywhere else.</b>"), "../../frogpilot/assets/toggle_icons/icon_quality_of_life.png"},
    {"CameraView", tr("Camera View"), tr("<b>Choose which camera the driving screen shows, from the wide one to the driver-facing one.</b><br><br>openpilot keeps using every camera to drive no matter which one you put on screen."), ""},
    {"DriverCamera", tr("Show Driver Camera When In Reverse"), tr("<b>Switch the driving screen to the driver camera whenever you put the car in reverse.</b><br><br>This is not a backup camera. It faces you, not the road behind."), ""},
    {"StoppedTimer", tr("Stopped Timer"), tr("<b>Replace your speed with a running timer once you come to a complete stop, so you can see how long you have been waiting.</b>"), ""}
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
      QMap<int, QString> developerSidebarMetricOptions = developerMetricOptions();

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
      QMap<int, QString> styleMap = mapStyleOptions();

      ButtonControl *mapStyleButton = new ButtonControl(title, tr("SELECT"), desc);
      QObject::connect(mapStyleButton, &ButtonControl::clicked, [mapStyleButton, styleMap, this]() {
        QString selection = MultiOptionDialog::getSelection(tr("Select a map style"), styleMap.values(), styleMap.value(params.getInt("MapStyle")), this);
        if (!selection.isEmpty()) {
          int selectedStyle = styleMap.key(selection);

          params.putInt("MapStyle", selectedStyle);

          mapStyleButton->setValue(selection);
        }
      });

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

  QSet<QString> forceUpdateKeys = {"AdvancedCustomUI", "HideLeadMarker", "ShowSpeedLimits"};
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

  const QMap<int, QString> metricOptions = developerMetricOptions();
  for (const QString &key : developerSidebarKeys) {
    static_cast<ButtonControl*>(toggles[key])->setValue(metricOptions.value(params.getInt(key.toStdString()), tr("None")));
  }
  static_cast<ButtonControl*>(toggles["MapStyle"])->setValue(mapStyleOptions().value(params.getInt("MapStyle")));

  sidebarMetricsToggle->clearCheckedButtons();
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

    long smallDistanceMax = metric ? 60 : 24;
    float distanceMax = metric ? 3.0f : 10.0f;

    params.putInt("LaneLinesWidth", std::clamp<long>(std::lround(params.getInt("LaneLinesWidth") * smallDistanceConversion), 0, smallDistanceMax));
    params.putInt("RoadEdgesWidth", std::clamp<long>(std::lround(params.getInt("RoadEdgesWidth") * smallDistanceConversion), 0, smallDistanceMax));

    params.putFloat("PathWidth", std::clamp<float>(std::round(params.getFloat("PathWidth") * distanceConversion * 10.0f) / 10.0f, 0.0f, distanceMax));
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

    for (int i = 0; i <= 30; ++i) {
      float key = i / 10.0f;
      metricDistanceLabels[key] = i == 0 ? tr("Off") : i == 10 ? QString::number(key, 'f', 1) + tr(" meter") : QString::number(key, 'f', 1) + tr(" meters");
    }

    for (int i = 0; i <= 60; ++i) {
      metricSmallDistanceLabels[i] = i == 0 ? tr("Off") : i == 1 ? QString::number(i) + tr(" cm") : QString::number(i) + tr(" cm");
    }

    labelsInitialized = true;
  }

  FrogPilotParamValueControl *laneLinesWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["LaneLinesWidth"]);
  FrogPilotParamValueControl *pathWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["PathWidth"]);
  FrogPilotParamValueControl *roadEdgesWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["RoadEdgesWidth"]);

  if (metric) {
    laneLinesWidthToggle->setDescription(tr("<b>Set how thick the lane lines are drawn on the driving screen.</b><br><br>The default matches the 10 cm lines actually painted on roads. This only changes the picture, never where openpilot steers."));
    pathWidthToggle->setDescription(tr("<b>Set how wide the driving path is drawn ahead of your car.</b><br><br>The default of 1.9 meters is roughly the width of a real car."));
    roadEdgesWidthToggle->setDescription(tr("<b>Set how thick the road edges are drawn on the driving screen.</b><br><br>The default is half a lane line."));

    laneLinesWidthToggle->updateControl(0, 60, metricSmallDistanceLabels);
    roadEdgesWidthToggle->updateControl(0, 60, metricSmallDistanceLabels);

    pathWidthToggle->updateControl(0, 3, metricDistanceLabels);
  } else {
    laneLinesWidthToggle->setDescription(tr("<b>Set how thick the lane lines are drawn on the driving screen.</b><br><br>The default matches the 4 inch lines actually painted on US roads. This only changes the picture, never where openpilot steers."));
    pathWidthToggle->setDescription(tr("<b>Set how wide the driving path is drawn ahead of your car.</b><br><br>The default of 6.1 feet is roughly the width of a real car."));
    roadEdgesWidthToggle->setDescription(tr("<b>Set how thick the road edges are drawn on the driving screen.</b><br><br>The default is half a lane line."));

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

    bool setVisible = parent->tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (key == "AccelerationPath") {
      setVisible &= parent->hasOpenpilotLongitudinal;
    }

    else if (key == "AdjacentLeadsUI") {
      setVisible &= parent->hasRadar && !(params.getBool("AdvancedCustomUI") && params.getBool("HideLeadMarker"));
    }

    else if (key == "BlindSpotPath") {
      setVisible &= parent->hasBSM;
    }

    else if (key == "HideLeadMarker") {
      setVisible &= parent->hasOpenpilotLongitudinal;
    }

    else if (key == "LeadInfo") {
      setVisible &= parent->hasOpenpilotLongitudinal && !(params.getBool("AdvancedCustomUI") && params.getBool("HideLeadMarker"));
    }

    else if (key == "OnroadDistanceButton") {
      setVisible &= parent->hasOpenpilotLongitudinal;
    }

    else if (key == "PedalsOnUI") {
      setVisible &= parent->hasOpenpilotLongitudinal;
    }

    else if (key == "RadarTracksUI") {
      setVisible &= parent->hasRadar;
    }

    else if (key == "ShowSpeedLimits") {
      setVisible &= !params.getBool("SpeedLimitController") || !parent->hasOpenpilotLongitudinal;
    }

    else if (key == "ShowStoppingPoint") {
      setVisible &= parent->hasOpenpilotLongitudinal;
    }

    else if (key == "SLCMapboxFiller") {
      setVisible &= params.getBool("ShowSpeedLimits") && !(parent->hasOpenpilotLongitudinal && params.getBool("SpeedLimitController"));
      setVisible &= !params.get("MapboxPublicKey").empty();
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (advancedCustomOnroadUIKeys.contains(key)) {
        toggles["AdvancedCustomUI"]->setVisible(true);
      } else if (customOnroadUIKeys.contains(key)) {
        toggles["CustomUI"]->setVisible(true);
      } else if (developerMetricKeys.contains(key)) {
        toggles["DeveloperMetrics"]->setVisible(true);
      } else if (developerSidebarKeys.contains(key)) {
        toggles["DeveloperSidebar"]->setVisible(true);
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

  borderMetricsButton->setVisibleButton(0, parent->hasBSM);

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
