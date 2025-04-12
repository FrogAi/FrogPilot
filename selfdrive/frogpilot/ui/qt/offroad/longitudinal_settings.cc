#include "selfdrive/frogpilot/ui/qt/offroad/longitudinal_settings.h"

FrogPilotLongitudinalPanel::FrogPilotLongitudinalPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QStackedLayout *longitudinalLayout = new QStackedLayout();
  addItem(longitudinalLayout);

  FrogPilotListWidget *longitudinalList = new FrogPilotListWidget(this);

  ScrollView *longitudinalPanel = new ScrollView(longitudinalList, this);

  longitudinalLayout->addWidget(longitudinalPanel);

  FrogPilotListWidget *aggressivePersonalityList = new FrogPilotListWidget(this);
  FrogPilotListWidget *conditionalExperimentalList = new FrogPilotListWidget(this);
  FrogPilotListWidget *curveSpeedList = new FrogPilotListWidget(this);
  FrogPilotListWidget *customDrivingPersonalityList = new FrogPilotListWidget(this);
  FrogPilotListWidget *longitudinalTuneList = new FrogPilotListWidget(this);
  FrogPilotListWidget *qolList = new FrogPilotListWidget(this);
  FrogPilotListWidget *relaxedPersonalityList = new FrogPilotListWidget(this);
  FrogPilotListWidget *speedLimitControllerList = new FrogPilotListWidget(this);
  FrogPilotListWidget *speedLimitControllerOffsetsList = new FrogPilotListWidget(this);
  FrogPilotListWidget *speedLimitControllerQOLList = new FrogPilotListWidget(this);
  FrogPilotListWidget *speedLimitControllerVisualList = new FrogPilotListWidget(this);
  FrogPilotListWidget *standardPersonalityList = new FrogPilotListWidget(this);
  FrogPilotListWidget *trafficPersonalityList = new FrogPilotListWidget(this);

  ScrollView *aggressivePersonalityPanel = new ScrollView(aggressivePersonalityList, this);
  ScrollView *conditionalExperimentalPanel = new ScrollView(conditionalExperimentalList, this);
  ScrollView *curveSpeedPanel = new ScrollView(curveSpeedList, this);
  ScrollView *customDrivingPersonalityPanel = new ScrollView(customDrivingPersonalityList, this);
  ScrollView *longitudinalTunePanel = new ScrollView(longitudinalTuneList, this);
  ScrollView *qolPanel = new ScrollView(qolList, this);
  ScrollView *relaxedPersonalityPanel = new ScrollView(relaxedPersonalityList, this);
  ScrollView *speedLimitControllerPanel = new ScrollView(speedLimitControllerList, this);
  ScrollView *speedLimitControllerOffsetsPanel = new ScrollView(speedLimitControllerOffsetsList, this);
  ScrollView *speedLimitControllerQOLPanel = new ScrollView(speedLimitControllerQOLList, this);
  ScrollView *speedLimitControllerVisualPanel = new ScrollView(speedLimitControllerVisualList, this);
  ScrollView *standardPersonalityPanel = new ScrollView(standardPersonalityList, this);
  ScrollView *trafficPersonalityPanel = new ScrollView(trafficPersonalityList, this);

  longitudinalLayout->addWidget(aggressivePersonalityPanel);
  longitudinalLayout->addWidget(conditionalExperimentalPanel);
  longitudinalLayout->addWidget(curveSpeedPanel);
  longitudinalLayout->addWidget(customDrivingPersonalityPanel);
  longitudinalLayout->addWidget(longitudinalTunePanel);
  longitudinalLayout->addWidget(qolPanel);
  longitudinalLayout->addWidget(relaxedPersonalityPanel);
  longitudinalLayout->addWidget(speedLimitControllerPanel);
  longitudinalLayout->addWidget(speedLimitControllerOffsetsPanel);
  longitudinalLayout->addWidget(speedLimitControllerQOLPanel);
  longitudinalLayout->addWidget(speedLimitControllerVisualPanel);
  longitudinalLayout->addWidget(standardPersonalityPanel);
  longitudinalLayout->addWidget(trafficPersonalityPanel);

  const std::vector<std::tuple<QString, QString, QString, QString>> longitudinalToggles {
    {"ConditionalExperimental", tr("Conditional Experimental Mode"), tr("Automatically switch to <b>Experimental Mode</b> when set conditions are met."), "../frogpilot/assets/toggle_icons/icon_conditional.png"},
    {"CESpeed", tr("Below"), tr("Switch to <b>Experimental Mode</b> when driving below this speed."), ""},
    {"CECurves", tr("Curve Detected Ahead"), tr("Switch to <b>Experimental Mode</b> when a curve is detected ahead. Useful for letting the model choose the appropriate speed for the curve."), ""},
    {"CELead", tr("Lead Detected Ahead"), tr("Switch to <b>Experimental Mode</b> when a slower or stopped vehicle is detected ahead. Can improve braking smoothness and reliability on some vehicles."), ""},
    {"CENavigation", tr("Navigation Data"), tr("Switch to <b>Experimental Mode</b> when approaching intersections or turns on the active route while using <b>Navigate on openpilot (NOO)</b>. Useful for letting the model choose the appropriate speed for upcoming navigation maneuvers."), ""},
    {"CEModelStopTime", tr("openpilot Wants to Stop In"), tr("Switch to <b>Experimental Mode</b> when openpilot wants to stop within the set amount of time. This is typically triggered by the driving model \"detecting\" a red light or stop sign."), ""},
    {"CESignalSpeed", tr("Turn Signal Below"), tr("Switch to <b>Experimental Mode</b> when using a turn signal below the set speed. Useful for letting the model choose the appropriate speed for upcoming left or right turns."), ""},
    {"ShowCEMStatus", tr("Status Widget"), tr("Show the <b>Conditional Experimental Mode</b> status on the driving screen."), ""},

    {"CurveSpeedControl", tr("Curve Speed Control"), tr("Automatically slow down for upcoming curves using downloaded maps or the driving model."), "../frogpilot/assets/toggle_icons/icon_speed_map.png"},
    {"CurveDetectionMethod", tr("Curve Detection Method"), tr("How curves are detected. <b>Map-Based</b> uses downloaded map data to identify curves and determine the appropriate speed in which to handle them at, while <b>Vision</b> relies solely on the driving model."), ""},
    {"MTSCCurvatureCheck", tr("Curve Detection Failsafe"), tr("Only trigger <b>Curve Speed Control</b> if a curve is detected with the model while using the <b>Map-Based</b> method. Useful to help prevent false positives."), ""},
    {"CurveSensitivity", tr("Curve Detection Sensitivity"), tr("How sensitive openpilot is when detecting curves. Higher values trigger earlier responses at the risk of triggering too often, while lower values increase confidence at the risk of triggering too infrequently."), ""},
    {"TurnAggressiveness", tr("Curve Speed Aggressiveness"), tr("How aggressive openpilot is when navigating through curves. Higher values result in faster turns but may reduce comfort or stability, while lower values result in slower, smoother turns at the risk of being overly cautious."), ""},
    {"ShowCSCStatus", tr("Status Widget"), tr("Show <b>Curve Speed Control</b>'s desired speed on the driving screen."), ""},

    {"CustomPersonalities", tr("Customize Driving Personalities"), tr("Customize the personality profiles to your driving style."), "../frogpilot/assets/toggle_icons/icon_personality.png"},

    {"TrafficPersonalityProfile", tr("Traffic Personality"), tr("Customize the <b>Traffic</b> personality profile. Tailored for navigating through traffic."), "../frogpilot/assets/stock_theme/distance_icons/traffic.png"},
    {"TrafficFollow", tr("Following Distance"), tr("The minimum following distance while in <b>Traffic Mode</b>. openpilot will dynamically adjust between this value and the value in the <b>Aggressive</b> profile based on your current speed."), ""},
    {"TrafficJerkAcceleration", tr("Acceleration Sensitivity"), tr("How sensitive openpilot is to changes in acceleration while in <b>Traffic Mode</b>. Higher values result in smoother, more gradual acceleration, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"TrafficJerkDeceleration", tr("Deceleration Sensitivity"), tr("How sensitive openpilot is to changes in deceleration while in <b>Traffic Mode</b>. Higher values result in smoother, more gradual deceleration, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"TrafficJerkDanger", tr("Safety Distance Sensitivity"), tr("How cautious openpilot is around other vehicles or obstacles while in <b>Traffic Mode</b>. Higher values increase following distances and prioritize safety, leading to more cautious driving, while lower values allow for closer following but may reduce reaction time."), ""},
    {"TrafficJerkSpeed", tr("Speed Increase Response"), tr("How quickly openpilot increases speed while in <b>Traffic Mode</b>. Higher values ensure smoother, more gradual speed changes when accelerating, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"TrafficJerkSpeedDecrease", tr("Speed Decrease Response"), tr("How quickly openpilot decreases speed while in <b>Traffic Mode</b>. Higher values ensure smoother, more gradual speed changes when slowing down, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"ResetTrafficPersonality", tr("Reset Settings"), tr("Reset <b>Traffic Mode</b> settings to default values."), ""},

    {"AggressivePersonalityProfile", tr("Aggressive Personality"), tr("Customize the <b>Aggressive</b> personality profile. Designed for a more assertive driving style."), "../frogpilot/assets/stock_theme/distance_icons/aggressive.png"},
    {"AggressiveFollow", tr("Following Distance"), tr("How many seconds openpilot will follow behind lead vehicles while using the <b>Aggressive</b> personality profile.<br><br>Default: 1.25 seconds."), ""},
    {"AggressiveJerkAcceleration", tr("Acceleration Sensitivity"), tr("How sensitive openpilot is to changes in acceleration while using the <b>Aggressive</b> personality profile. Higher values result in smoother, more gradual acceleration, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"AggressiveJerkDeceleration", tr("Deceleration Sensitivity"), tr("How sensitive openpilot is to changes in deceleration while using the <b>Aggressive</b> personality profile. Higher values result in smoother, more gradual deceleration, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"AggressiveJerkDanger", tr("Safety Distance Sensitivity"), tr("How cautious openpilot is around other vehicles or obstacles while using the <b>Aggressive</b> personality profile. Higher values increase following distances and prioritize safety, leading to more cautious driving, while lower values allow for closer following but may reduce reaction time."), ""},
    {"AggressiveJerkSpeed", tr("Speed Increase Response"), tr("How quickly openpilot increases speed while using the <b>Aggressive</b> personality profile. Higher values ensure smoother, more gradual speed changes when accelerating, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"AggressiveJerkSpeedDecrease", tr("Speed Decrease Response"), tr("How quickly openpilot decreases speed while using the <b>Aggressive</b> personality profile. Higher values ensure smoother, more gradual speed changes when slowing down, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"ResetAggressivePersonality", tr("Reset Settings"), tr("Reset the <b>Aggressive</b> personality profile settings to default values."), ""},

    {"StandardPersonalityProfile", tr("Standard Personality"), tr("Customize the <b>Standard</b> personality profile. Designed for a balanced driving style."), "../frogpilot/assets/stock_theme/distance_icons/standard.png"},
    {"StandardFollow", tr("Following Distance"), tr("How many seconds openpilot will follow behind lead vehicles while using the <b>Standard</b> personality profile.<br><br>Default: 1.45 seconds."), ""},
    {"StandardJerkAcceleration", tr("Acceleration Sensitivity"), tr("How sensitive openpilot is to changes in acceleration while using the <b>Standard</b> personality profile. Higher values result in smoother, more gradual acceleration, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"StandardJerkDeceleration", tr("Deceleration Sensitivity"), tr("How sensitive openpilot is to changes in deceleration while using the <b>Standard</b> personality profile. Higher values result in smoother, more gradual deceleration, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"StandardJerkDanger", tr("Safety Distance Sensitivity"), tr("How cautious openpilot is around other vehicles or obstacles while using the <b>Standard</b> personality profile. Higher values increase following distances and prioritize safety, leading to more cautious driving, while lower values allow for closer following but may reduce reaction time."), ""},
    {"StandardJerkSpeed", tr("Speed Increase Response"), tr("How quickly openpilot increases speed while using the <b>Standard</b> personality profile. Higher values ensure smoother, more gradual speed changes when accelerating, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"StandardJerkSpeedDecrease", tr("Speed Decrease Response"), tr("How quickly openpilot decreases speed while using the <b>Standard</b> personality profile. Higher values ensure smoother, more gradual speed changes when slowing down, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"ResetStandardPersonality", tr("Reset Settings"), tr("Reset the <b>Standard</b> personality profile settings to default values."), ""},

    {"RelaxedPersonalityProfile", tr("Relaxed Personality"), tr("Customize the <b>Relaxed</b> personality profile. Designed for a more laid-back driving style."), "../frogpilot/assets/stock_theme/distance_icons/relaxed.png"},
    {"RelaxedFollow", tr("Following Distance"), tr("How many seconds openpilot will follow behind lead vehicles while using the <b>Relaxed</b> personality profile.<br><br>Default: 1.75 seconds."), ""},
    {"RelaxedJerkAcceleration", tr("Acceleration Sensitivity"), tr("How sensitive openpilot is to changes in acceleration while using the <b>Relaxed</b> personality profile. Higher values result in smoother, more gradual acceleration, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"RelaxedJerkDeceleration", tr("Deceleration Sensitivity"), tr("How sensitive openpilot is to changes in deceleration while using the <b>Relaxed</b> personality profile. Higher values result in smoother, more gradual deceleration, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"RelaxedJerkDanger", tr("Safety Distance Sensitivity"), tr("How cautious openpilot is around other vehicles or obstacles while using the <b>Relaxed</b> personality profile. Higher values increase following distances and prioritize safety, leading to more cautious driving, while lower values allow for closer following but may reduce reaction time."), ""},
    {"RelaxedJerkSpeed", tr("Speed Increase Response"), tr("How quickly openpilot increases speed while using the <b>Relaxed</b> personality profile. Higher values ensure smoother, more gradual speed changes when accelerating, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"RelaxedJerkSpeedDecrease", tr("Speed Decrease Response"), tr("How quickly openpilot decreases speed while using the <b>Relaxed</b> personality profile. Higher values ensure smoother, more gradual speed changes when slowing down, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"ResetRelaxedPersonality", tr("Reset Settings"), tr("Reset the <b>Relaxed</b> personality profile settings to default values."), ""},

    {"LongitudinalTune", tr("Longitudinal Tuning"), tr("Settings that control how openpilot manages speed and acceleration."), "../frogpilot/assets/toggle_icons/icon_longitudinal_tune.png"},
    {"AccelerationProfile", tr("Acceleration Profile"), tr("Enable either a sporty or eco-friendly acceleration profile. <b>Sport+</b> aims to make openpilot accelerate as fast as possible!"), ""},
    {"DecelerationProfile", tr("Deceleration Profile"), tr("Enable either a sporty or eco-friendly deceleration profile."), ""},
    {"HumanAcceleration", tr("Human-Like Acceleration"), tr("Use the lead vehicle's acceleration rate when at a takeoff and ramp off the acceleration rate when approaching the maximum set speed for a more \"human-like\" driving experience."), ""},
    {"HumanFollowing", tr("Human-Like Approach Behind Leads"), tr("Dynamically adjust the desired following distance when approaching slower or stopped vehicles for a more \"human-like\" driving experience."), ""},
    {"LeadDetectionThreshold", tr("Lead Detection Confidence"), tr("How sensitive openpilot is to detecting vehicles ahead. A lower value can help detect vehicles sooner and from farther away, but increases the chance openpilot mistakes other objects for vehicles."), ""},
    {"MaxDesiredAcceleration", tr("Maximum Acceleration Rate"), tr("Set a cap on how fast openpilot can accelerate."), ""},
    {"TacoTune", tr("\"Taco Bell Run\" Turn Speed Hack"), tr("The turn speed hack from comma’s 2022 \"Taco Bell Run\" drive. Designed to slow down when taking left/right turns for smoother turns."), ""},

    {"QOLLongitudinal", tr("Quality of Life"), tr("Miscellaneous features to improve the acceleration and braking experience."), "../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"CustomCruise", tr("Cruise Interval"), tr("The interval used when changing the cruise control speed."), ""},
    {"CustomCruiseLong", tr("Cruise Interval (Long Press)"), tr("The interval used when changing the cruise control speed while holding down the button for 0.5+ seconds."), ""},
    {"ForceStandstill", tr("Force Keep openpilot in the Standstill State"), tr("Keep openpilot in the standstill state until either the gas pedal or <b>resume</b> button is pressed."), ""},
    {"ForceStops", tr("Force Stop for \"Detected\" Stop Lights/Signs"), tr("Force a stop whenever openpilot <b>detects</b> a potential red light/stop sign to prevent it from running the red light/stop sign."), ""},
    {"IncreasedStoppedDistance", tr("Increase Stopped Distance"), tr("Increase the distance openpilot stops behind vehicles."), ""},
    {"SetSpeedOffset", tr("Set Speed Offset"), tr("How much higher or lower the set speed should be compared to your current set speed. For example, if you prefer to drive 5 mph above the speed limit, this setting will automatically add that difference when you adjust your set speed."), ""},
    {"MapGears", tr("Map Accel/Decel to Gears"), tr("Map the acceleration and deceleration profiles to your car's <b>Eco</b> or <b>Sport</b> gear modes."), ""},
    {"ReverseCruise", tr("Reverse Cruise Increase"), tr("Reverse the <b>long press</b> cruise increase feature to increase the max speed by 5 instead of 1 on short presses."), ""},

    {"SpeedLimitController", tr("Speed Limit Controller"), tr("Limit openpilot's maximum driving speed based on data from downloaded maps, <b>Mapbox</b>, <b>Navigate on openpilot</b>, or the dashboard (supported vehicles: <b>Ford</b>, <b>Genesis</b>, <b>Hyundai</b>, <b>Kia</b>, <b>Lexus</b>, <b>Toyota</b>)."), "../assets/offroad/icon_speed_limit.png"},
    {"SLCFallback", tr("Fallback Speed"), tr("The speed limit source when no speed limit data is available."), ""},
    {"SLCOverride", tr("Override Speed"), tr("The speed openpilot uses after manually exceeding the posted speed limit.<br><br><b>- Set With Gas Pedal</b>: Uses the speed obtained while pressing the gas<br><b>- Max Set Speed</b>: Uses the cruise control set speed<br><br>Overrides clear upon disengagement."), ""},
    {"SLCQOL", tr("Quality of Life"), tr("Miscellaneous features to improve the <b>Speed Limit Controller</b> experience."), ""},
    {"SLCConfirmation", tr("Confirm New Speed Limits"), tr("Require confirmation before applying new speed limits. To accept, use the flashing widget on the driving screen or press the <b>Cruise Increase</b> button. To deny, press the <b>Cruise Decrease</b> button or simply ignore the prompt for 30 seconds."), ""},
    {"ForceMPHDashboard", tr("Force MPH Readings from Dashboard"), tr("Force dashboard speed limit readings to be in mph."), ""},
    {"SLCLookaheadHigher", tr("Higher Limit Lookahead Time"), tr("How far ahead openpilot anticipates upcoming higher speed limits from the downloaded map data."), ""},
    {"SLCLookaheadLower", tr("Lower Limit Lookahead Time"), tr("How far ahead openpilot anticipates upcoming lower speed limits from the downloaded map data."), ""},
    {"SetSpeedLimit", tr("Match Speed Limit on Engage"), tr("Automatically set cruise control speed to match the current speed limit when first enabling openpilot."), ""},
    {"SLCPriority", tr("Speed Limit Source Priority"), tr("Define the priority order for speed limit sources (<b>Dashboard</b>, <b>Map Data</b>, <b>Navigation</b>). Higher-priority sources override lower ones when multiple limits are detected."), ""},
    {"SLCOffsets", tr("Speed Limit Offsets"), tr("Offsets relative to the posted speed limit for a more \"human-like\" driving experience."), ""},
    {"Offset1", tr("Speed Limit Offset (0–24 mph)"), tr("Target speed offset applied for posted limits between 0 and 24 mph."), ""},
    {"Offset2", tr("Speed Limit Offset (25–34 mph)"), tr("Target speed offset applied for posted limits between 25 and 34 mph."), ""},
    {"Offset3", tr("Speed Limit Offset (35–44 mph)"), tr("Target speed offset applied for posted limits between 35 and 44 mph."), ""},
    {"Offset4", tr("Speed Limit Offset (45–54 mph)"), tr("Target speed offset applied for posted limits between 45 and 54 mph."), ""},
    {"Offset5", tr("Speed Limit Offset (55–64 mph)"), tr("Target speed offset applied for posted limits between 55 and 64 mph."), ""},
    {"Offset6", tr("Speed Limit Offset (65–74 mph)"), tr("Target speed offset applied for posted limits between 65 and 74 mph."), ""},
    {"Offset7", tr("Speed Limit Offset (75–99 mph)"), tr("Target speed offset applied for posted limits between 75 and 99 mph."), ""},
    {"SLCVisuals", tr("Visual Settings"), tr("Visual features to improve the <b>Speed Limit Controller</b> experience."), ""},
    {"ShowSLCOffset", tr("Show Speed Limit Offset"), tr("Display the speed limit offset separately on the driving screen."), ""},
    {"SpeedLimitSources", tr("Show Speed Limit Sources"), tr("Display the speed limit sources on the driving screen."), ""}
  };

  for (const auto &[param, title, desc, icon] : longitudinalToggles) {
    AbstractControl *longitudinalToggle;

    if (param == "CustomPersonalities") {
      FrogPilotManageControl *customPersonalitiesToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(customPersonalitiesToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, customDrivingPersonalityPanel]() {
        longitudinalLayout->setCurrentWidget(customDrivingPersonalityPanel);
      });
      longitudinalToggle = customPersonalitiesToggle;
    } else if (param == "ResetTrafficPersonality" || param == "ResetAggressivePersonality" || param == "ResetStandardPersonality" || param == "ResetRelaxedPersonality") {
      ButtonControl *resetBtn = new ButtonControl(title, tr("RESET"), desc);
      longitudinalToggle = resetBtn;
    } else if (param == "TrafficPersonalityProfile") {
      FrogPilotManageControl *trafficPersonalityToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(trafficPersonalityToggle, &FrogPilotManageControl::manageButtonClicked, [this, longitudinalLayout, trafficPersonalityPanel]() {
        openSubParentToggle();

        longitudinalLayout->setCurrentWidget(trafficPersonalityPanel);

        customPersonalityOpen = true;
      });
      longitudinalToggle = trafficPersonalityToggle;
    } else if (param == "AggressivePersonalityProfile") {
      FrogPilotManageControl *aggressivePersonalityToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(aggressivePersonalityToggle, &FrogPilotManageControl::manageButtonClicked, [this, longitudinalLayout, aggressivePersonalityPanel]() {
        openSubParentToggle();

        longitudinalLayout->setCurrentWidget(aggressivePersonalityPanel);

        customPersonalityOpen = true;
      });
      longitudinalToggle = aggressivePersonalityToggle;
    } else if (param == "StandardPersonalityProfile") {
      FrogPilotManageControl *standardPersonalityToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(standardPersonalityToggle, &FrogPilotManageControl::manageButtonClicked, [this, longitudinalLayout, standardPersonalityPanel]() {
        openSubParentToggle();

        longitudinalLayout->setCurrentWidget(standardPersonalityPanel);

        customPersonalityOpen = true;
      });
      longitudinalToggle = standardPersonalityToggle;
    } else if (param == "RelaxedPersonalityProfile") {
      FrogPilotManageControl *relaxedPersonalityToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(relaxedPersonalityToggle, &FrogPilotManageControl::manageButtonClicked, [this, longitudinalLayout, relaxedPersonalityPanel]() {
        openSubParentToggle();

        longitudinalLayout->setCurrentWidget(relaxedPersonalityPanel);

        customPersonalityOpen = true;
      });
      longitudinalToggle = relaxedPersonalityToggle;
    } else if (aggressivePersonalityKeys.find(param) != aggressivePersonalityKeys.end() ||
               standardPersonalityKeys.find(param) != standardPersonalityKeys.end() ||
               relaxedPersonalityKeys.find(param) != relaxedPersonalityKeys.end() ||
               trafficPersonalityKeys.find(param) != trafficPersonalityKeys.end()) {
      if (param == "TrafficFollow" || param == "AggressiveFollow" || param == "StandardFollow" || param == "RelaxedFollow") {
        std::map<float, QString> followTimeLabels;
        for (float i = 0; i <= 5; i += 0.01) {
          followTimeLabels[i] = std::lround(i / 0.01) == 1 / 0.01 ? QString::number(i, 'f', 2) + tr(" second") : QString::number(i, 'f', 2) + tr(" seconds");
        }
        if (param == "TrafficFollow") {
          longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.5, 5, QString(), followTimeLabels, 0.01, true);
        } else {
          longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 5, QString(), followTimeLabels, 0.01, true);
        }
      } else {
        longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 500, "%");
      }

    } else if (param == "ConditionalExperimental") {
      FrogPilotManageControl *conditionalExperimentalToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(conditionalExperimentalToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, conditionalExperimentalPanel]() {
        longitudinalLayout->setCurrentWidget(conditionalExperimentalPanel);
      });
      longitudinalToggle = conditionalExperimentalToggle;
    } else if (param == "CESpeed") {
      FrogPilotParamValueControl *CESpeed = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr("mph"), std::map<float, QString>(), 1, true, true);
      FrogPilotParamValueControl *CESpeedLead = new FrogPilotParamValueControl("CESpeedLead", tr("With Lead"), tr("Switch to <b>Experimental Mode</b> when driving below this speed with a lead."), icon, 0, 99, tr("mph"), std::map<float, QString>(), 1, true, true);
      FrogPilotDualParamValueControl *conditionalSpeeds = new FrogPilotDualParamValueControl(CESpeed, CESpeedLead);
      longitudinalToggle = reinterpret_cast<AbstractControl*>(conditionalSpeeds);
    } else if (param == "CECurves") {
      std::vector<QString> curveToggles{"CECurvesLead"};
      std::vector<QString> curveToggleNames{tr("With Lead")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, curveToggles, curveToggleNames);
    } else if (param == "CELead") {
      std::vector<QString> leadToggles{"CESlowerLead", "CEStoppedLead"};
      std::vector<QString> leadToggleNames{tr("Slower Lead"), tr("Stopped Lead")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, leadToggles, leadToggleNames);
    } else if (param == "CENavigation") {
      std::vector<QString> navigationToggles{"CENavigationIntersections", "CENavigationTurns", "CENavigationLead"};
      std::vector<QString> navigationToggleNames{tr("Intersections"), tr("Turns"), tr("With Lead")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, navigationToggles, navigationToggleNames);
    } else if (param == "CEModelStopTime") {
      std::map<float, QString> stopTimeLabels;
      for (int i = 0; i <= 10; ++i) {
        stopTimeLabels[i] = i == 0 ? tr("Off") : i == 1 ? QString::number(i) + tr(" second") : QString::number(i) + tr(" seconds");
      }
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, QString(), stopTimeLabels);
    } else if (param == "CESignalSpeed") {
      std::vector<QString> ceSignalToggles{"CESignalLaneDetection"};
      std::vector<QString> ceSignalToggleNames{"Only For Detected Lanes"};
      longitudinalToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0, 99, tr("mph"), std::map<float, QString>(), 1.0, true, ceSignalToggles, ceSignalToggleNames, true);

    } else if (param == "CurveSpeedControl") {
      FrogPilotManageControl *curveControlToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(curveControlToggle, &FrogPilotManageControl::manageButtonClicked, [this, longitudinalLayout, curveSpeedPanel]() {
        curveDetectionToggle->setEnabledButtons(0, QDir("/data/media/0/osm/offline").exists());

        longitudinalLayout->setCurrentWidget(curveSpeedPanel);
      });
      longitudinalToggle = curveControlToggle;
    } else if (param == "CurveDetectionMethod") {
      std::vector<QString> curveDetectionToggles{"MapTurnControl", "VisionTurnControl"};
      std::vector<QString> curveDetectionToggleNames{tr("Map Based"), tr("Vision")};
      curveDetectionToggle = new FrogPilotButtonsControl(title, desc, icon, curveDetectionToggleNames, true, false);
      for (int i = 0; i < curveDetectionToggles.size(); ++i) {
        if (params.getBool(curveDetectionToggles[i].toStdString())) {
          curveDetectionToggle->setCheckedButton(i);
        }
      }
      QObject::connect(curveDetectionToggle, &FrogPilotButtonsControl::buttonClicked, [this, curveDetectionToggles](int id) {
        params.putBool(curveDetectionToggles[id].toStdString(), !params.getBool(curveDetectionToggles[id].toStdString()));

        updateToggles();
      });
      QObject::connect(curveDetectionToggle, &FrogPilotButtonsControl::disabledButtonClicked, [this](int id) {
        if (id == 0) {
          ConfirmationDialog::alert(tr("The <b>Map Based</b> option is only available when some <b>Map Data</b> has been downloaded!"), this);
        }
      });
      longitudinalToggle = curveDetectionToggle;
    } else if (param == "CurveSensitivity" || param == "TurnAggressiveness") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 200, "%");

    } else if (param == "LongitudinalTune") {
      FrogPilotManageControl *longitudinalTuneToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(longitudinalTuneToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, longitudinalTunePanel]() {
        longitudinalLayout->setCurrentWidget(longitudinalTunePanel);
      });
      longitudinalToggle = longitudinalTuneToggle;
    } else if (param == "AccelerationProfile") {
      std::vector<QString> accelerationProfiles{tr("Standard"), tr("Eco"), tr("Sport"), tr("Sport+")};
      ButtonParamControl *accelerationProfileToggle = new ButtonParamControl(param, title, desc, icon, accelerationProfiles);
      longitudinalToggle = accelerationProfileToggle;
    } else if (param == "DecelerationProfile") {
      std::vector<QString> decelerationProfiles{tr("Standard"), tr("Eco"), tr("Sport")};
      ButtonParamControl *decelerationProfileToggle = new ButtonParamControl(param, title, desc, icon, decelerationProfiles);
      longitudinalToggle = decelerationProfileToggle;
    } else if (param == "LeadDetectionThreshold") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, "%");
    } else if (param == "MaxDesiredAcceleration") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.1, 4.0, "m/s", std::map<float, QString>(), 0.1);

    } else if (param == "QOLLongitudinal") {
      FrogPilotManageControl *qolLongitudinalToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(qolLongitudinalToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, qolPanel]() {
        longitudinalLayout->setCurrentWidget(qolPanel);
      });
      longitudinalToggle = qolLongitudinalToggle;
    } else if (param == "CustomCruise") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, tr("mph"));
    } else if (param == "CustomCruiseLong") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, tr("mph"));
    } else if (param == "IncreasedStoppedDistance") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, tr(" feet"));
    } else if (param == "MapGears") {
      std::vector<QString> mapGearsToggles{"MapAcceleration", "MapDeceleration"};
      std::vector<QString> mapGearsToggleNames{tr("Acceleration"), tr("Deceleration")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, mapGearsToggles, mapGearsToggleNames);
    } else if (param == "SetSpeedOffset") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr("mph"));

    } else if (param == "SpeedLimitController") {
      FrogPilotManageControl *speedLimitControllerToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(speedLimitControllerToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, speedLimitControllerPanel]() {
        longitudinalLayout->setCurrentWidget(speedLimitControllerPanel);
      });
      longitudinalToggle = speedLimitControllerToggle;
    } else if (param == "SLCFallback") {
      std::vector<QString> fallbackOptions{tr("Set Speed"), tr("Experimental Mode"), tr("Previous Limit")};
      ButtonParamControl *fallbackSelection = new ButtonParamControl(param, title, desc, icon, fallbackOptions);
      longitudinalToggle = fallbackSelection;
    } else if (param == "SLCOverride") {
      std::vector<QString> overrideOptions{tr("None"), tr("Set With Gas Pedal"), tr("Max Set Speed")};
      ButtonParamControl *overrideSelection = new ButtonParamControl(param, title, desc, icon, overrideOptions);
      longitudinalToggle = overrideSelection;
    } else if (param == "SLCPriority") {
      ButtonControl *slcPriorityButton = new ButtonControl(title, tr("SELECT"), desc);
      QStringList primaryPriorities = {tr("Dashboard"), tr("Map Data"), tr("Navigation"), tr("Highest"), tr("Lowest")};
      QStringList otherPriorities = {tr("None"), tr("Dashboard"), tr("Map Data"), tr("Navigation")};
      QStringList priorityPrompts = {tr("Select your primary priority"), tr("Select your secondary priority"), tr("Select your tertiary priority")};

      QObject::connect(slcPriorityButton, &ButtonControl::clicked, [=]() {
        QStringList selectedPriorities;

        for (int i = 1; i <= 3; ++i) {
          QStringList availablePriorities = i == 1 ? primaryPriorities : otherPriorities;
          availablePriorities = availablePriorities.toSet().subtract(selectedPriorities.toSet()).toList();

          if (!hasDashSpeedLimits) {
            availablePriorities.removeAll(tr("Dashboard"));
          }
          if (availablePriorities.size() == 1 && availablePriorities.contains(tr("None"))) {
            break;
          }

          QString selection = MultiOptionDialog::getSelection(priorityPrompts[i - 1], availablePriorities, "", this);
          if (selection.isEmpty()) {
            break;
          }

          selectedPriorities.append(selection);

          params.put(QString("SLCPriority%1").arg(i).toStdString(), selection.toStdString());
          if (selection == tr("None")) {
            for (int j = i + 1; j <= 3; ++j) {
              params.put(QString("SLCPriority%1").arg(j).toStdString(), tr("None").toStdString());
            }
            break;
          }

          if (selection == tr("Lowest") || selection == tr("Highest")) {
            break;
          }
        }

        selectedPriorities.removeAll(tr("None"));
        if (!selectedPriorities.isEmpty()) {
          slcPriorityButton->setValue(selectedPriorities.join(", "));
        }
      });

      QStringList selectedPriorities;
      for (int i = 1; i <= 3; ++i) {
        QString priority = QString::fromStdString(params.get(QString("SLCPriority%1").arg(i).toStdString()));
        if (primaryPriorities.contains(priority)) {
          selectedPriorities.append(priority);
        }
      }
      slcPriorityButton->setValue(selectedPriorities.join(", "));

      longitudinalToggle = slcPriorityButton;
    } else if (param == "SLCOffsets") {
      ButtonControl *manageSLCOffsetsBtn = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageSLCOffsetsBtn, &ButtonControl::clicked, [this, longitudinalLayout, speedLimitControllerOffsetsPanel]() {
        openSubParentToggle();

        longitudinalLayout->setCurrentWidget(speedLimitControllerOffsetsPanel);

        slcOpen = true;
      });
      longitudinalToggle = manageSLCOffsetsBtn;
    } else if (speedLimitControllerOffsetsKeys.find(param) != speedLimitControllerOffsetsKeys.end()) {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, -99, 99, tr("mph"));
    } else if (param == "SLCQOL") {
      ButtonControl *manageSLCQOLBtn = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageSLCQOLBtn, &ButtonControl::clicked, [this, longitudinalLayout, speedLimitControllerQOLPanel]() {
        openSubParentToggle();

        longitudinalLayout->setCurrentWidget(speedLimitControllerQOLPanel);

        slcOpen = true;
      });
      longitudinalToggle = manageSLCQOLBtn;
    } else if (param == "SLCConfirmation") {
      std::vector<QString> confirmationToggles{"SLCConfirmationLower", "SLCConfirmationHigher"};
      std::vector<QString> confirmationToggleNames{tr("Lower Limits"), tr("Higher Limits")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, confirmationToggles, confirmationToggleNames);
    } else if (param == "SLCLookaheadHigher" || param == "SLCLookaheadLower") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 30, tr(" seconds"));
    } else if (param == "SLCVisuals") {
      ButtonControl *manageSLCVisualsBtn = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageSLCVisualsBtn, &ButtonControl::clicked, [this, longitudinalLayout, speedLimitControllerVisualPanel]() {
        openSubParentToggle();

        longitudinalLayout->setCurrentWidget(speedLimitControllerVisualPanel);

        slcOpen = true;
      });
      longitudinalToggle = manageSLCVisualsBtn;

    } else {
      longitudinalToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = longitudinalToggle;

    if (aggressivePersonalityKeys.find(param) != aggressivePersonalityKeys.end()) {
      aggressivePersonalityList->addItem(longitudinalToggle);
    } else if (conditionalExperimentalKeys.find(param) != conditionalExperimentalKeys.end()) {
      conditionalExperimentalList->addItem(longitudinalToggle);
    } else if (curveSpeedKeys.find(param) != curveSpeedKeys.end()) {
      curveSpeedList->addItem(longitudinalToggle);
    } else if (customDrivingPersonalityKeys.find(param) != customDrivingPersonalityKeys.end()) {
      customDrivingPersonalityList->addItem(longitudinalToggle);
    } else if (longitudinalTuneKeys.find(param) != longitudinalTuneKeys.end()) {
      longitudinalTuneList->addItem(longitudinalToggle);
    } else if (qolKeys.find(param) != qolKeys.end()) {
      qolList->addItem(longitudinalToggle);
    } else if (relaxedPersonalityKeys.find(param) != relaxedPersonalityKeys.end()) {
      relaxedPersonalityList->addItem(longitudinalToggle);
    } else if (speedLimitControllerKeys.find(param) != speedLimitControllerKeys.end()) {
      speedLimitControllerList->addItem(longitudinalToggle);
    } else if (speedLimitControllerOffsetsKeys.find(param) != speedLimitControllerOffsetsKeys.end()) {
      speedLimitControllerOffsetsList->addItem(longitudinalToggle);
    } else if (speedLimitControllerQOLKeys.find(param) != speedLimitControllerQOLKeys.end()) {
      speedLimitControllerQOLList->addItem(longitudinalToggle);
    } else if (speedLimitControllerVisualKeys.find(param) != speedLimitControllerVisualKeys.end()) {
      speedLimitControllerVisualList->addItem(longitudinalToggle);
    } else if (standardPersonalityKeys.find(param) != standardPersonalityKeys.end()) {
      standardPersonalityList->addItem(longitudinalToggle);
    } else if (trafficPersonalityKeys.find(param) != trafficPersonalityKeys.end()) {
      trafficPersonalityList->addItem(longitudinalToggle);
    } else {
      longitudinalList->addItem(longitudinalToggle);

      parentKeys.insert(param);
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(longitudinalToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, this, &FrogPilotLongitudinalPanel::openParentToggle);
    }

    QObject::connect(longitudinalToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  FrogPilotParamValueControl *trafficFollowToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficFollow"]);
  FrogPilotParamValueControl *trafficAccelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkAcceleration"]);
  FrogPilotParamValueControl *trafficDecelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkDeceleration"]);
  FrogPilotParamValueControl *trafficDangerToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkDanger"]);
  FrogPilotParamValueControl *trafficSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkSpeed"]);
  FrogPilotParamValueControl *trafficSpeedDecreaseToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkSpeedDecrease"]);
  FrogPilotButtonsControl *trafficResetButton = static_cast<FrogPilotButtonsControl*>(toggles["ResetTrafficPersonality"]);
  QObject::connect(trafficResetButton, &FrogPilotButtonsControl::buttonClicked, this, [=]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for <b>Traffic Mode</b>?"), this)) {
      params.putFloat("TrafficFollow", params_default.getFloat("TrafficFollow"));
      params.putFloat("TrafficJerkAcceleration", params_default.getFloat("TrafficJerkAcceleration"));
      params.putFloat("TrafficJerkDeceleration", params_default.getFloat("TrafficJerkDeceleration"));
      params.putFloat("TrafficJerkDanger", params_default.getFloat("TrafficJerkDanger"));
      params.putFloat("TrafficJerkSpeed", params_default.getFloat("TrafficJerkSpeed"));
      params.putFloat("TrafficJerkSpeedDecrease", params_default.getFloat("TrafficJerkSpeedDecrease"));

      trafficFollowToggle->refresh();
      trafficAccelerationToggle->refresh();
      trafficDecelerationToggle->refresh();
      trafficDangerToggle->refresh();
      trafficSpeedToggle->refresh();
      trafficSpeedDecreaseToggle->refresh();
    }
  });

  FrogPilotParamValueControl *aggressiveFollowToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveFollow"]);
  FrogPilotParamValueControl *aggressiveAccelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkAcceleration"]);
  FrogPilotParamValueControl *aggressiveDecelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkDeceleration"]);
  FrogPilotParamValueControl *aggressiveDangerToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkDanger"]);
  FrogPilotParamValueControl *aggressiveSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkSpeed"]);
  FrogPilotParamValueControl *aggressiveSpeedDecreaseToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkSpeedDecrease"]);
  FrogPilotButtonsControl *aggressiveResetButton = static_cast<FrogPilotButtonsControl*>(toggles["ResetAggressivePersonality"]);
  QObject::connect(aggressiveResetButton, &FrogPilotButtonsControl::buttonClicked, this, [=]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the <b>Aggressive</b> personality?"), this)) {
      params.putFloat("AggressiveFollow", params_default.getFloat("AggressiveFollow"));
      params.putFloat("AggressiveJerkAcceleration", params_default.getFloat("AggressiveJerkAcceleration"));
      params.putFloat("AggressiveJerkDeceleration", params_default.getFloat("AggressiveJerkDeceleration"));
      params.putFloat("AggressiveJerkDanger", params_default.getFloat("AggressiveJerkDanger"));
      params.putFloat("AggressiveJerkSpeed", params_default.getFloat("AggressiveJerkSpeed"));
      params.putFloat("AggressiveJerkSpeedDecrease", params_default.getFloat("AggressiveJerkSpeedDecrease"));

      aggressiveFollowToggle->refresh();
      aggressiveAccelerationToggle->refresh();
      aggressiveDecelerationToggle->refresh();
      aggressiveDangerToggle->refresh();
      aggressiveSpeedToggle->refresh();
      aggressiveSpeedDecreaseToggle->refresh();
    }
  });

  FrogPilotParamValueControl *standardFollowToggle = static_cast<FrogPilotParamValueControl*>(toggles["StandardFollow"]);
  FrogPilotParamValueControl *standardAccelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["StandardJerkAcceleration"]);
  FrogPilotParamValueControl *standardDecelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["StandardJerkDeceleration"]);
  FrogPilotParamValueControl *standardDangerToggle = static_cast<FrogPilotParamValueControl*>(toggles["StandardJerkDanger"]);
  FrogPilotParamValueControl *standardSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["StandardJerkSpeed"]);
  FrogPilotParamValueControl *standardSpeedDecreaseToggle = static_cast<FrogPilotParamValueControl*>(toggles["StandardJerkSpeedDecrease"]);
  FrogPilotButtonsControl *standardResetButton = static_cast<FrogPilotButtonsControl*>(toggles["ResetStandardPersonality"]);
  QObject::connect(standardResetButton, &FrogPilotButtonsControl::buttonClicked, this, [=]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the <b>Standard</b> personality?"), this)) {
      params.putFloat("StandardFollow", params_default.getFloat("StandardFollow"));
      params.putFloat("StandardJerkAcceleration", params_default.getFloat("StandardJerkAcceleration"));
      params.putFloat("StandardJerkDeceleration", params_default.getFloat("StandardJerkDeceleration"));
      params.putFloat("StandardJerkDanger", params_default.getFloat("StandardJerkDanger"));
      params.putFloat("StandardJerkSpeed", params_default.getFloat("StandardJerkSpeed"));
      params.putFloat("StandardJerkSpeedDecrease", params_default.getFloat("StandardJerkSpeedDecrease"));

      standardFollowToggle->refresh();
      standardAccelerationToggle->refresh();
      standardDecelerationToggle->refresh();
      standardDangerToggle->refresh();
      standardSpeedToggle->refresh();
      standardSpeedDecreaseToggle->refresh();
    }
  });

  FrogPilotParamValueControl *relaxedFollowToggle = static_cast<FrogPilotParamValueControl*>(toggles["RelaxedFollow"]);
  FrogPilotParamValueControl *relaxedAccelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["RelaxedJerkAcceleration"]);
  FrogPilotParamValueControl *relaxedDecelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["RelaxedJerkDeceleration"]);
  FrogPilotParamValueControl *relaxedDangerToggle = static_cast<FrogPilotParamValueControl*>(toggles["RelaxedJerkDanger"]);
  FrogPilotParamValueControl *relaxedSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["RelaxedJerkSpeed"]);
  FrogPilotParamValueControl *relaxedSpeedDecreaseToggle = static_cast<FrogPilotParamValueControl*>(toggles["RelaxedJerkSpeedDecrease"]);
  FrogPilotButtonsControl *relaxedResetButton = static_cast<FrogPilotButtonsControl*>(toggles["ResetRelaxedPersonality"]);
  QObject::connect(relaxedResetButton, &FrogPilotButtonsControl::buttonClicked, this, [=]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the <b>Relaxed</b> personality?"), this)) {
      params.putFloat("RelaxedFollow", params_default.getFloat("RelaxedFollow"));
      params.putFloat("RelaxedJerkAcceleration", params_default.getFloat("RelaxedJerkAcceleration"));
      params.putFloat("RelaxedJerkDeceleration", params_default.getFloat("RelaxedJerkDeceleration"));
      params.putFloat("RelaxedJerkDanger", params_default.getFloat("RelaxedJerkDanger"));
      params.putFloat("RelaxedJerkSpeed", params_default.getFloat("RelaxedJerkSpeed"));
      params.putFloat("RelaxedJerkSpeedDecrease", params_default.getFloat("RelaxedJerkSpeedDecrease"));

      relaxedFollowToggle->refresh();
      relaxedAccelerationToggle->refresh();
      relaxedDecelerationToggle->refresh();
      relaxedDangerToggle->refresh();
      relaxedSpeedToggle->refresh();
      relaxedSpeedDecreaseToggle->refresh();
    }
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, [longitudinalLayout, longitudinalPanel] {longitudinalLayout->setCurrentWidget(longitudinalPanel);});
  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubParentToggle, [this, longitudinalLayout, customDrivingPersonalityPanel, speedLimitControllerPanel]() {
    if (customPersonalityOpen) {
      longitudinalLayout->setCurrentWidget(customDrivingPersonalityPanel);

      customPersonalityOpen = false;
    } else if (slcOpen) {
      longitudinalLayout->setCurrentWidget(speedLimitControllerPanel);

      slcOpen = false;
    }
  });
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, this, &FrogPilotLongitudinalPanel::updateMetric);
}

void FrogPilotLongitudinalPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  hasDashSpeedLimits = parent->hasDashSpeedLimits;
  hasPCMCruise = parent->hasPCMCruise;
  isGM = parent->isGM;
  isHKGCanFd = parent->isHKGCanFd;
  isToyota = parent->isToyota;
  isTSK = parent->isTSK;
  tuningLevel = parent->tuningLevel;

  updateToggles();
}

void FrogPilotLongitudinalPanel::updateMetric(bool metric, bool bootRun) {
  static bool previousMetric;
  if (metric != previousMetric && !bootRun) {
    double distanceConversion = metric ? FOOT_TO_METER : METER_TO_FOOT;
    double speedConversion = metric ? MILE_TO_KM : KM_TO_MILE;

    params.putIntNonBlocking("IncreasedStoppedDistance", params.getInt("IncreasedStoppedDistance") * distanceConversion);

    params.putIntNonBlocking("CESignalSpeed", params.getInt("CESignalSpeed") * speedConversion);
    params.putIntNonBlocking("CESpeed", params.getInt("CESpeed") * speedConversion);
    params.putIntNonBlocking("CESpeedLead", params.getInt("CESpeedLead") * speedConversion);
    params.putIntNonBlocking("CustomCruise", params.getInt("CustomCruise") * speedConversion);
    params.putIntNonBlocking("CustomCruiseLong", params.getInt("CustomCruiseLong") * speedConversion);
    params.putIntNonBlocking("Offset1", params.getInt("Offset1") * speedConversion);
    params.putIntNonBlocking("Offset2", params.getInt("Offset2") * speedConversion);
    params.putIntNonBlocking("Offset3", params.getInt("Offset3") * speedConversion);
    params.putIntNonBlocking("Offset4", params.getInt("Offset4") * speedConversion);
    params.putIntNonBlocking("Offset5", params.getInt("Offset5") * speedConversion);
    params.putIntNonBlocking("Offset6", params.getInt("Offset6") * speedConversion);
    params.putIntNonBlocking("Offset7", params.getInt("Offset7") * speedConversion);
    params.putIntNonBlocking("SetSpeedOffset", params.getInt("SetSpeedOffset") * speedConversion);
  }
  previousMetric = metric;

  static std::map<float, QString> imperialDistanceLabels;
  static std::map<float, QString> imperialSpeedLabels;
  static std::map<float, QString> metricDistanceLabels;
  static std::map<float, QString> metricSpeedLabels;

  static bool labelsInitialized = false;
  if (!labelsInitialized) {
    for (int i = 0; i <= 10; ++i) {
      imperialDistanceLabels[i] = i == 0 ? tr("Off") : i == 1 ? QString::number(i) + tr(" foot") : QString::number(i) + tr(" feet");
    }

    for (int i = 0; i <= 99; ++i) {
      imperialSpeedLabels[i] = i == 0 ? tr("Off") : QString::number(i) + tr("mph");
    }

    for (int i = 0; i <= 3; ++i) {
      metricDistanceLabels[i] = i == 0 ? tr("Off") : i == 1 ? QString::number(i) + tr(" meter") : QString::number(i) + tr(" meters");
    }

    for (int i = 0; i <= 150; ++i) {
      metricSpeedLabels[i] = i == 0 ? tr("Off") : QString::number(i) + tr("km/h");
    }

    labelsInitialized = true;
  }

  FrogPilotDualParamValueControl *ceSpeedToggle = reinterpret_cast<FrogPilotDualParamValueControl*>(toggles["CESpeed"]);
  FrogPilotParamValueButtonControl *ceSignal = static_cast<FrogPilotParamValueButtonControl*>(toggles["CESignalSpeed"]);
  FrogPilotParamValueControl *customCruiseToggle = static_cast<FrogPilotParamValueControl*>(toggles["CustomCruise"]);
  FrogPilotParamValueControl *customCruiseLongToggle = static_cast<FrogPilotParamValueControl*>(toggles["CustomCruiseLong"]);
  FrogPilotParamValueControl *offset1Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset1"]);
  FrogPilotParamValueControl *offset2Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset2"]);
  FrogPilotParamValueControl *offset3Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset3"]);
  FrogPilotParamValueControl *offset4Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset4"]);
  FrogPilotParamValueControl *offset5Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset5"]);
  FrogPilotParamValueControl *offset6Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset6"]);
  FrogPilotParamValueControl *offset7Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset7"]);
  FrogPilotParamValueControl *increasedStoppedDistanceToggle = static_cast<FrogPilotParamValueControl*>(toggles["IncreasedStoppedDistance"]);
  FrogPilotParamValueControl *setSpeedOffsetToggle = static_cast<FrogPilotParamValueControl*>(toggles["SetSpeedOffset"]);

  if (metric) {
    offset1Toggle->setTitle(tr("Speed Limit Offset (0–29 km/h)"));
    offset2Toggle->setTitle(tr("Speed Limit Offset (30–49 km/h)"));
    offset3Toggle->setTitle(tr("Speed Limit Offset (50–59 km/h)"));
    offset4Toggle->setTitle(tr("Speed Limit Offset (60–79 km/h)"));
    offset5Toggle->setTitle(tr("Speed Limit Offset (80–99 km/h)"));
    offset6Toggle->setTitle(tr("Speed Limit Offset (100–119 km/h)"));
    offset7Toggle->setTitle(tr("Speed Limit Offset (120–140 km/h)"));

    offset1Toggle->setDescription(tr("Target speed offset applied for posted limits between 0 and 29 km/h."));
    offset2Toggle->setDescription(tr("Target speed offset applied for posted limits between 30 and 49 km/h."));
    offset3Toggle->setDescription(tr("Target speed offset applied for posted limits between 50 and 59 km/h."));
    offset4Toggle->setDescription(tr("Target speed offset applied for posted limits between 60 and 79 km/h."));
    offset5Toggle->setDescription(tr("Target speed offset applied for posted limits between 80 and 99 km/h."));
    offset6Toggle->setDescription(tr("Target speed offset applied for posted limits between 100 and 119 km/h."));
    offset7Toggle->setDescription(tr("Target speed offset applied for posted limits between 120 and 140 km/h."));

    increasedStoppedDistanceToggle->updateControl(0, 3, metricDistanceLabels);

    ceSignal->updateControl(0, 150, metricSpeedLabels);
    ceSpeedToggle->updateControl(0, 150, metricSpeedLabels);
    customCruiseToggle->updateControl(1, 150, metricSpeedLabels);
    customCruiseLongToggle->updateControl(1, 150, metricSpeedLabels);
    offset1Toggle->updateControl(-150, 150, metricSpeedLabels);
    offset2Toggle->updateControl(-150, 150, metricSpeedLabels);
    offset3Toggle->updateControl(-150, 150, metricSpeedLabels);
    offset4Toggle->updateControl(-150, 150, metricSpeedLabels);
    offset5Toggle->updateControl(-150, 150, metricSpeedLabels);
    offset6Toggle->updateControl(-150, 150, metricSpeedLabels);
    offset7Toggle->updateControl(-150, 150, metricSpeedLabels);
    setSpeedOffsetToggle->updateControl(-150, 150, metricSpeedLabels);
  } else {
    offset1Toggle->setTitle(tr("Speed Limit Offset (0–24 mph)"));
    offset2Toggle->setTitle(tr("Speed Limit Offset (25–34 mph)"));
    offset3Toggle->setTitle(tr("Speed Limit Offset (35–44 mph)"));
    offset4Toggle->setTitle(tr("Speed Limit Offset (45–54 mph)"));
    offset5Toggle->setTitle(tr("Speed Limit Offset (55–64 mph)"));
    offset6Toggle->setTitle(tr("Speed Limit Offset (65–74 mph)"));
    offset7Toggle->setTitle(tr("Speed Limit Offset (75–99 mph)"));

    offset1Toggle->setDescription(tr("Target speed offset applied for posted limits between 0 and 24 mph."));
    offset2Toggle->setDescription(tr("Target speed offset applied for posted limits between 25 and 34 mph."));
    offset3Toggle->setDescription(tr("Target speed offset applied for posted limits between 35 and 44 mph."));
    offset4Toggle->setDescription(tr("Target speed offset applied for posted limits between 45 and 54 mph."));
    offset5Toggle->setDescription(tr("Target speed offset applied for posted limits between 55 and 64 mph."));
    offset6Toggle->setDescription(tr("Target speed offset applied for posted limits between 65 and 74 mph."));
    offset7Toggle->setDescription(tr("Target speed offset applied for posted limits between 75 and 99 mph."));

    increasedStoppedDistanceToggle->updateControl(0, 10, imperialDistanceLabels);

    ceSignal->updateControl(0, 99, imperialSpeedLabels);
    ceSpeedToggle->updateControl(0, 99, imperialSpeedLabels);
    customCruiseToggle->updateControl(1, 99, imperialSpeedLabels);
    customCruiseLongToggle->updateControl(1, 99, imperialSpeedLabels);
    offset1Toggle->updateControl(-99, 99, imperialSpeedLabels);
    offset2Toggle->updateControl(-99, 99, imperialSpeedLabels);
    offset3Toggle->updateControl(-99, 99, imperialSpeedLabels);
    offset4Toggle->updateControl(-99, 99, imperialSpeedLabels);
    offset5Toggle->updateControl(-99, 99, imperialSpeedLabels);
    offset6Toggle->updateControl(-99, 99, imperialSpeedLabels);
    offset7Toggle->updateControl(-99, 99, imperialSpeedLabels);
    setSpeedOffsetToggle->updateControl(0, 99, imperialSpeedLabels);
  }
}

void FrogPilotLongitudinalPanel::updateToggles() {
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

    if (key == "CurveSensitivity" || key == "TurnAggressiveness") {
      setVisible &= params.getBool("MapTurnControl") || params.getBool("VisionTurnControl");
    }

    if (key == "CustomCruise" || key == "CustomCruiseLong" || key == "SetSpeedLimit" || key == "SetSpeedOffset") {
      setVisible &= !hasPCMCruise;
    }

    if (key == "ForceMPHDashboard") {
      setVisible &= isToyota;
    }

    if (key == "MapGears") {
      setVisible &= isGM || isHKGCanFd || isToyota;
      setVisible &= !isTSK;
    }

    if (key == "MTSCCurvatureCheck") {
      setVisible &= params.getBool("MapTurnControl");
    }

    if (key == "ReverseCruise") {
      setVisible &= hasPCMCruise;
      setVisible &= isToyota;
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (aggressivePersonalityKeys.find(key) != aggressivePersonalityKeys.end()) {
        toggles["AggressivePersonalityProfile"]->setVisible(true);
      } else if (conditionalExperimentalKeys.find(key) != conditionalExperimentalKeys.end()) {
        toggles["ConditionalExperimental"]->setVisible(true);
      } else if (curveSpeedKeys.find(key) != curveSpeedKeys.end()) {
        toggles["CurveSpeedControl"]->setVisible(true);
      } else if (customDrivingPersonalityKeys.find(key) != customDrivingPersonalityKeys.end()) {
        toggles["CustomPersonalities"]->setVisible(true);
      } else if (longitudinalTuneKeys.find(key) != longitudinalTuneKeys.end()) {
        toggles["LongitudinalTune"]->setVisible(true);
      } else if (qolKeys.find(key) != qolKeys.end()) {
        toggles["QOLLongitudinal"]->setVisible(true);
      } else if (relaxedPersonalityKeys.find(key) != relaxedPersonalityKeys.end()) {
        toggles["RelaxedPersonalityProfile"]->setVisible(true);
      } else if (speedLimitControllerKeys.find(key) != speedLimitControllerKeys.end()) {
        toggles["SpeedLimitController"]->setVisible(true);
      } else if (speedLimitControllerOffsetsKeys.find(key) != speedLimitControllerOffsetsKeys.end()) {
        toggles["SLCOffsets"]->setVisible(true);
      } else if (speedLimitControllerQOLKeys.find(key) != speedLimitControllerQOLKeys.end()) {
        toggles["SLCQOL"]->setVisible(true);
      } else if (speedLimitControllerVisualKeys.find(key) != speedLimitControllerVisualKeys.end()) {
        toggles["SLCVisuals"]->setVisible(true);
      } else if (standardPersonalityKeys.find(key) != standardPersonalityKeys.end()) {
        toggles["StandardPersonalityProfile"]->setVisible(true);
      } else if (trafficPersonalityKeys.find(key) != trafficPersonalityKeys.end()) {
        toggles["TrafficPersonalityProfile"]->setVisible(true);
      }
    }
  }

  update();
}
