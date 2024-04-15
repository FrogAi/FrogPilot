#include <filesystem>
#include <iostream>

#include "selfdrive/frogpilot/ui/qt/offroad/control_settings.h"

namespace fs = std::filesystem;

bool checkCommaNNFFSupport(const std::string &carFingerprint) {
  const std::string filePath = "../car/torque_data/neural_ff_weights.json";

  if (!std::filesystem::exists(filePath)) {
    return false;
  }

  std::ifstream file(filePath);
  std::string line;
  while (std::getline(file, line)) {
    if (line.find(carFingerprint) != std::string::npos) {
      std::cout << "comma's NNFF supports fingerprint: " << carFingerprint << std::endl;
      return true;
    }
  }

  return false;
}

bool checkNNFFLogFileExists(const std::string &carFingerprint) {
  const fs::path dirPath("../car/torque_data/lat_models");

  if (!fs::exists(dirPath)) {
    std::cerr << "Directory does not exist: " << fs::absolute(dirPath) << std::endl;
    return false;
  }

  for (const auto &entry : fs::directory_iterator(dirPath)) {
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
    {"AlwaysOnLateralMain", tr("Enable On Cruise Main"), tr("Enable 'Always On Lateral' by clicking your 'Cruise Control' button without requring openpilot to be enabled first."), ""},
    {"PauseAOLOnBrake", tr("Pause On Brake"), tr("Pause 'Always On Lateral' when the brake pedal is being pressed below the set speed."), ""},
    {"HideAOLStatusBar", tr("Hide the Status Bar"), tr("Don't use the status bar for 'Always On Lateral'."), ""},

    {"ConditionalExperimental", tr("Conditional Experimental Mode"), tr("Automatically switches to 'Experimental Mode' under predefined conditions."), "../frogpilot/assets/toggle_icons/icon_conditional.png"},
    {"CECurves", tr("Curve Detected Ahead"), tr("Switch to 'Experimental Mode' when a curve is detected."), ""},
    {"CENavigation", tr("Navigation Based"), tr("Switch to 'Experimental Mode' based on navigation data. (i.e. Intersections, stop signs, upcoming turns, etc.)"), ""},
    {"CESlowerLead", tr("Slower Lead Detected Ahead"), tr("Switch to 'Experimental Mode' when a slower lead vehicle is detected ahead."), ""},
    {"CEStopLights", tr("Stop Lights and Stop Signs"), tr("Switch to 'Experimental Mode' when a stop light or stop sign is detected."), ""},
    {"CESignal", tr("Turn Signal When Below Highway Speeds"), tr("Switch to 'Experimental Mode' when using turn signals below highway speeds to help assist with turns."), ""},
    {"HideCEMStatusBar", tr("Hide the Status Bar"), tr("Don't use the status bar for 'Conditional Experimental Mode'."), ""},

    {"CustomPersonalities", tr("Custom Driving Personalities"), tr("Customize the driving personality profiles to your driving style."), "../frogpilot/assets/toggle_icons/icon_custom.png"},

    {"DeviceManagement", tr("Device Management"), tr("Tweak your device's behaviors to your personal preferences."), "../frogpilot/assets/toggle_icons/icon_device.png"},
    {"DeviceShutdown", tr("Device Shutdown Timer"), tr("Configure how quickly the device shuts down after going offroad."), ""},
    {"NoLogging", tr("Disable Logging"), tr("Turn off all data tracking to enhance privacy or reduce thermal load."), ""},
    {"NoUploads", tr("Disable Uploads"), tr("Turn off all data uploads to comma's servers."), ""},
    {"HigherBitrate", tr("Higher Bitrate Recording"), tr("Increases the quality of the footage uploaded to comma connect."), ""},
    {"IncreaseThermalLimits", tr("Increase Thermal Safety Limit"), tr("Allow the device to run at a temperature above comma's recommended thermal limits."), ""},
    {"LowVoltageShutdown", tr("Low Voltage Shutdown Threshold"), tr("Automatically shut the device down when your battery reaches a specific voltage level to prevent killing your battery."), ""},
    {"OfflineMode", tr("Offline Mode"), tr("Allow the device to be offline indefinitely."), ""},

    {"ExperimentalModeActivation", tr("Experimental Mode Activation"), tr("Toggle Experimental Mode with either buttons on the steering wheel or the screen.\n\nOverrides 'Conditional Experimental Mode'."), "../assets/img_experimental_white.svg"},
    {"ExperimentalModeViaLKAS", tr("Double Clicking the LKAS Button"), tr("Enable/disable 'Experimental Mode' by double clicking the 'LKAS' button on your steering wheel."), ""},
    {"ExperimentalModeViaTap", tr("Double Tapping the Onroad UI"), tr("Enable/disable 'Experimental Mode' by double tapping the onroad UI within a 0.5 second time frame."), ""},
    {"ExperimentalModeViaDistance", tr("Long Pressing the Distance Button"), tr("Enable/disable 'Experimental Mode' by holding down the 'distance' button on your steering wheel for 0.5 seconds."), ""},

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
    {"AggressiveAcceleration", tr("Increase Acceleration Behind Faster Lead"), tr("Increase aggressiveness when following a faster lead."), ""},
    {"StoppingDistance", tr("Increase Stop Distance Behind Lead"), tr("Increase the stopping distance for a more comfortable stop from lead vehicles."), ""},
    {"LeadDetectionThreshold", tr("Lead Detection Threshold"), tr("Increase or decrease the lead detection threshold to either detect leads sooner, or increase model confidence."), ""},
    {"SmoothBraking", tr("Smoother Braking"), tr("Smoothen out the braking behavior when approaching slower vehicles."), ""},
    {"TrafficMode", tr("Traffic Mode"), tr("Enable the ability to activate 'Traffic Mode' by holding down the 'distance' button for 2.5 seconds. When 'Traffic Mode' is active the onroad UI will turn red and openpilot will drive catered towards stop and go traffic."), ""},

    {"MTSCEnabled", tr("Map Turn Speed Control"), tr("Slow down for anticipated curves detected by the downloaded maps."), "../frogpilot/assets/toggle_icons/icon_speed_map.png"},
    {"DisableMTSCSmoothing", tr("Disable MTSC UI Smoothing"), tr("Disables the smoothing for the requested speed in the onroad UI to show exactly what speed MTSC is currently requesting."), ""},
    {"MTSCCurvatureCheck",  tr("Model Curvature Detection Failsafe"), tr("Only trigger MTSC when the model detects a curve in the road. Purely used as a failsafe to prevent false positives. Leave this off if you never experience false positives."), ""},
    {"MTSCAggressiveness", tr("Turn Speed Aggressiveness"), tr("Set turn speed aggressiveness. Higher values result in faster turns, lower values yield gentler turns.\n\nA change of +- 1% results in the speed being raised or lowered by about 1 mph."), ""},

    {"ModelSelector", tr("Model Selector"), tr("Manage openpilot's driving models."), "../assets/offroad/icon_calibration.png"},

    {"NudgelessLaneChange", tr("Nudgeless Lane Change"), tr("Enable lane changes without requiring manual steering input."), "../frogpilot/assets/toggle_icons/icon_lane.png"},
    {"LaneChangeTime", tr("Lane Change Timer"), tr("Set a delay before executing a nudgeless lane change."), ""},
    {"LaneDetectionWidth", tr("Lane Detection Threshold"), tr("Set the required lane width to be qualified as a lane."), ""},
    {"OneLaneChange", tr("One Lane Change Per Signal"), tr("Only allow one nudgeless lane change per turn signal activation."), ""},

    {"QOLControls", tr("Quality of Life"), tr("Miscellaneous quality of life changes to improve your overall openpilot experience."), "../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"CustomCruise", tr("Cruise Increase Interval"), tr("Set a custom interval to increase the max set speed by."), ""},
    {"CustomCruiseLong", tr("Cruise Increase Interval (Long Press)"), tr("Set a custom interval to increase the max set speed by when holding down the cruise increase button."), ""},
    {"DisableOnroadUploads", tr("Disable Onroad Uploads"), tr("Prevent uploads to comma connect unless you're offroad and connected to Wi-Fi."), ""},
    {"OnroadDistanceButton", tr("Onroad Distance Button"), tr("Simulate a distance button via the onroad UI to control personalities, 'Experimental Mode', and 'Traffic Mode'."), ""},
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
    {"SLCQOL", tr("Quality of Life Settings"), tr("Manage toggles related to 'Speed Limit Controller's quality of life features."), ""},
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
    ParamControl *toggle;

    if (param == "AlwaysOnLateral") {
      FrogPilotParamManageControl *aolToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(aolToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(aolKeys.find(key.c_str()) != aolKeys.end());
        }
      });
      toggle = aolToggle;
    } else if (param == "PauseAOLOnBrake") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, std::map<int, QString>(), this, false, tr(" mph"));

    } else if (param == "ConditionalExperimental") {
      FrogPilotParamManageControl *conditionalExperimentalToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(conditionalExperimentalToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        conditionalSpeedsImperial->setVisible(!isMetric);
        conditionalSpeedsMetric->setVisible(isMetric);
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(conditionalExperimentalKeys.find(key.c_str()) != conditionalExperimentalKeys.end());
        }
      });
      toggle = conditionalExperimentalToggle;
    } else if (param == "CECurves") {
      FrogPilotParamValueControl *CESpeedImperial = new FrogPilotParamValueControl("CESpeed", tr("Below"), tr("Switch to 'Experimental Mode' below this speed when not following a lead vehicle."), "", 0, 99,
                                                                                   std::map<int, QString>(), this, false, tr(" mph"));
      FrogPilotParamValueControl *CESpeedLeadImperial = new FrogPilotParamValueControl("CESpeedLead", tr("  w/Lead"), tr("Switch to 'Experimental Mode' below this speed when following a lead vehicle."), "", 0, 99,
                                                                                       std::map<int, QString>(), this, false, tr(" mph"));
      conditionalSpeedsImperial = new FrogPilotDualParamControl(CESpeedImperial, CESpeedLeadImperial, this);
      addItem(conditionalSpeedsImperial);

      FrogPilotParamValueControl *CESpeedMetric = new FrogPilotParamValueControl("CESpeed", tr("Below"), tr("Switch to 'Experimental Mode' below this speed in absence of a lead vehicle."), "", 0, 150,
                                                                                 std::map<int, QString>(), this, false, tr(" kph"));
      FrogPilotParamValueControl *CESpeedLeadMetric = new FrogPilotParamValueControl("CESpeedLead", tr("  w/Lead"), tr("Switch to 'Experimental Mode' below this speed when following a lead vehicle."), "", 0, 150,
                                                                                     std::map<int, QString>(), this, false, tr(" kph"));
      conditionalSpeedsMetric = new FrogPilotDualParamControl(CESpeedMetric, CESpeedLeadMetric, this);
      addItem(conditionalSpeedsMetric);

      std::vector<QString> curveToggles{"CECurvesLead"};
      std::vector<QString> curveToggleNames{tr("With Lead")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, curveToggles, curveToggleNames);
    } else if (param == "CENavigation") {
      std::vector<QString> navigationToggles{"CENavigationIntersections", "CENavigationTurns", "CENavigationLead"};
      std::vector<QString> navigationToggleNames{tr("Intersections"), tr("Turns"), tr("With Lead")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, navigationToggles, navigationToggleNames);
    } else if (param == "CEStopLights") {
      std::vector<QString> stopLightToggles{"CEStopLightsLead"};
      std::vector<QString> stopLightToggleNames{tr("With Lead")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, stopLightToggles, stopLightToggleNames);

    } else if (param == "CustomPersonalities") {
      FrogPilotParamManageControl *customPersonalitiesToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(customPersonalitiesToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(false);
        }
        aggressiveProfile->setVisible(true);
        standardProfile->setVisible(true);
        relaxedProfile->setVisible(true);
        trafficProfile->setVisible(!isRelease && params.getBool("TrafficMode"));
      });
      toggle = customPersonalitiesToggle;

      FrogPilotParamValueControl *trafficFollow = new FrogPilotParamValueControl("TrafficFollow", tr("Follow"),
                                                                              tr("Set the minimum following distance when using 'Traffic Mode'. Your following distance will dynamically "
                                                                                 "adjust between this distance and the following distance from the 'Aggressive' profile when driving between "
                                                                                 "0 and %1.\n\nFor example:\n\nTraffic Mode: 0.5s\nAggressive: 1.0s\n\n0%2 = 0.5s\n%3 = 0.75s\n%1 = 1.0s")
                                                                                 .arg(isMetric ? "90ph" : "55mph")
                                                                                 .arg(isMetric ? "kph" : "mph")
                                                                                 .arg(isMetric ? "45kph" : "27.5mph"),
                                                                                 "../frogpilot/assets/other_images/traffic.png",
                                                                                 0.5, 5, std::map<int, QString>(), this, false, tr(" sec"), 1, 0.01);
      FrogPilotParamValueControl *trafficJerk = new FrogPilotParamValueControl("TrafficJerk", tr("Jerk"),
                                                                            tr("Set the minimum jerk value when using 'Traffic Mode'. Your jerk will dynamically "
                                                                               "adjust between this jerk and the jerk from the 'Aggressive' profile when driving between "
                                                                               "0 and %1.\n\nFor example:\n\nTraffic Mode: 0.1\nAggressive: 1.0\n\n0%2 = 0.1\n%3 = 0.5\n%1 = 1.0")
                                                                               .arg(isMetric ? "90ph" : "55mph")
                                                                               .arg(isMetric ? "kph" : "mph")
                                                                               .arg(isMetric ? "45kph" : "27.5mph"),
                                                                               "",
                                                                               0.01, 5, std::map<int, QString>(), this, false, "", 1, 0.01);
      trafficProfile = new FrogPilotDualParamControl(trafficFollow, trafficJerk, this, true);
      addItem(trafficProfile);

      FrogPilotParamValueControl *aggressiveFollow = new FrogPilotParamValueControl("AggressiveFollow", tr("Follow"),
                                                                                 tr("Set the 'Aggressive' personality following distance. "
                                                                                    "Represents seconds to follow behind the lead vehicle.\n\nStock: 1.25 seconds."),
                                                                                    "../frogpilot/assets/other_images/aggressive.png",
                                                                                    1, 5, std::map<int, QString>(), this, false, tr(" sec"), 1, 0.01);
      FrogPilotParamValueControl *aggressiveJerk = new FrogPilotParamValueControl("AggressiveJerk", tr("Jerk"),
                                                                               tr("Adjust brake/gas pedal responsiveness for the 'Aggressive' personality. "
                                                                                  "Higher jerk value = Less likely to use the gas/brake.\nLower jerk value = More likely.\n\nStock: 0.5."),
                                                                                  "",
                                                                                  0.01, 5, std::map<int, QString>(), this, false, "", 1, 0.01);
      aggressiveProfile = new FrogPilotDualParamControl(aggressiveFollow, aggressiveJerk, this, true);
      addItem(aggressiveProfile);

      FrogPilotParamValueControl *standardFollow = new FrogPilotParamValueControl("StandardFollow", tr("Follow"),
                                                                               tr("Set the 'Standard' personality following distance. "
                                                                                  "Represents seconds to follow behind the lead vehicle.\n\nStock: 1.45 seconds."),
                                                                                  "../frogpilot/assets/other_images/standard.png",
                                                                                  1, 5, std::map<int, QString>(), this, false, tr(" sec"), 1, 0.01);
      FrogPilotParamValueControl *standardJerk = new FrogPilotParamValueControl("StandardJerk", tr("Jerk"),
                                                                             tr("Adjust brake/gas pedal responsiveness for the 'Standard' personality. "
                                                                                "Higher jerk value = Less likely to use the gas/brake.\nLower jerk value = More likely.\n\nStock: 1.0."),
                                                                                "",
                                                                                0.01, 5, std::map<int, QString>(), this, false, "", 1, 0.01);
      standardProfile = new FrogPilotDualParamControl(standardFollow, standardJerk, this, true);
      addItem(standardProfile);

      FrogPilotParamValueControl *relaxedFollow = new FrogPilotParamValueControl("RelaxedFollow", tr("Follow"),
                                                                              tr("Set the 'Relaxed' personality following distance. "
                                                                                 "Represents seconds to follow behind the lead vehicle.\n\nStock: 1.75 seconds."),
                                                                                 "../frogpilot/assets/other_images/relaxed.png",
                                                                                 1, 5, std::map<int, QString>(), this, false, tr(" sec"), 1, 0.01);
      FrogPilotParamValueControl *relaxedJerk = new FrogPilotParamValueControl("RelaxedJerk", tr("Jerk"),
                                                                            tr("Adjust brake/gas pedal responsiveness for the 'Relaxed' personality. "
                                                                               "Higher jerk value = Less likely to use the gas/brake.\nLower jerk value = More likely.\n\nStock: 1.0."),
                                                                               "",
                                                                               0.01, 5, std::map<int, QString>(), this, false, "", 1, 0.01);
      relaxedProfile = new FrogPilotDualParamControl(relaxedFollow, relaxedJerk, this, true);
      addItem(relaxedProfile);

    } else if (param == "DeviceManagement") {
      FrogPilotParamManageControl *deviceManagementToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(deviceManagementToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(deviceManagementKeys.find(key.c_str()) != deviceManagementKeys.end());
        }
      });
      toggle = deviceManagementToggle;
    } else if (param == "DeviceShutdown") {
      std::map<int, QString> shutdownLabels;
      for (int i = 0; i <= 33; ++i) {
        shutdownLabels[i] = i == 0 ? tr("5 mins") : i <= 3 ? QString::number(i * 15) + tr(" mins") : QString::number(i - 3) + (i == 4 ? tr(" hour") : tr(" hours"));
      }
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 33, shutdownLabels, this, false);
    } else if (param == "LowVoltageShutdown") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 11.8, 12.5, std::map<int, QString>(), this, false, tr(" volts"), 1, 0.01);

    } else if (param == "ExperimentalModeActivation") {
      FrogPilotParamManageControl *experimentalModeActivationToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(experimentalModeActivationToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(experimentalModeActivationKeys.find(key.c_str()) != experimentalModeActivationKeys.end());
        }
      });
      toggle = experimentalModeActivationToggle;

    } else if (param == "LateralTune") {
      FrogPilotParamManageControl *lateralTuneToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(lateralTuneToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedLateralTuneKeys = lateralTuneKeys;

          if (hasAutoTune || params.getBool("LateralTune") && params.getBool("NNFF")) {
            modifiedLateralTuneKeys.erase("ForceAutoTune");
          }

          if (hasCommaNNFFSupport) {
            modifiedLateralTuneKeys.erase("NNFF");
            modifiedLateralTuneKeys.erase("NNFFLite");
          } else if (hasNNFFLog) {
            modifiedLateralTuneKeys.erase("NNFFLite");
          } else {
            modifiedLateralTuneKeys.erase("NNFF");
          }

          if (isRelease ) {
            modifiedLateralTuneKeys.erase("TacoTune");
          }

          toggle->setVisible(modifiedLateralTuneKeys.find(key.c_str()) != modifiedLateralTuneKeys.end());
        }
      });
      toggle = lateralTuneToggle;
    } else if (param == "SteerRatio") {
      std::vector<QString> steerRatioToggles{"ResetSteerRatio"};
      std::vector<QString> steerRatioToggleNames{"Reset"};
      toggle = new FrogPilotParamValueToggleControl(param, title, desc, icon, steerRatioStock * 0.75, steerRatioStock * 1.25, std::map<int, QString>(), this, false, "", 1, 0.01, steerRatioToggles, steerRatioToggleNames);

    } else if (param == "LongitudinalTune") {
      FrogPilotParamManageControl *longitudinalTuneToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(longitudinalTuneToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedLongitudinalTuneKeys = longitudinalTuneKeys;

          if (params.get("Model") == "radical-turtle") {
            modifiedLongitudinalTuneKeys.erase("LeadDetectionThreshold");
          }

          toggle->setVisible(modifiedLongitudinalTuneKeys.find(key.c_str()) != modifiedLongitudinalTuneKeys.end());
        }
      });
      toggle = longitudinalTuneToggle;
    } else if (param == "AccelerationProfile") {
      std::vector<QString> profileOptions{tr("Standard"), tr("Eco"), tr("Sport"), tr("Sport+")};
      FrogPilotButtonParamControl *profileSelection = new FrogPilotButtonParamControl(param, title, desc, icon, profileOptions);
      toggle = profileSelection;

      QObject::connect(static_cast<FrogPilotButtonParamControl*>(toggle), &FrogPilotButtonParamControl::buttonClicked, [this](int id) {
        if (id == 3) {
          FrogPilotConfirmationDialog::toggleAlert(tr("WARNING: This maxes out openpilot's acceleration from 2.0 m/s to 4.0 m/s and may cause oscillations when accelerating!"),
          tr("I understand the risks."), this);
        }
      });
    } else if (param == "DecelerationProfile") {
      std::vector<QString> profileOptions{tr("Standard"), tr("Eco"), tr("Sport")};
      FrogPilotButtonParamControl *profileSelection = new FrogPilotButtonParamControl(param, title, desc, icon, profileOptions);
      toggle = profileSelection;
    } else if (param == "StoppingDistance") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, std::map<int, QString>(), this, false, tr(" feet"));
    } else if (param == "LeadDetectionThreshold") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 100, std::map<int, QString>(), this, false, "%");
    } else if (param == "SmoothBraking" && !isRelease) {
      std::vector<QString> brakingToggles{"SmoothBrakingJerk", "SmoothBrakingFarLead"};
      std::vector<QString> brakingToggleNames{tr("Apply to Jerk"), tr("Far Lead Offset")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, brakingToggles, brakingToggleNames);
      QObject::connect(static_cast<FrogPilotParamToggleControl*>(toggle), &FrogPilotParamToggleControl::buttonClicked, [this](bool checked) {
        if (checked) {
          FrogPilotConfirmationDialog::toggleAlert(
          tr("WARNING: This is very experimental and may cause the car to not brake or stop safely! Please report any issues in the FrogPilot Discord!"),
          tr("I understand the risks."), this);
        }
      });

    } else if (param == "MTSCEnabled") {
      FrogPilotParamManageControl *mtscToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(mtscToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(mtscKeys.find(key.c_str()) != mtscKeys.end());
        }
      });
      toggle = mtscToggle;
    } else if (param == "MTSCAggressiveness") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 200, std::map<int, QString>(), this, false, "%");

    } else if (param == "ModelSelector") {
      FrogPilotParamManageControl *modelsToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(modelsToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(false);
        }

        deleteModelBtn->setVisible(true);
        downloadModelBtn->setVisible(true);
        selectModelBtn->setVisible(true);
      });
      toggle = modelsToggle;

      QDir modelDir("/data/models/");

      deleteModelBtn = new ButtonControl(tr("Delete Model"), tr("DELETE"), "");
      QObject::connect(deleteModelBtn, &ButtonControl::clicked, [=]() {
        std::string currentModel = params.get("Model") + ".thneed";

        QStringList availableModels = QString::fromStdString(params.get("AvailableModels")).split(",");
        QStringList modelLabels = QString::fromStdString(params.get("AvailableModelsNames")).split(",");

        QStringList existingModelFiles = modelDir.entryList({"*.thneed"}, QDir::Files);
        QMap<QString, QString> labelToFileMap;
        QStringList deletableModelLabels;
        for (int i = 0; i < availableModels.size(); ++i) {
          QString modelFileName = availableModels[i] + ".thneed";
          if (existingModelFiles.contains(modelFileName) && modelFileName != QString::fromStdString(currentModel)) {
            QString readableName = modelLabels[i];
            deletableModelLabels.append(readableName);
            labelToFileMap[readableName] = modelFileName;
          }
        }

        QString selectedModel = MultiOptionDialog::getSelection(tr("Select a model to delete"), deletableModelLabels, "", this);
        if (!selectedModel.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete this model?"), tr("Delete"), this)) {
          std::thread([=]() {
            deleteModelBtn->setValue(tr("Deleting..."));

            deleteModelBtn->setEnabled(false);
            downloadModelBtn->setEnabled(false);
            selectModelBtn->setEnabled(false);

            QString modelToDelete = labelToFileMap[selectedModel];

            QFile::remove(modelDir.absoluteFilePath(modelToDelete));

            deleteModelBtn->setEnabled(true);
            downloadModelBtn->setEnabled(true);
            selectModelBtn->setEnabled(true);

            deleteModelBtn->setValue(tr("Deleted!"));
            std::this_thread::sleep_for(std::chrono::seconds(3));
            deleteModelBtn->setValue("");
          }).detach();
        }
      });
      addItem(deleteModelBtn);

      downloadModelBtn = new ButtonControl(tr("Download Model"), tr("DOWNLOAD"), "");
      QObject::connect(downloadModelBtn, &ButtonControl::clicked, [=]() {
        QStringList availableModels = QString::fromStdString(params.get("AvailableModels")).split(",");
        QStringList modelLabels = QString::fromStdString(params.get("AvailableModelsNames")).split(",");

        QMap<QString, QString> labelToModelMap;
        QStringList downloadableModelLabels;
        QStringList existingModelFiles = modelDir.entryList({"*.thneed"}, QDir::Files);
        for (int i = 0; i < availableModels.size(); ++i) {
          QString modelFileName = availableModels.at(i) + ".thneed";
          if (!existingModelFiles.contains(modelFileName)) {
            QString readableName = modelLabels.at(i);
            if (!readableName.endsWith("(Default)")) {
              downloadableModelLabels.append(readableName);
              labelToModelMap.insert(readableName, availableModels.at(i));
            }
          }
        }

        QString modelToDownload = MultiOptionDialog::getSelection(tr("Select a driving model to download"), downloadableModelLabels, "", this);
        if (!modelToDownload.isEmpty()) {
          QString selectedModelValue = labelToModelMap.value(modelToDownload);
          paramsMemory.put("ModelToDownload", selectedModelValue.toStdString());

          deleteModelBtn->setEnabled(false);
          downloadModelBtn->setEnabled(false);
          selectModelBtn->setEnabled(false);

          QTimer *failureTimer = new QTimer(this);
          failureTimer->setSingleShot(true);

          QTimer *progressTimer = new QTimer(this);
          progressTimer->setInterval(100);

          connect(failureTimer, &QTimer::timeout, this, [=]() {
            deleteModelBtn->setEnabled(true);
            downloadModelBtn->setEnabled(true);
            selectModelBtn->setEnabled(true);

            downloadModelBtn->setValue(tr("Download failed..."));
            paramsMemory.remove("ModelDownloadProgress");
            paramsMemory.remove("ModelToDownload");

            progressTimer->stop();
            progressTimer->deleteLater();

            QTimer::singleShot(3000, this, [this]() {
              downloadModelBtn->setValue("");
            });
          });

          connect(progressTimer, &QTimer::timeout, this, [=]() mutable {
            static int lastProgress = -1;
            int progress = paramsMemory.getInt("ModelDownloadProgress");

            if (progress == lastProgress) {
              if (!failureTimer->isActive()) {
                failureTimer->start(30000);
              }
            } else {
              lastProgress = progress;
              downloadModelBtn->setValue(QString::number(progress) + "%");
              failureTimer->stop();

              if (progress == 100) {
                deleteModelBtn->setEnabled(true);
                downloadModelBtn->setEnabled(true);
                selectModelBtn->setEnabled(true);

                downloadModelBtn->setValue(tr("Downloaded!"));
                paramsMemory.remove("ModelDownloadProgress");
                paramsMemory.remove("ModelToDownload");

                progressTimer->stop();
                progressTimer->deleteLater();

                QTimer::singleShot(3000, this, [this]() {
                  if (paramsMemory.get("ModelDownloadProgress").empty()) {
                    downloadModelBtn->setValue("");
                  }
                });
              }
            }
          });
          progressTimer->start();
        }
      });
      addItem(downloadModelBtn);

      selectModelBtn = new ButtonControl(tr("Select Model"), tr("SELECT"), "");
      QObject::connect(selectModelBtn, &ButtonControl::clicked, [=]() {
        QStringList availableModels = QString::fromStdString(params.get("AvailableModels")).split(",");
        QStringList modelLabels = QString::fromStdString(params.get("AvailableModelsNames")).split(",");

        QStringList modelFiles = modelDir.entryList({"*.thneed"}, QDir::Files);
        QSet<QString> modelFilesBaseNames;
        for (const QString &modelFile : modelFiles) {
          modelFilesBaseNames.insert(modelFile.section('.', 0, 0));
        }

        QStringList selectableModelLabels;
        for (int i = 0; i < availableModels.size(); ++i) {
          if (modelFilesBaseNames.contains(availableModels[i]) || modelLabels[i].endsWith("(Default)")) {
            selectableModelLabels.append(modelLabels[i]);
          }
        }

        QString modelToSelect = MultiOptionDialog::getSelection(tr("Select a model"), selectableModelLabels, "", this);
        if (!modelToSelect.isEmpty()) {
          selectModelBtn->setValue(modelToSelect);

          int modelIndex = modelLabels.indexOf(modelToSelect);
          if (modelIndex != -1) {
            QString selectedModel = availableModels.at(modelIndex);
            params.put("Model", selectedModel.toStdString());
            params.put("ModelName", modelToSelect.toStdString());
          }

          if (FrogPilotConfirmationDialog::yesorno(tr("Do you want to start with a fresh calibration for the newly selected model?"), this)) {
            params.remove("CalibrationParams");
            params.remove("LiveTorqueParameters");
          }
        }
      });
      addItem(selectModelBtn);
      selectModelBtn->setValue(QString::fromStdString(params.get("ModelName")));

    } else if (param == "QOLControls") {
      FrogPilotParamManageControl *qolToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(qolToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedQolKeys = qolKeys;

          if (!hasOpenpilotLongitudinal) {
            modifiedQolKeys.erase("OnroadDistanceButton");
          }

          if (!hasPCMCruise) {
            modifiedQolKeys.erase("ReverseCruise");
          } else {
            modifiedQolKeys.erase("CustomCruise");
            modifiedQolKeys.erase("CustomCruiseLong");
            modifiedQolKeys.erase("SetSpeedOffset");
          }

          toggle->setVisible(modifiedQolKeys.find(key.c_str()) != modifiedQolKeys.end());
        }
      });
      toggle = qolToggle;
    } else if (param == "CustomCruise") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, std::map<int, QString>(), this, false, tr(" mph"));
    } else if (param == "CustomCruiseLong") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, std::map<int, QString>(), this, false, tr(" mph"));
    } else if (param == "OnroadDistanceButton") {
      std::vector<QString> onroadDistanceToggles{"KaofuiIcons"};
      std::vector<QString> onroadDistanceToggleNames{tr("Kaofui's Icons")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, onroadDistanceToggles, onroadDistanceToggleNames);
    } else if (param == "PauseLateralSpeed") {
      std::vector<QString> pauseLateralToggles{"PauseLateralOnSignal"};
      std::vector<QString> pauseLateralToggleNames{"Turn Signal Only"};
      toggle = new FrogPilotParamValueToggleControl(param, title, desc, icon, 0, 99, std::map<int, QString>(), this, false, tr(" mph"), 1, 1, pauseLateralToggles, pauseLateralToggleNames);
    } else if (param == "PauseLateralOnSignal") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, std::map<int, QString>(), this, false, tr(" mph"));
    } else if (param == "ReverseCruise") {
      std::vector<QString> reverseCruiseToggles{"ReverseCruiseUI"};
      std::vector<QString> reverseCruiseNames{tr("Control Via UI")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, reverseCruiseToggles, reverseCruiseNames);
    } else if (param == "SetSpeedOffset") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, std::map<int, QString>(), this, false, tr(" mph"));

    } else if (param == "NudgelessLaneChange") {
      FrogPilotParamManageControl *laneChangeToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(laneChangeToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(laneChangeKeys.find(key.c_str()) != laneChangeKeys.end());
        }
      });
      toggle = laneChangeToggle;
    } else if (param == "LaneChangeTime") {
      std::map<int, QString> laneChangeTimeLabels;
      for (int i = 0; i <= 10; ++i) {
        laneChangeTimeLabels[i] = i == 0 ? "Instant" : QString::number(i / 2.0) + " seconds";
      }
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, laneChangeTimeLabels, this, false);
    } else if (param == "LaneDetectionWidth") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, std::map<int, QString>(), this, false, " feet", 10);

    } else if (param == "SpeedLimitController") {
      FrogPilotParamManageControl *speedLimitControllerToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(speedLimitControllerToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(speedLimitControllerKeys.find(key.c_str()) != speedLimitControllerKeys.end());
        }
      });
      toggle = speedLimitControllerToggle;
    } else if (param == "SLCControls") {
      FrogPilotParamManageControl *manageSLCControlsToggle = new FrogPilotParamManageControl(param, title, desc, icon, this, true);
      QObject::connect(manageSLCControlsToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(speedLimitControllerControlsKeys.find(key.c_str()) != speedLimitControllerControlsKeys.end());
          openSubParentToggle();
        }
        slcPriorityButton->setVisible(true);
      });
      toggle = manageSLCControlsToggle;
    } else if (param == "SLCQOL") {
      FrogPilotParamManageControl *manageSLCQOLToggle = new FrogPilotParamManageControl(param, title, desc, icon, this, true);
      QObject::connect(manageSLCQOLToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedSpeedLimitControllerQOLKeys = speedLimitControllerQOLKeys;

          if (hasPCMCruise) {
            modifiedSpeedLimitControllerQOLKeys.erase("SetSpeedLimit");
          }

          if (!isToyota) {
            modifiedSpeedLimitControllerQOLKeys.erase("ForceMPHDashboard");
          }

          toggle->setVisible(modifiedSpeedLimitControllerQOLKeys.find(key.c_str()) != modifiedSpeedLimitControllerQOLKeys.end());
          openSubParentToggle();
        }
      });
      toggle = manageSLCQOLToggle;
    } else if (param == "SLCConfirmation") {
      std::vector<QString> slcConfirmationToggles{"SLCConfirmationLower", "SLCConfirmationHigher"};
      std::vector<QString> slcConfirmationNames{tr("Lower Limits"), tr("Higher Limits")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, slcConfirmationToggles, slcConfirmationNames);
    } else if (param == "SLCLookaheadHigher" || param == "SLCLookaheadLower") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 60, std::map<int, QString>(), this, false, " seconds");
    } else if (param == "SLCVisuals") {
      FrogPilotParamManageControl *manageSLCVisualsToggle = new FrogPilotParamManageControl(param, title, desc, icon, this, true);
      QObject::connect(manageSLCVisualsToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(speedLimitControllerVisualsKeys.find(key.c_str()) != speedLimitControllerVisualsKeys.end());
          openSubParentToggle();
        }
      });
      toggle = manageSLCVisualsToggle;
    } else if (param == "Offset1" || param == "Offset2" || param == "Offset3" || param == "Offset4") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, -99, 99, std::map<int, QString>(), this, false, tr(" mph"));
    } else if (param == "ShowSLCOffset") {
      std::vector<QString> slcOffsetToggles{"ShowSLCOffsetUI"};
      std::vector<QString> slcOffsetToggleNames{tr("Control Via UI")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, slcOffsetToggles, slcOffsetToggleNames);
    } else if (param == "SLCFallback") {
      std::vector<QString> fallbackOptions{tr("Set Speed"), tr("Experimental Mode"), tr("Previous Limit")};
      FrogPilotButtonParamControl *fallbackSelection = new FrogPilotButtonParamControl(param, title, desc, icon, fallbackOptions);
      toggle = fallbackSelection;
    } else if (param == "SLCOverride") {
      std::vector<QString> overrideOptions{tr("None"), tr("Manual Set Speed"), tr("Set Speed")};
      FrogPilotButtonParamControl *overrideSelection = new FrogPilotButtonParamControl(param, title, desc, icon, overrideOptions);
      toggle = overrideSelection;
    } else if (param == "SLCPriority") {
      slcPriorityButton = new ButtonControl(title, tr("SELECT"), desc);
      QStringList primaryPriorities = {tr("None"), tr("Dashboard"), tr("Navigation"), tr("Offline Maps"), tr("Highest"), tr("Lowest")};
      QStringList secondaryTertiaryPriorities = {tr("None"), tr("Dashboard"), tr("Navigation"), tr("Offline Maps")};
      QStringList priorityPrompts = {tr("Select your primary priority"), tr("Select your secondary priority"), tr("Select your tertiary priority")};

      QObject::connect(slcPriorityButton, &ButtonControl::clicked, [=]() {
        QStringList selectedPriorities;

        for (int i = 1; i <= 3; ++i) {
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

          params.put(priorityKey.toStdString(), selection.toStdString());
          selectedPriorities.append(selection);

          if (selection == tr("Lowest") || selection == tr("Highest") || selection == tr("None")) break;

          updateToggles();
        }

        selectedPriorities.removeAll(tr("None"));
        slcPriorityButton->setValue(selectedPriorities.join(", "));
      });

      QStringList initialPriorities;
      for (int i = 1; i <= 3; ++i) {
        QString priorityKey = QString("SLCPriority%1").arg(i);
        QString priority = QString::fromStdString(params.get(priorityKey.toStdString()));

        if (!priority.isEmpty() && primaryPriorities.contains(priority) && priority != tr("None")) {
          initialPriorities.append(priority);
        }
      }
      slcPriorityButton->setValue(initialPriorities.join(", "));
      addItem(slcPriorityButton);

    } else if (param == "VisionTurnControl") {
      FrogPilotParamManageControl *visionTurnControlToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(visionTurnControlToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(visionTurnControlKeys.find(key.c_str()) != visionTurnControlKeys.end());
        }
      });
      toggle = visionTurnControlToggle;
    } else if (param == "CurveSensitivity" || param == "TurnAggressiveness") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 200, std::map<int, QString>(), this, false, "%");

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

    QObject::connect(static_cast<FrogPilotParamManageControl*>(toggle), &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
      update();
    });
  }

  QObject::connect(toggles["AlwaysOnLateralMain"], &ToggleControl::toggleFlipped, [this]() {
    if (params.getBool("AlwaysOnLateralMain")) {
      FrogPilotConfirmationDialog::toggleAlert(
      tr("WARNING: This isn't guaranteed to work, so if you run into any issues, please report it in the FrogPilot Discord!"),
      tr("I understand the risks."), this);
    }
  });

  QObject::connect(toggles["IncreaseThermalLimits"], &ToggleControl::toggleFlipped, [this]() {
    if (params.getBool("IncreaseThermalLimits")) {
      FrogPilotConfirmationDialog::toggleAlert(
      tr("WARNING: This can cause premature wear or damage by running the device over comma's recommended temperature limits!"),
      tr("I understand the risks."), this);
    }
  });

  QObject::connect(toggles["NoLogging"], &ToggleControl::toggleFlipped, [this]() {
    if (params.getBool("NoLogging")) {
      FrogPilotConfirmationDialog::toggleAlert(
      tr("WARNING: This will prevent your drives from being recorded and the data will be unobtainable!"),
      tr("I understand the risks."), this);
    }
  });

  QObject::connect(toggles["NoUploads"], &ToggleControl::toggleFlipped, [this]() {
    if (params.getBool("NoUploads")) {
      FrogPilotConfirmationDialog::toggleAlert(
      tr("WARNING: This will prevent your drives from appearing on comma connect which may impact debugging and support!"),
      tr("I understand the risks."), this);
    }
  });

  std::set<QString> rebootKeys = {"AlwaysOnLateral", "AlwaysOnLateralMain", "HigherBitrate", "NNFF", "NNFFLite"};
  for (const QString &key : rebootKeys) {
    QObject::connect(toggles[key.toStdString().c_str()], &ToggleControl::toggleFlipped, [this]() {
      if (started) {
        if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
          Hardware::reboot();
        }
      }
    });
  }

  modelManagerToggle = static_cast<FrogPilotParamManageControl*>(toggles["ModelSelector"]);
  steerRatioToggle = static_cast<FrogPilotParamValueToggleControl*>(toggles["SteerRatio"]);

  QObject::connect(parent, &SettingsWindow::closeParentToggle, this, &FrogPilotControlsPanel::hideToggles);
  QObject::connect(parent, &SettingsWindow::closeSubParentToggle, this, &FrogPilotControlsPanel::hideSubToggles);
  QObject::connect(parent, &SettingsWindow::updateMetric, this, &FrogPilotControlsPanel::updateMetric);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &FrogPilotControlsPanel::updateCarToggles);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotControlsPanel::updateState);

  updateMetric();
}

void FrogPilotControlsPanel::showEvent(QShowEvent *event, const UIState &s) {
  hasOpenpilotLongitudinal = hasOpenpilotLongitudinal && !params.getBool("DisableOpenpilotLongitudinal");
}

void FrogPilotControlsPanel::updateState(const UIState &s) {
  if (!isVisible()) return;

  started = s.scene.started;

  if (params.getBool("ResetSteerRatio")) {
    params.putFloat("SteerRatio", steerRatioStock);
    params.putBool("ResetSteerRatio", false);
    steerRatioToggle->refresh();
  }

  downloadModelBtn->setEnabled(s.scene.online);
  modelManagerToggle->setEnabled(!s.scene.started);
}

void FrogPilotControlsPanel::updateToggles() {
  std::thread([this]() {
    paramsMemory.putBool("FrogPilotTogglesUpdated", true);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    paramsMemory.putBool("FrogPilotTogglesUpdated", false);
  }).detach();
}

void FrogPilotControlsPanel::updateCarToggles() {
  auto carParams = params.get("CarParamsPersistent");
  if (!carParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(carParams.data(), carParams.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();
    auto carFingerprint = CP.getCarFingerprint();
    auto carName = CP.getCarName();

    hasAutoTune = (carName == "hyundai" || carName == "toyota") && CP.getLateralTuning().which() == cereal::CarParams::LateralTuning::TORQUE;
    uiState()->scene.has_auto_tune = hasAutoTune;
    hasCommaNNFFSupport = checkCommaNNFFSupport(carFingerprint);
    hasDashSpeedLimits = carName == "hyundai" || carName == "toyota";
    hasNNFFLog = checkNNFFLogFileExists(carFingerprint);
    hasOpenpilotLongitudinal = CP.getOpenpilotLongitudinalControl() && !params.getBool("DisableOpenpilotLongitudinal");
    hasPCMCruise = CP.getPcmCruise();
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

    params.putIntNonBlocking("CESpeed", std::nearbyint(params.getInt("CESpeed") * speedConversion));
    params.putIntNonBlocking("CESpeedLead", std::nearbyint(params.getInt("CESpeedLead") * speedConversion));
    params.putIntNonBlocking("CustomCruise", std::nearbyint(params.getInt("CustomCruise") * speedConversion));
    params.putIntNonBlocking("CustomCruiseLong", std::nearbyint(params.getInt("CustomCruiseLong") * speedConversion));
    params.putIntNonBlocking("LaneDetectionWidth", std::nearbyint(params.getInt("LaneDetectionWidth") * distanceConversion));
    params.putIntNonBlocking("Offset1", std::nearbyint(params.getInt("Offset1") * speedConversion));
    params.putIntNonBlocking("Offset2", std::nearbyint(params.getInt("Offset2") * speedConversion));
    params.putIntNonBlocking("Offset3", std::nearbyint(params.getInt("Offset3") * speedConversion));
    params.putIntNonBlocking("Offset4", std::nearbyint(params.getInt("Offset4") * speedConversion));
    params.putIntNonBlocking("PauseAOLOnBrake", std::nearbyint(params.getInt("PauseAOLOnBrake") * speedConversion));
    params.putIntNonBlocking("PauseLateralOnSignal", std::nearbyint(params.getInt("PauseLateralOnSignal") * speedConversion));
    params.putIntNonBlocking("PauseLateralSpeed", std::nearbyint(params.getInt("PauseLateralSpeed") * speedConversion));
    params.putIntNonBlocking("SetSpeedOffset", std::nearbyint(params.getInt("SetSpeedOffset") * speedConversion));
    params.putIntNonBlocking("StoppingDistance", std::nearbyint(params.getInt("StoppingDistance") * distanceConversion));
  }

  FrogPilotParamValueControl *customCruiseToggle = static_cast<FrogPilotParamValueControl*>(toggles["CustomCruise"]);
  FrogPilotParamValueControl *customCruiseLongToggle = static_cast<FrogPilotParamValueControl*>(toggles["CustomCruiseLong"]);
  FrogPilotParamValueControl *laneWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["LaneDetectionWidth"]);
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

    customCruiseToggle->updateControl(1, 150, tr(" kph"));
    customCruiseLongToggle->updateControl(1, 150, tr(" kph"));
    laneWidthToggle->updateControl(0, 30, tr(" meters"), 10);
    offset1Toggle->updateControl(-99, 99, tr(" kph"));
    offset2Toggle->updateControl(-99, 99, tr(" kph"));
    offset3Toggle->updateControl(-99, 99, tr(" kph"));
    offset4Toggle->updateControl(-99, 99, tr(" kph"));
    pauseAOLOnBrakeToggle->updateControl(0, 99, tr(" kph"));
    pauseLateralToggle->updateControl(0, 99, tr(" kph"));
    setSpeedOffsetToggle->updateControl(0, 150, tr(" kph"));
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

    customCruiseToggle->updateControl(1, 99, tr(" mph"));
    customCruiseLongToggle->updateControl(1, 99, tr(" mph"));
    laneWidthToggle->updateControl(0, 100, tr(" feet"), 10);
    offset1Toggle->updateControl(-99, 99, tr(" mph"));
    offset2Toggle->updateControl(-99, 99, tr(" mph"));
    offset3Toggle->updateControl(-99, 99, tr(" mph"));
    offset4Toggle->updateControl(-99, 99, tr(" mph"));
    pauseAOLOnBrakeToggle->updateControl(0, 99, tr(" mph"));
    pauseLateralToggle->updateControl(0, 99, tr(" mph"));
    setSpeedOffsetToggle->updateControl(0, 99, tr(" mph"));
    stoppingDistanceToggle->updateControl(0, 10, tr(" feet"));
  }

  customCruiseToggle->refresh();
  customCruiseLongToggle->refresh();
  laneWidthToggle->refresh();
  offset1Toggle->refresh();
  offset2Toggle->refresh();
  offset3Toggle->refresh();
  offset4Toggle->refresh();
  pauseAOLOnBrakeToggle->refresh();
  pauseLateralToggle->refresh();
  setSpeedOffsetToggle->refresh();
  stoppingDistanceToggle->refresh();
}

void FrogPilotControlsPanel::hideToggles() {
  aggressiveProfile->setVisible(false);
  conditionalSpeedsImperial->setVisible(false);
  conditionalSpeedsMetric->setVisible(false);
  deleteModelBtn->setVisible(false);
  downloadModelBtn->setVisible(false);
  selectModelBtn->setVisible(false);
  slcPriorityButton->setVisible(false);
  standardProfile->setVisible(false);
  relaxedProfile->setVisible(false);
  trafficProfile->setVisible(false);

  std::set<QString> longitudinalKeys = {"ConditionalExperimental", "CustomPersonalities", "ExperimentalModeActivation",
                                        "LongitudinalTune", "MTSCEnabled", "SpeedLimitController", "VisionTurnControl"};

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(false);

    if (!hasOpenpilotLongitudinal && longitudinalKeys.find(key.c_str()) != longitudinalKeys.end()) {
      continue;
    }

    bool subToggles = aolKeys.find(key.c_str()) != aolKeys.end() ||
                      conditionalExperimentalKeys.find(key.c_str()) != conditionalExperimentalKeys.end() ||
                      experimentalModeActivationKeys.find(key.c_str()) != experimentalModeActivationKeys.end() ||
                      deviceManagementKeys.find(key.c_str()) != deviceManagementKeys.end() ||
                      laneChangeKeys.find(key.c_str()) != laneChangeKeys.end() ||
                      lateralTuneKeys.find(key.c_str()) != lateralTuneKeys.end() ||
                      longitudinalTuneKeys.find(key.c_str()) != longitudinalTuneKeys.end() ||
                      mtscKeys.find(key.c_str()) != mtscKeys.end() ||
                      qolKeys.find(key.c_str()) != qolKeys.end() ||
                      speedLimitControllerKeys.find(key.c_str()) != speedLimitControllerKeys.end() ||
                      speedLimitControllerControlsKeys.find(key.c_str()) != speedLimitControllerControlsKeys.end() ||
                      speedLimitControllerQOLKeys.find(key.c_str()) != speedLimitControllerQOLKeys.end() ||
                      speedLimitControllerVisualsKeys.find(key.c_str()) != speedLimitControllerVisualsKeys.end() ||
                      visionTurnControlKeys.find(key.c_str()) != visionTurnControlKeys.end();
    toggle->setVisible(!subToggles);
  }

  update();
}

void FrogPilotControlsPanel::hideSubToggles() {
  slcPriorityButton->setVisible(false);

  for (auto &[key, toggle] : toggles) {
    bool isVisible = speedLimitControllerKeys.find(key.c_str()) != speedLimitControllerKeys.end();
    toggle->setVisible(isVisible);
  }

  update();
}
