#include "frogpilot/ui/qt/offroad/visual_settings.h"

FrogPilotVisualsPanel::FrogPilotVisualsPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  QStackedLayout *visualsLayout = new QStackedLayout();
  addItem(visualsLayout);

  FrogPilotListWidget *visualsList = new FrogPilotListWidget(this);

  ScrollView *visualsPanel = new ScrollView(visualsList, this);

  visualsLayout->addWidget(visualsPanel);

  FrogPilotListWidget *advancedCustomList = new FrogPilotListWidget(this);
  FrogPilotListWidget *customUIList = new FrogPilotListWidget(this);
  FrogPilotListWidget *modelUIList = new FrogPilotListWidget(this);
  FrogPilotListWidget *navigationUIList = new FrogPilotListWidget(this);
  FrogPilotListWidget *qualityOfLifeList = new FrogPilotListWidget(this);

  ScrollView *advancedCustomPanel = new ScrollView(advancedCustomList, this);
  ScrollView *customUIPanel = new ScrollView(customUIList, this);
  ScrollView *modelUIPanel = new ScrollView(modelUIList, this);
  ScrollView *navigationUIPanel = new ScrollView(navigationUIList, this);
  ScrollView *qualityOfLifePanel = new ScrollView(qualityOfLifeList, this);

  visualsLayout->addWidget(advancedCustomPanel);
  visualsLayout->addWidget(customUIPanel);
  visualsLayout->addWidget(modelUIPanel);
  visualsLayout->addWidget(navigationUIPanel);
  visualsLayout->addWidget(qualityOfLifePanel);

  const std::vector<std::tuple<QString, QString, QString, QString>> visualToggles {
    {"AdvancedCustomUI", tr("Advanced UI Controls"), tr("<b>Fine-tune how the driving screen looks, beyond what the everyday settings cover.</b><br><br>None of these change how the car drives."), "../../frogpilot/assets/toggle_icons/icon_advanced_device.svg"},
    {"HideSpeed", tr("Hide Current Speed"), tr("<b>Take your current speed off the driving screen.</b><br><br>Your dashboard still shows it."), ""},
    {"HideLeadMarker", tr("Hide Lead Marker"), tr("<b>Take the marker off the car in front of you on the driving screen.</b><br><br>openpilot still tracks that car and still follows it.<br><br>\"Lead Info\" and \"Adjacent Leads Tracking\" have nothing left to attach to, so they disappear too."), ""},
    {"HideMaxSpeed", tr("Hide Max Speed"), tr("<b>Take the speed you have set off the driving screen.</b><br><br>openpilot still holds that speed."), ""},
    {"HideAlerts", tr("Hide Non-Critical Alerts"), tr("<b>Take the everyday informational messages off the driving screen.</b><br><br>Anything that actually needs you, like a warning or a takeover request, still comes through."), ""},
    {"HideSpeedLimit", tr("Hide Speed Limits"), tr("<b>Take the posted speed limit sign off the driving screen.</b><br><br>If you use \"Speed Limit Controller\" it keeps working on the limit it reads, so your speed can still change for a sign you can no longer see. The sign does come back when it asks you to confirm a new limit."), ""},
    {"WheelSpeed", tr("Use Wheel Speed"), tr("<b>Show the speed your wheels are actually turning at instead of the slightly optimistic number your dashboard shows.</b><br><br>Most cars read a little high on purpose, so this usually reads one or two lower. It only changes the number on screen, never how openpilot drives."), ""},

    {"CustomUI", tr("Driving Screen Widgets"), tr("<b>Add extra things to the driving screen that stock openpilot does not show.</b><br><br>One of these does change how the car drives: the \"Driving Personality Button\" switches your following distance."), "../assets/icons/calibration.png"},
    {"AccelerationPath", tr("Acceleration Path"), tr("<b>Colour the driving path green when openpilot is speeding up and red when it is slowing down.</b><br><br>Handy for seeing a slowdown coming before you feel it."), ""},
    {"AdjacentPath", tr("Adjacent Lanes"), tr("<b>Draw the paths of the lanes either side of you, so you can see where openpilot thinks they run.</b><br><br>They only appear above your \"Minimum Lane Change Speed\" (20 mph or 32 km/h by default), and only where openpilot sees a lane between you and the road edge."), ""},
    {"BlindSpotPath", tr("Blind Spot Path"), tr("<b>Turn the lane beside you red whenever your car's sensors see something in that blind spot.</b><br><br>It only shows up above your \"Minimum Lane Change Speed\" (20 mph or 32 km/h by default), so it stays away in slow traffic and car parks. Keep checking your mirrors regardless."), ""},
    {"Compass", tr("Compass"), tr("<b>Add a compass to the driving screen showing which way you are heading.</b><br><br>Without a GPS fix it freezes on the last heading it knew rather than disappearing, so treat a compass that never moves as no reading at all."), ""},
    {"OnroadDistanceButton", tr("Driving Personality Button"), tr("<b>Add a button to the driving screen that switches your following distance between Aggressive, Standard and Relaxed without going into the menus.</b><br><br>It works as a second \"Distance\" button, so holding it, or changing what that button does under \"WHEEL BUTTONS\", gives you those actions instead. This changes how the car actually drives, not just what you see. It also shows which one is active."), ""},
    {"PedalsOnUI", tr("Gas / Brake Pedal Indicators"), tr("<b>Show gas and brake indicators on the driving screen that light up as the car speeds up or slows down, whether openpilot or you are driving.</b><br><br>\"Dynamic\" fades them in and out with how hard the car is accelerating or braking. \"Static\" shows them fully lit when active and dim when not."), ""},
    {"RotatingWheel", tr("Rotating Steering Wheel"), tr("<b>Turn the steering wheel picture on screen in time with your real steering wheel.</b>"), ""},

    {"ModelUI", tr("Model UI"), tr("<b>Change how openpilot draws the road ahead, including the driving path, the lane lines and the road edges.</b><br><br>Turning this off falls back to stock openpilot's sizes, though the coloured path edges go away with it."), "../../frogpilot/assets/toggle_icons/icon_road.svg"},
    {"DynamicPathWidth", tr("Dynamic Path Width"), tr("<b>Make the driving path narrower when openpilot is doing less of the driving, so you can tell at a glance how much control it has.</b><br><br>Full width while openpilot is driving, three quarters while \"Always On Lateral\" is steering for you, and half the rest of the time."), ""},
    {"LaneLinesWidth", tr("Lane Lines Width"), tr("<b>Set how thick the lane lines are drawn on the driving screen.</b><br><br>The default matches the 4 inch lines actually painted on US roads. This only changes the picture, never where openpilot steers."), ""},
    {"PathEdgeWidth", tr("Path Edges Width"), tr("<b>Set how thick the coloured stripe down each side of the driving path is, which is what tells you which mode openpilot is in.</b><br><br>By default the two stripes together take up a fifth of the path width. Set it to zero to hide the stripe entirely.<br><br>Light blue means \"Always On Lateral\" is steering for you, green is normal driving, orange is Experimental Mode, red is Traffic Mode, and yellow means you have overridden Conditional Experimental Mode to keep Experimental Mode off."), ""},
    {"PathWidth", tr("Path Width"), tr("<b>Set how wide the driving path is drawn ahead of your car.</b><br><br>The default of 6.1 feet is roughly the width of a real car."), ""},
    {"RoadEdgesWidth", tr("Road Edges Width"), tr("<b>Set how thick the road edges are drawn on the driving screen.</b><br><br>The default is half a lane line."), ""},

    {"NavigationUI", tr("Navigation Widgets"), tr("<b>Change what navigation shows on the driving screen, from the road name to speed limit signs.</b><br><br>Some of these need map data downloaded under \"Maps and Navigation\" before they show anything."), "../../frogpilot/assets/toggle_icons/icon_map.svg"},
    {"RoadNameUI", tr("Road Name"), tr("<b>Show the name of the road you are on along the bottom of the driving screen.</b><br><br>It comes from downloaded map data, so it stays blank on roads you have not downloaded."), ""},
    {"ShowSpeedLimits", tr("Show Speed Limits"), tr("<b>Show the posted speed limit as a sign in the top-left corner of the driving screen.</b><br><br>The limit comes from your downloaded map data first, then from your car's dashboard where it can read one, and from signs read by the camera otherwise."), ""},
    {"SLCMapboxFiller", tr("Show Speed Limits from Mapbox"), tr("<b>Fall back to Mapbox for the speed limit when none of your downloaded maps, your dashboard or signs read by the camera know one.</b><br><br>Needs your Public Mapbox Key set up under \"Maps and Navigation\" and a working internet connection."), ""},
    {"UseVienna", tr("Use Vienna-Style Speed Signs"), tr("<b>Draw speed limit signs in the round European style instead of the rectangular American one.</b><br><br>Needs \"Show Speed Limits\" or \"Speed Limit Controller\" switched on, or the sign disappears instead of changing shape. It never changes the limit openpilot reads."), ""},

    {"QOLVisuals", tr("Quality of Life"), tr("<b>Pick which camera you watch, and a couple of smaller driving screen touches that did not fit anywhere else.</b>"), "../../frogpilot/assets/toggle_icons/icon_quality_of_life.svg"},
    {"CameraView", tr("Camera View"), tr("<b>Choose which camera the driving screen shows, from the wide one to the driver-facing one.</b><br><br>openpilot keeps using every camera to drive no matter which one you put on screen."), ""},
    {"DriverCamera", tr("Show Driver Camera When In Reverse"), tr("<b>Switch the driving screen to the driver camera whenever you put the car in reverse.</b><br><br>This is not a backup camera. It faces you, not the road behind."), ""},
    {"StoppedTimer", tr("Stopped Timer"), tr("<b>Replace your speed with a running timer once you come to a complete stop, so you can see how long you have been waiting.</b><br><br>It stays off for the first minute of a drive."), ""}
  };

  const std::map<QString, ScrollView*> managePanels {{"AdvancedCustomUI", advancedCustomPanel}, {"CustomUI", customUIPanel}, {"ModelUI", modelUIPanel}, {"NavigationUI", navigationUIPanel}, {"QOLVisuals", qualityOfLifePanel}};

  for (const auto &[param, title, desc, icon] : visualToggles) {
    AbstractControl *visualToggle;

    if (managePanels.count(param)) {
      ScrollView *managePanel = managePanels.at(param);
      FrogPilotManageControl *manageToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(manageToggle, &FrogPilotManageControl::manageButtonClicked, [visualsLayout, managePanel]() {
        visualsLayout->setCurrentWidget(managePanel);
      });
      visualToggle = manageToggle;

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

    } else if (param == "LaneLinesWidth" || param == "RoadEdgesWidth") {
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 24, tr(" inches"));
    } else if (param == "PathEdgeWidth") {
      std::map<float, QString> pathEdgeLabels{{0, tr("Off")}};
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, "%", pathEdgeLabels);
    } else if (param == "PathWidth") {
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, tr(" feet"), std::map<float, QString>(), 0.1);

    } else if (param == "CameraView") {
      std::vector<QString> cameraOptions{tr("Auto"), tr("Driver"), tr("Standard"), tr("Wide")};
      visualToggle = new ButtonParamControl(param, title, desc, icon, cameraOptions);

    } else {
      visualToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = visualToggle;

    if (advancedCustomOnroadUIKeys.contains(param)) {
      advancedCustomList->addItem(visualToggle);
    } else if (customOnroadUIKeys.contains(param)) {
      customUIList->addItem(visualToggle);
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
  }

  QObject::connect(static_cast<ToggleControl*>(toggles["ShowSpeedLimits"]), &ToggleControl::toggleFlipped, this, &FrogPilotVisualsPanel::updateToggles);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [visualsLayout, visualsPanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    visualsLayout->setCurrentWidget(visualsPanel);
  });
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, this, &FrogPilotVisualsPanel::updateMetric);
}

void FrogPilotVisualsPanel::showEvent(QShowEvent *event) {
  updateToggles();
}

void FrogPilotVisualsPanel::updateMetric(bool metric, bool bootRun) {
  static bool previousMetric;
  if (metric != previousMetric && !bootRun) {
    double distanceConversion = metric ? FOOT_TO_METER : METER_TO_FOOT;
    double smallDistanceConversion = metric ? INCH_TO_CM : CM_TO_INCH;

    float smallDistanceMax = metric ? 60.0f : 24.0f;
    float distanceMax = metric ? 3.0f : 10.0f;

    params.putFloat("LaneLinesWidth", std::clamp<float>(std::round(params.getFloat("LaneLinesWidth") * smallDistanceConversion), 0.0f, smallDistanceMax));
    params.putFloat("RoadEdgesWidth", std::clamp<float>(std::round(params.getFloat("RoadEdgesWidth") * smallDistanceConversion), 0.0f, smallDistanceMax));

    params.putFloat("PathWidth", std::clamp<float>(std::round(params.getFloat("PathWidth") * distanceConversion * 10.0f) / 10.0f, 0.0f, distanceMax));
  }
  previousMetric = metric;

  static std::map<float, QString> imperialDistanceLabels;
  static std::map<float, QString> imperialSmallDistanceLabels;
  static std::map<float, QString> metricDistanceLabels;
  static std::map<float, QString> metricSmallDistanceLabels;

  static bool labelsInitialized = false;
  if (!labelsInitialized) {
    imperialDistanceLabels[0] = tr("Off");
    imperialDistanceLabels[1] = "1.0" + tr(" foot");

    imperialSmallDistanceLabels[0] = tr("Off");
    imperialSmallDistanceLabels[1] = "1" + tr(" inch");

    for (int i = 0; i <= 30; ++i) {
      float key = i / 10.0f;
      metricDistanceLabels[key] = i == 0 ? tr("Off") : i == 10 ? QString::number(key, 'f', 1) + tr(" meter") : QString::number(key, 'f', 1) + tr(" meters");
    }

    metricSmallDistanceLabels[0] = tr("Off");

    labelsInitialized = true;
  }

  FrogPilotParamValueControl *laneLinesWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["LaneLinesWidth"]);
  FrogPilotParamValueControl *pathWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["PathWidth"]);
  FrogPilotParamValueControl *roadEdgesWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["RoadEdgesWidth"]);

  if (metric) {
    laneLinesWidthToggle->setDescription(tr("<b>Set how thick the lane lines are drawn on the driving screen.</b><br><br>The default matches the 10 cm lines actually painted on roads. This only changes the picture, never where openpilot steers."));
    pathWidthToggle->setDescription(tr("<b>Set how wide the driving path is drawn ahead of your car.</b><br><br>The default of 1.9 meters is roughly the width of a real car."));

    laneLinesWidthToggle->updateControl(0, 60, metricSmallDistanceLabels, tr(" cm"));
    roadEdgesWidthToggle->updateControl(0, 60, metricSmallDistanceLabels, tr(" cm"));

    pathWidthToggle->updateControl(0, 3, metricDistanceLabels);
  } else {
    laneLinesWidthToggle->setDescription(tr("<b>Set how thick the lane lines are drawn on the driving screen.</b><br><br>The default matches the 4 inch lines actually painted on US roads. This only changes the picture, never where openpilot steers."));
    pathWidthToggle->setDescription(tr("<b>Set how wide the driving path is drawn ahead of your car.</b><br><br>The default of 6.1 feet is roughly the width of a real car."));

    laneLinesWidthToggle->updateControl(0, 24, imperialSmallDistanceLabels, tr(" inches"));
    roadEdgesWidthToggle->updateControl(0, 24, imperialSmallDistanceLabels, tr(" inches"));

    pathWidthToggle->updateControl(0, 10, imperialDistanceLabels, tr(" feet"));
  }
}

void FrogPilotVisualsPanel::updateToggles() {
  QSet<QString> visibleParents;

  bool showSpeedLimits = params.getBool("ShowSpeedLimits");
  bool speedLimitController = params.getBool("SpeedLimitController");

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      continue;
    }

    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels.value(key).toDouble();

    if (longitudinalKeys.contains(key)) {
      setVisible &= parent->hasOpenpilotLongitudinal;
    }

    if (key == "BlindSpotPath") {
      setVisible &= parent->hasBSM;
    }

    else if (key == "ShowSpeedLimits") {
      setVisible &= !speedLimitController || !parent->hasOpenpilotLongitudinal;
    }

    else if (key == "SLCMapboxFiller") {
      setVisible &= showSpeedLimits;
      setVisible &= !speedLimitController || !parent->hasOpenpilotLongitudinal;
      setVisible &= !params.get("MapboxPublicKey").empty();
    }

    else if (key == "UseVienna") {
      setVisible &= showSpeedLimits || (parent->hasOpenpilotLongitudinal && speedLimitController);
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (advancedCustomOnroadUIKeys.contains(key)) {
        visibleParents.insert("AdvancedCustomUI");
      } else if (customOnroadUIKeys.contains(key)) {
        visibleParents.insert("CustomUI");
      } else if (modelUIKeys.contains(key)) {
        visibleParents.insert("ModelUI");
      } else if (navigationUIKeys.contains(key)) {
        visibleParents.insert("NavigationUI");
      } else if (qualityOfLifeKeys.contains(key)) {
        visibleParents.insert("QOLVisuals");
      }
    }
  }

  for (const QString &key : parentKeys) {
    toggles[key]->setVisible(visibleParents.contains(key));
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
