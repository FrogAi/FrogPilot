#include <filesystem>
#include <iostream>

#include "selfdrive/frogpilot/ui/qt/offroad/control_settings.h"

bool checkCommaNNFFSupport(const std::string &carFingerprint) {
  std::ifstream file("../car/torque_data/neural_ff_weights.json");
  for (std::string line; std::getline(file, line);) {
    if (line.find(carFingerprint) != std::string::npos) {
      std::cout << "comma's NNFF supports fingerprint: " << carFingerprint << std::endl;
      return true;
    }
  }
  return false;
}

bool checkNNFFLogFileExists(const std::string &carFingerprint) {
  for (const auto &entry : std::filesystem::directory_iterator("../car/torque_data/lat_models")) {
    if (entry.path().filename().string().find(carFingerprint) == 0) {
      std::cout << "NNFF supports fingerprint: " << entry.path().filename() << std::endl;
      return true;
    }
  }
  return false;
}

FrogPilotControlsPanel::FrogPilotControlsPanel(SettingsWindow *parent) : FrogPilotListWidget(parent) {
  std::string branch = params.get("GitBranch");
  isRelease = branch == "FrogPilot";

  const std::vector<std::tuple<QString, QString, QString, QString>> controlToggles {
    {"AlwaysOnLateral", tr("Always on Lateral"), tr("Maintain openpilot lateral control when the brake or gas pedals are used.\n\nDeactivation occurs only through the 'Cruise Control' button."), "../frogpilot/assets/toggle_icons/icon_always_on_lateral.png"},
    {"AlwaysOnLateralLKAS", tr("Control Via LKAS Button"), tr("Enable or disable 'Always On Lateral' by clicking your 'LKAS' button."), ""},
    {"AlwaysOnLateralMain", tr("Enable On Cruise Main"), tr("Enable 'Always On Lateral' by clicking your 'Cruise Control' button without requiring openpilot to be enabled first."), ""},
    {"PauseAOLOnBrake", tr("Pause On Brake Below"), tr("Pause 'Always On Lateral' when the brake pedal is being pressed below the set speed."), ""},
    {"HideAOLStatusBar", tr("Hide the Status Bar"), tr("Don't use the status bar for 'Always On Lateral'."), ""},

    {"ConditionalExperimental", tr("Conditional Experimental Mode"), tr("Automatically switches to 'Experimental Mode' under predefined conditions."), "../frogpilot/assets/toggle_icons/icon_conditional.png"},
    {"CESpeed", tr("Below"), tr("Switch to 'Experimental Mode' below this speed when not following a lead vehicle."), ""},
    {"CECurves", tr("Curve Detected Ahead"), tr("Switch to 'Experimental Mode' when a curve is detected."), ""},
    {"CELead", tr("Lead Detected Ahead"), tr("Switch to 'Experimental Mode' when a slower or stopped lead vehicle is detected ahead."), ""},
    {"CEModelStopTime", tr("Model Wants To Stop In The Next"), tr("Switch to 'Experimental Mode' when the model wants to stop like when it detects a stop light or stop sign."), ""},
    {"CENavigation", tr("Navigation Based"), tr("Switch to 'Experimental Mode' based on navigation data. (i.e. Intersections, stop signs, upcoming turns, etc.)"), ""},
    {"CESignal", tr("Turn Signal When Below Highway Speeds"), tr("Switch to 'Experimental Mode' when using turn signals below highway speeds to help assist with turns."), ""},
    {"HideCEMStatusBar", tr("Hide the Status Bar"), tr("Don't use the status bar for 'Conditional Experimental Mode'."), ""},

    {"DeviceManagement", tr("Device Management"), tr("Tweak your device's behaviors to your personal preferences."), "../frogpilot/assets/toggle_icons/icon_device.png"},
    {"DeviceShutdown", tr("Device Shutdown Timer"), tr("Configure how quickly the device shuts down after going offroad."), ""},
    {"NoLogging", tr("Disable Logging"), tr("Turn off all data tracking to enhance privacy or reduce thermal load."), ""},
    {"NoUploads", tr("Disable Uploads"), tr("Turn off all data uploads to comma's servers."), ""},
    {"IncreaseThermalLimits", tr("Increase Thermal Safety Limit"), tr("Allow the device to run at a temperature above comma's recommended thermal limits."), ""},
    {"LowVoltageShutdown", tr("Low Voltage Shutdown Threshold"), tr("Automatically shut the device down when your battery reaches a specific voltage level to prevent killing your battery."), ""},
    {"OfflineMode", tr("Offline Mode"), tr("Allow the device to be offline indefinitely."), ""},

    {"DrivingPersonalities", tr("Driving Personalities"), tr("Manage the driving behaviors of comma's 'Personality Profiles'."), "../frogpilot/assets/toggle_icons/icon_personality.png"},
    {"CustomPersonalities", tr("Customize Personalities"), tr("Customize the driving personality profiles to your driving style."), ""},
    {"PersonalityInfo", tr("What Do All These Do?"), tr("Learn what all the values in 'Custom Personality Profiles' do on openpilot's driving behaviors."), ""},
    {"TrafficPersonalityProfile", tr("Traffic Personality"), tr("Customize the 'Traffic' personality profile."), "../frogpilot/assets/stock_theme/distance_icons/traffic.png"},
    {"TrafficFollow", tr("Following Distance"), tr("Set the minimum following distance when using 'Traffic Mode'. Your following distance will dynamically adjust between this distance and the following distance from the 'Aggressive' profile."), ""},
    {"TrafficJerkAcceleration", tr("Acceleration Jerk"), tr("Customize the acceleration jerk when using 'Traffic Mode'."), ""},
    {"TrafficJerkDanger", tr("Danger Zone Jerk"), tr("Customize the danger zone jerk when using the 'Traffic' personality."), ""},
    {"TrafficJerkSpeed", tr("Speed Control Jerk"), tr("Customize the speed control jerk when using 'Traffic Mode'."), ""},
    {"ResetTrafficPersonality", tr("Reset Settings"), tr("Reset the values for the 'Traffic Mode' personality back to stock."), ""},
    {"AggressivePersonalityProfile", tr("Aggressive Personality"), tr("Customize the 'Aggressive' personality profile."), "../frogpilot/assets/stock_theme/distance_icons/aggressive.png"},
    {"AggressiveFollow", tr("Following Distance"), tr("Set the 'Aggressive' personality following distance. Represents seconds to follow behind the lead vehicle.\n\nStock: 1.25 seconds."), ""},
    {"AggressiveJerkAcceleration", tr("Acceleration Jerk"), tr("Customize the acceleration jerk when using the 'Aggressive' personality."), ""},
    {"AggressiveJerkDanger", tr("Danger Zone Jerk"), tr("Customize the danger zone jerk when using the 'Aggressive' personality."), ""},
    {"AggressiveJerkSpeed", tr("Speed Control Jerk"), tr("Customize the speed control jerk when using the 'Aggressive' personality."), ""},
    {"ResetAggressivePersonality", tr("Reset Settings"), tr("Reset the values for the 'Aggressive' personality back to stock."), ""},
    {"StandardPersonalityProfile", tr("Standard Personality"), tr("Customize the 'Standard' personality profile."), "../frogpilot/assets/stock_theme/distance_icons/standard.png"},
    {"StandardFollow", tr("Following Distance"), tr("Set the 'Standard' personality following distance. Represents seconds to follow behind the lead vehicle.\n\nStock: 1.45 seconds."), ""},
    {"StandardJerkAcceleration", tr("Acceleration Jerk"), tr("Customize the acceleration jerk when using the 'Standard' personality."), ""},
    {"StandardJerkDanger", tr("Danger Zone Jerk"), tr("Customize the danger zone jerk when using the 'Standard' personality."), ""},
    {"StandardJerkSpeed", tr("Speed Control Jerk"), tr("Customize the speed control jerk when using the 'Standard' personality."), ""},
    {"ResetStandardPersonality", tr("Reset Settings"), tr("Reset the values for the 'Standard' personality back to stock."), ""},
    {"RelaxedPersonalityProfile", tr("Relaxed Personality"), tr("Customize the 'Relaxed' personality profile."), "../frogpilot/assets/stock_theme/distance_icons/relaxed.png"},
    {"RelaxedFollow", tr("Following Distance"), tr("Set the 'Relaxed' personality following distance. Represents seconds to follow behind the lead vehicle.\n\nStock: 1.75 seconds."), ""},
    {"RelaxedJerkAcceleration", tr("Acceleration Jerk"), tr("Customize the acceleration jerk when using the 'Relaxed' personality."), ""},
    {"RelaxedJerkDanger", tr("Danger Zone Jerk"), tr("Customize the danger zone jerk when using the 'Relaxed' personality."), ""},
    {"RelaxedJerkSpeed", tr("Speed Control Jerk"), tr("Customize the speed control jerk when using the 'Relaxed' personality."), ""},
    {"ResetRelaxedPersonality", tr("Reset Settings"), tr("Reset the values for the 'Relaxed' personality back to stock."), ""},
    {"OnroadDistanceButton", tr("Onroad Distance Button"), tr("Simulate a distance button via the onroad UI to control personalities, 'Experimental Mode', and 'Traffic Mode'."), ""},
    {"OnroadDistanceButtonButtons", "Icon Pack", "", ""},
    {"DownloadStatusLabel", tr("Download Status"), "", ""},

    {"ExperimentalModeActivation", tr("Experimental Mode Activation"), tr("Toggle Experimental Mode with either buttons on the steering wheel or the screen. \n\nOverrides 'Conditional Experimental Mode'."), "../assets/img_experimental_white.svg"},
    {"ExperimentalModeViaLKAS", tr("Click LKAS Button"), tr("Enable/disable 'Experimental Mode' by clicking the 'LKAS' button on your steering wheel."), ""},
    {"ExperimentalModeViaTap", tr("Double Tap the UI"), tr("Enable/disable 'Experimental Mode' by double tapping the onroad UI within a 0.5 second time frame."), ""},
    {"ExperimentalModeViaDistance", tr("Long Press Distance"), tr("Enable/disable 'Experimental Mode' by holding down the 'distance' button on your steering wheel for 0.5 seconds."), ""},

    {"LaneChangeCustomizations", tr("Lane Change Customizations"), tr("Customize the lane change behaviors in openpilot."), "../frogpilot/assets/toggle_icons/icon_lane.png"},
    {"LaneChangeTime", tr("Lane Change Timer"), tr("Set a delay before executing a lane change."), ""},
    {"LaneDetectionWidth", tr("Lane Detection Threshold"), tr("Set the required lane width to be qualified as a lane."), ""},
    {"MinimumLaneChangeSpeed", tr("Minimum Lane Change Speed"), tr("Customize the minimum driving speed to allow openpilot to change lanes."), ""},
    {"NudgelessLaneChange", tr("Nudgeless Lane Change"), tr("Enable lane changes without requiring manual steering input."), ""},
    {"OneLaneChange", tr("One Lane Change Per Signal"), tr("Only allow one lane change per turn signal activation."), ""},

    {"LateralTune", tr("Lateral Tuning"), tr("Modify openpilot's steering behavior."), "../frogpilot/assets/toggle_icons/icon_lateral_tune.png"},
    {"ForceAutoTune", tr("Force Auto Tune"), tr("Forces comma's auto lateral tuning for unsupported vehicles."), ""},
    {"NNFF", tr("NNFF"), tr("Use Twilsonco's Neural Network Feedforward for enhanced precision in lateral control."), ""},
    {"NNFFLite", tr("NNFF-Lite"), tr("Use Twilsonco's Neural Network Feedforward for enhanced precision in lateral control for cars without available NNFF logs."), ""},
    {"SteerRatio", steerRatioStock != 0 ? QString(tr("Steer Ratio (Default: %1)")).arg(QString::number(steerRatioStock, 'f', 2)) : tr("Steer Ratio"), tr("Use a custom steer ratio as opposed to comma's auto tune value."), ""},
    {"TacoTune", tr("Taco Tune"), tr("Use comma's 'Taco Tune' designed for handling left and right turns."), ""},
    {"TurnDesires", tr("Use Turn Desires"), tr("Use turn desires for greater precision in turns below the minimum lane change speed."), ""},

    {"LongitudinalTune", tr("Longitudinal Tuning"), tr("Modify openpilot's acceleration and braking behavior."), "../frogpilot/assets/toggle_icons/icon_longitudinal_tune.png"},
    {"AccelerationProfile", tr("Acceleration Profile"), tr("Change the acceleration rate to be either sporty or eco-friendly."), ""},
    {"DecelerationProfile", tr("Deceleration Profile"), tr("Change the deceleration rate to be either sporty or eco-friendly."), ""},
    {"HumanAcceleration", tr("Human-Like Acceleration"), tr("Tweaks the acceleration behavior to be more 'human-like'."), ""},
    {"HumanFollowing", tr("Human-Like Following Distance"), tr("Tweaks the following distance dynamically to be more 'human-like' when coming up behind slower/stopped leads or following faster leads."), ""},
    {"StoppingDistance", tr("Increase Stop Distance Behind Lead"), tr("Increase the stopping distance for a more comfortable stop from lead vehicles."), ""},
    {"LeadDetectionThreshold", tr("Lead Detection Threshold"), tr("Increase or decrease the lead detection threshold to either detect leads sooner, or increase model confidence."), ""},
    {"TrafficMode", tr("Traffic Mode"), tr("Enable the ability to activate 'Traffic Mode' by holding down the 'distance' button for 2.5 seconds. When 'Traffic Mode' is active the onroad UI will turn red and openpilot will drive catered towards stop and go traffic."), ""},

    {"MTSCEnabled", tr("Map Turn Speed Control"), tr("Slow down for anticipated curves detected by the downloaded maps."), "../frogpilot/assets/toggle_icons/icon_speed_map.png"},
    {"DisableMTSCSmoothing", tr("Disable MTSC UI Smoothing"), tr("Disables the smoothing for the requested speed in the onroad UI to show exactly what speed MTSC is currently requesting."), ""},
    {"MTSCCurvatureCheck",  tr("Model Curvature Detection Failsafe"), tr("Only trigger MTSC when the model detects a curve in the road. Purely used as a failsafe to prevent false positives. Leave this off if you never experience false positives."), ""},
    {"MTSCAggressiveness", tr("Turn Speed Aggressiveness"), tr("Set turn speed aggressiveness. Higher values result in faster turns, lower values yield gentler turns. \n\nA change of +- 1% results in the speed being raised or lowered by about 1 mph."), ""},

    {"ModelManagement", tr("Model Management"), tr("Manage openpilot's driving models."), "../assets/offroad/icon_calibration.png"},
    {"AutomaticallyUpdateModels", tr("Automatically Update and Download Models"), tr("Automatically download models as they're updated or added to the model list."), ""},
    {"ModelRandomizer", tr("Model Randomizer"), tr("Have a random model be selected each drive that can be reviewed at the end of each drive to find your preferred model."), ""},
    {"ManageBlacklistedModels", tr("Manage Model Blacklist"), "Manage the models on your blacklist.", ""},
    {"ResetScores", tr("Reset Model Scores"), tr("Reset the scores you have rated the openpilot models."), ""},
    {"ReviewScores", tr("Review Model Scores"), tr("View the scores FrogPilot and yourself have rated the openpilot models."), ""},
    {"DeleteModel", tr("Delete Model"), "", ""},
    {"DownloadModel", tr("Download Model"), "", ""},
    {"DownloadAllModels", tr("Download All Models"), "", ""},
    {"SelectModel", tr("Select Model"), "", ""},
    {"ResetCalibrations", tr("Reset Model Calibrations"), tr("Reset the driving model calibrations."), ""},

    {"QOLControls", tr("Quality of Life"), tr("Miscellaneous quality of life changes to improve your overall openpilot experience."), "../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"CustomCruise", tr("Cruise Increase Interval"), tr("Set a custom interval to increase the max set speed by."), ""},
    {"CustomCruiseLong", tr("Cruise Increase Interval (Long Press)"), tr("Set a custom interval to increase the max set speed by when holding down the cruise increase button."), ""},
    {"ForceStandstill", tr("Force Standstill State"), tr("Keeps openpilot in the 'standstill' state until the gas pedal is pressed."), ""},
    {"MapGears", tr("Map Accel/Decel To Gears"), tr("Map your acceleration/deceleration profile to your 'Eco' and/or 'Sport' gears."), ""},
    {"PauseLateralSpeed", tr("Pause Lateral Below"), tr("Pause lateral control on all speeds below the set speed."), ""},
    {"ReverseCruise", tr("Reverse Cruise Increase"), tr("Reverses the 'long press' functionality logic to increase the max set speed by 5 instead of 1. Useful to increase the max speed quickly."), ""},
    {"SetSpeedOffset", tr("Set Speed Offset"), tr("Set an offset for your desired set speed."), ""},

    {"SpeedLimitController", tr("Speed Limit Controller"), tr("Automatically adjust the max speed to match the current speed limit using 'Open Street Maps', 'Navigate On openpilot', or your car's dashboard (Toyotas/Lexus/HKG only)."), "../assets/offroad/icon_speed_limit.png"},
    {"SLCControls", tr("Controls Settings"), tr("Manage toggles related to 'Speed Limit Controller's controls."), ""},
    {"Offset1", tr("Speed Limit Offset (0-34 mph)"), tr("Speed limit offset for speed limits between 0-34 mph."), ""},
    {"Offset2", tr("Speed Limit Offset (35-54 mph)"), tr("Speed limit offset for speed limits between 35-54 mph."), ""},
    {"Offset3", tr("Speed Limit Offset (55-64 mph)"), tr("Speed limit offset for speed limits between 55-64 mph."), ""},
    {"Offset4", tr("Speed Limit Offset (65-99 mph)"), tr("Speed limit offset for speed limits between 65-99 mph."), ""},
    {"SLCFallback", tr("Fallback Method"), tr("Choose your fallback method when there is no speed limit available."), ""},
    {"SLCOverride", tr("Override Method"), tr("Choose your preferred method to override the current speed limit."), ""},
    {"SLCPriority", tr("Priority Order"), tr("Configure the speed limit priority order."), ""},
    {"SLCQOL", tr("Quality of Life"), tr("Manage toggles related to 'Speed Limit Controller's quality of life features."), ""},
    {"SLCConfirmation", tr("Confirm New Speed Limits"), tr("Don't automatically start using the new speed limit until it's been manually confirmed."), ""},
    {"ForceMPHDashboard", tr("Force MPH From Dashboard Readings"), tr("Force MPH readings from the dashboard. Only use this if you live in an area where the speed limits from your dashboard are in KPH, but you use MPH."), ""},
    {"SLCLookaheadHigher", tr("Prepare For Higher Speed Limits"), tr("Set a 'lookahead' value to prepare for upcoming speed limits higher than your current speed limit using the data stored in 'Open Street Maps'."), ""},
    {"SLCLookaheadLower", tr("Prepare For Lower Speed Limits"), tr("Set a 'lookahead' value to prepare for upcoming speed limits lower than your current speed limit using the data stored in 'Open Street Maps'."), ""},
    {"SetSpeedLimit", tr("Use Current Speed Limit As Set Speed"), tr("Sets your max speed to the current speed limit if one is populated when you initially enable openpilot."), ""},
    {"SLCVisuals", tr("Visuals Settings"), tr("Manage toggles related to 'Speed Limit Controller's visuals."), ""},
    {"ShowSLCOffset", tr("Show Speed Limit Offset"), tr("Show the speed limit offset separated from the speed limit in the onroad UI when using 'Speed Limit Controller'."), ""},
    {"SpeedLimitChangedAlert", tr("Speed Limit Changed Alert"), tr("Trigger an alert whenever the speed limit changes."), ""},
    {"UseVienna", tr("Use Vienna Speed Limit Signs"), tr("Use the Vienna (EU) speed limit style signs as opposed to MUTCD (US)."), ""},

    {"VisionTurnControl", tr("Vision Turn Speed Controller"), tr("Slow down for detected curves in the road."), "../frogpilot/assets/toggle_icons/icon_vtc.png"},
    {"DisableVTSCSmoothing", tr("Disable VTSC UI Smoothing"), tr("Disables the smoothing for the requested speed in the onroad UI."), ""},
    {"CurveSensitivity", tr("Curve Detection Sensitivity"), tr("Set curve detection sensitivity. Higher values prompt earlier responses, lower values lead to smoother but later reactions."), ""},
    {"TurnAggressiveness", tr("Turn Speed Aggressiveness"), tr("Set turn speed aggressiveness. Higher values result in faster turns, lower values yield gentler turns."), ""},
  };

  for (const auto &[param, title, desc, icon] : controlToggles) {
    AbstractControl *controlToggle;

    if (param == "AlwaysOnLateral") {
      FrogPilotParamManageControl *aolToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(aolToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(aolKeys.find(key.c_str()) != aolKeys.end());
        }
      });
      controlToggle = aolToggle;
    } else if (param == "PauseAOLOnBrake") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, std::map<int, QString>(), this, false, tr("mph"));

    } else if (param == "ConditionalExperimental") {
      FrogPilotParamManageControl *conditionalExperimentalToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(conditionalExperimentalToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(conditionalExperimentalKeys.find(key.c_str()) != conditionalExperimentalKeys.end());
        }
      });
      controlToggle = conditionalExperimentalToggle;
    } else if (param == "CESpeed") {
      FrogPilotParamValueControl *CESpeed = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, std::map<int, QString>(), this, false, tr("mph"));
      FrogPilotParamValueControl *CESpeedLead = new FrogPilotParamValueControl("CESpeedLead", tr(" With Lead"), tr("Switch to 'Experimental Mode' below this speed when following a lead vehicle."), icon, 0, 99, std::map<int, QString>(), this, false, tr("mph"));
      FrogPilotDualParamControl *conditionalSpeeds = new FrogPilotDualParamControl(CESpeed, CESpeedLead, this);
      controlToggle = reinterpret_cast<AbstractControl*>(conditionalSpeeds);
    } else if (param == "CECurves") {
      std::vector<QString> curveToggles{"CECurvesLead"};
      std::vector<QString> curveToggleNames{tr("With Lead")};
      controlToggle = new FrogPilotParamToggleControl(param, title, desc, icon, curveToggles, curveToggleNames);
    } else if (param == "CELead") {
      std::vector<QString> leadToggles{"CESlowerLead", "CEStoppedLead"};
      std::vector<QString> leadToggleNames{tr("Slower Lead"), tr("Stopped Lead")};
      controlToggle = new FrogPilotParamToggleControl(param, title, desc, icon, leadToggles, leadToggleNames);
    } else if (param == "CENavigation") {
      std::vector<QString> navigationToggles{"CENavigationIntersections", "CENavigationTurns", "CENavigationLead"};
      std::vector<QString> navigationToggleNames{tr("Intersections"), tr("Turns"), tr("With Lead")};
      controlToggle = new FrogPilotParamToggleControl(param, title, desc, icon, navigationToggles, navigationToggleNames);
    } else if (param == "CEModelStopTime") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, std::map<int, QString>(), this, false, tr(" seconds"));

    } else if (param == "DeviceManagement") {
      FrogPilotParamManageControl *deviceManagementToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(deviceManagementToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(deviceManagementKeys.find(key.c_str()) != deviceManagementKeys.end());
        }
      });
      controlToggle = deviceManagementToggle;
    } else if (param == "DeviceShutdown") {
      std::map<int, QString> shutdownLabels;
      for (int i = 0; i <= 33; i++) {
        shutdownLabels[i] = i == 0 ? tr("5 mins") : i <= 3 ? QString::number(i * 15) + tr(" mins") : QString::number(i - 3) + (i == 4 ? tr(" hour") : tr(" hours"));
      }
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 33, shutdownLabels, this, false);
    } else if (param == "NoUploads") {
      std::vector<QString> uploadsToggles{"DisableOnroadUploads"};
      std::vector<QString> uploadsToggleNames{tr("Only Onroad")};
      controlToggle = new FrogPilotParamToggleControl(param, title, desc, icon, uploadsToggles, uploadsToggleNames);
    } else if (param == "LowVoltageShutdown") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 11.8, 12.5, std::map<int, QString>(), this, false, tr(" volts"), 1, 0.01);

    } else if (param == "DrivingPersonalities") {
      FrogPilotParamManageControl *drivingPersonalitiesToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(drivingPersonalitiesToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        drivingPersonalitiesOpen = true;

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(drivingPersonalityKeys.find(key.c_str()) != drivingPersonalityKeys.end());
        }

        downloadStatusLabel->setVisible(onroadDistanceButton);
        manageDistanceIconsBtn->setVisible(onroadDistanceButton);
      });
      controlToggle = drivingPersonalitiesToggle;
    } else if (param == "CustomPersonalities") {
      FrogPilotParamManageControl *customPersonalitiesToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(customPersonalitiesToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openSubParentToggle();

        customPersonalitiesOpen = true;

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(customdrivingPersonalityKeys.find(key.c_str()) != customdrivingPersonalityKeys.end());
        }
      });
      controlToggle = customPersonalitiesToggle;
    } else if (param == "PersonalityInfo") {
      ButtonControl *personalitiesInfoBtn = new ButtonControl(title, tr("VIEW"), desc);
      QObject::connect(personalitiesInfoBtn, &ButtonControl::clicked, [=]() {
        const std::string txt = util::read_file("../frogpilot/ui/qt/offroad/personalities_info.txt");
        ConfirmationDialog::rich(QString::fromStdString(txt), this);
      });
      controlToggle = reinterpret_cast<AbstractControl*>(personalitiesInfoBtn);
    } else if (param == "ResetTrafficPersonality" || param == "ResetAggressivePersonality" || param == "ResetStandardPersonality" || param == "ResetRelaxedPersonality") {
      std::vector<QString> personalityOptions{tr("Reset")};
      FrogPilotButtonsControl *profileBtn = new FrogPilotButtonsControl(title, desc, icon, personalityOptions);
      controlToggle = profileBtn;
    } else if (param == "TrafficPersonalityProfile") {
      FrogPilotParamManageControl *trafficPersonalityToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(trafficPersonalityToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openSubSubParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(trafficPersonalityKeys.find(key.c_str()) != trafficPersonalityKeys.end());
        }
      });
      controlToggle = trafficPersonalityToggle;
    } else if (param == "AggressivePersonalityProfile") {
      FrogPilotParamManageControl *aggressivePersonalityToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(aggressivePersonalityToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openSubSubParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(aggressivePersonalityKeys.find(key.c_str()) != aggressivePersonalityKeys.end());
        }
      });
      controlToggle = aggressivePersonalityToggle;
    } else if (param == "StandardPersonalityProfile") {
      FrogPilotParamManageControl *standardPersonalityToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(standardPersonalityToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openSubSubParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(standardPersonalityKeys.find(key.c_str()) != standardPersonalityKeys.end());
        }
      });
      controlToggle = standardPersonalityToggle;
    } else if (param == "RelaxedPersonalityProfile") {
      FrogPilotParamManageControl *relaxedPersonalityToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(relaxedPersonalityToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openSubSubParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(relaxedPersonalityKeys.find(key.c_str()) != relaxedPersonalityKeys.end());
        }
      });
      controlToggle = relaxedPersonalityToggle;
    } else if (trafficPersonalityKeys.find(param) != trafficPersonalityKeys.end() ||
               aggressivePersonalityKeys.find(param) != aggressivePersonalityKeys.end() ||
               standardPersonalityKeys.find(param) != standardPersonalityKeys.end() ||
               relaxedPersonalityKeys.find(param) != relaxedPersonalityKeys.end()) {
      if (param == "TrafficFollow" || param == "AggressiveFollow" || param == "StandardFollow" || param == "RelaxedFollow") {
        if (param == "TrafficFollow") {
          controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.5, 5, std::map<int, QString>(), this, false, tr(" seconds"), 1, 0.01);
        } else {
          controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 5, std::map<int, QString>(), this, false, tr(" seconds"), 1, 0.01);
        }
      } else {
        controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 500, std::map<int, QString>(), this, false, "%");
      }

    } else if (param == "OnroadDistanceButtonButtons") {
      std::vector<QString> CustomDistanceIconsOptions{tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")};
      manageDistanceIconsBtn = new FrogPilotButtonsControl(title, desc, icon, CustomDistanceIconsOptions);

      std::function<QString(QString)> formatIconName = [](QString name) -> QString {
        QChar separator = name.contains('_') ? '_' : '-';
        QStringList parts = name.replace(separator, ' ').split(' ');

        for (int i = 0; i < parts.size(); ++i) {
          parts[i][0] = parts[i][0].toUpper();
        }

        if (separator == '-' && parts.size() > 1) {
          return parts.first() + " (" + parts.last() + ")";
        }

        return parts.join(' ');
      };

      std::function<QString(QString)> formatIconNameForStorage = [](QString name) -> QString {
        name = name.toLower();
        name = name.replace(" (", "-");
        name = name.replace(' ', '_');
        name.remove('(').remove(')');
        return name;
      };

      QObject::connect(manageDistanceIconsBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        QDir themesDir{"/data/themes/distance_icons"};
        QFileInfoList dirList = themesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

        QString currentDistanceIcon = QString::fromStdString(params.get("CustomDistanceIcons")).replace('_', ' ').replace('-', " (").toLower();
        currentDistanceIcon[0] = currentDistanceIcon[0].toUpper();
        for (int i = 1; i < currentDistanceIcon.length(); i++) {
          if (currentDistanceIcon[i - 1] == ' ' || currentDistanceIcon[i - 1] == '(') {
            currentDistanceIcon[i] = currentDistanceIcon[i].toUpper();
          }
        }
        if (currentDistanceIcon.contains(" (")) {
          currentDistanceIcon.append(')');
        }

        QStringList availableIcons;
        for (const QFileInfo &dirInfo : dirList) {
          QString iconPackDir = dirInfo.absoluteFilePath();

          availableIcons << formatIconName(dirInfo.fileName());
        }
        availableIcons.append("Stock");
        std::sort(availableIcons.begin(), availableIcons.end());

        if (id == 0) {
          QStringList iconPackList = availableIcons;
          iconPackList.removeAll("Stock");
          iconPackList.removeAll(currentDistanceIcon);

          QString iconPackToDelete = MultiOptionDialog::getSelection(tr("Select an icon pack to delete"), iconPackList, "", this);
          if (!iconPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' icon pack?").arg(iconPackToDelete), tr("Delete"), this)) {
            themeDeleting = true;
            iconsDownloaded = false;

            QString selectedIconPack = formatIconNameForStorage(iconPackToDelete);
            for (const QFileInfo &dirInfo : dirList) {
              if (dirInfo.fileName() == selectedIconPack) {
                QDir iconPackDir(dirInfo.absoluteFilePath() + "/distance_icons");
                if (iconPackDir.exists()) {
                  iconPackDir.removeRecursively();
                }
              }
            }

            QStringList downloadableIcons = QString::fromStdString(params.get("DownloadableDistanceIcons")).split(",");
            downloadableIcons << iconPackToDelete;
            downloadableIcons.removeDuplicates();
            downloadableIcons.removeAll("");
            std::sort(downloadableIcons.begin(), downloadableIcons.end());

            params.put("DownloadableDistanceIcons", downloadableIcons.join(",").toStdString());
            themeDeleting = false;
          }
        } else if (id == 1) {
          if (manageDistanceIconsBtn->getButton(id)->text() == tr("CANCEL")) {
            paramsMemory.putBool("CancelThemeDownload", true);
            cancellingDownload = true;

            QTimer::singleShot(2000, [=]() {
              paramsMemory.putBool("CancelThemeDownload", false);
              cancellingDownload = false;
              iconsDownloading = false;
              themeDownloading = false;
            });
          } else {
            QStringList downloadableIcons = QString::fromStdString(params.get("DownloadableDistanceIcons")).split(",");
            QString iconPackToDownload = MultiOptionDialog::getSelection(tr("Select an icon pack to download"), downloadableIcons, "", this);

            if (!iconPackToDownload.isEmpty()) {
              QString convertedIconPack = formatIconNameForStorage(iconPackToDownload);
              paramsMemory.put("DistanceIconToDownload", convertedIconPack.toStdString());
              downloadStatusLabel->setText("Downloading...");
              paramsMemory.put("ThemeDownloadProgress", "Downloading...");
              iconsDownloading = true;
              themeDownloading = true;

              downloadableIcons.removeAll(iconPackToDownload);
              params.put("DownloadableDistanceIcons", downloadableIcons.join(",").toStdString());
            }
          }
        } else if (id == 2) {
          QString iconPackToSelect = MultiOptionDialog::getSelection(tr("Select an icon pack"), availableIcons, currentDistanceIcon, this);
          if (!iconPackToSelect.isEmpty()) {
            params.put("CustomDistanceIcons", formatIconNameForStorage(iconPackToSelect).toStdString());
            manageDistanceIconsBtn->setValue(iconPackToSelect);
            paramsMemory.putBool("UpdateTheme", true);
          }
        }
      });

      QString currentDistanceIcon = QString::fromStdString(params.get("CustomDistanceIcons")).replace('_', ' ').replace('-', " (").toLower();
      currentDistanceIcon[0] = currentDistanceIcon[0].toUpper();
      for (int i = 1; i < currentDistanceIcon.length(); i++) {
        if (currentDistanceIcon[i - 1] == ' ' || currentDistanceIcon[i - 1] == '(') {
          currentDistanceIcon[i] = currentDistanceIcon[i].toUpper();
        }
      }
      if (currentDistanceIcon.contains(" (")) {
        currentDistanceIcon.append(')');
      }
      manageDistanceIconsBtn->setValue(currentDistanceIcon);
      controlToggle = reinterpret_cast<AbstractControl*>(manageDistanceIconsBtn);
    } else if (param == "DownloadStatusLabel") {
      downloadStatusLabel = new LabelControl(title, "Idle");
      controlToggle = reinterpret_cast<AbstractControl*>(downloadStatusLabel);

    } else if (param == "ExperimentalModeActivation") {
      FrogPilotParamManageControl *experimentalModeActivationToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(experimentalModeActivationToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedExperimentalModeActivationKeys = experimentalModeActivationKeys;

          if (params.getBool("AlwaysOnLateralLKAS")) {
            modifiedExperimentalModeActivationKeys.erase("ExperimentalModeViaLKAS");
          }

          toggle->setVisible(modifiedExperimentalModeActivationKeys.find(key.c_str()) != modifiedExperimentalModeActivationKeys.end());
        }
      });
      controlToggle = experimentalModeActivationToggle;

    } else if (param == "LateralTune") {
      FrogPilotParamManageControl *lateralTuneToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(lateralTuneToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedLateralTuneKeys = lateralTuneKeys;

          if (hasAutoTune || params.getBool("LateralTune") && params.getBool("NNFF")) {
            modifiedLateralTuneKeys.erase("ForceAutoTune");
          }

          if (hasCommaNNFFSupport || !hasNNFFLog) {
            modifiedLateralTuneKeys.erase("NNFF");
          } else {
            modifiedLateralTuneKeys.erase("NNFFLite");
          }

          toggle->setVisible(modifiedLateralTuneKeys.find(key.c_str()) != modifiedLateralTuneKeys.end());
        }
      });
      controlToggle = lateralTuneToggle;
    } else if (param == "SteerRatio") {
      std::vector<QString> steerRatioToggles{"ResetSteerRatio"};
      std::vector<QString> steerRatioToggleNames{"Reset"};
      controlToggle = new FrogPilotParamValueToggleControl(param, title, desc, icon, steerRatioStock * 0.75, steerRatioStock * 1.25, std::map<int, QString>(), this, false, "", 1, 0.01, steerRatioToggles, steerRatioToggleNames);

    } else if (param == "LongitudinalTune") {
      FrogPilotParamManageControl *longitudinalTuneToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(longitudinalTuneToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        bool radarlessModel = QString::fromStdString(params.get("RadarlessModels")).split(",").contains(QString::fromStdString(params.get("Model")));

        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedLongitudinalTuneKeys = longitudinalTuneKeys;

          if (radarlessModel) {
            modifiedLongitudinalTuneKeys.erase("LeadDetectionThreshold");
          }

          toggle->setVisible(modifiedLongitudinalTuneKeys.find(key.c_str()) != modifiedLongitudinalTuneKeys.end());
        }

      });
      controlToggle = longitudinalTuneToggle;
    } else if (param == "AccelerationProfile") {
      std::vector<QString> profileOptions{tr("Standard"), tr("Eco"), tr("Sport"), tr("Sport+")};
      FrogPilotButtonParamControl *profileSelection = new FrogPilotButtonParamControl(param, title, desc, icon, profileOptions);
      controlToggle = profileSelection;
    } else if (param == "DecelerationProfile") {
      std::vector<QString> profileOptions{tr("Standard"), tr("Eco"), tr("Sport")};
      FrogPilotButtonParamControl *profileSelection = new FrogPilotButtonParamControl(param, title, desc, icon, profileOptions);
      controlToggle = profileSelection;
    } else if (param == "StoppingDistance") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, std::map<int, QString>(), this, false, tr(" feet"));
    } else if (param == "LeadDetectionThreshold") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, std::map<int, QString>(), this, false, "%");

    } else if (param == "MTSCEnabled") {
      FrogPilotParamManageControl *mtscToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(mtscToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(mtscKeys.find(key.c_str()) != mtscKeys.end());
        }

      });
      controlToggle = mtscToggle;
    } else if (param == "MTSCAggressiveness") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 200, std::map<int, QString>(), this, false, "%");

    } else if (param == "ModelManagement") {
      FrogPilotParamManageControl *modelManagementToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(modelManagementToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        availableModelNames = QString::fromStdString(params.get("AvailableModelsNames")).split(",");
        availableModels = QString::fromStdString(params.get("AvailableModels")).split(",");
        experimentalModels = QString::fromStdString(params.get("ExperimentalModels")).split(",");

        modelManagementOpen = true;

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(modelManagementKeys.find(key.c_str()) != modelManagementKeys.end());
        }

        QString currentModel = QString::fromStdString(params.get("Model")) + ".thneed";
        QStringList modelFiles = modelDir.entryList({"*.thneed"}, QDir::Files);
        modelFiles.removeAll(currentModel);
        haveModelsDownloaded = modelFiles.size() > 1;
        modelsDownloaded = params.getBool("ModelsDownloaded");

      });
      controlToggle = modelManagementToggle;
    } else if (param == "ModelRandomizer") {
      FrogPilotParamManageControl *modelRandomizerToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(modelRandomizerToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openSubParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(modelRandomizerKeys.find(key.c_str()) != modelRandomizerKeys.end());
        }
      });
      controlToggle = modelRandomizerToggle;
    } else if (param == "ManageBlacklistedModels") {
      FrogPilotButtonsControl *blacklistBtn = new FrogPilotButtonsControl(title, desc, icon, {tr("ADD"), tr("REMOVE")});
      QObject::connect(blacklistBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        QStringList blacklistedModels = QString::fromStdString(params.get("BlacklistedModels")).split(",", QString::SkipEmptyParts);
        QMap<QString, QString> labelToModelMap;
        QStringList selectableModels, deletableModels;

        for (int i = 0; i < availableModels.size(); i++) {
          QString model = availableModels[i];
          if (blacklistedModels.contains(model)) {
            deletableModels.append(availableModelNames[i]);
          } else {
            selectableModels.append(availableModelNames[i]);
          }
          labelToModelMap[availableModelNames[i]] = model;
        }

        if (id == 0) {
          if (selectableModels.size() == 1) {
            FrogPilotConfirmationDialog::toggleAlert(tr("There's no more models to blacklist! The only available model is \"%1\"!").arg(selectableModels.first()), tr("OK"), this);
          } else {
            QString selectedModel = MultiOptionDialog::getSelection(tr("Select a model to add to the blacklist"), selectableModels, "", this);
            if (!selectedModel.isEmpty()) {
              if (ConfirmationDialog::confirm(tr("Are you sure you want to add the '%1' model to the blacklist?").arg(selectedModel), tr("Add"), this)) {
                QString modelToAdd = labelToModelMap[selectedModel];
                if (!blacklistedModels.contains(modelToAdd)) {
                  blacklistedModels.append(modelToAdd);
                  params.putNonBlocking("BlacklistedModels", blacklistedModels.join(",").toStdString());
                }
              }
            }
          }
        } else if (id == 1) {
          QString selectedModel = MultiOptionDialog::getSelection(tr("Select a model to remove from the blacklist"), deletableModels, "", this);
          if (!selectedModel.isEmpty()) {
            if (ConfirmationDialog::confirm(tr("Are you sure you want to remove the '%1' model from the blacklist?").arg(selectedModel), tr("Remove"), this)) {
              QString modelToRemove = labelToModelMap[selectedModel];
              if (blacklistedModels.contains(modelToRemove)) {
                blacklistedModels.removeAll(modelToRemove);
                params.putNonBlocking("BlacklistedModels", blacklistedModels.join(",").toStdString());
                paramsStorage.put("BlacklistedModels", blacklistedModels.join(",").toStdString());
              }
            }
          }
        }
      });
      controlToggle = reinterpret_cast<AbstractControl*>(blacklistBtn);
    } else if (param == "ResetScores") {
      ButtonControl *resetCalibrationsBtn = new ButtonControl(title, tr("RESET"), desc);
      QObject::connect(resetCalibrationsBtn, &ButtonControl::clicked, [=]() {
        if (FrogPilotConfirmationDialog::yesorno(tr("Reset all model scores?"), this)) {
          for (const QString &model : availableModelNames) {
            QString cleanedModel = processModelName(model);
            params.remove(QString("%1Drives").arg(cleanedModel).toStdString());
            paramsStorage.remove(QString("%1Drives").arg(cleanedModel).toStdString());
            params.remove(QString("%1Score").arg(cleanedModel).toStdString());
            paramsStorage.remove(QString("%1Score").arg(cleanedModel).toStdString());
          }
          updateModelLabels();
        }
      });
      controlToggle = reinterpret_cast<AbstractControl*>(resetCalibrationsBtn);
    } else if (param == "ReviewScores") {
      ButtonControl *reviewScoresBtn = new ButtonControl(title, tr("VIEW"), desc);
      QObject::connect(reviewScoresBtn, &ButtonControl::clicked, [=]() {
        openSubSubParentToggle();

        for (LabelControl *label : labelControls) {
          label->setVisible(true);
        }

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(false);
        }
      });
      controlToggle = reinterpret_cast<AbstractControl*>(reviewScoresBtn);
    } else if (param == "DeleteModel") {
      deleteModelBtn = new ButtonControl(title, tr("DELETE"), desc);
      QObject::connect(deleteModelBtn, &ButtonControl::clicked, [=]() {
        QStringList deletableModels, existingModels = modelDir.entryList({"*.thneed"}, QDir::Files);
        QMap<QString, QString> labelToFileMap;
        QString currentModel = QString::fromStdString(params.get("Model")) + ".thneed";

        for (int i = 0; i < availableModels.size(); i++) {
          QString modelFile = availableModels[i] + ".thneed";
          if (existingModels.contains(modelFile) && modelFile != currentModel && !availableModelNames[i].contains("(Default)")) {
            deletableModels.append(availableModelNames[i]);
            labelToFileMap[availableModelNames[i]] = modelFile;
          }
        }

        QString selectedModel = MultiOptionDialog::getSelection(tr("Select a model to delete"), deletableModels, "", this);
        if (!selectedModel.isEmpty()) {
          if (ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' model?").arg(selectedModel), tr("Delete"), this)) {
            std::thread([=]() {
              modelDeleting = true;
              modelsDownloaded = false;
              update();

              params.putBoolNonBlocking("ModelsDownloaded", false);
              deleteModelBtn->setValue(tr("Deleting..."));

              QFile::remove(modelDir.absoluteFilePath(labelToFileMap[selectedModel]));
              deleteModelBtn->setValue(tr("Deleted!"));

              util::sleep_for(1000);
              deleteModelBtn->setValue("");
              modelDeleting = false;

              QStringList modelFiles = modelDir.entryList({"*.thneed"}, QDir::Files);
              modelFiles.removeAll(currentModel);

              haveModelsDownloaded = modelFiles.size() > 1;
              update();
            }).detach();
          }
        }
      });
      controlToggle = reinterpret_cast<AbstractControl*>(deleteModelBtn);
    } else if (param == "DownloadModel") {
      downloadModelBtn = new ButtonControl(title, tr("DOWNLOAD"), desc);
      QObject::connect(downloadModelBtn, &ButtonControl::clicked, [=]() {
        if (downloadModelBtn->text() == tr("CANCEL")) {
          paramsMemory.remove("ModelToDownload");
          paramsMemory.putBool("CancelModelDownload", true);
          cancellingDownload = true;
        } else {
          QMap<QString, QString> labelToModelMap;
          QStringList existingModels = modelDir.entryList({"*.thneed"}, QDir::Files);
          QStringList downloadableModels;

          for (int i = 0; i < availableModels.size(); i++) {
            QString modelFile = availableModels[i] + ".thneed";
            if (!existingModels.contains(modelFile) && !availableModelNames[i].contains("(Default)")) {
              downloadableModels.append(availableModelNames[i]);
              labelToModelMap.insert(availableModelNames[i], availableModels[i]);
            }
          }

          QString modelToDownload = MultiOptionDialog::getSelection(tr("Select a driving model to download"), downloadableModels, "", this);
          if (!modelToDownload.isEmpty()) {
            modelDownloading = true;
            paramsMemory.put("ModelToDownload", labelToModelMap.value(modelToDownload).toStdString());
            paramsMemory.put("ModelDownloadProgress", "0%");

            downloadModelBtn->setValue(tr("Downloading %1...").arg(modelToDownload.remove(QRegularExpression("[🗺️👀📡]")).trimmed()));

            QTimer *progressTimer = new QTimer(this);
            progressTimer->setInterval(100);

            QObject::connect(progressTimer, &QTimer::timeout, this, [=]() {
              QString progress = QString::fromStdString(paramsMemory.get("ModelDownloadProgress"));
              bool downloadComplete = progress.contains(QRegularExpression("downloaded", QRegularExpression::CaseInsensitiveOption));
              bool downloadFailed = progress.contains(QRegularExpression("cancelled|exists|failed|offline", QRegularExpression::CaseInsensitiveOption));

              if (!progress.isEmpty() && progress != "0%") {
                downloadModelBtn->setValue(progress);
              }

              if (downloadComplete || downloadFailed) {
                bool lastModelDownloaded = downloadComplete;

                if (downloadComplete) {
                  haveModelsDownloaded = true;
                  update();
                }

                if (downloadComplete) {
                  for (const QString &model : availableModels) {
                    if (!QFile::exists(modelDir.filePath(model + ".thneed"))) {
                      lastModelDownloaded = false;
                      break;
                    }
                  }
                }

                downloadModelBtn->setValue(progress);

                paramsMemory.remove("CancelModelDownload");
                paramsMemory.remove("ModelDownloadProgress");

                progressTimer->stop();
                progressTimer->deleteLater();

                QTimer::singleShot(2000, this, [=]() {
                  cancellingDownload = false;
                  modelDownloading = false;

                  downloadModelBtn->setValue("");

                  if (lastModelDownloaded) {
                    modelsDownloaded = true;
                    update();

                    params.putBoolNonBlocking("ModelsDownloaded", modelsDownloaded);
                  }
                });
              }
            });
            progressTimer->start();
          }
        }
      });
      controlToggle = reinterpret_cast<AbstractControl*>(downloadModelBtn);
    } else if (param == "DownloadAllModels") {
      downloadAllModelsBtn = new ButtonControl(title, tr("DOWNLOAD"), desc);
      QObject::connect(downloadAllModelsBtn, &ButtonControl::clicked, [=]() {
        if (downloadAllModelsBtn->text() == tr("CANCEL")) {
          paramsMemory.remove("DownloadAllModels");
          paramsMemory.putBool("CancelModelDownload", true);
          cancellingDownload = true;
        } else {
          startDownloadAllModels();
        }
      });
      controlToggle = reinterpret_cast<AbstractControl*>(downloadAllModelsBtn);
    } else if (param == "SelectModel") {
      selectModelBtn = new ButtonControl(title, tr("SELECT"), desc);
      QObject::connect(selectModelBtn, &ButtonControl::clicked, [=]() {
        QSet<QString> modelFilesBaseNames = QSet<QString>::fromList(modelDir.entryList({"*.thneed"}, QDir::Files).replaceInStrings(QRegExp("\\.thneed$"), ""));
        QStringList selectableModels;

        for (int i = 0; i < availableModels.size(); i++) {
          if (modelFilesBaseNames.contains(availableModels[i]) || availableModelNames[i].contains("(Default)")) {
            selectableModels.append(availableModelNames[i]);
          }
        }

        QString modelToSelect = MultiOptionDialog::getSelection(tr("Select a model - 🗺️ = Navigation | 📡 = Radar | 👀 = VOACC"), selectableModels, "", this);
        if (!modelToSelect.isEmpty()) {
          selectModelBtn->setValue(modelToSelect);
          int modelIndex = availableModelNames.indexOf(modelToSelect);

          params.putNonBlocking("Model", availableModels.at(modelIndex).toStdString());
          params.putNonBlocking("ModelName", modelToSelect.toStdString());

          if (experimentalModels.contains(availableModels.at(modelIndex))) {
            FrogPilotConfirmationDialog::toggleAlert(tr("WARNING: This is a very experimental model and may drive dangerously!"), tr("I understand the risks."), this);
          }

          QString model = availableModelNames.at(modelIndex);
          QString part_model_param = processModelName(model);

          if (!params.checkKey(part_model_param.toStdString() + "CalibrationParams") || !params.checkKey(part_model_param.toStdString() + "LiveTorqueParameters")) {
            if (FrogPilotConfirmationDialog::yesorno(tr("Start with a fresh calibration for the newly selected model?"), this)) {
              params.remove("CalibrationParams");
              params.remove("LiveTorqueParameters");
            }
          }

          if (started) {
            if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
              Hardware::reboot();
            }
          }
        }
      });
      selectModelBtn->setValue(QString::fromStdString(params.get("ModelName")));
      controlToggle = reinterpret_cast<AbstractControl*>(selectModelBtn);
    } else if (param == "ResetCalibrations") {
      FrogPilotButtonsControl *resetCalibrationsBtn = new FrogPilotButtonsControl(title, desc, icon, {tr("RESET ALL"), tr("RESET ONE")});
      QObject::connect(resetCalibrationsBtn, &FrogPilotButtonsControl::showDescriptionEvent, this, &FrogPilotControlsPanel::updateCalibrationDescription);
      QObject::connect(resetCalibrationsBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        if (id == 0) {
          if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset all of your model calibrations?"), this)) {
            for (const QString &model : availableModelNames) {
              QString cleanedModel = processModelName(model);
              params.remove(QString("%1CalibrationParams").arg(cleanedModel).toStdString());
              paramsStorage.remove(QString("%1CalibrationParams").arg(cleanedModel).toStdString());
              params.remove(QString("%1LiveTorqueParameters").arg(cleanedModel).toStdString());
              paramsStorage.remove(QString("%1LiveTorqueParameters").arg(cleanedModel).toStdString());
            }
          }
        } else if (id == 1) {
          QStringList selectableModelLabels;
          for (int i = 0; i < availableModels.size(); i++) {
            selectableModelLabels.append(availableModelNames[i]);
          }

          QString modelToReset = MultiOptionDialog::getSelection(tr("Select a model to reset"), selectableModelLabels, "", this);
          if (!modelToReset.isEmpty()) {
            if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset this model's calibrations?"), this)) {
              QString cleanedModel = processModelName(modelToReset);
              params.remove(QString("%1CalibrationParams").arg(cleanedModel).toStdString());
              paramsStorage.remove(QString("%1CalibrationParams").arg(cleanedModel).toStdString());
              params.remove(QString("%1LiveTorqueParameters").arg(cleanedModel).toStdString());
              paramsStorage.remove(QString("%1LiveTorqueParameters").arg(cleanedModel).toStdString());
            }
          }
        }
      });
      controlToggle = reinterpret_cast<AbstractControl*>(resetCalibrationsBtn);

    } else if (param == "QOLControls") {
      FrogPilotParamManageControl *qolToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(qolToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedQolKeys = qolKeys;

          if (!hasPCMCruise) {
            modifiedQolKeys.erase("ReverseCruise");
          } else {
            modifiedQolKeys.erase("CustomCruise");
            modifiedQolKeys.erase("CustomCruiseLong");
            modifiedQolKeys.erase("SetSpeedOffset");
          }

          if (!isToyota && !isGM && !isHKGCanFd) {
            modifiedQolKeys.erase("MapGears");
          }

          toggle->setVisible(modifiedQolKeys.find(key.c_str()) != modifiedQolKeys.end());
        }

      });
      controlToggle = qolToggle;
    } else if (param == "CustomCruise") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, std::map<int, QString>(), this, false, tr("mph"));
    } else if (param == "CustomCruiseLong") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, std::map<int, QString>(), this, false, tr("mph"));
    } else if (param == "ForceStandstill") {
      std::vector<QString> forceStopToggles{"ForceStops"};
      std::vector<QString> forceStopToggleNames{tr("Only For Stop Lights/Stop Signs")};
      controlToggle = new FrogPilotParamToggleControl(param, title, desc, icon, forceStopToggles, forceStopToggleNames);
    } else if (param == "MapGears") {
      std::vector<QString> mapGearsToggles{"MapAcceleration", "MapDeceleration"};
      std::vector<QString> mapGearsToggleNames{tr("Acceleration"), tr("Deceleration")};
      controlToggle = new FrogPilotParamToggleControl(param, title, desc, icon, mapGearsToggles, mapGearsToggleNames);
    } else if (param == "PauseLateralSpeed") {
      std::vector<QString> pauseLateralToggles{"PauseLateralOnSignal"};
      std::vector<QString> pauseLateralToggleNames{"Turn Signal Only"};
      controlToggle = new FrogPilotParamValueToggleControl(param, title, desc, icon, 0, 99, std::map<int, QString>(), this, false, tr("mph"), 1, 1, pauseLateralToggles, pauseLateralToggleNames);
    } else if (param == "PauseLateralOnSignal") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, std::map<int, QString>(), this, false, tr("mph"));
    } else if (param == "ReverseCruise") {
      std::vector<QString> reverseCruiseToggles{"ReverseCruiseUI"};
      std::vector<QString> reverseCruiseNames{tr("Control Via UI")};
      controlToggle = new FrogPilotParamToggleControl(param, title, desc, icon, reverseCruiseToggles, reverseCruiseNames);
    } else if (param == "SetSpeedOffset") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, std::map<int, QString>(), this, false, tr("mph"));

    } else if (param == "LaneChangeCustomizations") {
      FrogPilotParamManageControl *laneChangeToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(laneChangeToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(laneChangeKeys.find(key.c_str()) != laneChangeKeys.end());
        }

      });
      controlToggle = laneChangeToggle;
    } else if (param == "LaneChangeTime") {
      std::map<int, QString> laneChangeTimeLabels;
      for (int i = 0; i <= 10; i++) {
        laneChangeTimeLabels[i] = i == 0 ? "Instant" : QString::number(i / 2.0) + " seconds";
      }
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, laneChangeTimeLabels, this, false);
    } else if (param == "LaneDetectionWidth") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, std::map<int, QString>(), this, false, " feet", 10);
    } else if (param == "MinimumLaneChangeSpeed") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, std::map<int, QString>(), this, false, tr("mph"));

    } else if (param == "SpeedLimitController") {
      FrogPilotParamManageControl *speedLimitControllerToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(speedLimitControllerToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        slcOpen = true;
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(speedLimitControllerKeys.find(key.c_str()) != speedLimitControllerKeys.end());
        }

      });
      controlToggle = speedLimitControllerToggle;
    } else if (param == "SLCControls") {
      FrogPilotParamManageControl *manageSLCControlsToggle = new FrogPilotParamManageControl(param, title, desc, icon, this, true);
      QObject::connect(manageSLCControlsToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openSubParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(speedLimitControllerControlsKeys.find(key.c_str()) != speedLimitControllerControlsKeys.end());
        }
      });
      controlToggle = manageSLCControlsToggle;
    } else if (param == "SLCQOL") {
      FrogPilotParamManageControl *manageSLCQOLToggle = new FrogPilotParamManageControl(param, title, desc, icon, this, true);
      QObject::connect(manageSLCQOLToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openSubParentToggle();

        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedSpeedLimitControllerQOLKeys = speedLimitControllerQOLKeys;

          if (hasPCMCruise) {
            modifiedSpeedLimitControllerQOLKeys.erase("SetSpeedLimit");
          }

          if (!isToyota) {
            modifiedSpeedLimitControllerQOLKeys.erase("ForceMPHDashboard");
          }

          toggle->setVisible(modifiedSpeedLimitControllerQOLKeys.find(key.c_str()) != modifiedSpeedLimitControllerQOLKeys.end());
        }

      });
      controlToggle = manageSLCQOLToggle;
    } else if (param == "SLCConfirmation") {
      std::vector<QString> slcConfirmationToggles{"SLCConfirmationLower", "SLCConfirmationHigher"};
      std::vector<QString> slcConfirmationNames{tr("Lower Limits"), tr("Higher Limits")};
      controlToggle = new FrogPilotParamToggleControl(param, title, desc, icon, slcConfirmationToggles, slcConfirmationNames);
    } else if (param == "SLCLookaheadHigher" || param == "SLCLookaheadLower") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 60, std::map<int, QString>(), this, false, " seconds");
    } else if (param == "SLCVisuals") {
      FrogPilotParamManageControl *manageSLCVisualsToggle = new FrogPilotParamManageControl(param, title, desc, icon, this, true);
      QObject::connect(manageSLCVisualsToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openSubParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(speedLimitControllerVisualsKeys.find(key.c_str()) != speedLimitControllerVisualsKeys.end());
        }
      });
      controlToggle = manageSLCVisualsToggle;
    } else if (param == "Offset1" || param == "Offset2" || param == "Offset3" || param == "Offset4") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, -99, 99, std::map<int, QString>(), this, false, tr("mph"));
    } else if (param == "ShowSLCOffset") {
      std::vector<QString> slcOffsetToggles{"ShowSLCOffsetUI"};
      std::vector<QString> slcOffsetToggleNames{tr("Control Via UI")};
      controlToggle = new FrogPilotParamToggleControl(param, title, desc, icon, slcOffsetToggles, slcOffsetToggleNames);
    } else if (param == "SLCFallback") {
      std::vector<QString> fallbackOptions{tr("Set Speed"), tr("Experimental Mode"), tr("Previous Limit")};
      FrogPilotButtonParamControl *fallbackSelection = new FrogPilotButtonParamControl(param, title, desc, icon, fallbackOptions);
      controlToggle = fallbackSelection;
    } else if (param == "SLCOverride") {
      std::vector<QString> overrideOptions{tr("None"), tr("Manual Set Speed"), tr("Set Speed")};
      FrogPilotButtonParamControl *overrideSelection = new FrogPilotButtonParamControl(param, title, desc, icon, overrideOptions);
      controlToggle = overrideSelection;
    } else if (param == "SLCPriority") {
      ButtonControl *slcPriorityButton = new ButtonControl(title, tr("SELECT"), desc);
      QStringList primaryPriorities = {tr("None"), tr("Dashboard"), tr("Navigation"), tr("Offline Maps"), tr("Highest"), tr("Lowest")};
      QStringList secondaryTertiaryPriorities = {tr("None"), tr("Dashboard"), tr("Navigation"), tr("Offline Maps")};
      QStringList priorityPrompts = {tr("Select your primary priority"), tr("Select your secondary priority"), tr("Select your tertiary priority")};

      QObject::connect(slcPriorityButton, &ButtonControl::clicked, [=]() {
        QStringList selectedPriorities;

        for (int i = 1; i <= 3; i++) {
          QStringList currentPriorities = (i == 1) ? primaryPriorities : secondaryTertiaryPriorities;
          QStringList prioritiesToDisplay = currentPriorities;
          for (const auto &selectedPriority : qAsConst(selectedPriorities)) {
            prioritiesToDisplay.removeAll(selectedPriority);
          }

          if (!hasDashSpeedLimits) {
            prioritiesToDisplay.removeAll(tr("Dashboard"));
          }

          if (prioritiesToDisplay.size() == 1 && prioritiesToDisplay.contains(tr("None"))) {
            break;
          }

          QString priorityKey = QString("SLCPriority%1").arg(i);
          QString selection = MultiOptionDialog::getSelection(priorityPrompts[i - 1], prioritiesToDisplay, "", this);

          if (selection.isEmpty()) break;

          params.putNonBlocking(priorityKey.toStdString(), selection.toStdString());
          selectedPriorities.append(selection);

          if (selection == tr("Lowest") || selection == tr("Highest") || selection == tr("None")) break;

          updateFrogPilotToggles();
        }

        selectedPriorities.removeAll(tr("None"));
        slcPriorityButton->setValue(selectedPriorities.join(", "));
      });

      QStringList initialPriorities;
      for (int i = 1; i <= 3; i++) {
        QString priorityKey = QString("SLCPriority%1").arg(i);
        QString priority = QString::fromStdString(params.get(priorityKey.toStdString()));

        if (!priority.isEmpty() && primaryPriorities.contains(priority) && priority != tr("None")) {
          initialPriorities.append(priority);
        }
      }
      slcPriorityButton->setValue(initialPriorities.join(", "));
      controlToggle = slcPriorityButton;

    } else if (param == "VisionTurnControl") {
      FrogPilotParamManageControl *visionTurnControlToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(visionTurnControlToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(visionTurnControlKeys.find(key.c_str()) != visionTurnControlKeys.end());
        }

      });
      controlToggle = visionTurnControlToggle;
    } else if (param == "CurveSensitivity" || param == "TurnAggressiveness") {
      controlToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 200, std::map<int, QString>(), this, false, "%");

    } else {
      controlToggle = new ParamControl(param, title, desc, icon, this);
    }

    addItem(controlToggle);
    toggles[param.toStdString()] = controlToggle;

    QObject::connect(static_cast<ToggleControl*>(controlToggle), &ToggleControl::toggleFlipped, &updateFrogPilotToggles);
    QObject::connect(static_cast<FrogPilotParamToggleControl*>(controlToggle), &FrogPilotParamToggleControl::buttonTypeClicked, &updateFrogPilotToggles);
    QObject::connect(static_cast<FrogPilotParamValueControl*>(controlToggle), &FrogPilotParamValueControl::valueChanged, &updateFrogPilotToggles);

    QObject::connect(controlToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });

    QObject::connect(static_cast<FrogPilotParamManageControl*>(controlToggle), &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
      update();
    });
  }

  QObject::connect(static_cast<ToggleControl*>(toggles["IncreaseThermalLimits"]), &ToggleControl::toggleFlipped, [this](bool state) {
    if (state) {
      FrogPilotConfirmationDialog::toggleAlert(
        tr("WARNING: This can cause premature wear or damage by running the device over comma's recommended temperature limits!"),
        tr("I understand the risks."), this);
    }
  });

  QObject::connect(static_cast<ToggleControl*>(toggles["NoLogging"]), &ToggleControl::toggleFlipped, [this](bool state) {
    if (state) {
      FrogPilotConfirmationDialog::toggleAlert(
        tr("WARNING: This will prevent your drives from being recorded and the data will be unobtainable!"),
        tr("I understand the risks."), this);
    }
  });

  QObject::connect(static_cast<ToggleControl*>(toggles["NoUploads"]), &ToggleControl::toggleFlipped, [this](bool state) {
    if (state) {
      FrogPilotConfirmationDialog::toggleAlert(
        tr("WARNING: This will prevent your drives from appearing on comma connect which may impact debugging and support!"),
        tr("I understand the risks."), this);
    }
  });

  QObject::connect(static_cast<ToggleControl*>(toggles["TrafficMode"]), &ToggleControl::toggleFlipped, [this](bool state) {
    if (state) {
      FrogPilotConfirmationDialog::toggleAlert(
        tr("To activate 'Traffic Mode' you hold down the 'distance' button on your steering wheel for 2.5 seconds."),
        tr("Sounds good!"), this);
    }
  });

  std::set<QString> rebootKeys = {"AlwaysOnLateral", "NNFF", "NNFFLite"};
  for (const QString &key : rebootKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key.toStdString().c_str()]), &ToggleControl::toggleFlipped, [this, key](bool state) {
      if (started) {
        if (key == "AlwaysOnLateral" && state) {
          if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
            Hardware::reboot();
          }
        } else if (key != "AlwaysOnLateral") {
          if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
            Hardware::reboot();
          }
        }
      }
    });
  }

  QObject::connect(static_cast<ToggleControl*>(toggles["ModelRandomizer"]), &ToggleControl::toggleFlipped, [this](bool state) {
    modelRandomizer = state;
    if (state && !modelsDownloaded) {
      if (FrogPilotConfirmationDialog::yesorno(tr("The 'Model Randomizer' only works with downloaded models. Do you want to download all the driving models?"), this)) {
        startDownloadAllModels();
      }
    }
  });

  QObject::connect(static_cast<ToggleControl*>(toggles["OnroadDistanceButton"]), &ToggleControl::toggleFlipped, [this](bool state) {
    downloadStatusLabel->setVisible(state);
    manageDistanceIconsBtn->setVisible(state);
    onroadDistanceButton = state;
    update();
  });

  FrogPilotParamValueControl *trafficFollowToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficFollow"]);
  FrogPilotParamValueControl *trafficAccelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkAcceleration"]);
  FrogPilotParamValueControl *trafficDangerToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkDanger"]);
  FrogPilotParamValueControl *trafficSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkSpeed"]);
  FrogPilotButtonsControl *trafficResetButton = static_cast<FrogPilotButtonsControl*>(toggles["ResetTrafficPersonality"]);

  QObject::connect(trafficResetButton, &FrogPilotButtonsControl::buttonClicked, this, [=]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the 'Traffic Mode' personality?"), this)) {
      params.putFloat("TrafficFollow", 0.5);
      params.putFloat("TrafficJerkAcceleration", 50);
      params.putFloat("TrafficJerkDanger", 100);
      params.putFloat("TrafficJerkSpeed", 50);
      trafficFollowToggle->refresh();
      trafficAccelerationToggle->refresh();
      trafficDangerToggle->refresh();
      trafficSpeedToggle->refresh();
      updateFrogPilotToggles();
    }
  });

  FrogPilotParamValueControl *aggressiveFollowToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveFollow"]);
  FrogPilotParamValueControl *aggressiveAccelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkAcceleration"]);
  FrogPilotParamValueControl *aggressiveDangerToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkDanger"]);
  FrogPilotParamValueControl *aggressiveSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkSpeed"]);
  FrogPilotButtonsControl *aggressiveResetButton = static_cast<FrogPilotButtonsControl*>(toggles["ResetAggressivePersonality"]);

  QObject::connect(aggressiveResetButton, &FrogPilotButtonsControl::buttonClicked, this, [=]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the 'Aggressive' personality?"), this)) {
      params.putFloat("AggressiveFollow", 1.25);
      params.putFloat("AggressiveJerkAcceleration", 50);
      params.putFloat("AggressiveJerkDanger", 100);
      params.putFloat("AggressiveJerkSpeed", 50);
      aggressiveFollowToggle->refresh();
      aggressiveAccelerationToggle->refresh();
      aggressiveDangerToggle->refresh();
      aggressiveSpeedToggle->refresh();
      updateFrogPilotToggles();
    }
  });

  FrogPilotParamValueControl *standardFollowToggle = static_cast<FrogPilotParamValueControl*>(toggles["StandardFollow"]);
  FrogPilotParamValueControl *standardAccelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["StandardJerkAcceleration"]);
  FrogPilotParamValueControl *standardDangerToggle = static_cast<FrogPilotParamValueControl*>(toggles["StandardJerkDanger"]);
  FrogPilotParamValueControl *standardSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["StandardJerkSpeed"]);
  FrogPilotButtonsControl *standardResetButton = static_cast<FrogPilotButtonsControl*>(toggles["ResetStandardPersonality"]);

  QObject::connect(standardResetButton, &FrogPilotButtonsControl::buttonClicked, this, [=]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the 'Standard' personality?"), this)) {
      params.putFloat("StandardFollow", 1.45);
      params.putFloat("StandardJerkAcceleration", 100);
      params.putFloat("StandardJerkDanger", 100);
      params.putFloat("StandardJerkSpeed", 100);
      standardFollowToggle->refresh();
      standardAccelerationToggle->refresh();
      standardDangerToggle->refresh();
      standardSpeedToggle->refresh();
      updateFrogPilotToggles();
    }
  });

  FrogPilotParamValueControl *relaxedFollowToggle = static_cast<FrogPilotParamValueControl*>(toggles["RelaxedFollow"]);
  FrogPilotParamValueControl *relaxedAccelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["RelaxedJerkAcceleration"]);
  FrogPilotParamValueControl *relaxedDangerToggle = static_cast<FrogPilotParamValueControl*>(toggles["RelaxedJerkDanger"]);
  FrogPilotParamValueControl *relaxedSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["RelaxedJerkSpeed"]);
  FrogPilotButtonsControl *relaxedResetButton = static_cast<FrogPilotButtonsControl*>(toggles["ResetRelaxedPersonality"]);

  QObject::connect(relaxedResetButton, &FrogPilotButtonsControl::buttonClicked, this, [=]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the 'Relaxed' personality?"), this)) {
      params.putFloat("RelaxedFollow", 1.75);
      params.putFloat("RelaxedJerkAcceleration", 100);
      params.putFloat("RelaxedJerkDanger", 100);
      params.putFloat("RelaxedJerkSpeed", 100);
      relaxedFollowToggle->refresh();
      relaxedAccelerationToggle->refresh();
      relaxedDangerToggle->refresh();
      relaxedSpeedToggle->refresh();
      updateFrogPilotToggles();
    }
  });

  steerRatioToggle = static_cast<FrogPilotParamValueToggleControl*>(toggles["SteerRatio"]);

  QObject::connect(steerRatioToggle, &FrogPilotParamValueToggleControl::buttonClicked, this, [this]() {
    params.putFloat("SteerRatio", steerRatioStock);
    steerRatioToggle->refresh();
    updateFrogPilotToggles();
  });

  QObject::connect(parent, &SettingsWindow::closeParentToggle, this, &FrogPilotControlsPanel::hideToggles);
  QObject::connect(parent, &SettingsWindow::closeSubParentToggle, this, &FrogPilotControlsPanel::hideSubToggles);
  QObject::connect(parent, &SettingsWindow::closeSubSubParentToggle, this, &FrogPilotControlsPanel::hideSubSubToggles);
  QObject::connect(parent, &SettingsWindow::updateMetric, this, &FrogPilotControlsPanel::updateMetric);
  QObject::connect(uiState(), &UIState::driveRated, this, &FrogPilotControlsPanel::updateModelLabels);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &FrogPilotControlsPanel::updateCarToggles);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotControlsPanel::updateState);

  updateMetric();
  updateModelLabels();
}

void FrogPilotControlsPanel::showEvent(QShowEvent *event) {
  disableOpenpilotLongitudinal = params.getBool("DisableOpenpilotLongitudinal");
  iconsDownloaded = params.get("DownloadableDistanceIcons").empty();
  modelRandomizer = params.getBool("ModelRandomizer");
  onroadDistanceButton = params.getBool("OnroadDistanceButton");
}

void FrogPilotControlsPanel::updateState(const UIState &s) {
  if (!isVisible()) return;

  if (drivingPersonalitiesOpen && onroadDistanceButton) {
    if (themeDownloading) {
      QString progress = QString::fromStdString(paramsMemory.get("ThemeDownloadProgress"));
      bool downloadFailed = progress.contains(QRegularExpression("cancelled|exists|Failed|offline", QRegularExpression::CaseInsensitiveOption));

      if (progress != "Downloading...") {
        downloadStatusLabel->setText(progress);
      }

      if (progress == "Downloaded!" || downloadFailed) {
        QTimer::singleShot(2000, [=]() {
          if (!themeDownloading) {
            downloadStatusLabel->setText("Idle");
          }
        });
        paramsMemory.remove("ThemeDownloadProgress");
        iconsDownloading = false;
        themeDownloading = false;

        iconsDownloaded = params.get("DownloadableDistanceIcons").empty();
      }
    }

    manageDistanceIconsBtn->setText(1, iconsDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    manageDistanceIconsBtn->setButtonEnabled(0, !themeDeleting && !themeDownloading);
    manageDistanceIconsBtn->setButtonEnabled(1, s.scene.online && (!themeDownloading || iconsDownloading) && !cancellingDownload && !themeDeleting && !iconsDownloaded);
    manageDistanceIconsBtn->setButtonEnabled(2, !themeDeleting && !themeDownloading);
  } else if (modelManagementOpen) {
    downloadAllModelsBtn->setText(modelDownloading && allModelsDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    downloadModelBtn->setText(modelDownloading && !allModelsDownloading ? tr("CANCEL") : tr("DOWNLOAD"));

    deleteModelBtn->setEnabled(!modelDeleting && !modelDownloading);
    downloadAllModelsBtn->setEnabled(s.scene.online && !cancellingDownload && !modelDeleting && (!modelDownloading || allModelsDownloading) && !modelsDownloaded);
    downloadModelBtn->setEnabled(s.scene.online && !cancellingDownload && !modelDeleting && !allModelsDownloading && !modelsDownloaded);
    selectModelBtn->setEnabled(!modelDeleting && !modelDownloading && !modelRandomizer);
  }

  started = s.scene.started;
}

void FrogPilotControlsPanel::updateCarToggles() {
  auto carParams = params.get("CarParamsPersistent");
  if (!carParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(carParams.data(), carParams.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();
    auto carFingerprint = CP.getCarFingerprint();
    auto carName = CP.getCarName();
    auto safetyConfigs = CP.getSafetyConfigs();
    auto safetyModel = safetyConfigs[0].getSafetyModel();

    hasAutoTune = (carName == "hyundai" || carName == "toyota") && CP.getLateralTuning().which() == cereal::CarParams::LateralTuning::TORQUE;
    bool forcingAutoTune = params.getBool("LateralTune") && params.getBool("ForceAutoTune");
    uiState()->scene.has_auto_tune = hasAutoTune || forcingAutoTune;
    hasCommaNNFFSupport = checkCommaNNFFSupport(carFingerprint);
    hasDashSpeedLimits = carName == "hyundai" || carName == "toyota";
    hasNNFFLog = checkNNFFLogFileExists(carFingerprint);
    hasOpenpilotLongitudinal = hasLongitudinalControl(CP);
    hasPCMCruise = CP.getPcmCruise();
    isGM = carName == "gm";
    isHKGCanFd = carName == "hyundai" && safetyModel == cereal::CarParams::SafetyModel::HYUNDAI_CANFD;
    isToyota = carName == "toyota";
    steerRatioStock = CP.getSteerRatio();

    steerRatioToggle->setTitle(QString(tr("Steer Ratio (Default: %1)")).arg(QString::number(steerRatioStock, 'f', 2)));
    steerRatioToggle->updateControl(steerRatioStock * 0.75, steerRatioStock * 1.25, "", 0.01);
    steerRatioToggle->refresh();
  } else {
    hasAutoTune = false;
    hasCommaNNFFSupport = false;
    hasDashSpeedLimits = true;
    hasNNFFLog = true;
    hasOpenpilotLongitudinal = true;
    hasPCMCruise = true;
    isGM = true;
    isHKGCanFd = true;
    isToyota = true;
  }

  hideToggles();
}

void FrogPilotControlsPanel::updateMetric() {
  bool previousIsMetric = isMetric;
  isMetric = params.getBool("IsMetric");

  if (isMetric != previousIsMetric) {
    double distanceConversion = isMetric ? FOOT_TO_METER : METER_TO_FOOT;
    double speedConversion = isMetric ? MILE_TO_KM : KM_TO_MILE;

    params.putIntNonBlocking("LaneDetectionWidth", std::nearbyint(params.getInt("LaneDetectionWidth") * distanceConversion));
    params.putIntNonBlocking("StoppingDistance", std::nearbyint(params.getInt("StoppingDistance") * distanceConversion));

    params.putIntNonBlocking("CESpeed", std::nearbyint(params.getInt("CESpeed") * speedConversion));
    params.putIntNonBlocking("CESpeedLead", std::nearbyint(params.getInt("CESpeedLead") * speedConversion));
    params.putIntNonBlocking("CustomCruise", std::nearbyint(params.getInt("CustomCruise") * speedConversion));
    params.putIntNonBlocking("CustomCruiseLong", std::nearbyint(params.getInt("CustomCruiseLong") * speedConversion));
    params.putIntNonBlocking("MinimumLaneChangeSpeed", std::nearbyint(params.getInt("MinimumLaneChangeSpeed") * speedConversion));
    params.putIntNonBlocking("Offset1", std::nearbyint(params.getInt("Offset1") * speedConversion));
    params.putIntNonBlocking("Offset2", std::nearbyint(params.getInt("Offset2") * speedConversion));
    params.putIntNonBlocking("Offset3", std::nearbyint(params.getInt("Offset3") * speedConversion));
    params.putIntNonBlocking("Offset4", std::nearbyint(params.getInt("Offset4") * speedConversion));
    params.putIntNonBlocking("PauseAOLOnBrake", std::nearbyint(params.getInt("PauseAOLOnBrake") * speedConversion));
    params.putIntNonBlocking("PauseLateralOnSignal", std::nearbyint(params.getInt("PauseLateralOnSignal") * speedConversion));
    params.putIntNonBlocking("PauseLateralSpeed", std::nearbyint(params.getInt("PauseLateralSpeed") * speedConversion));
    params.putIntNonBlocking("SetSpeedOffset", std::nearbyint(params.getInt("SetSpeedOffset") * speedConversion));
  }

  FrogPilotDualParamControl *ceSpeedToggle = reinterpret_cast<FrogPilotDualParamControl*>(toggles["CESpeed"]);
  FrogPilotParamValueControl *customCruiseToggle = static_cast<FrogPilotParamValueControl*>(toggles["CustomCruise"]);
  FrogPilotParamValueControl *customCruiseLongToggle = static_cast<FrogPilotParamValueControl*>(toggles["CustomCruiseLong"]);
  FrogPilotParamValueControl *laneWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["LaneDetectionWidth"]);
  FrogPilotParamValueControl *minimumLaneChangeSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["MinimumLaneChangeSpeed"]);
  FrogPilotParamValueControl *offset1Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset1"]);
  FrogPilotParamValueControl *offset2Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset2"]);
  FrogPilotParamValueControl *offset3Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset3"]);
  FrogPilotParamValueControl *offset4Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset4"]);
  FrogPilotParamValueControl *pauseAOLOnBrakeToggle = static_cast<FrogPilotParamValueControl*>(toggles["PauseAOLOnBrake"]);
  FrogPilotParamValueControl *pauseLateralToggle = static_cast<FrogPilotParamValueControl*>(toggles["PauseLateralSpeed"]);
  FrogPilotParamValueControl *setSpeedOffsetToggle = static_cast<FrogPilotParamValueControl*>(toggles["SetSpeedOffset"]);
  FrogPilotParamValueControl *stoppingDistanceToggle = static_cast<FrogPilotParamValueControl*>(toggles["StoppingDistance"]);

  if (isMetric) {
    offset1Toggle->setTitle(tr("Speed Limit Offset (0-34 kph)"));
    offset2Toggle->setTitle(tr("Speed Limit Offset (35-54 kph)"));
    offset3Toggle->setTitle(tr("Speed Limit Offset (55-64 kph)"));
    offset4Toggle->setTitle(tr("Speed Limit Offset (65-99 kph)"));

    offset1Toggle->setDescription(tr("Set speed limit offset for limits between 0-34 kph."));
    offset2Toggle->setDescription(tr("Set speed limit offset for limits between 35-54 kph."));
    offset3Toggle->setDescription(tr("Set speed limit offset for limits between 55-64 kph."));
    offset4Toggle->setDescription(tr("Set speed limit offset for limits between 65-99 kph."));

    ceSpeedToggle->updateControl(0, 150, tr("kph"));
    customCruiseToggle->updateControl(1, 150, tr("kph"));
    customCruiseLongToggle->updateControl(1, 150, tr("kph"));
    minimumLaneChangeSpeedToggle->updateControl(0, 150, tr("kph"));
    offset1Toggle->updateControl(-99, 99, tr("kph"));
    offset2Toggle->updateControl(-99, 99, tr("kph"));
    offset3Toggle->updateControl(-99, 99, tr("kph"));
    offset4Toggle->updateControl(-99, 99, tr("kph"));
    pauseAOLOnBrakeToggle->updateControl(0, 99, tr("kph"));
    pauseLateralToggle->updateControl(0, 99, tr("kph"));
    setSpeedOffsetToggle->updateControl(0, 150, tr("kph"));

    laneWidthToggle->updateControl(0, 30, tr(" meters"), 10);
    stoppingDistanceToggle->updateControl(0, 5, tr(" meters"));
  } else {
    offset1Toggle->setTitle(tr("Speed Limit Offset (0-34 mph)"));
    offset2Toggle->setTitle(tr("Speed Limit Offset (35-54 mph)"));
    offset3Toggle->setTitle(tr("Speed Limit Offset (55-64 mph)"));
    offset4Toggle->setTitle(tr("Speed Limit Offset (65-99 mph)"));

    offset1Toggle->setDescription(tr("Set speed limit offset for limits between 0-34 mph."));
    offset2Toggle->setDescription(tr("Set speed limit offset for limits between 35-54 mph."));
    offset3Toggle->setDescription(tr("Set speed limit offset for limits between 55-64 mph."));
    offset4Toggle->setDescription(tr("Set speed limit offset for limits between 65-99 mph."));

    ceSpeedToggle->updateControl(0, 99, tr("mph"));
    customCruiseToggle->updateControl(1, 99, tr("mph"));
    customCruiseLongToggle->updateControl(1, 99, tr("mph"));
    minimumLaneChangeSpeedToggle->updateControl(0, 99, tr("mph"));
    offset1Toggle->updateControl(-99, 99, tr("mph"));
    offset2Toggle->updateControl(-99, 99, tr("mph"));
    offset3Toggle->updateControl(-99, 99, tr("mph"));
    offset4Toggle->updateControl(-99, 99, tr("mph"));
    pauseAOLOnBrakeToggle->updateControl(0, 99, tr("mph"));
    pauseLateralToggle->updateControl(0, 99, tr("mph"));
    setSpeedOffsetToggle->updateControl(0, 99, tr("mph"));

    laneWidthToggle->updateControl(0, 100, tr(" feet"), 10);
    stoppingDistanceToggle->updateControl(0, 10, tr(" feet"));
  }

  ceSpeedToggle->refresh();
  customCruiseToggle->refresh();
  customCruiseLongToggle->refresh();
  laneWidthToggle->refresh();
  minimumLaneChangeSpeedToggle->refresh();
  offset1Toggle->refresh();
  offset2Toggle->refresh();
  offset3Toggle->refresh();
  offset4Toggle->refresh();
  pauseAOLOnBrakeToggle->refresh();
  pauseLateralToggle->refresh();
  setSpeedOffsetToggle->refresh();
  stoppingDistanceToggle->refresh();
}

void FrogPilotControlsPanel::startDownloadAllModels() {
  allModelsDownloading = true;
  modelDownloading = true;

  paramsMemory.putBool("DownloadAllModels", true);

  downloadAllModelsBtn->setValue(tr("Downloading models..."));

  QTimer *progressTimer = new QTimer(this);
  progressTimer->setInterval(100);

  QObject::connect(progressTimer, &QTimer::timeout, this, [=]() {
    QString progress = QString::fromStdString(paramsMemory.get("ModelDownloadProgress"));
    bool downloadComplete = progress.contains(QRegularExpression("All models downloaded!", QRegularExpression::CaseInsensitiveOption));
    bool downloadFailed = progress.contains(QRegularExpression("cancelled|exists|failed|offline", QRegularExpression::CaseInsensitiveOption));

    if (!progress.isEmpty() && progress != "0%") {
      downloadAllModelsBtn->setValue(progress);
    }

    if (downloadComplete || downloadFailed) {
      if (downloadComplete) {
        haveModelsDownloaded = true;
        update();
      }

      downloadAllModelsBtn->setValue(progress);

      paramsMemory.remove("CancelModelDownload");
      paramsMemory.remove("ModelDownloadProgress");

      progressTimer->stop();
      progressTimer->deleteLater();

      QTimer::singleShot(2000, this, [=]() {
        cancellingDownload = false;
        modelDownloading = false;

        paramsMemory.remove("DownloadAllModels");

        downloadAllModelsBtn->setValue("");
      });
    }
  });
  progressTimer->start();
}

QString FrogPilotControlsPanel::processModelName(const QString &modelName) {
  QString modelCleaned = modelName;
  modelCleaned = modelCleaned.remove(QRegularExpression("[🗺️👀📡]")).simplified();
  QString scoreParam = modelCleaned.remove(QRegularExpression("[^a-zA-Z0-9()-]")).replace(" ", "").simplified();
  scoreParam = scoreParam.replace("(Default)", "").replace("-", "");
  return scoreParam;
}

void FrogPilotControlsPanel::updateCalibrationDescription() {
  QString model = QString::fromStdString(params.get("ModelName"));
  QString part_model_param = processModelName(model);

  QString desc =
      tr("openpilot requires the device to be mounted within 4° left or right and "
         "within 5° up or 9° down. openpilot is continuously calibrating, resetting is rarely required.");
  std::string calib_bytes = params.get(part_model_param.toStdString() + "CalibrationParams");
  if (!calib_bytes.empty()) {
    try {
      AlignedBuffer aligned_buf;
      capnp::FlatArrayMessageReader cmsg(aligned_buf.align(calib_bytes.data(), calib_bytes.size()));
      auto calib = cmsg.getRoot<cereal::Event>().getLiveCalibration();
      if (calib.getCalStatus() != cereal::LiveCalibrationData::Status::UNCALIBRATED) {
        double pitch = calib.getRpyCalib()[1] * (180 / M_PI);
        double yaw = calib.getRpyCalib()[2] * (180 / M_PI);
        desc += tr(" Your device is pointed %1° %2 and %3° %4.")
                    .arg(QString::number(std::abs(pitch), 'g', 1), pitch > 0 ? tr("down") : tr("up"),
                         QString::number(std::abs(yaw), 'g', 1), yaw > 0 ? tr("left") : tr("right"));
      }
    } catch (kj::Exception) {
      qInfo() << "invalid CalibrationParams";
    }
  }
  qobject_cast<FrogPilotButtonsControl *>(sender())->setDescription(desc);
}

void FrogPilotControlsPanel::updateModelLabels() {
  QVector<QPair<QString, int>> modelScores;
  availableModelNames = QString::fromStdString(params.get("AvailableModelsNames")).split(",");

  for (const QString &model : availableModelNames) {
    QString cleanedModel = processModelName(model);
    int score = params.getInt((cleanedModel + "Score").toStdString());

    if (model.contains("(Default)")) {
      modelScores.prepend(qMakePair(model, score));
    } else {
      modelScores.append(qMakePair(model, score));
    }
  }

  labelControls.clear();

  for (const auto &pair : modelScores) {
    QString scoreDisplay = pair.second == 0 ? "N/A" : QString::number(pair.second) + "%";
    LabelControl *labelControl = new LabelControl(pair.first, scoreDisplay, "", this);
    addItem(labelControl);
    labelControls.append(labelControl);
  }

  for (LabelControl *label : labelControls) {
    label->setVisible(false);
  }
}

void FrogPilotControlsPanel::hideToggles() {
  customPersonalitiesOpen = false;
  drivingPersonalitiesOpen = false;
  modelManagementOpen = false;
  slcOpen = false;

  for (LabelControl *label : labelControls) {
    label->setVisible(false);
  }

  std::set<QString> longitudinalKeys = {"ConditionalExperimental", "DrivingPersonalities", "ExperimentalModeActivation",
                                        "LongitudinalTune", "MTSCEnabled", "SpeedLimitController", "VisionTurnControl"};

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(false);

    if ((!hasOpenpilotLongitudinal || disableOpenpilotLongitudinal) && longitudinalKeys.find(key.c_str()) != longitudinalKeys.end()) {
      continue;
    }

    bool subToggles = aggressivePersonalityKeys.find(key.c_str()) != aggressivePersonalityKeys.end() ||
                      aolKeys.find(key.c_str()) != aolKeys.end() ||
                      conditionalExperimentalKeys.find(key.c_str()) != conditionalExperimentalKeys.end() ||
                      customdrivingPersonalityKeys.find(key.c_str()) != customdrivingPersonalityKeys.end() ||
                      deviceManagementKeys.find(key.c_str()) != deviceManagementKeys.end() ||
                      drivingPersonalityKeys.find(key.c_str()) != drivingPersonalityKeys.end() ||
                      experimentalModeActivationKeys.find(key.c_str()) != experimentalModeActivationKeys.end() ||
                      laneChangeKeys.find(key.c_str()) != laneChangeKeys.end() ||
                      lateralTuneKeys.find(key.c_str()) != lateralTuneKeys.end() ||
                      longitudinalTuneKeys.find(key.c_str()) != longitudinalTuneKeys.end() ||
                      modelManagementKeys.find(key.c_str()) != modelManagementKeys.end() ||
                      modelRandomizerKeys.find(key.c_str()) != modelRandomizerKeys.end() ||
                      mtscKeys.find(key.c_str()) != mtscKeys.end() ||
                      qolKeys.find(key.c_str()) != qolKeys.end() ||
                      relaxedPersonalityKeys.find(key.c_str()) != relaxedPersonalityKeys.end() ||
                      speedLimitControllerKeys.find(key.c_str()) != speedLimitControllerKeys.end() ||
                      speedLimitControllerControlsKeys.find(key.c_str()) != speedLimitControllerControlsKeys.end() ||
                      speedLimitControllerQOLKeys.find(key.c_str()) != speedLimitControllerQOLKeys.end() ||
                      speedLimitControllerVisualsKeys.find(key.c_str()) != speedLimitControllerVisualsKeys.end() ||
                      standardPersonalityKeys.find(key.c_str()) != standardPersonalityKeys.end() ||
                      trafficPersonalityKeys.find(key.c_str()) != trafficPersonalityKeys.end() ||
                      visionTurnControlKeys.find(key.c_str()) != visionTurnControlKeys.end();
    toggle->setVisible(!subToggles);
  }

  update();
}

void FrogPilotControlsPanel::hideSubToggles() {
  if (customPersonalitiesOpen) {
    for (auto &[key, toggle] : toggles) {
      bool isVisible = drivingPersonalityKeys.find(key.c_str()) != drivingPersonalityKeys.end();
      toggle->setVisible(isVisible);
    }

    downloadStatusLabel->setVisible(onroadDistanceButton);
    manageDistanceIconsBtn->setVisible(onroadDistanceButton);
  } else if (slcOpen) {
    for (auto &[key, toggle] : toggles) {
      bool isVisible = speedLimitControllerKeys.find(key.c_str()) != speedLimitControllerKeys.end();
      toggle->setVisible(isVisible);
    }
  } else if (modelManagementOpen) {
    for (LabelControl *label : labelControls) {
      label->setVisible(false);
    }

    for (auto &[key, toggle] : toggles) {
      bool isVisible = modelManagementKeys.find(key.c_str()) != modelManagementKeys.end();
      toggle->setVisible(isVisible);
    }
  }

  update();
}

void FrogPilotControlsPanel::hideSubSubToggles() {
  if (customPersonalitiesOpen) {
    for (auto &[key, toggle] : toggles) {
      bool isVisible = customdrivingPersonalityKeys.find(key.c_str()) != customdrivingPersonalityKeys.end();
      toggle->setVisible(isVisible);
    }
  } else if (modelManagementOpen) {
    for (LabelControl *label : labelControls) {
      label->setVisible(false);
    }

    for (auto &[key, toggle] : toggles) {
      bool isVisible = modelRandomizerKeys.find(key.c_str()) != modelRandomizerKeys.end();
      toggle->setVisible(isVisible);
    }
  }

  update();
}
