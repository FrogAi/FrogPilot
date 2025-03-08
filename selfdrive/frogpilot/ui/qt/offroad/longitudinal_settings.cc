#include "selfdrive/frogpilot/ui/qt/offroad/longitudinal_settings.h"

FrogPilotLongitudinalPanel::FrogPilotLongitudinalPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> longitudinalToggles {
    {"ConditionalExperimental", tr("Conditional Experimental Mode"), tr("Automatically switch to 'Experimental Mode' when specific conditions are met."), "../frogpilot/assets/toggle_icons/icon_conditional.png"},
    {"CESpeed", tr("Below"), tr("Triggers 'Experimental Mode' when driving below the set speed without a lead vehicle."), ""},
    {"CECurves", tr("Curve Detected Ahead"), tr("Triggers 'Experimental Mode' when a curve is detected in the road ahead."), ""},
    {"CELead", tr("Lead Detected Ahead"), tr("Triggers 'Experimental Mode' when a slower or stopped vehicle is detected ahead."), ""},
    {"CENavigation", tr("Navigation Data"), tr("Triggers 'Experimental Mode' based on navigation data, such as upcoming intersections or turns."), ""},
    {"CEModelStopTime", tr("openpilot Wants to Stop In"), tr("Triggers 'Experimental Mode' when openpilot wants to stop such as for a stop sign or red light."), ""},
    {"CESignalSpeed", tr("Turn Signal Below"), tr("Triggers 'Experimental Mode' when using turn signals below the set speed."), ""},
    {"ShowCEMStatus", tr("Status Widget"), tr("Show the current status in the onroad UI."), ""},

    {"CurveSpeedControl", tr("Curve Speed Control"), tr("Automatically slow down for curves detected ahead or through the downloaded maps."), "../frogpilot/assets/toggle_icons/icon_speed_map.png"},
    {"CurveDetectionMethod", tr("Curve Detection Method"), tr("Uses data from either the downloaded maps or the model to determine where curves are."), ""},
    {"MTSCCurvatureCheck", tr("Curve Detection Failsafe"), tr("Triggers 'Curve Speed Control' only when a curve is detected with the model as well when using the 'Map Based' method."), ""},
    {"CurveSensitivity", tr("Curve Detection Sensitivity"), tr("Controls how sensitive openpilot is to detecting curves. Higher values trigger earlier responses at the risk of triggering too often, while lower values increase confidence at the risk of triggering too infrequently."), ""},
    {"TurnAggressiveness", tr("Speed Aggressiveness"), tr("Controls how aggressive openpilot takes turns. Higher values result in faster turns, while lower values result in slower turns."), ""},
    {"HideCSCUI", tr("Hide Desired Speed Widget From UI"), tr("Hides the desired speed widget from the onroad UI."), ""},

    {"CustomPersonalities", tr("Customize Driving Personalities"), tr("Customize the personality profiles to suit your driving style."), "../frogpilot/assets/toggle_icons/icon_personality.png"},
    {"TrafficPersonalityProfile", tr("Traffic Personality"), tr("Customizes the 'Traffic' personality profile, tailored for navigating through traffic."), "../frogpilot/assets/stock_theme/distance_icons/traffic.png"},
    {"TrafficFollow", tr("Following Distance"), tr("Controls the minimum following distance in 'Traffic' mode. openpilot will automatically dynamically between this value and the 'Aggressive' profile distance based on your current speed."), ""},
    {"TrafficJerkAcceleration", tr("Acceleration Sensitivity"), tr("Controls how sensitive openpilot is to changes in acceleration in 'Traffic' mode. Higher values result in smoother, more gradual acceleration, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"TrafficJerkDeceleration", tr("Deceleration Sensitivity"), tr("Controls how sensitive openpilot is to changes in deceleration in 'Traffic' mode. Higher values result in smoother, more gradual deceleration, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"TrafficJerkDanger", tr("Safety Distance Sensitivity"), tr("Adjusts how cautious openpilot is around other vehicles or obstacles in 'Traffic' mode. Higher values increase following distances and prioritize safety, leading to more cautious driving, while lower values allow for closer following but may reduce reaction time."), ""},
    {"TrafficJerkSpeed", tr("Speed Increase Responsiveness"), tr("Controls how quickly openpilot increases speed in 'Traffic' mode. Higher values ensure smoother, more gradual speed changes when accelerating, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"TrafficJerkSpeedDecrease", tr("Speed Decrease Responsiveness"), tr("Controls how quickly openpilot decreases speed in 'Traffic' mode. Higher values ensure smoother, more gradual speed changes when slowing down, while lower values allow for quicker, more responsive changes that may feel abrupt."), ""},
    {"ResetTrafficPersonality", tr("Reset Settings"), tr("Restores the 'Traffic Mode' settings to their default values."), ""},

    {"AggressivePersonalityProfile", tr("Aggressive Personality"), tr("Customize the 'Aggressive' personality profile, designed for a more assertive driving style."), "../frogpilot/assets/stock_theme/distance_icons/aggressive.png"},
    {"AggressiveFollow", tr("Following Distance"), tr("Sets the following distance for 'Aggressive' mode. This determines roughly how many seconds you'll follow behind the car ahead.\n\nDefault: 1.25 seconds."), ""},
    {"AggressiveJerkAcceleration", tr("Acceleration Sensitivity"), tr("Controls how sensitive openpilot is to changes in acceleration in 'Aggressive' mode. Higher values result in smoother, more gradual acceleration, while lower values allow for quicker, more responsive changes that may feel abrupt.\n\nDefault: 0.5."), ""},
    {"AggressiveJerkDeceleration", tr("Deceleration Sensitivity"), tr("Controls how sensitive openpilot is to changes in deceleration in 'Aggressive' mode. Higher values result in smoother, more gradual deceleration, while lower values allow for quicker, more responsive changes that may feel abrupt.\n\nDefault: 0.5."), ""},
    {"AggressiveJerkDanger", tr("Safety Distance Sensitivity"), tr("Adjusts how cautious openpilot is around other vehicles or obstacles in 'Aggressive' mode. Higher values increase following distances and prioritize safety, leading to more cautious driving, while lower values allow for closer following but may reduce reaction time.\n\nDefault: 1.0."), ""},
    {"AggressiveJerkSpeed", tr("Speed Increase Responsiveness"), tr("Controls how quickly openpilot increases speed in 'Aggressive' mode. Higher values ensure smoother, more gradual speed changes when accelerating, while lower values allow for quicker, more responsive changes that may feel abrupt.\n\nDefault: 0.5."), ""},
    {"AggressiveJerkSpeedDecrease", tr("Speed Decrease Responsiveness"), tr("Controls how quickly openpilot decreases speed in 'Aggressive' mode. Higher values ensure smoother, more gradual speed changes when slowing down, while lower values allow for quicker, more responsive changes that may feel abrupt.\n\nDefault: 0.5."), ""},
    {"ResetAggressivePersonality", tr("Reset Settings"), tr("Restores the 'Aggressive' settings to their default values."), ""},

    {"StandardPersonalityProfile", tr("Standard Personality"), tr("Customize the 'Standard' personality profile, optimized for balanced driving."), "../frogpilot/assets/stock_theme/distance_icons/standard.png"},
    {"StandardFollow", tr("Following Distance"), tr("Set the following distance for 'Standard' mode. This determines roughly how many seconds you'll follow behind the car ahead.\n\nDefault: 1.45 seconds."), ""},
    {"StandardJerkAcceleration", tr("Acceleration Sensitivity"), tr("Controls how sensitive openpilot is to changes in acceleration in 'Standard' mode. Higher values result in smoother, more gradual acceleration, while lower values allow for quicker, more responsive changes that may feel abrupt.\n\nDefault: 1.0."), ""},
    {"StandardJerkDeceleration", tr("Deceleration Sensitivity"), tr("Controls how sensitive openpilot is to changes in deceleration in 'Standard' mode. Higher values result in smoother braking, while lower values allow for quicker, more immediate braking that may feel abrupt.\n\nDefault: 1.0."), ""},
    {"StandardJerkDanger", tr("Safety Distance Sensitivity"), tr("Adjusts how cautious openpilot is around other vehicles or obstacles in 'Standard' mode. Higher values increase following distances and prioritize safety, leading to more cautious driving, while lower values allow for closer following but may reduce reaction time.\n\nDefault: 1.0."), ""},
    {"StandardJerkSpeed", tr("Speed Increase Responsiveness"), tr("Controls how quickly openpilot increases speed in 'Standard' mode. Higher values ensure smoother, more gradual speed changes when accelerating, while lower values allow for quicker, more responsive changes that may feel abrupt.\n\nDefault: 1.0."), ""},
    {"StandardJerkSpeedDecrease", tr("Speed Decrease Responsiveness"), tr("Controls how quickly openpilot decreases speed in 'Standard' mode. Higher values ensure smoother, more gradual speed changes when slowing down, while lower values allow for quicker, more responsive changes that may feel abrupt.\n\nDefault: 1.0."), ""},
    {"ResetStandardPersonality", tr("Reset Settings"), tr("Restores the 'Standard' settings to their default values."), ""},

    {"RelaxedPersonalityProfile", tr("Relaxed Personality"), tr("Customize the 'Relaxed' personality profile, ideal for a more laid-back driving style."), "../frogpilot/assets/stock_theme/distance_icons/relaxed.png"},
    {"RelaxedFollow", tr("Following Distance"), tr("Set the following distance for 'Relaxed' mode. This determines roughly how many seconds you'll follow behind the car ahead.\n\nDefault: 1.75 seconds."), ""},
    {"RelaxedJerkAcceleration", tr("Acceleration Sensitivity"), tr("Controls how sensitive openpilot is to changes in acceleration in 'Relaxed' mode. Higher values result in smoother, more gradual acceleration, while lower values allow for quicker, more responsive changes that may feel abrupt.\n\nDefault: 1.0."), ""},
    {"RelaxedJerkDeceleration", tr("Deceleration Sensitivity"), tr("Controls how sensitive openpilot is to changes in deceleration in 'Relaxed' mode. Higher values result in smoother braking, while lower values allow for quicker, more immediate braking that may feel abrupt.\n\nDefault: 1.0."), ""},
    {"RelaxedJerkDanger", tr("Safety Distance Sensitivity"), tr("Adjusts how cautious openpilot is around other vehicles or obstacles in 'Relaxed' mode. Higher values increase following distances and prioritize safety, leading to more cautious driving, while lower values allow for closer following but may reduce reaction time.\n\nDefault: 1.0."), ""},
    {"RelaxedJerkSpeed", tr("Speed Increase Responsiveness"), tr("Controls how quickly openpilot increases speed in 'Relaxed' mode. Higher values ensure smoother, more gradual speed changes when accelerating, while lower values allow for quicker, more responsive changes that may feel abrupt.\n\nDefault: 1.0."), ""},
    {"RelaxedJerkSpeedDecrease", tr("Speed Decrease Responsiveness"), tr("Controls how quickly openpilot decreases speed in 'Relaxed' mode. Higher values ensure smoother, more gradual speed changes when slowing down, while lower values allow for quicker, more responsive changes that may feel abrupt.\n\nDefault: 1.0."), ""},
    {"ResetRelaxedPersonality", tr("Reset Settings"), tr("Restores the 'Relaxed' settings to their default values."), ""},

    {"ExperimentalModeActivation", tr("Experimental Mode Activation"), tr("Toggle 'Experimental Mode' on/off using either the steering wheel buttons or screen.\n\nThis overrides 'Conditional Experimental Mode'."), "../assets/img_experimental_white.svg"},
    {"ExperimentalModeViaLKAS", tr("Click the LKAS Button"), tr("Toggles 'Experimental Mode' by pressing the 'LKAS' button on the steering wheel."), ""},
    {"ExperimentalModeViaTap", tr("Double-Tap the Screen"), tr("Toggles 'Experimental Mode' by double-tapping the onroad UI within a 0.5 second period."), ""},
    {"ExperimentalModeViaDistance", tr("Long Press the Distance Button"), tr("Toggles 'Experimental Mode' by holding down the 'distance' button on the steering wheel for 0.5 seconds."), ""},

    {"LongitudinalTune", tr("Longitudinal Tuning"), tr("Settings that control how openpilot manages speed and acceleration."), "../frogpilot/assets/toggle_icons/icon_longitudinal_tune.png"},
    {"AccelerationProfile", tr("Acceleration Profile"), tr("Enables either a sporty or eco-friendly acceleration rate. 'Sport+' aims to make openpilot accelerate as fast as possible."), ""},
    {"DecelerationProfile", tr("Deceleration Profile"), tr("Enables either a sporty or eco-friendly deceleration rate."), ""},
    {"HumanAcceleration", tr("Human-Like Acceleration"), tr("Uses the lead's acceleration rate when at a takeoff and ramps off the acceleration rate when approaching the maximum set speed for a more 'human-like' driving experience."), ""},
    {"HumanFollowing", tr("Human-Like Approach Behind Leads"), tr("Dynamically adjusts the following distance when approaching slower or stopped vehicles for a more 'human-like' driving experience."), ""},
    {"LeadDetectionThreshold", tr("Lead Detection Confidence"), tr("Controls how sensitive openpilot is to detecting vehicles ahead. A lower value can help detect vehicles sooner and from farther away, but increases the chance openpilot mistakes other objects for vehicles."), ""},
    {"MaxDesiredAcceleration", tr("Maximum Acceleration Rate"), tr("Sets a cap on how fast openpilot can accelerate."), ""},
    {"TacoTune", tr("'Taco Bell Run' Turn Speed Hack"), tr("Enable comma's speed hack they used to help handle left and right turns more precisely during their 2022 'Taco Bell' drive by reducing the maximum allowed speed and acceleration while turning."), ""},

    {"QOLLongitudinal", tr("Quality of Life Improvements"), tr("Miscellaneous longitudinal focused features to improve your overall openpilot experience."), "../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"CustomCruise", tr("Cruise Increase"), tr("Controls the interval used when increasing the cruise control speed."), ""},
    {"CustomCruiseLong", tr("Cruise Increase (Long Press)"), tr("Controls the interval used when increasing the cruise control speed while holding down the button for 0.5+ seconds."), ""},
    {"ForceStandstill", tr("Force Keep openpilot in the Standstill State"), tr("Keeps openpilot in the 'standstill' state until the gas pedal or 'resume' button is pressed."), ""},
    {"ForceStops", tr("Force Stop for 'Detected' Stop Lights/Signs"), tr("Forces a stop whenever openpilot 'detects' a potential red light/stop sign to prevent it from running the red light/stop sign."), ""},
    {"IncreasedStoppedDistance", tr("Increase Stopped Distance"), tr("Increases the distance to stop behind vehicles."), ""},
    {"SetSpeedOffset", tr("Set Speed Offset"), tr("Controls how much higher or lower the set speed should be compared to your current set speed. For example, if you prefer to drive 5 mph above the speed limit, this setting will automatically add that difference when you adjust your set speed."), ""},
    {"MapGears", tr("Map Accel/Decel to Gears"), tr("Maps the acceleration and deceleration profiles to your car's 'Eco' or 'Sport' gear modes."), ""},
    {"ReverseCruise", tr("Reverse Cruise Increase"), tr("Reverses the long press cruise increase feature to increase the max speed by 5 mph instead of 1 on short presses."), ""},

    {"SpeedLimitController", tr("Speed Limit Controller"), tr("Automatically adjust your max speed to match the speed limit using downloaded 'Open Street Maps' data, 'Navigate on openpilot', or your car's dashboard (Toyota/Lexus/HKG only)."), "../assets/offroad/icon_speed_limit.png"},
    {"SLCFallback", tr("Fallback Method"), tr("Controls what happens when no speed limit data is available."), ""},
    {"SLCOverride", tr("Override Method"), tr("Controls how the current speed limit is overriden.\n\n"), ""},
    {"SLCQOL", tr("Quality of Life Improvements"), tr("Miscellaneous 'Speed Limit Controller' focused features to improve your overall openpilot experience."), ""},
    {"SLCConfirmation", tr("Confirm New Speed Limits"), tr("Enables manual confirmations before using a new speed limit."), ""},
    {"ForceMPHDashboard", tr("Force MPH Readings from Dashboard"), tr("Forces speed limit readings from the dashboard to MPH if it normally displays them in KPH."), ""},
    {"SLCLookaheadHigher", tr("Prepare for Higher Speed Limits"), tr("Sets a lookahead value to prepare for upcoming higher speed limits when using downloaded map data."), ""},
    {"SLCLookaheadLower", tr("Prepare for Lower Speed Limits"), tr("Sets a lookahead value to prepare for upcoming lower speed limits when using downloaded map data."), ""},
    {"SetSpeedLimit", tr("Set Speed to Current Limit"), tr("Sets your max speed to match the current speed limit when enabling openpilot."), ""},
    {"SLCPriority", tr("Speed Limit Source Priority Order"), tr("Sets the order of priority for speed limit data sources."), ""},
    {"SLCOffsets", tr("Speed Limit Offsets"), tr("Set speed limit offsets to drive over the posted speed limit."), ""},
    {"Offset1", tr("Speed Limit Offset (0-34 mph)"), tr("Sets the speed limit offset for speeds between 0 and 34 mph."), ""},
    {"Offset2", tr("Speed Limit Offset (35-54 mph)"), tr("Sets the speed limit offset for speeds between 35 and 54 mph."), ""},
    {"Offset3", tr("Speed Limit Offset (55-64 mph)"), tr("Sets the speed limit offset for speeds between 55 and 64 mph."), ""},
    {"Offset4", tr("Speed Limit Offset (65-99 mph)"), tr("Sets the speed limit offset for speeds between 65 and 99 mph."), ""},
    {"SLCVisuals", tr("Visuals"), tr("Visual 'Speed Limit Controller' features to improve your overall openpilot experience."), ""},
    {"ShowSLCOffset", tr("Show Speed Limit Offset"), tr("Displays the speed limit offset separately in the onroad UI when using 'Speed Limit Controller'."), ""},
    {"SpeedLimitSources", tr("Show Speed Limit Sources"), tr("Displays the speed limit sources in the onroad UI when using 'Speed Limit Controller'."), ""},
  };

  for (const auto &[param, title, desc, icon] : longitudinalToggles) {
    AbstractControl *longitudinalToggle;

    if (param == "CustomPersonalities") {
      FrogPilotManageControl *customPersonalitiesToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(customPersonalitiesToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        showToggles(customDrivingPersonalityKeys);
      });
      longitudinalToggle = customPersonalitiesToggle;
    } else if (param == "ResetTrafficPersonality" || param == "ResetAggressivePersonality" || param == "ResetStandardPersonality" || param == "ResetRelaxedPersonality") {
      ButtonControl *resetBtn = new ButtonControl(title, tr("RESET"), desc);
      longitudinalToggle = resetBtn;
    } else if (param == "TrafficPersonalityProfile") {
      FrogPilotManageControl *trafficPersonalityToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(trafficPersonalityToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        customPersonalityOpen = true;
        openSubParentToggle();
        showToggles(trafficPersonalityKeys);
      });
      longitudinalToggle = trafficPersonalityToggle;
    } else if (param == "AggressivePersonalityProfile") {
      FrogPilotManageControl *aggressivePersonalityToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(aggressivePersonalityToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        customPersonalityOpen = true;
        openSubParentToggle();
        showToggles(aggressivePersonalityKeys);
      });
      longitudinalToggle = aggressivePersonalityToggle;
    } else if (param == "StandardPersonalityProfile") {
      FrogPilotManageControl *standardPersonalityToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(standardPersonalityToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        customPersonalityOpen = true;
        openSubParentToggle();
        showToggles(standardPersonalityKeys);
      });
      longitudinalToggle = standardPersonalityToggle;
    } else if (param == "RelaxedPersonalityProfile") {
      FrogPilotManageControl *relaxedPersonalityToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(relaxedPersonalityToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        customPersonalityOpen = true;
        openSubParentToggle();
        showToggles(relaxedPersonalityKeys);
      });
      longitudinalToggle = relaxedPersonalityToggle;
    } else if (trafficPersonalityKeys.find(param) != trafficPersonalityKeys.end() ||
               aggressivePersonalityKeys.find(param) != aggressivePersonalityKeys.end() ||
               standardPersonalityKeys.find(param) != standardPersonalityKeys.end() ||
               relaxedPersonalityKeys.find(param) != relaxedPersonalityKeys.end()) {
      if (param == "TrafficFollow" || param == "AggressiveFollow" || param == "StandardFollow" || param == "RelaxedFollow") {
        std::map<float, QString> followTimeLabels;
        for (float i = 0; i <= 5; i += 0.01) {
          followTimeLabels[i] = QString::number(i, 'f', 2) + tr((QString::number(i, 'f', 2) == "1.0") ? " second" : " seconds");
        }
        if (param == "TrafficFollow") {
          longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.5, 5, QString(), followTimeLabels, 0.01);
        } else {
          longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 5, QString(), followTimeLabels, 0.01);
        }
      } else {
        longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 500, "%");
      }

    } else if (param == "ConditionalExperimental") {
      FrogPilotManageControl *conditionalExperimentalToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(conditionalExperimentalToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        showToggles(conditionalExperimentalKeys);
      });
      longitudinalToggle = conditionalExperimentalToggle;
    } else if (param == "CESpeed") {
      FrogPilotParamValueControl *CESpeed = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr("mph"), std::map<float, QString>(), 1, true);
      FrogPilotParamValueControl *CESpeedLead = new FrogPilotParamValueControl("CESpeedLead", tr(" With Lead"), tr("Switches to 'Experimental Mode' when driving below the set speed with a lead vehicle."), icon, 0, 99, tr("mph"), std::map<float, QString>(), 1, true);
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
      longitudinalToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0, 99, tr("mph"), std::map<float, QString>(), 1.0, ceSignalToggles, ceSignalToggleNames, true);

    } else if (param == "CurveSpeedControl") {
      FrogPilotManageControl *curveControlToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(curveControlToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        curveDetectionToggle->setEnabledButtons(0, QDir("/data/media/0/osm/offline").exists());

        std::set<QString> modifiedCurveSpeedKeys = curveSpeedKeys;

        if (!params.getBool("MapTurnControl")) {
          modifiedCurveSpeedKeys.erase("MTSCCurvatureCheck");
        }

        if (!(params.getBool("MapTurnControl") || params.getBool("VisionTurnControl"))) {
          modifiedCurveSpeedKeys.erase("CurveSensitivity");
          modifiedCurveSpeedKeys.erase("TurnAggressiveness");
        }

        showToggles(modifiedCurveSpeedKeys);
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

        std::set<QString> modifiedCurveSpeedKeys = curveSpeedKeys;

        if (!params.getBool("MapTurnControl")) {
          modifiedCurveSpeedKeys.erase("MTSCCurvatureCheck");
        }

        if (!(params.getBool("MapTurnControl") || params.getBool("VisionTurnControl"))) {
          modifiedCurveSpeedKeys.erase("CurveSensitivity");
          modifiedCurveSpeedKeys.erase("TurnAggressiveness");
        }

        showToggles(modifiedCurveSpeedKeys);
      });
      QObject::connect(curveDetectionToggle, &FrogPilotButtonsControl::disabledButtonClicked, [this](int id) {
        if (id == 0) {
          ConfirmationDialog::alert(tr("The 'Map Based' option is only available when some 'Map Data' has been downloaded!"), this);
        }
      });
      longitudinalToggle = curveDetectionToggle;
    } else if (param == "CurveSensitivity" || param == "TurnAggressiveness") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 200, "%");

    } else if (param == "ExperimentalModeActivation") {
      FrogPilotManageControl *experimentalModeActivationToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(experimentalModeActivationToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedExperimentalModeActivationKeys = experimentalModeActivationKeys;

        if (isSubaru || (params.getBool("AlwaysOnLateral") && params.getBool("AlwaysOnLateralLKAS"))) {
          modifiedExperimentalModeActivationKeys.erase("ExperimentalModeViaLKAS");
        }

        showToggles(modifiedExperimentalModeActivationKeys);
      });
      longitudinalToggle = experimentalModeActivationToggle;

    } else if (param == "LongitudinalTune") {
      FrogPilotManageControl *longitudinalTuneToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(longitudinalTuneToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        showToggles(longitudinalTuneKeys);
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
      QObject::connect(qolLongitudinalToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedQolKeys = qolKeys;

        if (!hasPCMCruise) {
          modifiedQolKeys.erase("ReverseCruise");
        } else {
          modifiedQolKeys.erase("CustomCruise");
          modifiedQolKeys.erase("CustomCruiseLong");
          modifiedQolKeys.erase("SetSpeedOffset");
        }

        if (!(isGM || isHKGCanFd || isToyota)) {
          modifiedQolKeys.erase("MapGears");
        }

        showToggles(modifiedQolKeys);
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
      QObject::connect(speedLimitControllerToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        slcOpen = true;

        showToggles(speedLimitControllerKeys);
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
          QStringList availablePriorities = (i == 1) ? primaryPriorities : otherPriorities;
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

          params.put(QString("SLCPriority%1").arg(i).toStdString(), selection.toStdString());
          selectedPriorities.append(selection);

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
        slcPriorityButton->setValue(selectedPriorities.join(", "));
      });

      QStringList initialPriorities;
      for (int i = 1; i <= 3; ++i) {
        QString priority = QString::fromStdString(params.get(QString("SLCPriority%1").arg(i).toStdString()));
        if (!priority.isEmpty() && priority != tr("None") && primaryPriorities.contains(priority)) {
          initialPriorities.append(priority);
        }
      }
      slcPriorityButton->setValue(initialPriorities.join(", "));
      longitudinalToggle = slcPriorityButton;
    } else if (param == "SLCOffsets") {
      ButtonControl *manageSLCOffsetsBtn = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageSLCOffsetsBtn, &ButtonControl::clicked, [this]() {
        openSubParentToggle();
        showToggles(speedLimitControllerOffsetsKeys);
      });
      longitudinalToggle = manageSLCOffsetsBtn;
    } else if (param == "SLCQOL") {
      ButtonControl *manageSLCQOLBtn = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageSLCQOLBtn, &ButtonControl::clicked, [this]() {
        openSubParentToggle();

        std::set<QString> modifiedSpeedLimitControllerQOLKeys = speedLimitControllerQOLKeys;

        if (hasPCMCruise) {
          modifiedSpeedLimitControllerQOLKeys.erase("SetSpeedLimit");
        }

        if (!isToyota) {
          modifiedSpeedLimitControllerQOLKeys.erase("ForceMPHDashboard");
        }

        showToggles(modifiedSpeedLimitControllerQOLKeys);
      });
      longitudinalToggle = manageSLCQOLBtn;
    } else if (param == "SLCConfirmation") {
      std::vector<QString> confirmationToggles{"SLCConfirmationLower", "SLCConfirmationHigher"};
      std::vector<QString> confirmationToggleNames{tr("Lower Limits"), tr("Higher Limits")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, confirmationToggles, confirmationToggleNames);
    } else if (param == "SLCLookaheadHigher" || param == "SLCLookaheadLower") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 30, tr(" seconds"));
    } else if (param == "Offset1" || param == "Offset2" || param == "Offset3" || param == "Offset4") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, -99, 99, tr("mph"));
    } else if (param == "SLCVisuals") {
      ButtonControl *manageSLCVisualsBtn = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageSLCVisualsBtn, &ButtonControl::clicked, [this]() {
        openSubParentToggle();
        showToggles(speedLimitControllerVisualKeys);
      });
      longitudinalToggle = manageSLCVisualsBtn;

    } else {
      longitudinalToggle = new ParamControl(param, title, desc, icon);
    }

    addItem(longitudinalToggle);
    toggles[param] = longitudinalToggle;

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(longitudinalToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, this, &FrogPilotLongitudinalPanel::openParentToggle);
    }
  }

  QObject::connect(static_cast<ToggleControl*>(toggles["ExperimentalModeViaLKAS"]), &ToggleControl::toggleFlipped, [this](bool state) {
    if (state && params.getBool("AlwaysOnLateralLKAS")) {
      params.putBool("AlwaysOnLateralLKAS", false);
    }
  });

  FrogPilotParamValueControl *trafficFollowToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficFollow"]);
  FrogPilotParamValueControl *trafficAccelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkAcceleration"]);
  FrogPilotParamValueControl *trafficDecelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkDeceleration"]);
  FrogPilotParamValueControl *trafficDangerToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkDanger"]);
  FrogPilotParamValueControl *trafficSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkSpeed"]);
  FrogPilotParamValueControl *trafficSpeedDecreaseToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkSpeedDecrease"]);
  FrogPilotButtonsControl *trafficResetButton = static_cast<FrogPilotButtonsControl*>(toggles["ResetTrafficPersonality"]);
  QObject::connect(trafficResetButton, &FrogPilotButtonsControl::buttonClicked, this, [=]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for 'Traffic Mode'?"), this)) {
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
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the 'Aggressive' personality?"), this)) {
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
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the 'Standard' personality?"), this)) {
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
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the 'Relaxed' personality?"), this)) {
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

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, this, &FrogPilotLongitudinalPanel::hideToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubParentToggle, this, &FrogPilotLongitudinalPanel::hideSubToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, this, &FrogPilotLongitudinalPanel::updateMetric);
}

void FrogPilotLongitudinalPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  hasDashSpeedLimits = parent->hasDashSpeedLimits;
  hasPCMCruise = parent->hasPCMCruise;
  isGM = parent->isGM;
  isHKGCanFd = parent->isHKGCanFd;
  isSubaru = parent->isSubaru;
  isToyota = parent->isToyota;
  tuningLevel = parent->tuningLevel;

  hideToggles();
}

void FrogPilotLongitudinalPanel::updateMetric(bool metric, bool bootRun) {
  static bool previousMetric;
  if (metric != previousMetric && !bootRun) {
    double distanceConversion = metric ? FOOT_TO_METER : METER_TO_FOOT;
    double speedConversion = metric ? MILE_TO_KM : KM_TO_MILE;

    params.putFloatNonBlocking("IncreasedStoppedDistance", params.getFloat("IncreasedStoppedDistance") * distanceConversion);

    params.putFloatNonBlocking("CESignalSpeed", params.getFloat("CESignalSpeed") * speedConversion);
    params.putFloatNonBlocking("CESpeed", params.getFloat("CESpeed") * speedConversion);
    params.putFloatNonBlocking("CESpeedLead", params.getFloat("CESpeedLead") * speedConversion);
    params.putFloatNonBlocking("CustomCruise", params.getFloat("CustomCruise") * speedConversion);
    params.putFloatNonBlocking("CustomCruiseLong", params.getFloat("CustomCruiseLong") * speedConversion);
    params.putFloatNonBlocking("Offset1", params.getFloat("Offset1") * speedConversion);
    params.putFloatNonBlocking("Offset2", params.getFloat("Offset2") * speedConversion);
    params.putFloatNonBlocking("Offset3", params.getFloat("Offset3") * speedConversion);
    params.putFloatNonBlocking("Offset4", params.getFloat("Offset4") * speedConversion);
    params.putFloatNonBlocking("SetSpeedOffset", params.getFloat("SetSpeedOffset") * speedConversion);
  }
  previousMetric = metric;

  FrogPilotDualParamValueControl *ceSpeedToggle = reinterpret_cast<FrogPilotDualParamValueControl*>(toggles["CESpeed"]);
  FrogPilotParamValueButtonControl *ceSignal = static_cast<FrogPilotParamValueButtonControl*>(toggles["CESignalSpeed"]);
  FrogPilotParamValueControl *customCruiseToggle = static_cast<FrogPilotParamValueControl*>(toggles["CustomCruise"]);
  FrogPilotParamValueControl *customCruiseLongToggle = static_cast<FrogPilotParamValueControl*>(toggles["CustomCruiseLong"]);
  FrogPilotParamValueControl *offset1Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset1"]);
  FrogPilotParamValueControl *offset2Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset2"]);
  FrogPilotParamValueControl *offset3Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset3"]);
  FrogPilotParamValueControl *offset4Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset4"]);
  FrogPilotParamValueControl *increasedStoppedDistanceToggle = static_cast<FrogPilotParamValueControl*>(toggles["IncreasedStoppedDistance"]);
  FrogPilotParamValueControl *setSpeedOffsetToggle = static_cast<FrogPilotParamValueControl*>(toggles["SetSpeedOffset"]);

  if (metric) {
    offset1Toggle->setTitle(tr("Speed Limit Offset (0-34 kph)"));
    offset2Toggle->setTitle(tr("Speed Limit Offset (35-54 kph)"));
    offset3Toggle->setTitle(tr("Speed Limit Offset (55-64 kph)"));
    offset4Toggle->setTitle(tr("Speed Limit Offset (65-99 kph)"));

    offset1Toggle->setDescription(tr("Sets the speed limit offset for speeds between 0-34 kph."));
    offset2Toggle->setDescription(tr("Sets the speed limit offset for speeds between 35-54 kph."));
    offset3Toggle->setDescription(tr("Sets the speed limit offset for speeds between 55-64 kph."));
    offset4Toggle->setDescription(tr("Sets the speed limit offset for speeds between 65-99 kph."));

    ceSignal->updateControl(0, 150, tr("kph"));
    ceSpeedToggle->updateControl(0, 150, tr("kph"));
    customCruiseToggle->updateControl(1, 150, tr("kph"));
    customCruiseLongToggle->updateControl(1, 150, tr("kph"));
    offset1Toggle->updateControl(-99, 99, tr("kph"));
    offset2Toggle->updateControl(-99, 99, tr("kph"));
    offset3Toggle->updateControl(-99, 99, tr("kph"));
    offset4Toggle->updateControl(-99, 99, tr("kph"));
    setSpeedOffsetToggle->updateControl(0, 150, tr("kph"));

    increasedStoppedDistanceToggle->updateControl(0, 3, tr(" meters"));
  } else {
    offset1Toggle->setTitle(tr("Speed Limit Offset (0-34 mph)"));
    offset2Toggle->setTitle(tr("Speed Limit Offset (35-54 mph)"));
    offset3Toggle->setTitle(tr("Speed Limit Offset (55-64 mph)"));
    offset4Toggle->setTitle(tr("Speed Limit Offset (65-99 mph)"));

    offset1Toggle->setDescription(tr("Sets the speed limit offset for speeds between 0-34 mph."));
    offset2Toggle->setDescription(tr("Sets the speed limit offset for speeds between 35-54 mph."));
    offset3Toggle->setDescription(tr("Sets the speed limit offset for speeds between 55-64 mph."));
    offset4Toggle->setDescription(tr("Sets the speed limit offset for speeds between 65-99 mph."));

    ceSignal->updateControl(0, 99, tr("mph"));
    ceSpeedToggle->updateControl(0, 99, tr("mph"));
    customCruiseToggle->updateControl(1, 99, tr("mph"));
    customCruiseLongToggle->updateControl(1, 99, tr("mph"));
    offset1Toggle->updateControl(-99, 99, tr("mph"));
    offset2Toggle->updateControl(-99, 99, tr("mph"));
    offset3Toggle->updateControl(-99, 99, tr("mph"));
    offset4Toggle->updateControl(-99, 99, tr("mph"));
    setSpeedOffsetToggle->updateControl(0, 99, tr("mph"));

    increasedStoppedDistanceToggle->updateControl(0, 10, tr(" feet"));
  }
}

void FrogPilotLongitudinalPanel::showToggles(const std::set<QString> &keys) {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(keys.find(key) != keys.end() && tuningLevel >= frogpilotToggleLevels[key].toDouble());
  }

  setUpdatesEnabled(true);

  update();
}

void FrogPilotLongitudinalPanel::hideToggles() {
  setUpdatesEnabled(false);

  customPersonalityOpen = false;
  slcOpen = false;

  for (auto &[key, toggle] : toggles) {
    bool subToggles = aggressivePersonalityKeys.find(key) != aggressivePersonalityKeys.end() ||
                      conditionalExperimentalKeys.find(key) != conditionalExperimentalKeys.end() ||
                      curveSpeedKeys.find(key) != curveSpeedKeys.end() ||
                      customDrivingPersonalityKeys.find(key) != customDrivingPersonalityKeys.end() ||
                      experimentalModeActivationKeys.find(key) != experimentalModeActivationKeys.end() ||
                      longitudinalTuneKeys.find(key) != longitudinalTuneKeys.end() ||
                      qolKeys.find(key) != qolKeys.end() ||
                      relaxedPersonalityKeys.find(key) != relaxedPersonalityKeys.end() ||
                      speedLimitControllerKeys.find(key) != speedLimitControllerKeys.end() ||
                      speedLimitControllerOffsetsKeys.find(key) != speedLimitControllerOffsetsKeys.end() ||
                      speedLimitControllerQOLKeys.find(key) != speedLimitControllerQOLKeys.end() ||
                      speedLimitControllerVisualKeys.find(key) != speedLimitControllerVisualKeys.end() ||
                      standardPersonalityKeys.find(key) != standardPersonalityKeys.end() ||
                      trafficPersonalityKeys.find(key) != trafficPersonalityKeys.end();

    toggle->setVisible(!subToggles && tuningLevel >= frogpilotToggleLevels[key].toDouble());
  }

  std::set<QString> toggleKeys = {"ConditionalExperimental", "CurveSpeedControl"};
  for (const QString &key : toggleKeys) {
    FrogPilotManageControl *control = static_cast<FrogPilotManageControl*>(toggles[key]);
    control->setManageVisibility(tuningLevel > frogpilotToggleLevels[key].toDouble());
  }

  setUpdatesEnabled(true);

  update();
}

void FrogPilotLongitudinalPanel::hideSubToggles() {
  if (customPersonalityOpen) {
    customPersonalityOpen = false;
    showToggles(customDrivingPersonalityKeys);
  } else if (slcOpen) {
    showToggles(speedLimitControllerKeys);
  }
}
