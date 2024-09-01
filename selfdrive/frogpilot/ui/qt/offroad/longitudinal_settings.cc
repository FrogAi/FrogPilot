#include "selfdrive/frogpilot/ui/qt/offroad/longitudinal_settings.h"

FrogPilotLongitudinalPanel::FrogPilotLongitudinalPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> longitudinalToggles {
    {"ConditionalExperimental", tr("Conditional Experimental Mode"), tr("Automatically switches to 'Experimental Mode' when specific conditions are met."), "../frogpilot/assets/toggle_icons/icon_conditional.png"},
    {"CESpeed", tr("Below"), tr("'Experimental Mode' is active when driving below the set speed without a lead vehicle."), ""},
    {"CECurves", tr("Curve Detected Ahead"), tr("'Experimental Mode' is active when a curve is detected in the road ahead."), ""},
    {"CELead", tr("Lead Detected Ahead"), tr("'Experimental Mode' is active when a slower or stopped vehicle is detected ahead."), ""},
    {"CENavigation", tr("Navigation Data"), tr("'Experimental Mode' is active based on navigation data, such as upcoming intersections or turns."), ""},
    {"CEModelStopTime", tr("openpilot Wants to Stop In"), tr("'Experimental Mode' is active when openpilot wants to stop such as for a stop sign or red light."), ""},
    {"CESignalSpeed", tr("Turn Signal Below"), tr("'Experimental Mode' is active when using turn signals below the set speed."), ""},
    {"HideCEMStatusBar", tr("Hide the Status Bar"), tr("The status bar for 'Conditional Experimental Mode' is hidden."), ""},

    {"CurveSpeedControl", tr("Curve Speed Control"), tr("Automatically slow down for curves detected ahead or through the downloaded maps."), "../frogpilot/assets/toggle_icons/icon_speed_map.png"},
    {"CurveDetectionMethod", tr("Curve Detection Method"), tr("The method used to detect curves."), ""},
    {"MTSCCurvatureCheck", tr("Curve Detection Failsafe"), tr("Curve control is triggered only when a curve is detected ahead. Use this as a failsafe to prevent false positives when using the 'Map Based' method."), ""},
    {"CurveSensitivity", tr("Curve Sensitivity"), tr("How sensitive openpilot is to detecting curves. Higher values trigger earlier responses at the risk of triggering too often, while lower values increase confidence at the risk of triggering too infrequently."), ""},
    {"TurnAggressiveness", tr("Turn Speed Aggressiveness"), tr("How aggressive openpilot takes turns. Higher values result in quicker turns, while lower values provide gentler turns."), ""},
    {"DisableCurveSpeedSmoothing", tr("Disable Speed Value Smoothing In the UI"), tr("Speed value smoothing is disabled in the UI to instead display the exact speed requested by the curve control."), ""},

    {"ExperimentalModeActivation", tr("Experimental Mode Activation"), tr("'Experimental Mode' is toggled off/on using the steering wheel buttons or the on-screen controls.\n\nThis overrides 'Conditional Experimental Mode'."), "../assets/img_experimental_white.svg"},
    {"ExperimentalModeViaLKAS", tr("Click the LKAS Button"), tr("'Experimental Mode' is toggled by pressing the 'LKAS' button on the steering wheel."), ""},
    {"ExperimentalModeViaTap", tr("Double-Tap the Screen"), tr("'Experimental Mode' is toggled by double-tapping the onroad UI within 0.5 seconds."), ""},
    {"ExperimentalModeViaDistance", tr("Long Press the Distance Button"), tr("'Experimental Mode' is toggled by holding the 'distance' button on the steering wheel for 0.5+ seconds."), ""},

    {"LongitudinalTune", tr("Longitudinal Tuning"), tr("Settings that control how openpilot manages speed and acceleration."), "../frogpilot/assets/toggle_icons/icon_longitudinal_tune.png"},
    {"AccelerationProfile", tr("Acceleration Profile"), tr("Choose between a sporty or eco-friendly acceleration rate."), ""},
    {"DecelerationProfile", tr("Deceleration Profile"), tr("Choose between a sporty or eco-friendly deceleration rate."), ""},
    {"HumanAcceleration", tr("Human-Like Acceleration"), tr("Uses the lead's acceleration rate when at a takeoff and ramps off the acceleration rate when approaching the maximum set speed for a smoother max speed approach."), ""},
    {"HumanFollowing", tr("Human-Like Following Distance"), tr("Dynamically adjusts the following distance to feel more natural when approaching slower or stopped vehicles."), ""},
    {"IncreasedStoppedDistance", tr("Increase Stopped Distance"), tr("Increases the distance to stop behind vehicles."), ""},

    {"QOLLongitudinal", tr("Quality of Life Improvements"), tr("Miscellaneous longitudinal focused features to improve your overall openpilot experience."), "../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"CustomCruise", tr("Cruise Increase Interval"), tr("Interval used when increasing the cruise control speed."), ""},
    {"CustomCruiseLong", tr("Custom Cruise Interval (Long Press)"), tr("Interval used when increasing the cruise control speed when holding down the button for 0.5+ seconds."), ""},
    {"MapGears", tr("Map Accel/Decel to Gears"), tr("Map the acceleration and deceleration profiles to the 'Eco' or 'Sport' gear modes."), ""},
    {"OnroadDistanceButton", tr("Onroad Personality Button"), tr("The current driving personality is displayed on the screen. Tap to switch personalities, or long press for 2.5 seconds to activate 'Traffic Mode'."), ""},
    {"ReverseCruise", tr("Reverse Cruise Increase"), tr("The long press feature is reversed in order to increase speed by 5 mph instead of 1."), ""},

    {"SpeedLimitController", tr("Speed Limit Controller"), tr("Automatically adjust your max speed to match the speed limit using 'Open Street Maps', 'Navigate on openpilot', or your car's dashboard (Toyota/Lexus/HKG only)."), "../assets/offroad/icon_speed_limit.png"},
    {"SLCConfirmation", tr("Confirm New Speed Limits"), tr("Require manual confirmation before using a new speed limit."), ""},
    {"SLCFallback", tr("Fallback Method"), tr("Choose what happens when no speed limit data is available."), ""},
    {"SLCOverride", tr("Override Method"), tr("Choose how you want to override the current speed limit.\n\n"), ""},
    {"SLCPriority", tr("Speed Limit Source Priority"), tr("Set the order of priority for speed limit data sources."), ""},
    {"SLCOffsets", tr("Speed Limit Offsets"), tr("Manage toggles related to 'Speed Limit Controller's controls."), ""},
    {"Offset1", tr("Speed Limit Offset (0-34 mph)"), tr("Set the speed limit offset for speeds between 0 and 34 mph."), ""},
    {"Offset2", tr("Speed Limit Offset (35-54 mph)"), tr("Set the speed limit offset for speeds between 35 and 54 mph."), ""},
    {"Offset3", tr("Speed Limit Offset (55-64 mph)"), tr("Set the speed limit offset for speeds between 55 and 64 mph."), ""},
    {"Offset4", tr("Speed Limit Offset (65-99 mph)"), tr("Set the speed limit offset for speeds between 65 and 99 mph."), ""},
    {"SLCQOL", tr("Quality of Life Improvements"), tr("Miscellaneous 'Speed Limit Controller' focused features to improve your overall openpilot experience."), ""},
    {"ForceMPHDashboard", tr("Force MPH Readings from Dashboard"), tr("Force speed limit readings in MPH from the dashboard if it normally displays in KPH."), ""},
    {"SLCLookaheadHigher", tr("Prepare for Higher Speed Limits"), tr("Set a lookahead value to prepare for upcoming higher speed limits based on map data."), ""},
    {"SLCLookaheadLower", tr("Prepare for Lower Speed Limits"), tr("Set a lookahead value to prepare for upcoming lower speed limits based on map data."), ""},
    {"SetSpeedLimit", tr("Set Speed to Current Limit"), tr("Set your max speed to match the current speed limit when enabling openpilot."), ""},
    {"SLCVisuals", tr("Visual Settings"), tr("Manage visual settings for the 'Speed Limit Controller'."), ""},
    {"UseVienna", tr("Use Vienna-Style Speed Signs"), tr("Switch to Vienna-style (EU) speed limit signs instead of MUTCD (US)."), ""},
    {"ShowSLCOffset", tr("Show Speed Limit Offset"), tr("Display the speed limit offset separately in the onroad UI when using the Speed Limit Controller."), ""},
  };

  for (const auto &[param, title, desc, icon] : longitudinalToggles) {
    AbstractControl *longitudinalToggle;

    if (param == "ConditionalExperimental") {
      FrogPilotParamManageControl *conditionalExperimentalToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(conditionalExperimentalToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        showToggles(conditionalExperimentalKeys);
      });
      longitudinalToggle = conditionalExperimentalToggle;
    } else if (param == "CESpeed") {
      FrogPilotParamValueControl *CESpeed = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr("mph"), std::map<int, QString>(), 1.0, true);
      FrogPilotParamValueControl *CESpeedLead = new FrogPilotParamValueControl("CESpeedLead", tr(" With Lead"), tr("Switches to 'Experimental Mode' when driving below the set speed with a lead vehicle."), icon, 0, 99, tr("mph"), std::map<int, QString>(), 1.0, true);
      FrogPilotDualParamControl *conditionalSpeeds = new FrogPilotDualParamControl(CESpeed, CESpeedLead);
      longitudinalToggle = reinterpret_cast<AbstractControl*>(conditionalSpeeds);
    } else if (param == "CECurves") {
      std::vector<QString> curveToggles{"CECurvesLead"};
      std::vector<QString> curveToggleNames{tr("With Lead")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, curveToggles, curveToggleNames);
    } else if (param == "CELead") {
      std::vector<QString> leadToggles{"CESlowerLead", "CEStoppedLead"};
      std::vector<QString> leadToggleNames{tr("Slower Lead"), tr("Stopped Lead")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, leadToggles, leadToggleNames);
    } else if (param == "CENavigation") {
      std::vector<QString> navigationToggles{"CENavigationIntersections", "CENavigationTurns", "CENavigationLead"};
      std::vector<QString> navigationToggleNames{tr("Intersections"), tr("Turns"), tr("With Lead")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, navigationToggles, navigationToggleNames);
    } else if (param == "CEModelStopTime") {
      std::map<int, QString> modelStopTimeLabels;
      for (int i = 0; i <= 10; ++i) {
        modelStopTimeLabels[i] = (i == 0) ? tr("Off") : QString::number(i) + " seconds";
      }
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, QString(), modelStopTimeLabels);
    } else if (param == "CESignalSpeed") {
      std::vector<QString> ceSignalToggles{"CESignalLaneDetection"};
      std::vector<QString> ceSignalToggleNames{"Lane Detection"};
      longitudinalToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0, 99, tr("mph"), std::map<int, QString>(), 1.0, ceSignalToggles, ceSignalToggleNames);

    } else if (param == "CurveSpeedControl") {
      FrogPilotParamManageControl *curveControlToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(curveControlToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        curveDetectionBtn->setEnabledButtons(0, QDir("/data/media/0/osm/offline").exists());
        curveDetectionBtn->setCheckedButton(0, params.getBool("MTSCEnabled"));
        curveDetectionBtn->setCheckedButton(1, params.getBool("VisionTurnControl"));

        std::set<QString> modifiedCurveSpeedKeys = curveSpeedKeys;

        if (!params.getBool("MTSCEnabled")) {
          modifiedCurveSpeedKeys.erase("MTSCCurvatureCheck");
        }

        showToggles(modifiedCurveSpeedKeys);
      });
      longitudinalToggle = curveControlToggle;
    } else if (param == "CurveDetectionMethod") {
      curveDetectionBtn = new FrogPilotButtonsControl(title, desc, {tr("Map Based"), tr("Vision")}, true, false);
      QObject::connect(curveDetectionBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        bool mtscEnabled = params.getBool("MTSCEnabled");
        bool vtscEnabled = params.getBool("VisionTurnControl");

        if (id == 0) {
          if (mtscEnabled && !vtscEnabled) {
            curveDetectionBtn->setCheckedButton(0, true);
            return;
          }

          params.putBool("MTSCEnabled", !mtscEnabled);
          curveDetectionBtn->setCheckedButton(0, !mtscEnabled);

          std::set<QString> modifiedCurveSpeedKeys = curveSpeedKeys;

          if (mtscEnabled) {
            modifiedCurveSpeedKeys.erase("MTSCCurvatureCheck");
          }

          showToggles(modifiedCurveSpeedKeys);
        } else if (id == 1) {
          if (vtscEnabled && !mtscEnabled) {
            curveDetectionBtn->setCheckedButton(1, true);
            return;
          }

          params.putBool("VisionTurnControl", !vtscEnabled);
          curveDetectionBtn->setCheckedButton(1, !vtscEnabled);
        }
      });
      longitudinalToggle = curveDetectionBtn;
    } else if (param == "CurveSensitivity" || param == "TurnAggressiveness") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 200, "%");

    } else if (param == "ExperimentalModeActivation") {
      FrogPilotParamManageControl *experimentalModeActivationToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(experimentalModeActivationToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedExperimentalModeActivationKeys = experimentalModeActivationKeys;

        if (isSubaru || (params.getBool("AlwaysOnLateral") && params.getBool("AlwaysOnLateralLKAS"))) {
          modifiedExperimentalModeActivationKeys.erase("ExperimentalModeViaLKAS");
        }

        showToggles(modifiedExperimentalModeActivationKeys);
      });
      longitudinalToggle = experimentalModeActivationToggle;

    } else if (param == "LongitudinalTune") {
      FrogPilotParamManageControl *longitudinalTuneToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(longitudinalTuneToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        showToggles(longitudinalTuneKeys);
      });
      longitudinalToggle = longitudinalTuneToggle;
    } else if (param == "AccelerationProfile") {
      std::vector<QString> profileOptions{tr("Standard"), tr("Eco"), tr("Sport"), tr("Sport+")};
      ButtonParamControl *profileSelection = new ButtonParamControl(param, title, desc, icon, profileOptions);
      longitudinalToggle = profileSelection;
    } else if (param == "DecelerationProfile") {
      std::vector<QString> profileOptions{tr("Standard"), tr("Eco"), tr("Sport")};
      ButtonParamControl *profileSelection = new ButtonParamControl(param, title, desc, icon, profileOptions);
      longitudinalToggle = profileSelection;
    } else if (param == "IncreasedStoppedDistance") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 15, tr(" feet"));

    } else if (param == "QOLLongitudinal") {
      FrogPilotParamManageControl *qolLongitudinalToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(qolLongitudinalToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedQolKeys = qolKeys;

        if (!hasPCMCruise) {
          modifiedQolKeys.erase("ReverseCruise");
        } else {
          modifiedQolKeys.erase("CustomCruise");
          modifiedQolKeys.erase("CustomCruiseLong");
        }

        if (!isToyota && !isGM && !isHKGCanFd) {
          modifiedQolKeys.erase("MapGears");
        }

        showToggles(modifiedQolKeys);
      });
      longitudinalToggle = qolLongitudinalToggle;
    } else if (param == "CustomCruise") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, tr("mph"));
    } else if (param == "CustomCruiseLong") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, tr("mph"));
    } else if (param == "MapGears") {
      std::vector<QString> mapGearsToggles{"MapAcceleration", "MapDeceleration"};
      std::vector<QString> mapGearsToggleNames{tr("Acceleration"), tr("Deceleration")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, mapGearsToggles, mapGearsToggleNames);

    } else if (param == "SpeedLimitController") {
      FrogPilotParamManageControl *speedLimitControllerToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(speedLimitControllerToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        bool slcLower = params.getBool("SLCConfirmationLower");
        bool slcHigher = params.getBool("SLCConfirmationHigher");

        slcConfirmationBtn->setCheckedButton(0, slcLower);
        slcConfirmationBtn->setCheckedButton(1, slcHigher);
        slcConfirmationBtn->setCheckedButton(2, !(slcLower || slcHigher));

        slcOpen = true;
        showToggles(speedLimitControllerKeys);
      });
      longitudinalToggle = speedLimitControllerToggle;
    } else if (param == "SLCConfirmation") {
      slcConfirmationBtn = new FrogPilotButtonsControl(title, desc, {tr("Lower Limits"), tr("Higher Limits"), tr("None")}, true, false);
      QObject::connect(slcConfirmationBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        bool lowerEnabled = params.getBool("SLCConfirmationLower");
        bool higherEnabled = params.getBool("SLCConfirmationHigher");

        if (id == 0) {
          params.putBool("SLCConfirmationLower", !lowerEnabled);
          slcConfirmationBtn->setCheckedButton(0, !lowerEnabled);
          slcConfirmationBtn->setCheckedButton(2, false);

          if (lowerEnabled & !higherEnabled) {
            slcConfirmationBtn->setCheckedButton(2, true);
          }
        } else if (id == 1) {
          params.putBool("SLCConfirmationHigher", !higherEnabled);
          slcConfirmationBtn->setCheckedButton(1, !higherEnabled);
          slcConfirmationBtn->setCheckedButton(2, false);

          if (higherEnabled & !lowerEnabled) {
            slcConfirmationBtn->setCheckedButton(2, true);
          }
        } else {
          params.putBool("SLCConfirmationLower", false);
          params.putBool("SLCConfirmationHigher", false);
          slcConfirmationBtn->setCheckedButton(0, false);
          slcConfirmationBtn->setCheckedButton(1, false);
          slcConfirmationBtn->setCheckedButton(2, true);
        }
      });
      longitudinalToggle = slcConfirmationBtn;
    } else if (param == "SLCFallback") {
      std::vector<QString> fallbackOptions{tr("Set Speed"), tr("Experimental Mode"), tr("Previous Limit")};
      ButtonParamControl *fallbackSelection = new ButtonParamControl(param, title, desc, icon, fallbackOptions);
      longitudinalToggle = fallbackSelection;
    } else if (param == "SLCOverride") {
      std::vector<QString> overrideOptions{tr("None"), tr("Gas Pedal Press"), tr("Cruise Set Speed")};
      ButtonParamControl *overrideSelection = new ButtonParamControl(param, title, desc, icon, overrideOptions);
      longitudinalToggle = overrideSelection;
    } else if (param == "SLCPriority") {
      ButtonControl *slcPriorityButton = new ButtonControl(title, tr("SELECT"), desc);
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

          params.putNonBlocking(priorityKey.toStdString(), selection.toStdString());
          selectedPriorities.append(selection);

          if (selection == tr("Lowest") || selection == tr("Highest") || selection == tr("None")) break;

          updateFrogPilotToggles();
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
      longitudinalToggle = slcPriorityButton;
    } else if (param == "SLCOffsets") {
      ButtonControl *manageSLCOffsetsBtn = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageSLCOffsetsBtn, &ButtonControl::clicked, [this]() {
        openSubParentToggle();
        showToggles(speedLimitControllerOffsetsKeys);
      });
      longitudinalToggle = reinterpret_cast<AbstractControl*>(manageSLCOffsetsBtn);
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
      longitudinalToggle = reinterpret_cast<AbstractControl*>(manageSLCQOLBtn);
    } else if (param == "SLCLookaheadHigher" || param == "SLCLookaheadLower") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 60, tr(" seconds"));
    } else if (param == "SLCVisuals") {
      ButtonControl *manageSLCVisualsBtn = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageSLCVisualsBtn, &ButtonControl::clicked, [this]() {
        openSubParentToggle();
        showToggles(speedLimitControllerVisualsKeys);
      });
      longitudinalToggle = reinterpret_cast<AbstractControl*>(manageSLCVisualsBtn);
    } else if (param == "Offset1" || param == "Offset2" || param == "Offset3" || param == "Offset4") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, -99, 99, tr("mph"));
    } else if (param == "ShowSLCOffset") {
      std::vector<QString> slcOffsetToggles{"ShowSLCOffsetUI"};
      std::vector<QString> slcOffsetToggleNames{tr("Control Via UI")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, slcOffsetToggles, slcOffsetToggleNames);

    } else {
      longitudinalToggle = new ParamControl(param, title, desc, icon);
    }

    addItem(longitudinalToggle);
    toggles[param] = longitudinalToggle;

    makeConnections(longitudinalToggle);

    if (FrogPilotParamManageControl *frogPilotManageToggle = qobject_cast<FrogPilotParamManageControl*>(longitudinalToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotParamManageControl::manageButtonClicked, this, &FrogPilotLongitudinalPanel::openParentToggle);
    }

    QObject::connect(longitudinalToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  QObject::connect(static_cast<ToggleControl*>(toggles["ExperimentalModeViaLKAS"]), &ToggleControl::toggleFlipped, [this](bool state) {
    if (state && params.getBool("AlwaysOnLateralLKAS")) {
      params.putBoolNonBlocking("AlwaysOnLateralLKAS", false);
    }
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, this, &FrogPilotLongitudinalPanel::hideToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubParentToggle, this, &FrogPilotLongitudinalPanel::hideSubToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::updateCarToggles, this, &FrogPilotLongitudinalPanel::updateCarToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, this, &FrogPilotLongitudinalPanel::updateMetric);

  updateMetric();
}

void FrogPilotLongitudinalPanel::updateCarToggles() {
  hasDashSpeedLimits = parent->hasDashSpeedLimits;
  hasPCMCruise = parent->hasPCMCruise;
  isGM = parent->isGM;
  isHKGCanFd = parent->isHKGCanFd;
  isSubaru = parent->isSubaru;
  isToyota = parent->isToyota;

  hideToggles();
}

void FrogPilotLongitudinalPanel::updateMetric() {
  bool previousIsMetric = isMetric;
  isMetric = params.getBool("IsMetric");

  if (isMetric != previousIsMetric) {
    double distanceConversion = isMetric ? FOOT_TO_METER : METER_TO_FOOT;
    double speedConversion = isMetric ? MILE_TO_KM : KM_TO_MILE;

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
  }

  FrogPilotDualParamControl *ceSpeedToggle = reinterpret_cast<FrogPilotDualParamControl*>(toggles["CESpeed"]);
  FrogPilotParamValueButtonControl *ceSignal = reinterpret_cast<FrogPilotParamValueButtonControl*>(toggles["CESignalSpeed"]);
  FrogPilotParamValueControl *customCruiseToggle = static_cast<FrogPilotParamValueControl*>(toggles["CustomCruise"]);
  FrogPilotParamValueControl *customCruiseLongToggle = static_cast<FrogPilotParamValueControl*>(toggles["CustomCruiseLong"]);
  FrogPilotParamValueControl *offset1Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset1"]);
  FrogPilotParamValueControl *offset2Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset2"]);
  FrogPilotParamValueControl *offset3Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset3"]);
  FrogPilotParamValueControl *offset4Toggle = static_cast<FrogPilotParamValueControl*>(toggles["Offset4"]);
  FrogPilotParamValueControl *increasedStoppedDistanceToggle = static_cast<FrogPilotParamValueControl*>(toggles["IncreasedStoppedDistance"]);

  if (isMetric) {
    offset1Toggle->setTitle(tr("Speed Limit Offset (0-34 kph)"));
    offset2Toggle->setTitle(tr("Speed Limit Offset (35-54 kph)"));
    offset3Toggle->setTitle(tr("Speed Limit Offset (55-64 kph)"));
    offset4Toggle->setTitle(tr("Speed Limit Offset (65-99 kph)"));

    offset1Toggle->setDescription(tr("Set speed limit offset for limits between 0-34 kph."));
    offset2Toggle->setDescription(tr("Set speed limit offset for limits between 35-54 kph."));
    offset3Toggle->setDescription(tr("Set speed limit offset for limits between 55-64 kph."));
    offset4Toggle->setDescription(tr("Set speed limit offset for limits between 65-99 kph."));

    ceSignal->updateControl(0, 150, tr("kph"));
    ceSpeedToggle->updateControl(0, 150, tr("kph"));
    customCruiseToggle->updateControl(1, 150, tr("kph"));
    customCruiseLongToggle->updateControl(1, 150, tr("kph"));
    offset1Toggle->updateControl(-99, 99, tr("kph"));
    offset2Toggle->updateControl(-99, 99, tr("kph"));
    offset3Toggle->updateControl(-99, 99, tr("kph"));
    offset4Toggle->updateControl(-99, 99, tr("kph"));

    increasedStoppedDistanceToggle->updateControl(0, 5, tr(" meters"));
  } else {
    offset1Toggle->setTitle(tr("Speed Limit Offset (0-34 mph)"));
    offset2Toggle->setTitle(tr("Speed Limit Offset (35-54 mph)"));
    offset3Toggle->setTitle(tr("Speed Limit Offset (55-64 mph)"));
    offset4Toggle->setTitle(tr("Speed Limit Offset (65-99 mph)"));

    offset1Toggle->setDescription(tr("Set speed limit offset for limits between 0-34 mph."));
    offset2Toggle->setDescription(tr("Set speed limit offset for limits between 35-54 mph."));
    offset3Toggle->setDescription(tr("Set speed limit offset for limits between 55-64 mph."));
    offset4Toggle->setDescription(tr("Set speed limit offset for limits between 65-99 mph."));

    ceSignal->updateControl(0, 99, tr("mph"));
    ceSpeedToggle->updateControl(0, 99, tr("mph"));
    customCruiseToggle->updateControl(1, 99, tr("mph"));
    customCruiseLongToggle->updateControl(1, 99, tr("mph"));
    offset1Toggle->updateControl(-99, 99, tr("mph"));
    offset2Toggle->updateControl(-99, 99, tr("mph"));
    offset3Toggle->updateControl(-99, 99, tr("mph"));
    offset4Toggle->updateControl(-99, 99, tr("mph"));

    increasedStoppedDistanceToggle->updateControl(0, 15, tr(" feet"));
  }
}

void FrogPilotLongitudinalPanel::showToggles(const std::set<QString> &keys) {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(keys.find(key) != keys.end());
  }

  setUpdatesEnabled(true);
  update();
}

void FrogPilotLongitudinalPanel::hideToggles() {
  setUpdatesEnabled(false);

  slcOpen = false;

  for (auto &[key, toggle] : toggles) {
    bool subToggles = conditionalExperimentalKeys.find(key) != conditionalExperimentalKeys.end() ||
                      curveSpeedKeys.find(key) != curveSpeedKeys.end() ||
                      experimentalModeActivationKeys.find(key) != experimentalModeActivationKeys.end() ||
                      longitudinalTuneKeys.find(key) != longitudinalTuneKeys.end() ||
                      qolKeys.find(key) != qolKeys.end() ||
                      speedLimitControllerKeys.find(key) != speedLimitControllerKeys.end() ||
                      speedLimitControllerOffsetsKeys.find(key) != speedLimitControllerOffsetsKeys.end() ||
                      speedLimitControllerQOLKeys.find(key) != speedLimitControllerQOLKeys.end() ||
                      speedLimitControllerVisualsKeys.find(key) != speedLimitControllerVisualsKeys.end();

    toggle->setVisible(!subToggles);
  }

  setUpdatesEnabled(true);
  update();
}

void FrogPilotLongitudinalPanel::hideSubToggles() {
  if (slcOpen) {
    showToggles(speedLimitControllerKeys);
  }
}
