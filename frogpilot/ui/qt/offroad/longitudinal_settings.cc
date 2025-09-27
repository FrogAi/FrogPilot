#include "frogpilot/ui/qt/offroad/longitudinal_settings.h"

FrogPilotLongitudinalPanel::FrogPilotLongitudinalPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

  QStackedLayout *longitudinalLayout = new QStackedLayout();
  addItem(longitudinalLayout);

  FrogPilotListWidget *longitudinalList = new FrogPilotListWidget(this);

  ScrollView *longitudinalPanel = new ScrollView(longitudinalList, this);

  longitudinalLayout->addWidget(longitudinalPanel);

  FrogPilotListWidget *advancedLongitudinalTuneList = new FrogPilotListWidget(this);
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

  ScrollView *advancedLongitudinalTunePanel = new ScrollView(advancedLongitudinalTuneList, this);
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

  longitudinalLayout->addWidget(advancedLongitudinalTunePanel);
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
    {"AdvancedLongitudinalTune", tr("Advanced Longitudinal Tuning"), tr("<b>Advanced acceleration and braking control changes</b> to fine-tune how openpilot drives."), "../../frogpilot/assets/toggle_icons/icon_advanced_longitudinal_tune.png"},
    {"LongitudinalActuatorDelay", parent->longitudinalActuatorDelay != 0 ? QString(tr("Actuator Delay (Default: %1)")).arg(QString::number(parent->longitudinalActuatorDelay, 'f', 2)) : tr("Actuator Delay"), tr("<b>The time between openpilot's throttle or brake command and the vehicle's response.</b> Increase if the vehicle feels slow to react; decrease if it feels too eager or overshoots."), ""},
    {"MaxDesiredAcceleration", tr("Maximum Acceleration"), tr("<b>Limit the strongest acceleration</b> openpilot can command."), ""},
    {"StartAccel", parent->startAccel != 0 ? QString(tr("Start Acceleration (Default: %1)")).arg(QString::number(parent->startAccel, 'f', 2)) : tr("Start Acceleration"), tr("<b>Extra acceleration applied when starting from a stop.</b> Increase for quicker takeoffs; decrease for smoother, gentler starts."), ""},
    {"VEgoStarting", parent->vEgoStarting != 0 ? QString(tr("Start Speed (Default: %1)")).arg(QString::number(parent->vEgoStarting, 'f', 2)) : tr("Start Speed"), tr("<b>The speed at which openpilot exits the stopped state.</b> Increase to reduce creeping; decrease to move sooner after stopping."), ""},
    {"StopAccel", parent->stopAccel != 0 ? QString(tr("Stop Acceleration (Default: %1)")).arg(QString::number(parent->stopAccel, 'f', 2)) : tr("Stop Acceleration"), tr("<b>Brake force applied to hold the vehicle at a standstill.</b> Increase to prevent rolling on hills; decrease for smoother, softer stops."), ""},
    {"StoppingDecelRate", parent->stoppingDecelRate != 0 ? QString(tr("Stopping Rate (Default: %1)")).arg(QString::number(parent->stoppingDecelRate, 'f', 2)) : tr("Stopping Rate"), tr("<b>How quickly braking ramps up when stopping.</b> Increase for shorter, firmer stops; decrease for smoother, longer stops."), ""},
    {"VEgoStopping", parent->vEgoStopping != 0 ? QString(tr("Stop Speed (Default: %1)")).arg(QString::number(parent->vEgoStopping, 'f', 2)) : tr("Stop Speed"), tr("<b>The speed at which openpilot considers the vehicle stopped.</b> Increase to brake earlier and stop smoothly; decrease to wait longer but risk overshooting."), ""},

    {"ConditionalExperimental", tr("Conditional Experimental Mode"), tr("<b>Automatically switch to \"Experimental Mode\" when set conditions are met.</b> Allows the model to handle challenging situations with smarter decision making."), "../../frogpilot/assets/toggle_icons/icon_conditional.png"},
    {"CESpeed", tr("Below"), tr("<b>Switch to \"Experimental Mode\" when driving below this speed without a lead</b> to help openpilot handle low-speed situations more smoothly."), ""},
    {"CECurves", tr("Curve Detected Ahead"), tr("<b>Switch to \"Experimental Mode\" when a curve is detected</b> to allow the model to set an appropriate speed for the curve."), ""},
    {"CELead", tr("Lead Detected Ahead"), tr("<b>Switch to \"Experimental Mode\" when a slower or stopped vehicle is detected.</b> Can make braking smoother and more reliable on some vehicles."), ""},
    {"CENavigation", tr("Navigation-Based"), tr("<b>Switch to \"Experimental Mode\" when approaching intersections or turns on the active route</b> while using \"Navigate on openpilot\" (NOO) to allow the model to set an appropriate speed for upcoming maneuvers."), ""},
    {"CEModelStopTime", tr("Predicted Stop In"), tr("<b>Switch to \"Experimental Mode\" when openpilot predicts a stop within the set time.</b> This is usually triggered when the model \"sees\" a red light or stop sign ahead.<br><br><i><b>Disclaimer</b>: openpilot does not explicitly detect traffic lights or stop signs. In \"Experimental Mode\", openpilot makes end-to-end driving decisions from camera input, which means it may stop even when there's no clear reason.</i>"), ""},
    {"CESignalSpeed", tr("Turn Signal Below"), tr("<b>Switch to \"Experimental Mode\" when using a turn signal below the set speed</b> to allow the model to choose an appropriate speed for smoother left and right turns."), ""},
    {"ShowCEMStatus", tr("Status Widget"), tr("<b>Show which condition triggered \"Experimental Mode\"</b> on the driving screen."), ""},

    {"CurveSpeedController", tr("Curve Speed Controller"), tr("<b>Automatically slow down for upcoming curves</b> using data learned from your driving style, adapting to curves as you would."), "../../frogpilot/assets/toggle_icons/icon_speed_map.png"},
    {"CalibratedLateralAcceleration", tr("Calibrated Lateral Acceleration"), tr("<b>The learned lateral acceleration from collected driving data.</b> This sets how fast openpilot will take curves. Higher values allow faster cornering; lower values slow the vehicle for gentler turns."), ""},
    {"CalibrationProgress", tr("Calibration Progress"), tr("<b>How much curve data has been collected.</b> This is a progress meter; it is normal for the value to stay low and rarely reach 100%."), ""},
    {"ResetCurveData", tr("Reset Curve Data"), tr("<b>Reset collected user data for \"Curve Speed Controller\".</b>"), ""},
    {"ShowCSCStatus", tr("Status Widget"), tr("<b>Show the \"Curve Speed Controller\" target speed on the driving screen.</b>"), ""},

    {"CustomPersonalities", tr("Driving Personalities"), tr("<b>Customize the \"Driving Personalities\"</b> to better match your driving style."), "../../frogpilot/assets/toggle_icons/icon_personality.png"},

    {"TrafficPersonalityProfile", tr("Traffic Mode"), tr("<b>Customize the \"Traffic Mode\" personality profile.</b> Designed for stop-and-go driving."), "../../frogpilot/assets/stock_theme/distance_icons/traffic.png"},
    {"TrafficFollow", tr("Following Distance"), tr("<b>The minimum following distance to the lead vehicle in \"Traffic Mode\".</b> openpilot blends between this value and the \"Aggressive\" profile as speed increases. Increase for more space; decrease for tighter gaps."), ""},
    {"TrafficJerkAcceleration", tr("Acceleration Smoothness"), tr("<b>How smoothly openpilot accelerates in \"Traffic Mode\".</b> Increase for gentler starts; decrease for faster but more abrupt takeoffs."), ""},
    {"TrafficJerkDeceleration", tr("Braking Smoothness"), tr("<b>How smoothly openpilot brakes in \"Traffic Mode\".</b> Increase for gentler stops; decrease for quicker but sharper braking."), ""},
    {"TrafficJerkDanger", tr("Safety Gap Bias"), tr("<b>How much extra space openpilot keeps from the vehicle ahead in \"Traffic Mode\".</b> Increase for larger gaps and more cautious following; decrease for tighter gaps and closer following."), ""},
    {"TrafficJerkSpeedDecrease", tr("Slowdown Response"), tr("<b>How smoothly openpilot slows down in \"Traffic Mode\".</b> Increase for more gradual deceleration; decrease for faster but sharper slowdowns."), ""},
    {"TrafficJerkSpeed", tr("Speed-Up Response"), tr("<b>How smoothly openpilot speeds up in \"Traffic Mode\".</b> Increase for more gradual acceleration; decrease for quicker but more jolting acceleration."), ""},
    {"ResetTrafficPersonality", tr("Reset to Defaults"), tr("<b>Reset \"Traffic Mode\" settings to defaults.</b>"), ""},

    {"AggressivePersonalityProfile", tr("Aggressive"), tr("<b>Customize the \"Aggressive\" personality profile.</b> Designed for assertive driving with tighter gaps."), "../../frogpilot/assets/stock_theme/distance_icons/aggressive.png"},
    {"AggressiveFollow", tr("Following Distance"), tr("<b>How many seconds openpilot follows behind lead vehicles when using the \"Aggressive\" profile.</b> Increase for more space; decrease for tighter gaps.<br><br>Default: 1.25 seconds."), ""},
    {"AggressiveJerkAcceleration", tr("Acceleration Smoothness"), tr("<b>How smoothly openpilot accelerates with the \"Aggressive\" profile.</b> Increase for gentler starts; decrease for faster but more abrupt takeoffs."), ""},
    {"AggressiveJerkDeceleration", tr("Braking Smoothness"), tr("<b>How smoothly openpilot brakes with the \"Aggressive\" profile.</b> Increase for gentler stops; decrease for quicker but sharper braking."), ""},
    {"AggressiveJerkDanger", tr("Safety Gap Bias"), tr("<b>How much extra space openpilot keeps from the vehicle ahead with the \"Aggressive\" profile.</b> Increase for larger gaps and more cautious following; decrease for tighter gaps and closer following."), ""},
    {"AggressiveJerkSpeedDecrease", tr("Slowdown Response"), tr("<b>How smoothly openpilot slows down with the \"Aggressive\" profile.</b> Increase for more gradual deceleration; decrease for faster but sharper slowdowns."), ""},
    {"AggressiveJerkSpeed", tr("Speed-Up Response"), tr("<b>How smoothly openpilot speeds up with the \"Aggressive\" profile.</b> Increase for more gradual acceleration; decrease for quicker but more jolting acceleration."), ""},
    {"ResetAggressivePersonality", tr("Reset to Defaults"), tr("<b>Reset the \"Aggressive\" profile to defaults.</b>"), ""},

    {"StandardPersonalityProfile", tr("Standard"), tr("<b>Customize the \"Standard\" personality profile.</b> Designed for balanced driving with moderate gaps."), "../../frogpilot/assets/stock_theme/distance_icons/standard.png"},
    {"StandardFollow", tr("Following Distance"), tr("<b>How many seconds openpilot follows behind lead vehicles when using the \"Standard\" profile.</b> Increase for more space; decrease for tighter gaps.<br><br>Default: 1.45 seconds."), ""},
    {"StandardJerkAcceleration", tr("Acceleration Smoothness"), tr("<b>How smoothly openpilot accelerates with the \"Standard\" profile.</b> Increase for gentler starts; decrease for faster but more abrupt takeoffs."), ""},
    {"StandardJerkDeceleration", tr("Braking Smoothness"), tr("<b>How smoothly openpilot brakes with the \"Standard\" profile.</b> Increase for gentler stops; decrease for quicker but sharper braking."), ""},
    {"StandardJerkDanger", tr("Safety Gap Bias"), tr("<b>How much extra space openpilot keeps from the vehicle ahead with the \"Standard\" profile.</b> Increase for larger gaps and more cautious following; decrease for tighter gaps and closer following."), ""},
    {"StandardJerkSpeedDecrease", tr("Slowdown Response"), tr("<b>How smoothly openpilot slows down with the \"Standard\" profile.</b> Increase for more gradual deceleration; decrease for faster but sharper slowdowns."), ""},
    {"StandardJerkSpeed", tr("Speed-Up Response"), tr("<b>How smoothly openpilot speeds up with the \"Standard\" profile.</b> Increase for more gradual acceleration; decrease for quicker but more jolting acceleration."), ""},
    {"ResetStandardPersonality", tr("Reset to Defaults"), tr("<b>Reset the \"Standard\" profile to defaults.</b>"), ""},

    {"RelaxedPersonalityProfile", tr("Relaxed"), tr("<b>Customize the \"Relaxed\" personality profile.</b> Designed for smoother, more comfortable driving with larger gaps."), "../../frogpilot/assets/stock_theme/distance_icons/relaxed.png"},
    {"RelaxedFollow", tr("Following Distance"), tr("<b>How many seconds openpilot follows behind lead vehicles when using the \"Relaxed\" profile.</b> Increase for more space; decrease for tighter gaps.<br><br>Default: 1.75 seconds."), ""},
    {"RelaxedJerkAcceleration", tr("Acceleration Smoothness"), tr("<b>How smoothly openpilot accelerates with the \"Relaxed\" profile.</b> Increase for gentler starts; decrease for faster but more abrupt takeoffs."), ""},
    {"RelaxedJerkDeceleration", tr("Braking Smoothness"), tr("<b>How smoothly openpilot brakes with the \"Relaxed\" profile.</b> Increase for gentler stops; decrease for quicker but sharper braking."), ""},
    {"RelaxedJerkDanger", tr("Safety Gap Bias"), tr("<b>How much extra space openpilot keeps from the vehicle ahead with the \"Relaxed\" profile.</b> Increase for larger gaps and more cautious following; decrease for tighter gaps and closer following."), ""},
    {"RelaxedJerkSpeedDecrease", tr("Slowdown Response"), tr("<b>How smoothly openpilot slows down with the \"Relaxed\" profile.</b> Increase for more gradual deceleration; decrease for faster but sharper slowdowns."), ""},
    {"RelaxedJerkSpeed", tr("Speed-Up Response"), tr("<b>How smoothly openpilot speeds up with the \"Relaxed\" profile.</b> Increase for more gradual acceleration; decrease for quicker but more jolting acceleration."), ""},
    {"ResetRelaxedPersonality", tr("Reset to Defaults"), tr("<b>Reset the \"Relaxed\" profile to defaults.</b>"), ""},

    {"LongitudinalTune", tr("Longitudinal Tuning"), tr("<b>Acceleration and braking control changes</b> to fine-tune how openpilot drives."), "../../frogpilot/assets/toggle_icons/icon_longitudinal_tune.png"},
    {"AccelerationProfile", tr("Acceleration Profile"), tr("<b>How quickly openpilot speeds up.</b> \"Eco\" is gentle and efficient, \"Sport\" is firmer and more responsive, and \"Sport+\" accelerates at the maximum rate allowed."), ""},
    {"DecelerationProfile", tr("Deceleration Profile"), tr("<b>How firmly openpilot slows down.</b> \"Eco\" favors coasting, \"Sport\" applies stronger braking."), ""},
    {"HumanAcceleration", tr("Human-Like Acceleration"), tr("<b>Acceleration that mimics human behavior</b> by easing the throttle at low speeds and adding extra power when taking off from a stop."), ""},
    {"HumanFollowing", tr("Human-Like Following"), tr("<b>Following behavior that mimics human drivers</b> by closing gaps behind faster vehicles for quicker takeoffs and dynamically adjusting the desired following distance for gentler, more efficient braking."), ""},
    {"LeadDetectionThreshold", tr("Lead Detection Sensitivity"), tr("<b>How sensitive openpilot is to detecting vehicles.</b> Higher sensitivity allows quicker detection at longer distances but may react to non-vehicle objects; lower sensitivity is more conservative and reduces false detections."), ""},
    {"TacoTune", tr("\"Taco Bell Run\" Turn Speed Hack"), tr("<b>The turn-speed hack from comma's 2022 \"Taco Bell Run\".</b> Designed to slow down for left and right turns."), ""},

    {"QOLLongitudinal", tr("Quality of Life"), tr("<b>Miscellaneous acceleration and braking control changes</b> to fine-tune how openpilot drives."), "../../frogpilot/assets/toggle_icons/icon_quality_of_life.png"},
    {"CustomCruise", tr("Cruise Interval"), tr("<b>How much the set speed increases or decreases</b> for each + or – cruise control button press."), ""},
    {"CustomCruiseLong", tr("Cruise Interval (Hold)"), tr("<b>How much the set speed increases or decreases while holding the + or – cruise control buttons.</b>"), ""},
    {"ForceStops", tr("Force Stop at \"Detected\" Stop Lights/Signs"), tr("<b>Force openpilot to stop whenever the driving model \"detects\" a red light or stop sign.</b><br><br><i><b>Disclaimer</b>: openpilot does not explicitly detect traffic lights or stop signs. In \"Experimental Mode\", openpilot makes end-to-end driving decisions from camera input, which means it may stop even when there's no clear reason.</i>"), ""},
    {"IncreasedStoppedDistance", tr("Increase Stopped Distance by:"), tr("<b>Add extra space when stopped behind vehicles.</b> Increase for more room; decrease for shorter gaps."), ""},
    {"MapGears", tr("Map Accel/Decel to Gears"), tr("<b>Map the Acceleration or Deceleration profiles to the vehicle's \"Eco\" and \"Sport\" gear modes.</b>"), ""},
    {"SetSpeedOffset", tr("Offset Set Speed by:"), tr("<b>Increase the set speed by the chosen offset.</b> For example, set +5 if you usually drive 5 over the limit."), ""},
    {"ReverseCruise", tr("Reverse Cruise Increase"), tr("<b>Reverse the cruise control button behavior</b> so a short press increases the set speed by 5 instead of 1."), ""},

    {"SpeedLimitController", tr("Speed Limit Controller"), tr("<b>Limit openpilot's maximum driving speed to the current speed limit</b> obtained from downloaded maps, Mapbox, Navigate on openpilot, or the dashboard for supported vehicles (Ford, Genesis, Hyundai, Kia, Lexus, Toyota)."), "../../frogpilot/assets/toggle_icons/icon_speed_limit.png"},
    {"SLCFallback", tr("Fallback Speed"), tr("<b>The speed used by \"Speed Limit Controller\" when no speed limit is found.</b><br><br>- <b>Set Speed</b>: Use the cruise set speed<br>- <b>Experimental Mode</b>: Estimate the limit using the driving model<br>- <b>Previous Limit</b>: Keep using the last confirmed limit"), ""},
    {"SLCOverride", tr("Override Speed"), tr("<b>The speed used by \"Speed Limit Controller\" after you manually drive faster than the posted limit.</b><br><br>- <b>Set with Gas Pedal</b>: Use the highest speed reached while pressing the gas<br>- <b>Max Set Speed</b>: Use the cruise set speed<br><br>Overrides clear when openpilot disengages."), ""},
    {"SLCQOL", tr("Quality of Life"), tr("<b>Miscellaneous \"Speed Limit Controller\" changes</b> to fine-tune how openpilot drives."), ""},
    {"SLCConfirmation", tr("Confirm New Speed Limits"), tr("<b>Ask before changing to a new speed limit.</b> To accept, tap the flashing on-screen widget or press the Cruise Increase button. To deny, press the Cruise Decrease button or ignore the prompt for 30 seconds."), ""},
    {"ForceMPHDashboard", tr("Force MPH from Dashboard"), tr("<b>Always read dashboard speed limit signs in mph.</b> Turn this on if the cluster shows mph but the limit is interpreted as km/h."), ""},
    {"SLCLookaheadHigher", tr("Higher Limit Lookahead Time"), tr("<b>How far ahead openpilot anticipates upcoming higher speed limits</b> from downloaded map data."), ""},
    {"SLCLookaheadLower", tr("Lower Limit Lookahead Time"), tr("<b>How far ahead openpilot anticipates upcoming lower speed limits</b> from downloaded map data."), ""},
    {"SetSpeedLimit", tr("Match Speed Limit on Engage"), tr("<b>When openpilot is first enabled, automatically set the max speed to the current posted limit.</b>"), ""},
    {"SLCMapboxFiller", tr("Use Mapbox as Fallback"), tr("<b>Use Mapbox speed-limit data when no other source is available.</b>"), ""},
    {"SLCPriority", tr("Speed Limit Source Priority"), tr("<b>The source order for speed limits</b> when more than one is available."), ""},
    {"SLCOffsets", tr("Speed Limit Offsets"), tr("<b>Add an offset to the posted speed limit</b> to better match your driving style."), ""},
    {"Offset1", tr("Speed Offset (0–24 mph)"), tr("<b>How much to offset posted speed-limits</b> between 0 and 24 mph."), ""},
    {"Offset2", tr("Speed Offset (25–34 mph)"), tr("<b>How much to offset posted speed-limits</b> between 25 and 34 mph."), ""},
    {"Offset3", tr("Speed Offset (35–44 mph)"), tr("<b>How much to offset posted speed-limits</b> between 35 and 44 mph."), ""},
    {"Offset4", tr("Speed Offset (45–54 mph)"), tr("<b>How much to offset posted speed-limits</b> between 45 and 54 mph."), ""},
    {"Offset5", tr("Speed Offset (55–64 mph)"), tr("<b>How much to offset posted speed-limits</b> between 55 and 64 mph."), ""},
    {"Offset6", tr("Speed Offset (65–74 mph)"), tr("<b>How much to offset posted speed-limits</b> between 65 and 74 mph."), ""},
    {"Offset7", tr("Speed Offset (75–99 mph)"), tr("<b>How much to offset posted speed-limits</b> between 75 and 99 mph."), ""},
    {"SLCVisuals", tr("Visual Settings"), tr("<b>Visual \"Speed Limit Controller\" changes</b> to fine-tune how the driving screen looks."), ""},
    {"ShowSLCOffset", tr("Show Speed Limit Offset"), tr("<b>Show the current offset from the posted limit</b> on the driving screen."), ""},
    {"SpeedLimitSources", tr("Show Speed Limit Sources"), tr("<b>Display the speed-limit sources and their current values</b> on the driving screen."), ""}
  };

  for (const auto &[param, title, desc, icon] : longitudinalToggles) {
    AbstractControl *longitudinalToggle;

    if (param == "AdvancedLongitudinalTune") {
      FrogPilotManageControl *advancedLongitudinalTuneToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(advancedLongitudinalTuneToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, advancedLongitudinalTunePanel]() {
        longitudinalLayout->setCurrentWidget(advancedLongitudinalTunePanel);
      });
      longitudinalToggle = advancedLongitudinalTuneToggle;
    } else if (param == "LongitudinalActuatorDelay") {
      longitudinalActuatorDelayToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 1, tr(" seconds"), std::map<float, QString>(), 0.01);
      longitudinalToggle = longitudinalActuatorDelayToggle;
    } else if (param == "MaxDesiredAcceleration") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.1, 4.0, tr(" m/s²"), std::map<float, QString>(), 0.1);
    } else if (param == "StartAccel") {
      startAccelToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 4, tr(" m/s²"), std::map<float, QString>(), 0.01, true);
      longitudinalToggle = startAccelToggle;
    } else if (param == "VEgoStarting") {
      vEgoStartingToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.01, 1, tr(" m/s²"), std::map<float, QString>(), 0.01);
      longitudinalToggle = vEgoStartingToggle;
    } else if (param == "StopAccel") {
      stopAccelToggle = new FrogPilotParamValueControl(param, title, desc, icon, -4, 0, tr(" m/s²"), std::map<float, QString>(), 0.01, true);
      longitudinalToggle = stopAccelToggle;
    } else if (param == "StoppingDecelRate") {
      stoppingDecelRateToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.001, 1, tr(" m/s²"), std::map<float, QString>(), 0.001, true);
      longitudinalToggle = stoppingDecelRateToggle;
    } else if (param == "VEgoStopping") {
      vEgoStoppingToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.01, 1, tr(" m/s²"), std::map<float, QString>(), 0.01);
      longitudinalToggle = vEgoStoppingToggle;

    } else if (param == "ConditionalExperimental") {
      FrogPilotManageControl *conditionalExperimentalToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(conditionalExperimentalToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, conditionalExperimentalPanel]() {
        longitudinalLayout->setCurrentWidget(conditionalExperimentalPanel);
      });
      longitudinalToggle = conditionalExperimentalToggle;
    } else if (param == "CESpeed") {
      FrogPilotParamValueControl *CESpeed = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr(" mph"), std::map<float, QString>(), 1, true, 175);
      FrogPilotParamValueControl *CESpeedLead = new FrogPilotParamValueControl("CESpeedLead", tr("With Lead"), tr("<b>Switch to \"Experimental Mode\" when driving below this speed with a lead</b> to help openpilot handle low-speed situations more smoothly."), icon, 0, 99, tr(" mph"), std::map<float, QString>(), 1, true, 175);
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
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 9, QString(), stopTimeLabels);
    } else if (param == "CESignalSpeed") {
      std::vector<QString> ceSignalToggles{"CESignalLaneDetection"};
      std::vector<QString> ceSignalToggleNames{tr("Not For Detected Lanes")};
      longitudinalToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0, 99, tr(" mph"), std::map<float, QString>(), 1.0, true, ceSignalToggles, ceSignalToggleNames, true);

    } else if (param == "CurveSpeedController") {
      FrogPilotManageControl *curveControlToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(curveControlToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, curveSpeedPanel]() {
        longitudinalLayout->setCurrentWidget(curveSpeedPanel);
      });
      longitudinalToggle = curveControlToggle;
    } else if (param == "CalibrationProgress") {
      calibrationProgressLabel = new LabelControl(title, QString::number(params.getFloat("CalibrationProgress"), 'f', 2) + "%", desc);
      longitudinalToggle = calibrationProgressLabel;
    } else if (param == "CalibratedLateralAcceleration") {
      calibratedLateralAccelerationLabel = new LabelControl(title, QString::number(params.getFloat("CalibratedLateralAcceleration"), 'f', 2) + tr(" m/s²"), desc);
      longitudinalToggle = calibratedLateralAccelerationLabel;
    } else if (param == "ResetCurveData") {
      ButtonControl *resetCurveDataButton = new ButtonControl(title, tr("RESET"), desc);
      QObject::connect(resetCurveDataButton, &ButtonControl::clicked, [this]() {
        if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your curvature data?"), this)) {
          params.putFloat("CalibratedLateralAcceleration", 2.00);
          params.remove("CalibrationProgress");
          params.remove("CurvatureData");

          params_cache.putFloat("CalibratedLateralAcceleration", 2.00);
          params_cache.remove("CalibrationProgress");
          params_cache.remove("CurvatureData");

          calibratedLateralAccelerationLabel->setText(QString::number(2.00, 'f', 2) + tr(" m/s²"));
          calibrationProgressLabel->setText(QString::number(0.00, 'f', 2) + "%");
        }
      });
      longitudinalToggle = resetCurveDataButton;

    } else if (param == "CustomPersonalities") {
      FrogPilotManageControl *customPersonalitiesToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(customPersonalitiesToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, customDrivingPersonalityPanel]() {
        longitudinalLayout->setCurrentWidget(customDrivingPersonalityPanel);
      });
      longitudinalToggle = customPersonalitiesToggle;
    } else if (param == "ResetTrafficPersonality" || param == "ResetAggressivePersonality" || param == "ResetStandardPersonality" || param == "ResetRelaxedPersonality") {
      ButtonControl *resetButton = new ButtonControl(title, tr("RESET"), desc);
      longitudinalToggle = resetButton;
    } else if (param == "TrafficPersonalityProfile") {
      FrogPilotManageControl *trafficPersonalityToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(trafficPersonalityToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, trafficPersonalityPanel, this]() {
        openSubSubPanel();

        longitudinalLayout->setCurrentWidget(trafficPersonalityPanel);

        customPersonalityOpen = true;
      });
      longitudinalToggle = trafficPersonalityToggle;
    } else if (param == "AggressivePersonalityProfile") {
      FrogPilotManageControl *aggressivePersonalityToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(aggressivePersonalityToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, aggressivePersonalityPanel, this]() {
        openSubSubPanel();

        longitudinalLayout->setCurrentWidget(aggressivePersonalityPanel);

        customPersonalityOpen = true;
      });
      longitudinalToggle = aggressivePersonalityToggle;
    } else if (param == "StandardPersonalityProfile") {
      FrogPilotManageControl *standardPersonalityToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(standardPersonalityToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, standardPersonalityPanel, this]() {
        openSubSubPanel();

        longitudinalLayout->setCurrentWidget(standardPersonalityPanel);

        customPersonalityOpen = true;
      });
      longitudinalToggle = standardPersonalityToggle;
    } else if (param == "RelaxedPersonalityProfile") {
      FrogPilotManageControl *relaxedPersonalityToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(relaxedPersonalityToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, relaxedPersonalityPanel, this]() {
        openSubSubPanel();

        longitudinalLayout->setCurrentWidget(relaxedPersonalityPanel);

        customPersonalityOpen = true;
      });
      longitudinalToggle = relaxedPersonalityToggle;
    } else if (aggressivePersonalityKeys.contains(param) || standardPersonalityKeys.contains(param) || relaxedPersonalityKeys.contains(param) || trafficPersonalityKeys.contains(param)) {
      if (param == "TrafficFollow" || param == "AggressiveFollow" || param == "StandardFollow" || param == "RelaxedFollow") {
        std::map<float, QString> followTimeLabels;
        for (float i = 0; i <= 3; i += 0.01) {
          followTimeLabels[i] = std::lround(i / 0.01) == 1 / 0.01 ? QString::number(i, 'f', 2) + tr(" second") : QString::number(i, 'f', 2) + tr(" seconds");
        }
        if (param == "TrafficFollow") {
          longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.5, 3, QString(), followTimeLabels, 0.01, true);
        } else {
          longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 3, QString(), followTimeLabels, 0.01, true);
        }
      } else {
        longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 25, 200, "%");
      }

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
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 25, 50, "%");

    } else if (param == "QOLLongitudinal") {
      FrogPilotManageControl *qolLongitudinalToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(qolLongitudinalToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, qolPanel]() {
        longitudinalLayout->setCurrentWidget(qolPanel);
      });
      longitudinalToggle = qolLongitudinalToggle;
    } else if (param == "CustomCruise") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, tr(" mph"));
    } else if (param == "CustomCruiseLong") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, tr(" mph"));
    } else if (param == "IncreasedStoppedDistance") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, tr(" feet"));
    } else if (param == "MapGears") {
      std::vector<QString> mapGearsToggles{"MapAcceleration", "MapDeceleration"};
      std::vector<QString> mapGearsToggleNames{tr("Acceleration"), tr("Deceleration")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, mapGearsToggles, mapGearsToggleNames);
    } else if (param == "SetSpeedOffset") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr(" mph"));

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

          if (!parent->hasDashSpeedLimits) {
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
      ButtonControl *manageSLCOffsetsButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageSLCOffsetsButton, &ButtonControl::clicked, [longitudinalLayout, speedLimitControllerOffsetsPanel, this]() {
        openSubSubPanel();

        longitudinalLayout->setCurrentWidget(speedLimitControllerOffsetsPanel);

        slcOpen = true;
      });
      longitudinalToggle = manageSLCOffsetsButton;
    } else if (speedLimitControllerOffsetsKeys.contains(param)) {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, -99, 99, tr(" mph"));
    } else if (param == "SLCQOL") {
      ButtonControl *manageSLCQOLButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageSLCQOLButton, &ButtonControl::clicked, [longitudinalLayout, speedLimitControllerQOLPanel, this]() {
        openSubSubPanel();

        longitudinalLayout->setCurrentWidget(speedLimitControllerQOLPanel);

        slcOpen = true;
      });
      longitudinalToggle = manageSLCQOLButton;
    } else if (param == "SLCConfirmation") {
      std::vector<QString> confirmationToggles{"SLCConfirmationLower", "SLCConfirmationHigher"};
      std::vector<QString> confirmationToggleNames{tr("Lower Limits"), tr("Higher Limits")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, confirmationToggles, confirmationToggleNames);
    } else if (param == "SLCLookaheadHigher" || param == "SLCLookaheadLower") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 30, tr(" seconds"));
    } else if (param == "SLCVisuals") {
      ButtonControl *manageSLCVisualsButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageSLCVisualsButton, &ButtonControl::clicked, [longitudinalLayout, speedLimitControllerVisualPanel, this]() {
        openSubSubPanel();

        longitudinalLayout->setCurrentWidget(speedLimitControllerVisualPanel);

        slcOpen = true;
      });
      longitudinalToggle = manageSLCVisualsButton;

    } else {
      longitudinalToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = longitudinalToggle;

    if (advancedLongitudinalTuneKeys.contains(param)) {
      advancedLongitudinalTuneList->addItem(longitudinalToggle);
    } else if (aggressivePersonalityKeys.contains(param)) {
      aggressivePersonalityList->addItem(longitudinalToggle);
    } else if (conditionalExperimentalKeys.contains(param)) {
      conditionalExperimentalList->addItem(longitudinalToggle);
    } else if (curveSpeedKeys.contains(param)) {
      curveSpeedList->addItem(longitudinalToggle);
    } else if (customDrivingPersonalityKeys.contains(param)) {
      customDrivingPersonalityList->addItem(longitudinalToggle);
    } else if (longitudinalTuneKeys.contains(param)) {
      longitudinalTuneList->addItem(longitudinalToggle);
    } else if (qolKeys.contains(param)) {
      qolList->addItem(longitudinalToggle);
    } else if (relaxedPersonalityKeys.contains(param)) {
      relaxedPersonalityList->addItem(longitudinalToggle);
    } else if (speedLimitControllerKeys.contains(param)) {
      speedLimitControllerList->addItem(longitudinalToggle);
    } else if (speedLimitControllerOffsetsKeys.contains(param)) {
      speedLimitControllerOffsetsList->addItem(longitudinalToggle);
    } else if (speedLimitControllerQOLKeys.contains(param)) {
      speedLimitControllerQOLList->addItem(longitudinalToggle);
    } else if (speedLimitControllerVisualKeys.contains(param)) {
      speedLimitControllerVisualList->addItem(longitudinalToggle);
    } else if (standardPersonalityKeys.contains(param)) {
      standardPersonalityList->addItem(longitudinalToggle);
    } else if (trafficPersonalityKeys.contains(param)) {
      trafficPersonalityList->addItem(longitudinalToggle);
    } else {
      longitudinalList->addItem(longitudinalToggle);

      parentKeys.insert(param);
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(longitudinalToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        emit openSubPanel();
        openDescriptions(forceOpenDescriptions, toggles);
      });
    }

    QObject::connect(longitudinalToggle, &AbstractControl::hideDescriptionEvent, [this]() {
      update();
    });
    QObject::connect(longitudinalToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  QSet<QString> forceUpdateKeys = {"HumanAcceleration", "LongitudinalTune"};
  for (const QString &key : forceUpdateKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, this, &FrogPilotLongitudinalPanel::updateToggles);
  }

  FrogPilotParamValueControl *trafficFollowToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficFollow"]);
  FrogPilotParamValueControl *trafficAccelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkAcceleration"]);
  FrogPilotParamValueControl *trafficDecelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkDeceleration"]);
  FrogPilotParamValueControl *trafficDangerToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkDanger"]);
  FrogPilotParamValueControl *trafficSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkSpeed"]);
  FrogPilotParamValueControl *trafficSpeedDecreaseToggle = static_cast<FrogPilotParamValueControl*>(toggles["TrafficJerkSpeedDecrease"]);
  FrogPilotButtonsControl *trafficResetButton = static_cast<FrogPilotButtonsControl*>(toggles["ResetTrafficPersonality"]);
  QObject::connect(trafficResetButton, &FrogPilotButtonsControl::buttonClicked, [=]() {
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
  QObject::connect(aggressiveResetButton, &FrogPilotButtonsControl::buttonClicked, [=]() {
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
  QObject::connect(standardResetButton, &FrogPilotButtonsControl::buttonClicked, [=]() {
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
  QObject::connect(relaxedResetButton, &FrogPilotButtonsControl::buttonClicked, [=]() {
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

  openDescriptions(forceOpenDescriptions, toggles);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [longitudinalLayout, longitudinalPanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    longitudinalLayout->setCurrentWidget(longitudinalPanel);
  });
  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubSubPanel, [longitudinalLayout, customDrivingPersonalityPanel, speedLimitControllerPanel, this]() {
    openDescriptions(forceOpenDescriptions, toggles);

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

  calibratedLateralAccelerationLabel->setText(QString::number(params.getFloat("CalibratedLateralAcceleration"), 'f', 2) + tr(" m/s²"));
  calibrationProgressLabel->setText(QString::number(params.getFloat("CalibrationProgress"), 'f', 2) + "%");

  longitudinalActuatorDelayToggle->setTitle(QString(tr("Actuator Delay (Default: %1)")).arg(QString::number(parent->longitudinalActuatorDelay, 'f', 2)));
  startAccelToggle->setTitle(QString(tr("Start Acceleration (Default: %1)")).arg(QString::number(parent->startAccel, 'f', 2)));
  stopAccelToggle->setTitle(QString(tr("Stop Acceleration (Default: %1)")).arg(QString::number(parent->stopAccel, 'f', 2)));
  stoppingDecelRateToggle->setTitle(QString(tr("Stopping Rate (Default: %1)")).arg(QString::number(parent->stoppingDecelRate, 'f', 2)));
  vEgoStartingToggle->setTitle(QString(tr("Start Speed (Default: %1)")).arg(QString::number(parent->vEgoStarting, 'f', 2)));
  vEgoStoppingToggle->setTitle(QString(tr("Stop Speed (Default: %1)")).arg(QString::number(parent->vEgoStopping, 'f', 2)));

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
      imperialSpeedLabels[i] = i == 0 ? tr("Off") : QString::number(i) + tr(" mph");
    }

    for (int i = 0; i <= 3; ++i) {
      metricDistanceLabels[i] = i == 0 ? tr("Off") : i == 1 ? QString::number(i) + tr(" meter") : QString::number(i) + tr(" meters");
    }

    for (int i = 0; i <= 150; ++i) {
      metricSpeedLabels[i] = i == 0 ? tr("Off") : QString::number(i) + tr(" km/h");
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
    offset1Toggle->setTitle(tr("Speed Offset (0–29 km/h)"));
    offset2Toggle->setTitle(tr("Speed Offset (30–49 km/h)"));
    offset3Toggle->setTitle(tr("Speed Offset (50–59 km/h)"));
    offset4Toggle->setTitle(tr("Speed Offset (60–79 km/h)"));
    offset5Toggle->setTitle(tr("Speed Offset (80–99 km/h)"));
    offset6Toggle->setTitle(tr("Speed Offset (100–119 km/h)"));
    offset7Toggle->setTitle(tr("Speed Offset (120–140 km/h)"));

    offset1Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 0 and 24 mph."));
    offset2Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 25 and 34 mph."));
    offset3Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 35 and 44 mph."));
    offset4Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 45 and 54 mph."));
    offset5Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 55 and 64 mph."));
    offset6Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 65 and 74 mph."));
    offset7Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 75 and 99 mph."));

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
    offset1Toggle->setTitle(tr("Speed Offset (0–24 mph)"));
    offset2Toggle->setTitle(tr("Speed Offset (25–34 mph)"));
    offset3Toggle->setTitle(tr("Speed Offset (35–44 mph)"));
    offset4Toggle->setTitle(tr("Speed Offset (45–54 mph)"));
    offset5Toggle->setTitle(tr("Speed Offset (55–64 mph)"));
    offset6Toggle->setTitle(tr("Speed Offset (65–74 mph)"));
    offset7Toggle->setTitle(tr("Speed Offset (75–99 mph)"));

    offset1Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 0 and 24 mph."));
    offset2Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 25 and 34 mph."));
    offset3Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 35 and 44 mph."));
    offset4Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 45 and 54 mph."));
    offset5Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 55 and 64 mph."));
    offset6Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 65 and 74 mph."));
    offset7Toggle->setDescription(tr("<b>How much to offset posted speed-limits</b> between 75 and 99 mph."));

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
    if (parentKeys.contains(key)) {
      toggle->setVisible(false);
    }
  }

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      continue;
    }

    bool setVisible = parent->tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (key == "CustomCruise" || key == "CustomCruiseLong" || key == "SetSpeedLimit" || key == "SetSpeedOffset") {
      setVisible &= !parent->hasPCMCruise;
    }

    else if (key == "ForceMPHDashboard") {
      setVisible &= parent->isToyota;
    }

    else if (key == "MapGears") {
      setVisible &= parent->isGM || parent->isHKGCanFd || parent->isToyota;
      setVisible &= !parent->isTSK;
    }

    else if (key == "ReverseCruise") {
      setVisible &= parent->isToyota;
    }

    else if (key == "SLCMapboxFiller") {
      setVisible &= !params.get("MapboxSecretKey").empty();
    }

    else if (key == "StartAccel") {
      setVisible &= !(params.getBool("LongitudinalTune") && params.getBool("HumanAcceleration"));
    }

    else if (key == "StoppingDecelRate" || key == "VEgoStarting" || key == "VEgoStopping") {
      setVisible &= !parent->isGM || !params.getBool("ExperimentalGMTune");
      setVisible &= !parent->isToyota || !params.getBool("FrogsGoMoosTweak");
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (advancedLongitudinalTuneKeys.contains(key)) {
        toggles["AdvancedLongitudinalTune"]->setVisible(true);
      } else if (aggressivePersonalityKeys.contains(key)) {
        toggles["AggressivePersonalityProfile"]->setVisible(true);
      } else if (conditionalExperimentalKeys.contains(key)) {
        toggles["ConditionalExperimental"]->setVisible(true);
      } else if (curveSpeedKeys.contains(key)) {
        toggles["CurveSpeedController"]->setVisible(true);
      } else if (customDrivingPersonalityKeys.contains(key)) {
        toggles["CustomPersonalities"]->setVisible(true);
      } else if (longitudinalTuneKeys.contains(key)) {
        toggles["LongitudinalTune"]->setVisible(true);
      } else if (qolKeys.contains(key)) {
        toggles["QOLLongitudinal"]->setVisible(true);
      } else if (relaxedPersonalityKeys.contains(key)) {
        toggles["RelaxedPersonalityProfile"]->setVisible(true);
      } else if (speedLimitControllerKeys.contains(key)) {
        toggles["SpeedLimitController"]->setVisible(true);
      } else if (speedLimitControllerOffsetsKeys.contains(key)) {
        toggles["SLCOffsets"]->setVisible(true);
      } else if (speedLimitControllerQOLKeys.contains(key)) {
        toggles["SLCQOL"]->setVisible(true);
      } else if (speedLimitControllerVisualKeys.contains(key)) {
        toggles["SLCVisuals"]->setVisible(true);
      } else if (standardPersonalityKeys.contains(key)) {
        toggles["StandardPersonalityProfile"]->setVisible(true);
      } else if (trafficPersonalityKeys.contains(key)) {
        toggles["TrafficPersonalityProfile"]->setVisible(true);
      }
    }
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
