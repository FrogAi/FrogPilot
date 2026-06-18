#include "frogpilot/ui/qt/offroad/longitudinal_settings.h"

namespace {
float getMaxLateralAcceleration(Params &params, FrogPilotSettingsWindow *settings) {
  QJsonObject profile = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("MaxLateralAcceleration"))).object();
  if (profile.value("car_fingerprint").toString().toStdString() == settings->carFingerprint) {
    return profile.value("value").toDouble(settings->maxLateralAccel);
  }
  return settings->maxLateralAccel;
}
}

FrogPilotLongitudinalPanel::FrogPilotLongitudinalPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  networkManager = new QNetworkAccessManager(this);

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
  FrogPilotListWidget *weatherList = new FrogPilotListWidget(this);
  FrogPilotListWidget *weatherLowVisibilityList = new FrogPilotListWidget(this);
  FrogPilotListWidget *weatherRainList = new FrogPilotListWidget(this);
  FrogPilotListWidget *weatherRainStormList = new FrogPilotListWidget(this);
  FrogPilotListWidget *weatherSnowList = new FrogPilotListWidget(this);

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
  ScrollView *weatherLowVisibilityPanel = new ScrollView(weatherLowVisibilityList, this);
  ScrollView *weatherPanel = new ScrollView(weatherList, this);
  ScrollView *weatherRainPanel = new ScrollView(weatherRainList, this);
  ScrollView *weatherRainStormPanel = new ScrollView(weatherRainStormList, this);
  ScrollView *weatherSnowPanel = new ScrollView(weatherSnowList, this);

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
  longitudinalLayout->addWidget(weatherLowVisibilityPanel);
  longitudinalLayout->addWidget(weatherPanel);
  longitudinalLayout->addWidget(weatherRainPanel);
  longitudinalLayout->addWidget(weatherRainStormPanel);
  longitudinalLayout->addWidget(weatherSnowPanel);

  const std::vector<std::tuple<QString, QString, QString, QString>> longitudinalToggles {
    {"AdvancedLongitudinalTune", tr("Advanced Longitudinal Tuning"), tr("<b>Hand-set the acceleration and braking numbers openpilot normally takes from your car.</b>"), "../../frogpilot/assets/toggle_icons/icon_advanced_longitudinal_tune.png"},
    {"LongitudinalActuatorDelay", parent->longitudinalActuatorDelay != 0 ? QString(tr("Actuator Delay (Default: %1)")).arg(QString::number(parent->longitudinalActuatorDelay, 'f', 2)) : tr("Actuator Delay"), tr("<b>How long your car takes to respond after openpilot presses the gas or brake.</b><br><br>Raise it if your car feels slow to react. Lower it if it feels too eager or overshoots."), ""},
    {"MaxDesiredAcceleration", tr("Maximum Acceleration"), tr("<b>The hardest openpilot is ever allowed to accelerate, no matter which profile you pick.</b><br><br>Lower it for a calmer car everywhere. This caps acceleration only, never braking."), ""},
    {"StartAccel", parent->startAccel != 0 ? QString(tr("Start Acceleration (Default: %1)")).arg(QString::number(parent->startAccel, 'f', 2)) : tr("Start Acceleration"), tr("<b>How hard openpilot pulls away in the first moment after a stop.</b><br><br>Raise it for quicker takeoffs. Lower it for gentler ones. Only some cars use a fixed launch push like this, so on most cars this row does nothing."), ""},
    {"VEgoStarting", parent->vEgoStarting != 0 ? QString(tr("Start Speed (Default: %1)")).arg(QString::number(parent->vEgoStarting, 'f', 2)) : tr("Start Speed"), tr("<b>The speed that marks the end of pulling away from a stop, which sets how long openpilot holds its takeoff acceleration.</b><br><br>Raise it to keep that takeoff push going up to a higher speed. Lower it to hand back to normal gas and brake control almost as soon as the wheels turn, which makes the start softer. Only some cars have a separate takeoff stage at all, so on most cars this row does nothing."), ""},
    {"StopAccel", parent->stopAccel != 0 ? QString(tr("Stop Acceleration (Default: %1)")).arg(QString::number(parent->stopAccel, 'f', 2)) : tr("Stop Acceleration"), tr("<b>How hard openpilot holds the brakes as your car finishes stopping and while it sits still.</b><br><br>The number is negative because it is braking, so -2.00 holds harder than -0.50. Go more negative if your car creeps or rolls back on a hill, and closer to zero if the last moment of the stop feels too abrupt."), ""},
    {"StoppingDecelRate", parent->stoppingDecelRate != 0 ? QString(tr("Stopping Rate (Default: %1)")).arg(QString::number(parent->stoppingDecelRate, 'f', 2)) : tr("Stopping Rate"), tr("<b>How quickly openpilot builds up brake pressure as it comes to a stop.</b><br><br>Raise it for shorter, firmer stops. Lower it for longer, smoother ones."), ""},
    {"VEgoStopping", parent->vEgoStopping != 0 ? QString(tr("Stop Speed (Default: %1)")).arg(QString::number(parent->vEgoStopping, 'f', 2)) : tr("Stop Speed"), tr("<b>The speed below which openpilot treats your car as stopped and switches to holding the brakes.</b><br><br>Raise it to settle into the stop earlier and more smoothly. Lower it to keep normal braking going longer, at the risk of rolling past your mark."), ""},

    {"ConditionalExperimental", tr("Conditional Experimental Mode"), tr("<b>Automatically switch to \"Experimental Mode\" when set conditions are met.</b> Allows the model to handle challenging situations with smarter decision making."), "../../frogpilot/assets/toggle_icons/icon_conditional.png"},
    {"CESpeed", tr("Below"), tr("<b>Switch to \"Experimental Mode\" below this speed when there is no car ahead of you.</b><br><br>It helps openpilot handle slow, fiddly situations more smoothly."), ""},
    {"CECurves", tr("Curve Detected Ahead"), tr("<b>Switch to \"Experimental Mode\" when openpilot sees a curve coming up.</b><br><br>The model picks its own speed for the curve instead of holding your set speed."), ""},
    {"CEStopLights", tr("\"Detected\" Stop Lights/Signs"), tr("<b>Switch to \"Experimental Mode\" whenever the driving model \"detects\" a red light or stop sign.</b><br><br>It only fires when there is no car close ahead of you, so it stays quiet when you roll up to a red light behind traffic.<br><br><i><b>Disclaimer</b>: openpilot does not explicitly detect traffic lights or stop signs. In \"Experimental Mode\", openpilot makes end-to-end driving decisions from camera input, which means it may stop even when there's no clear reason!</i>"), ""},
    {"CELead", tr("Lead Detected Ahead"), tr("<b>Switch to \"Experimental Mode\" when the car ahead is slower than you or has stopped.</b><br><br>\"Slower Lead\" and \"Stopped Lead\" both start off, so pick at least one with the buttons on this row or nothing happens."), ""},
    {"CEModelStopTime", tr("Predicted Stop In"), tr("<b>Switch to \"Experimental Mode\" when openpilot predicts a stop within the set time.</b> This is usually triggered when the model \"sees\" a red light or stop sign ahead.<br><br><i><b>Disclaimer</b>: openpilot does not explicitly detect traffic lights or stop signs. In \"Experimental Mode\", openpilot makes end-to-end driving decisions from camera input, which means it may stop even when there's no clear reason!</i>"), ""},
    {"CESignalSpeed", tr("Turn Signal Below"), tr("<b>Switch to \"Experimental Mode\" when you signal below the speed you set, so openpilot picks its own speed through the turn instead of holding your set speed.</b><br><br>This runs off the \"Not For Detected Lanes\" button on this row, which has to stay on. With it on, openpilot only reads a signal as a turn when the space beside you is narrower than the \"Minimum Lane Width\" under \"Lane Changes\" in the \"STEERING\" panel. That width starts at zero, so nothing happens until you raise it, and turning the button off stops it firing at all."), ""},
    {"ShowCEMStatus", tr("Status Widget"), tr("<b>Show which condition switched \"Experimental Mode\" on, right on the driving screen.</b>"), ""},

    {"CurveSpeedController", tr("Curve Speed Controller"), tr("<b>openpilot slows down on its own for curves ahead, and you pick how fast it takes them with \"Curve Speed Profile\".</b><br><br>It comes set to \"Adaptive\", which learns how you prefer to take curves."), "../../frogpilot/assets/toggle_icons/icon_speed_map.png"},
    {"CalibratedLateralAcceleration", tr("Calibrated Lateral Acceleration"), tr("<b>How hard you corner, learned from your own driving.</b><br><br>The \"Adaptive\" profile uses this to take curves the way you do, but never harder than your steering has proven it can hold. A higher number means carrying more speed through curves. Lower means taking them gentler."), ""},
    {"CalibrationProgress", tr("Calibration Progress"), tr("<b>How much of your own cornering openpilot has learned from.</b><br><br>This only grows while you're the one controlling the speed, so it fills up as you drive curves yourself. At 100% openpilot has gathered enough of your cornering to match the way you take curves."), ""},
    {"CurveSpeedProfile", tr("Curve Speed Profile"), tr("<b>How fast openpilot takes curves.</b><br><br>\"Gentle\" and \"Standard\" hold to a fixed, relaxed pace, \"Sport\" uses your car's maximum configured or live-tuned cornering limit, and \"Adaptive\" learns how you prefer to take curves."), ""},
    {"MaxLateralAcceleration", tr("Maximum Lateral Acceleration"), tr("<b>How fast the \"Sport\" profile is allowed to take curves.</b><br><br>This is also the maximum for every other curve speed profile. openpilot learns the limit from your car when possible and otherwise uses the value configured for it."), ""},
    {"ResetCurveData", tr("Reset Curve Data"), tr("<b>Throw away everything openpilot has learned about how you take curves and start over.</b><br><br>\"Adaptive\" goes back to its starting value and relearns as you drive. Only available while the car is off."), ""},
    {"ShowCSCStatus", tr("Status Widget"), tr("<b>Show the speed openpilot is aiming for through the curve, right on the driving screen.</b><br><br>It also shows a \"Training...\" note while openpilot is learning from the way you take a curve yourself."), ""},

    {"CustomPersonalities", tr("Driving Personalities"), tr("<b>Change what Aggressive, Standard and Relaxed actually do, so they match how you like to drive.</b>"), "../../frogpilot/assets/toggle_icons/icon_personality.png"},

    {"AggressivePersonalityProfile", tr("Aggressive"), tr("<b>Customize the \"Aggressive\" personality profile.</b> Designed for assertive driving with tighter gaps."), "../../frogpilot/assets/stock_theme/distance_icons/aggressive.png"},
    {"AggressiveFollow", tr("Following Distance"), tr("<b>How many seconds of space openpilot keeps behind the car ahead with the \"Aggressive\" profile.</b><br><br>Raise it for more room. Lower it for tighter gaps.<br><br>Default: 1.25 seconds."), ""},
    {"AggressiveJerkAcceleration", tr("Acceleration Smoothness"), tr("<b>How smoothly openpilot changes its acceleration any time your car is not slowing down with the \"Aggressive\" profile, not just when pulling away from a stop.</b><br><br>Raise it for gentler starts and pickup. Lower it for faster but more abrupt ones. \"Speed-Up Response\" is a second smoothness control over those same moments, and this one is switched off entirely while your car is sitting still. \"Traffic Mode\" ignores both and uses its own fixed values."), ""},
    {"AggressiveJerkDeceleration", tr("Braking Smoothness"), tr("<b>How smoothly openpilot changes its braking any time your car is slowing down with the \"Aggressive\" profile, not just when coming to a stop.</b><br><br>Raise it for gentler stops and slowdowns. Lower it for quicker but sharper ones. \"Slowdown Response\" is a second smoothness control over those same moments, and this one is switched off entirely while your car is sitting still. \"Traffic Mode\" ignores both and uses its own fixed values."), ""},
    {"AggressiveJerkDanger", tr("Safety Gap Bias"), tr("<b>How hard openpilot works to protect your following distance with the \"Aggressive\" profile.</b><br><br>Raise it and openpilot reacts sooner and harder when the gap starts closing. Lower it and it tolerates the gap shrinking before doing anything."), ""},
    {"AggressiveJerkSpeedDecrease", tr("Slowdown Response"), tr("<b>How smoothly openpilot sheds speed any time your car is slowing down with the \"Aggressive\" profile, like easing off for a slower car ahead.</b><br><br>Raise it for more gradual slowdowns. Lower it for faster but sharper ones. It works alongside \"Braking Smoothness\", a separate control over those same moments. \"Traffic Mode\" ignores both and uses its own fixed values."), ""},
    {"AggressiveJerkSpeed", tr("Speed-Up Response"), tr("<b>How smoothly openpilot builds speed any time your car is not slowing down with the \"Aggressive\" profile, like catching up to your set speed.</b><br><br>Raise it for more gradual pickup. Lower it for quicker but more jolting pickup. It works alongside \"Acceleration Smoothness\", a separate control over those same moments. \"Traffic Mode\" ignores both and uses its own fixed values."), ""},
    {"ResetAggressivePersonality", tr("Reset to Defaults"), tr("<b>Put every \"Aggressive\" profile value back the way it shipped.</b><br><br>Only affects this profile. Your other profiles are left alone."), ""},

    {"StandardPersonalityProfile", tr("Standard"), tr("<b>Customize the \"Standard\" personality profile.</b> Designed for balanced driving with moderate gaps."), "../../frogpilot/assets/stock_theme/distance_icons/standard.png"},
    {"StandardFollow", tr("Following Distance"), tr("<b>How many seconds of space openpilot keeps behind the car ahead with the \"Standard\" profile.</b><br><br>Raise it for more room. Lower it for tighter gaps.<br><br>Default: 1.45 seconds."), ""},
    {"StandardJerkAcceleration", tr("Acceleration Smoothness"), tr("<b>How smoothly openpilot changes its acceleration any time your car is not slowing down with the \"Standard\" profile, not just when pulling away from a stop.</b><br><br>Raise it for gentler starts and pickup. Lower it for faster but more abrupt ones. \"Speed-Up Response\" is a second smoothness control over those same moments, and this one is switched off entirely while your car is sitting still. \"Traffic Mode\" ignores both and uses its own fixed values."), ""},
    {"StandardJerkDeceleration", tr("Braking Smoothness"), tr("<b>How smoothly openpilot changes its braking any time your car is slowing down with the \"Standard\" profile, not just when coming to a stop.</b><br><br>Raise it for gentler stops and slowdowns. Lower it for quicker but sharper ones. \"Slowdown Response\" is a second smoothness control over those same moments, and this one is switched off entirely while your car is sitting still. \"Traffic Mode\" ignores both and uses its own fixed values."), ""},
    {"StandardJerkDanger", tr("Safety Gap Bias"), tr("<b>How hard openpilot works to protect your following distance with the \"Standard\" profile.</b><br><br>Raise it and openpilot reacts sooner and harder when the gap starts closing. Lower it and it tolerates the gap shrinking before doing anything."), ""},
    {"StandardJerkSpeedDecrease", tr("Slowdown Response"), tr("<b>How smoothly openpilot sheds speed any time your car is slowing down with the \"Standard\" profile, like easing off for a slower car ahead.</b><br><br>Raise it for more gradual slowdowns. Lower it for faster but sharper ones. It works alongside \"Braking Smoothness\", a separate control over those same moments. \"Traffic Mode\" ignores both and uses its own fixed values."), ""},
    {"StandardJerkSpeed", tr("Speed-Up Response"), tr("<b>How smoothly openpilot builds speed any time your car is not slowing down with the \"Standard\" profile, like catching up to your set speed.</b><br><br>Raise it for more gradual pickup. Lower it for quicker but more jolting pickup. It works alongside \"Acceleration Smoothness\", a separate control over those same moments. \"Traffic Mode\" ignores both and uses its own fixed values."), ""},
    {"ResetStandardPersonality", tr("Reset to Defaults"), tr("<b>Put every \"Standard\" profile value back the way it shipped.</b><br><br>Only affects this profile. Your other profiles are left alone."), ""},

    {"RelaxedPersonalityProfile", tr("Relaxed"), tr("<b>Customize the \"Relaxed\" personality profile.</b> Designed for smoother, more comfortable driving with larger gaps."), "../../frogpilot/assets/stock_theme/distance_icons/relaxed.png"},
    {"RelaxedFollow", tr("Following Distance"), tr("<b>How many seconds of space openpilot keeps behind the car ahead with the \"Relaxed\" profile.</b><br><br>Raise it for more room. Lower it for tighter gaps.<br><br>Default: 1.75 seconds."), ""},
    {"RelaxedJerkAcceleration", tr("Acceleration Smoothness"), tr("<b>How smoothly openpilot changes its acceleration any time your car is not slowing down with the \"Relaxed\" profile, not just when pulling away from a stop.</b><br><br>Raise it for gentler starts and pickup. Lower it for faster but more abrupt ones. \"Speed-Up Response\" is a second smoothness control over those same moments, and this one is switched off entirely while your car is sitting still. \"Traffic Mode\" ignores both and uses its own fixed values."), ""},
    {"RelaxedJerkDeceleration", tr("Braking Smoothness"), tr("<b>How smoothly openpilot changes its braking any time your car is slowing down with the \"Relaxed\" profile, not just when coming to a stop.</b><br><br>Raise it for gentler stops and slowdowns. Lower it for quicker but sharper ones. \"Slowdown Response\" is a second smoothness control over those same moments, and this one is switched off entirely while your car is sitting still. \"Traffic Mode\" ignores both and uses its own fixed values."), ""},
    {"RelaxedJerkDanger", tr("Safety Gap Bias"), tr("<b>How hard openpilot works to protect your following distance with the \"Relaxed\" profile.</b><br><br>Raise it and openpilot reacts sooner and harder when the gap starts closing. Lower it and it tolerates the gap shrinking before doing anything."), ""},
    {"RelaxedJerkSpeedDecrease", tr("Slowdown Response"), tr("<b>How smoothly openpilot sheds speed any time your car is slowing down with the \"Relaxed\" profile, like easing off for a slower car ahead.</b><br><br>Raise it for more gradual slowdowns. Lower it for faster but sharper ones. It works alongside \"Braking Smoothness\", a separate control over those same moments. \"Traffic Mode\" ignores both and uses its own fixed values."), ""},
    {"RelaxedJerkSpeed", tr("Speed-Up Response"), tr("<b>How smoothly openpilot builds speed any time your car is not slowing down with the \"Relaxed\" profile, like catching up to your set speed.</b><br><br>Raise it for more gradual pickup. Lower it for quicker but more jolting pickup. It works alongside \"Acceleration Smoothness\", a separate control over those same moments. \"Traffic Mode\" ignores both and uses its own fixed values."), ""},
    {"ResetRelaxedPersonality", tr("Reset to Defaults"), tr("<b>Put every \"Relaxed\" profile value back the way it shipped.</b><br><br>Only affects this profile. Your other profiles are left alone."), ""},

    {"LongitudinalTune", tr("Longitudinal Tuning"), tr("<b>Change how openpilot speeds up and slows down.</b>"), "../../frogpilot/assets/toggle_icons/icon_longitudinal_tune.png"},
    {"AccelerationProfile", tr("Acceleration Profile"), tr("<b>How quickly openpilot speeds up.</b><br><br>\"Standard\" is openpilot's normal acceleration, \"Eco\" is gentle and efficient, \"Sport\" is firmer and more responsive, and \"Sport+\" accelerates as hard as your car allows. None of these apply while \"Experimental Mode\" is running, including when \"Conditional Experimental Mode\" switches it on for you."), ""},
    {"DecelerationProfile", tr("Deceleration Profile"), tr("<b>How gently openpilot slows down when the road ahead is clear.</b><br><br>\"Standard\" brakes at full strength, \"Eco\" brakes about half as hard so you coast more, and \"Eco+\" brakes about a quarter as hard for the gentlest slowdowns. Whenever there is a car ahead, or while \"Experimental Mode\" is running, openpilot goes back to full-strength braking so it can still stop in time."), ""},
    {"HumanAcceleration", tr("Human-Like Acceleration"), tr("<b>openpilot builds speed more like a person, holding back while your set speed is low and easing off as you close in on it.</b><br><br>At a 25 mph set speed you get roughly half the acceleration you otherwise would. On cars with a fixed launch push, this replaces that push with the same smooth target openpilot uses everywhere else, and the \"Start Acceleration\" row disappears while this is on."), ""},
    {"HumanFollowing", tr("Human-Like Following"), tr("<b>openpilot follows the car ahead more naturally, reading where that car is headed so it eases off and starts braking earlier and softer instead of reacting late.</b><br><br>On the default driving model this only works while openpilot can clearly see the car ahead. Older driving models need radar for it, and on a radarless car with an older model nothing changes."), ""},
    {"HumanLaneChanges", tr("Human-Like Lane Changes"), tr("<b>openpilot watches the cars in the lane it is moving into and adjusts its speed for them during a lane change.</b><br><br>Without this it only reacts to the car directly ahead until the move is finished."), ""},
    {"LeadDetectionThreshold", tr("Lead Detection Sensitivity"), tr("<b>How sure openpilot has to be that something in front of you is really a car before it starts following it and braking for it.</b><br><br>Lower numbers pick up cars sooner and farther away, with more chances of reacting to something that is not a moving car. Higher numbers wait for a clearer look, so there are fewer false alarms but openpilot notices the car ahead later.<br><br>Default: 35%."), ""},
    {"TacoTune", tr("\"Taco Bell Run\" Turn Speed Hack"), tr("<b>openpilot slows down more for left and right turns, using the trick comma built for their 2022 \"Taco Bell Run\" drive.</b><br><br>It never switches off, so it is still working on fast highway curves. What stops at about 45 mph is the ramp: openpilot allows more cornering force the faster you go, up to that speed, then holds it flat. That makes the slowdown most noticeable in slow, tight turns."), ""},

    {"QOLLongitudinal", tr("Quality of Life"), tr("<b>Smaller changes to how openpilot handles the gas and brake.</b>"), "../../frogpilot/assets/toggle_icons/icon_quality_of_life.png"},
    {"CustomCruise", tr("Cruise Interval"), tr("<b>How much your set speed moves with each tap of the + or - cruise button.</b><br><br>Set it to 1 to land on any speed exactly, or higher to get where you are going in fewer taps."), ""},
    {"CustomCruiseLong", tr("Cruise Interval (Hold)"), tr("<b>How much your set speed moves while you hold the + or - cruise button down.</b><br><br>The default is 5, against 1 for a single tap."), ""},
    {"ForceStops", tr("Force Stop at \"Detected\" Stop Lights/Signs"), tr("<b>openpilot comes to a full stop whenever it thinks it sees a red light or stop sign, whether or not \"Experimental Mode\" is running.</b><br><br>It only kicks in when openpilot is not already tracking a car ahead, so behind a queue at a light your normal following does the stopping instead. Touching the gas cancels a forced stop for the next 10 seconds.<br><br><i><b>Heads up</b>: openpilot never actually reads traffic lights or stop signs. It decides from what the camera sees, so it can stop when there is no reason to.</i>"), ""},
    {"IncreasedStoppedDistance", tr("Increase Stopped Distance by:"), tr("<b>Adds a set amount of extra room between you and the car ahead, and keeps that room at every speed, not just when you are stopped.</b><br><br>You notice it most at red lights, where a few feet stops openpilot creeping up close. While moving, that same room means openpilot starts slowing a little sooner. \"Traffic Mode\" ignores this setting."), ""},
    {"MapGears", tr("Map Accel/Decel to Gears"), tr("<b>Lets your car's \"Eco\" and \"Sport\" gear modes take over how openpilot speeds up, how it slows down, or both.</b><br><br>Pick \"Acceleration\", \"Deceleration\" or both with the buttons on this row, since neither starts on and nothing changes until you do.<br><br>\"Eco\" gear makes openpilot accelerate gently and \"Sport\" gear makes it accelerate firmly. Braking goes the other way: \"Eco\" gear halves how hard openpilot can brake and \"Sport\" gear cuts it to a quarter, so \"Sport\" coasts the longest. The braking change only applies when there is no car ahead."), ""},
    {"SetSpeedOffset", tr("Offset Set Speed by:"), tr("<b>Adds an extra amount on top of the \"Cruise Interval (Hold)\" step, but only when you press and hold the + cruise button.</b><br><br>The - button does not mirror it. Holding - moves your set speed down by twice the \"Cruise Interval (Hold)\" amount minus this offset, so with the shipped 5 hold interval and 5 chosen here you just get a plain 5 down. A quick tap is never affected, and 0 turns this off."), ""},
    {"ReverseCruise", tr("Reverse Cruise Increase"), tr("<b>Every tap of the + cruise button raises your set speed by 5 instead of 1, the same amount you already get from holding it.</b><br><br>Leave it off when you want to land on an exact speed like 63."), ""},
    {"WeatherPresets", tr("Weather Condition Offsets"), tr("<b>openpilot drives more cautiously on its own when the weather turns bad.</b><br><br>It checks the current weather where you are and applies whichever set of adjustments below matches. Every one of those adjustments starts at 0, so a weather icon shows up on the driving screen but nothing about how openpilot drives changes until you open the sets below and put in your own numbers."), ""},

    {"LowVisibilityOffsets", tr("Low Visibility"), tr("<b>How openpilot drives when fog or haze cuts your visibility.</b><br><br>These add to your normal settings rather than replacing them."), ""},
    {"IncreaseFollowingLowVisibility", tr("Increase Following Distance by:"), tr("<b>Adds extra seconds of space between you and the car ahead in low visibility.</b><br><br>Your total gap never goes past 3.00 seconds, so this only adds what is left below that."), ""},
    {"IncreasedStoppedDistanceLowVisibility", tr("Increase Stopped Distance by:"), tr("<b>Adds extra room between you and the car ahead in low visibility, at every speed and not just when stopped.</b><br><br>This stacks on top of the same setting under \"Quality of Life\"."), ""},
    {"ReduceAccelerationLowVisibility", tr("Reduce Acceleration by:"), tr("<b>Holds openpilot back from accelerating as hard in low visibility.</b>"), ""},
    {"ReduceLateralAccelerationLowVisibility", tr("Reduce Cornering Force by:"), tr("<b>Eases off how hard openpilot corners in low visibility.</b><br><br>Only does anything while \"Curve Speed Controller\" is on. Raise it for gentler, safer cornering on a slippery road. Curve speed drops by less than this number, because cornering force rises with the square of speed."), ""},

    {"RainOffsets", tr("Rain"), tr("<b>How openpilot drives in the rain.</b><br><br>These add to your normal settings rather than replacing them."), ""},
    {"IncreaseFollowingRain", tr("Increase Following Distance by:"), tr("<b>Adds extra seconds of space between you and the car ahead in rain.</b><br><br>Your total gap never goes past 3.00 seconds, so this only adds what is left below that."), ""},
    {"IncreasedStoppedDistanceRain", tr("Increase Stopped Distance by:"), tr("<b>Adds extra room between you and the car ahead in rain, at every speed and not just when stopped.</b><br><br>This stacks on top of the same setting under \"Quality of Life\"."), ""},
    {"ReduceAccelerationRain", tr("Reduce Acceleration by:"), tr("<b>Holds openpilot back from accelerating as hard in rain.</b><br><br>Raise it for softer, more controlled pickup on a slippery road."), ""},
    {"ReduceLateralAccelerationRain", tr("Reduce Cornering Force by:"), tr("<b>Eases off how hard openpilot corners in rain.</b><br><br>Only does anything while \"Curve Speed Controller\" is on. Raise it for gentler, safer cornering on a slippery road. Curve speed drops by less than this number, because cornering force rises with the square of speed."), ""},

    {"RainStormOffsets", tr("Rainstorms"), tr("<b>How openpilot drives in heavy rain.</b><br><br>These add to your normal settings rather than replacing them."), ""},
    {"IncreaseFollowingRainStorm", tr("Increase Following Distance by:"), tr("<b>Adds extra seconds of space between you and the car ahead in a rainstorm.</b><br><br>Your total gap never goes past 3.00 seconds, so this only adds what is left below that."), ""},
    {"IncreasedStoppedDistanceRainStorm", tr("Increase Stopped Distance by:"), tr("<b>Adds extra room between you and the car ahead in a rainstorm, at every speed and not just when stopped.</b><br><br>This stacks on top of the same setting under \"Quality of Life\"."), ""},
    {"ReduceAccelerationRainStorm", tr("Reduce Acceleration by:"), tr("<b>Holds openpilot back from accelerating as hard in a rainstorm.</b><br><br>Raise it for softer, more controlled pickup on a slippery road."), ""},
    {"ReduceLateralAccelerationRainStorm", tr("Reduce Cornering Force by:"), tr("<b>Eases off how hard openpilot corners in a rainstorm.</b><br><br>Only does anything while \"Curve Speed Controller\" is on. Raise it for gentler, safer cornering on a slippery road. Curve speed drops by less than this number, because cornering force rises with the square of speed."), ""},

    {"SnowOffsets", tr("Snow"), tr("<b>How openpilot drives in snow.</b><br><br>These add to your normal settings rather than replacing them."), ""},
    {"IncreaseFollowingSnow", tr("Increase Following Distance by:"), tr("<b>Adds extra seconds of space between you and the car ahead in snow.</b><br><br>Your total gap never goes past 3.00 seconds, so this only adds what is left below that."), ""},
    {"IncreasedStoppedDistanceSnow", tr("Increase Stopped Distance by:"), tr("<b>Adds extra room between you and the car ahead in snow, at every speed and not just when stopped.</b><br><br>This stacks on top of the same setting under \"Quality of Life\"."), ""},
    {"ReduceAccelerationSnow", tr("Reduce Acceleration by:"), tr("<b>Holds openpilot back from accelerating as hard in snow.</b><br><br>Raise it for softer, more controlled pickup on a slippery road."), ""},
    {"ReduceLateralAccelerationSnow", tr("Reduce Cornering Force by:"), tr("<b>Eases off how hard openpilot corners in snow.</b><br><br>Only does anything while \"Curve Speed Controller\" is on. Raise it for gentler, safer cornering on a slippery road. Curve speed drops by less than this number, because cornering force rises with the square of speed."), ""},

    {"SetWeatherKey", tr("Set Your Own Key"), tr("<b>Set your own \"OpenWeatherMap\" key to increase the weather update rate.</b><br><br><i>Personal keys grant 1,000 free calls per day, allowing for updates every minute. The default key is shared and only updates every 15 minutes.</i>"), ""},

    {"SpeedLimitController", tr("Speed Limit Controller"), tr("<b>Hold openpilot's max speed to the posted speed limit.</b><br><br>The limit comes from your downloaded maps, Mapbox, \"Navigate on openpilot\", or, on supported Ford, Genesis, Hyundai, Kia, Lexus and Toyota models, your dashboard."), "../../frogpilot/assets/toggle_icons/icon_speed_limit.png"},
    {"SLCFallback", tr("Fallback Speed"), tr("<b>The speed used by \"Speed Limit Controller\" when no speed limit is found.</b><br><br>- <b>Set Speed</b>: Use the cruise set speed<br>- <b>Experimental Mode</b>: Let openpilot pick the speed from what the camera sees, never going above your set speed<br>- <b>Previous Limit</b>: Keep using the last confirmed limit"), ""},
    {"SLCOverride", tr("Override Speed"), tr("<b>The speed used by \"Speed Limit Controller\" after you manually drive faster than the posted limit.</b><br><br>- <b>None</b>: Go back to the posted limit as soon as you are off the gas<br>- <b>Set With Gas Pedal</b>: Use the highest speed reached while pressing the gas<br>- <b>Max Set Speed</b>: Use the cruise set speed<br><br>Overrides clear when openpilot disengages."), ""},
    {"SLCQOL", tr("Quality of Life"), tr("<b>Smaller changes to how \"Speed Limit Controller\" behaves.</b>"), ""},
    {"SLCConfirmation", tr("Confirm New Speed Limits"), tr("<b>Ask before changing to a new speed limit, with the \"Lower Limits\" and \"Higher Limits\" buttons choosing which changes need your approval.</b><br><br>Neither button starts on, so pick at least one or openpilot keeps accepting every new limit without asking. To accept, tap the flashing widget on the driving screen or press the Cruise Increase button. To deny, press Cruise Decrease or ignore it for 30 seconds."), ""},
    {"SLCLookaheadHigher", tr("Higher Limit Lookahead Time"), tr("<b>How far ahead openpilot looks for a higher speed limit coming up.</b><br><br>This reads from your downloaded map data."), ""},
    {"SLCLookaheadLower", tr("Lower Limit Lookahead Time"), tr("<b>How far ahead openpilot looks for a lower speed limit coming up.</b><br><br>This reads from your downloaded map data."), ""},
    {"SetSpeedLimit", tr("Match Speed Limit on Engage"), tr("<b>Engaging openpilot sets your max speed to the current speed limit with your \"Speed Limit Offsets\" added on top.</b><br><br>The offsets do not start at zero, so set them all to 0 if you want the max speed to land on the posted number. This only happens when openpilot has no set speed to go back to, since engaging with the Resume or + button brings back your last set speed instead."), ""},
    {"SLCMapboxFiller", tr("Use Mapbox as Fallback"), tr("<b>Fall back to Mapbox for the speed limit when none of your chosen sources have one.</b><br><br>Needs your Public Mapbox Key and a working internet connection."), ""},
    {"SLCPriority", tr("Speed Limit Source Priority"), tr("<b>Choose which sources openpilot checks for the speed limit and in what order, or have it always use the highest or lowest limit being reported.</b><br><br>Pick up to two sources and openpilot uses the first one that currently has a limit. \"Highest\" and \"Lowest\" ignore the order and take the fastest or slowest limit any source reports, so one wrong map entry can hold you well below the posted limit."), ""},
    {"SLCOffsets", tr("Speed Limit Offsets"), tr("<b>Drive a set amount above or below the posted speed limit.</b><br><br>Each speed range below gets its own offset."), ""},
    {"Offset1", tr("Speed Offset (0–24 mph)"), tr("<b>How far above or below the posted limit openpilot drives between 0 and 24 mph.</b>"), ""},
    {"Offset2", tr("Speed Offset (25–34 mph)"), tr("<b>How far above or below the posted limit openpilot drives between 25 and 34 mph.</b>"), ""},
    {"Offset3", tr("Speed Offset (35–44 mph)"), tr("<b>How far above or below the posted limit openpilot drives between 35 and 44 mph.</b>"), ""},
    {"Offset4", tr("Speed Offset (45–54 mph)"), tr("<b>How far above or below the posted limit openpilot drives between 45 and 54 mph.</b>"), ""},
    {"Offset5", tr("Speed Offset (55–64 mph)"), tr("<b>How far above or below the posted limit openpilot drives between 55 and 64 mph.</b>"), ""},
    {"Offset6", tr("Speed Offset (65–74 mph)"), tr("<b>How far above or below the posted limit openpilot drives between 65 and 74 mph.</b>"), ""},
    {"Offset7", tr("Speed Offset (75–99 mph)"), tr("<b>How far above or below the posted limit openpilot drives between 75 and 99 mph.</b>"), ""},
    {"SLCVisuals", tr("Visual Settings"), tr("<b>Change how \"Speed Limit Controller\" appears on the driving screen.</b>"), ""},
    {"ShowSLCOffset", tr("Show Speed Limit Offset"), tr("<b>The speed limit sign on the driving screen shows the posted limit with your offset printed underneath it.</b><br><br>With this off, the offset is added into the number on the sign instead, so a 65 mph road with a +10 offset shows 75 rather than 65."), ""},
    {"SpeedLimitSources", tr("Show Speed Limit Sources"), tr("<b>Show every speed limit source and what each one currently reports, on the driving screen.</b><br><br>Useful for working out which source to trust before setting your priority order."), ""}
  };

  for (const auto &[param, title, desc, icon] : longitudinalToggles) {
    QWidget *longitudinalToggle;

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
      vEgoStartingToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.01, 1, tr(" m/s"), std::map<float, QString>(), 0.01);
      longitudinalToggle = vEgoStartingToggle;
    } else if (param == "StopAccel") {
      stopAccelToggle = new FrogPilotParamValueControl(param, title, desc, icon, -4, 0, tr(" m/s²"), std::map<float, QString>(), 0.01, true);
      longitudinalToggle = stopAccelToggle;
    } else if (param == "StoppingDecelRate") {
      stoppingDecelRateToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.001, 1, tr(" m/s²/s"), std::map<float, QString>(), 0.001, true);
      longitudinalToggle = stoppingDecelRateToggle;
    } else if (param == "VEgoStopping") {
      vEgoStoppingToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.01, 1, tr(" m/s"), std::map<float, QString>(), 0.01);
      longitudinalToggle = vEgoStoppingToggle;

    } else if (param == "ConditionalExperimental") {
      FrogPilotManageControl *conditionalExperimentalToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(conditionalExperimentalToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, conditionalExperimentalPanel]() {
        longitudinalLayout->setCurrentWidget(conditionalExperimentalPanel);
      });
      longitudinalToggle = conditionalExperimentalToggle;
    } else if (param == "CESpeed") {
      FrogPilotParamValueControl *CESpeed = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr(" mph"), std::map<float, QString>(), 1, true, 175);
      FrogPilotParamValueControl *CESpeedLead = new FrogPilotParamValueControl("CESpeedLead", tr("With Lead"), tr("<b>Switch to \"Experimental Mode\" below this speed when you are close behind the car ahead.</b><br><br>\"With Lead\" means right up behind it, not just any car ahead. Below about 10 mph that window is narrower than the gap openpilot itself keeps, so the car stops counting and the \"Below\" value is used instead."), icon, 0, 99, tr(" mph"), std::map<float, QString>(), 1, true, 175);
      FrogPilotDualParamValueControl *conditionalSpeeds = new FrogPilotDualParamValueControl(CESpeed, CESpeedLead);
      longitudinalToggle = conditionalSpeeds;
    } else if (param == "CECurves") {
      std::vector<QString> curveToggles{"CECurvesLead"};
      std::vector<QString> curveToggleNames{tr("With Lead")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, curveToggles, curveToggleNames);
    } else if (param == "CELead") {
      std::vector<QString> leadToggles{"CESlowerLead", "CEStoppedLead"};
      std::vector<QString> leadToggleNames{tr("Slower Lead"), tr("Stopped Lead")};
      longitudinalToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, leadToggles, leadToggleNames);
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
    } else if (param == "CurveSpeedProfile") {
      std::vector<QString> curveSpeedProfiles{tr("Gentle"), tr("Standard"), tr("Sport"), tr("Adaptive")};
      longitudinalToggle = new ButtonParamControl(param, title, desc, icon, curveSpeedProfiles);
    } else if (param == "CalibrationProgress") {
      calibrationProgressLabel = new LabelControl(title, QString::number(params.getFloat("CalibrationProgress"), 'f', 0) + "%", desc);
      longitudinalToggle = calibrationProgressLabel;
    } else if (param == "CalibratedLateralAcceleration") {
      calibratedLateralAccelerationLabel = new LabelControl(title, QString::number(params.getFloat("CalibratedLateralAcceleration"), 'f', 2) + tr(" m/s²"), desc);
      longitudinalToggle = calibratedLateralAccelerationLabel;
    } else if (param == "MaxLateralAcceleration") {
      maxLateralAccelerationLabel = new LabelControl(title, QString::number(getMaxLateralAcceleration(params, parent), 'f', 2) + tr(" m/s²"), desc);
      longitudinalToggle = maxLateralAccelerationLabel;
    } else if (param == "ResetCurveData") {
      ButtonControl *resetCurveDataButton = new ButtonControl(title, tr("RESET"), desc);
      QObject::connect(resetCurveDataButton, &ButtonControl::clicked, [this]() {
        if (uiState()->scene.started) {
          ConfirmationDialog::alert(tr("Curve data can't be reset while the car is on. Turn the car off and try again."), this);
          return;
        }
        if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your curvature data?"), this)) {
          params.putFloat("CalibratedLateralAcceleration", 2.00);
          params.remove("CalibrationProgress");
          params.remove("CurvatureData");
          params.remove("MaxLateralAcceleration");

          calibratedLateralAccelerationLabel->setText(QString::number(2.00, 'f', 2) + tr(" m/s²"));
          calibrationProgressLabel->setText(QString::number(0.00, 'f', 0) + "%");
          maxLateralAccelerationLabel->setText(QString::number(this->parent->maxLateralAccel, 'f', 2) + tr(" m/s²"));
        }
      });
      longitudinalToggle = resetCurveDataButton;

    } else if (param == "CustomPersonalities") {
      FrogPilotManageControl *customPersonalitiesToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(customPersonalitiesToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, customDrivingPersonalityPanel]() {
        longitudinalLayout->setCurrentWidget(customDrivingPersonalityPanel);
      });
      longitudinalToggle = customPersonalitiesToggle;
    } else if (param == "ResetAggressivePersonality" || param == "ResetStandardPersonality" || param == "ResetRelaxedPersonality") {
      ButtonControl *resetButton = new ButtonControl(title, tr("RESET"), desc);
      longitudinalToggle = resetButton;
    } else if (param == "AggressivePersonalityProfile") {
      FrogPilotButtonsControl *aggressivePersonalityToggle = new FrogPilotButtonsControl(title, desc, icon, {tr("MANAGE")});
      QObject::connect(aggressivePersonalityToggle, &FrogPilotButtonsControl::buttonClicked, [longitudinalLayout, aggressivePersonalityPanel, this](int id) {
        openSubSubPanel();

        longitudinalLayout->setCurrentWidget(aggressivePersonalityPanel);

        customPersonalityOpen = true;
      });
      longitudinalToggle = aggressivePersonalityToggle;
    } else if (param == "StandardPersonalityProfile") {
      FrogPilotButtonsControl *standardPersonalityToggle = new FrogPilotButtonsControl(title, desc, icon, {tr("MANAGE")});
      QObject::connect(standardPersonalityToggle, &FrogPilotButtonsControl::buttonClicked, [longitudinalLayout, standardPersonalityPanel, this](int id) {
        openSubSubPanel();

        longitudinalLayout->setCurrentWidget(standardPersonalityPanel);

        customPersonalityOpen = true;
      });
      longitudinalToggle = standardPersonalityToggle;
    } else if (param == "RelaxedPersonalityProfile") {
      FrogPilotButtonsControl *relaxedPersonalityToggle = new FrogPilotButtonsControl(title, desc, icon, {tr("MANAGE")});
      QObject::connect(relaxedPersonalityToggle, &FrogPilotButtonsControl::buttonClicked, [longitudinalLayout, relaxedPersonalityPanel, this](int id) {
        openSubSubPanel();

        longitudinalLayout->setCurrentWidget(relaxedPersonalityPanel);

        customPersonalityOpen = true;
      });
      longitudinalToggle = relaxedPersonalityToggle;
    } else if (aggressivePersonalityKeys.contains(param) || standardPersonalityKeys.contains(param) || relaxedPersonalityKeys.contains(param)) {
      if (param == "AggressiveFollow" || param == "StandardFollow" || param == "RelaxedFollow") {
        std::map<float, QString> followTimeLabels;
        for (float i = 0; i <= 3; i += 0.01) {
          followTimeLabels[i] = std::lround(i / 0.01) == 1 / 0.01 ? QString::number(i, 'f', 2) + tr(" second") : QString::number(i, 'f', 2) + tr(" seconds");
        }
        longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 3, QString(), followTimeLabels, 0.01, true);
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
      std::vector<QString> decelerationProfiles{tr("Standard"), tr("Eco"), tr("Eco+")};
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
    } else if (param == "WeatherPresets") {
      FrogPilotManageControl *weatherToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(weatherToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, weatherPanel, this]() {
        openSubSubPanel();

        longitudinalLayout->setCurrentWidget(weatherPanel);

        qolOpen = true;
      });
      longitudinalToggle = weatherToggle;
    } else if (param == "SetWeatherKey") {
      weatherKeyControl = new FrogPilotButtonsControl(title, desc, icon, {tr("ADD"), tr("TEST")});
      QObject::connect(weatherKeyControl, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        if (id == 0) {
          if (!params.get("WeatherToken").empty()) {
            if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to remove your key?"), this)) {
              params.remove("WeatherToken");

              weatherKeyControl->setText(0, tr("ADD"));
              weatherKeyControl->setVisibleButton(1, false);
            }
          } else {
            int keyLength = 32;
            QString currentKey = QString::fromStdString(params.get("WeatherToken"));
            QString newKey = InputDialog::getText(tr("Enter your \"OpenWeatherMap\" key"), this, tr("Characters: 0/%1").arg(keyLength), false, -1, currentKey, keyLength).trimmed();
            if (!newKey.isEmpty()) {
              params.put("WeatherToken", newKey.toStdString());

              weatherKeyControl->setText(0, tr("REMOVE"));
              weatherKeyControl->setVisibleButton(1, true);
            }
          }
        } else if (id == 1) {
          weatherKeyControl->setValue(tr("Testing..."));

          QString key = QString::fromStdString(params.get("WeatherToken")).trimmed();
          QString url30 = QString("https://api.openweathermap.org/data/3.0/onecall?lat=42.4293&lon=-83.9850&exclude=current,minutely,hourly,daily,alerts&appid=%1").arg(key);

          QNetworkRequest request(url30);
          QNetworkReply *reply = networkManager->get(request);
          QObject::connect(reply, &QNetworkReply::finished, this, [=]() {
            reply->deleteLater();

            if (reply->error() == QNetworkReply::NoError) {
              weatherKeyControl->setValue("");
              ConfirmationDialog::alert(tr("Key is valid!"), this);
              return;
            }

            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 401 || status == 403) {
              QString url25 = QString("https://api.openweathermap.org/data/2.5/weather?lat=42.4293&lon=-83.9850&appid=%1").arg(key);

              QNetworkRequest request25(url25);
              QNetworkReply *reply25 = networkManager->get(request25);
              QObject::connect(reply25, &QNetworkReply::finished, this, [=]() {
                reply25->deleteLater();

                weatherKeyControl->setValue("");
                if (reply25->error() == QNetworkReply::NoError) {
                  ConfirmationDialog::alert(tr("Your key is valid for version 2.5, but version 3.0 is highly recommended! Please subscribe to the \"One Call API 3.0\" plan!"), this);
                } else {
                   int status25 = reply25->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                   ConfirmationDialog::alert(tr("Invalid key! (Error: %1)").arg(status25), this);
                }
              });
            } else {
              weatherKeyControl->setValue("");
              ConfirmationDialog::alert(tr("An error occurred: %1").arg(reply->errorString()), this);
            }
          });
        }
      });
      longitudinalToggle = weatherKeyControl;
    } else if (param == "LowVisibilityOffsets") {
      ButtonControl *manageLowVisibilitOffsetsButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageLowVisibilitOffsetsButton, &ButtonControl::clicked, [longitudinalLayout, weatherLowVisibilityPanel, this]() {
        openSubSubSubPanel();

        longitudinalLayout->setCurrentWidget(weatherLowVisibilityPanel);

        weatherOpen = true;
      });
      longitudinalToggle = manageLowVisibilitOffsetsButton;
    } else if (param == "RainOffsets") {
      ButtonControl *manageRainOffsetsButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageRainOffsetsButton, &ButtonControl::clicked, [longitudinalLayout, weatherRainPanel, this]() {
        openSubSubSubPanel();

        longitudinalLayout->setCurrentWidget(weatherRainPanel);

        weatherOpen = true;
      });
      longitudinalToggle = manageRainOffsetsButton;
    } else if (param == "RainStormOffsets") {
      ButtonControl *manageRainStormOffsetsButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageRainStormOffsetsButton, &ButtonControl::clicked, [longitudinalLayout, weatherRainStormPanel, this]() {
        openSubSubSubPanel();

        longitudinalLayout->setCurrentWidget(weatherRainStormPanel);

        weatherOpen = true;
      });
      longitudinalToggle = manageRainStormOffsetsButton;
    } else if (param == "SnowOffsets") {
      ButtonControl *manageSnowOffsetsButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageSnowOffsetsButton, &ButtonControl::clicked, [longitudinalLayout, weatherSnowPanel, this]() {
        openSubSubSubPanel();

        longitudinalLayout->setCurrentWidget(weatherSnowPanel);

        weatherOpen = true;
      });
      longitudinalToggle = manageSnowOffsetsButton;
    } else if (param == "IncreaseFollowingLowVisibility" || param == "IncreaseFollowingRain" || param == "IncreaseFollowingRainStorm" || param == "IncreaseFollowingSnow") {
      std::map<float, QString> followTimeLabels;
      for (float i = 0; i <= 3; i += 0.01) {
        followTimeLabels[i] = std::lround(i / 0.01) == 1 / 0.01 ? QString::number(i, 'f', 2) + tr(" second") : QString::number(i, 'f', 2) + tr(" seconds");
      }
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 3, QString(), followTimeLabels, 0.01, true);
    } else if (param == "IncreasedStoppedDistanceLowVisibility" || param == "IncreasedStoppedDistanceRain" || param == "IncreasedStoppedDistanceRainStorm" || param == "IncreasedStoppedDistanceSnow") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, tr(" feet"));
    } else if (param == "ReduceAccelerationLowVisibility" || param == "ReduceAccelerationRain" || param == "ReduceAccelerationRainStorm" || param == "ReduceAccelerationSnow") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, "%", std::map<float, QString>(), 1);
    } else if (param == "ReduceLateralAccelerationLowVisibility" || param == "ReduceLateralAccelerationRain" || param == "ReduceLateralAccelerationRainStorm" || param == "ReduceLateralAccelerationSnow") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, "%", std::map<float, QString>(), 1);

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
      QStringList primaryPriorities = {tr("Dashboard"), tr("Map Data"), tr("Highest"), tr("Lowest")};
      QStringList otherPriorities = {tr("None"), tr("Dashboard"), tr("Map Data")};
      QStringList priorityPrompts = {tr("Select your primary priority"), tr("Select your secondary priority")};

      QObject::connect(slcPriorityButton, &ButtonControl::clicked, [=]() {
        QStringList selectedPriorities;

        for (int i = 1; i <= 2; ++i) {
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
            for (int j = i + 1; j <= 2; ++j) {
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
      for (int i = 1; i <= 2; ++i) {
        QString priority = QString::fromStdString(params.get(QString("SLCPriority%1").arg(i).toStdString()));
        if (!parent->hasDashSpeedLimits && (priority == "Dashboard" || priority == tr("Dashboard"))) {
          continue;
        }
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
    } else if (weatherKeys.contains(param)) {
      weatherList->addItem(longitudinalToggle);
    } else if (weatherLowVisibilityKeys.contains(param)) {
      weatherLowVisibilityList->addItem(longitudinalToggle);
    } else if (weatherRainKeys.contains(param)) {
      weatherRainList->addItem(longitudinalToggle);
    } else if (weatherRainStormKeys.contains(param)) {
      weatherRainStormList->addItem(longitudinalToggle);
    } else if (weatherSnowKeys.contains(param)) {
      weatherSnowList->addItem(longitudinalToggle);
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

    if (AbstractControl *control = qobject_cast<AbstractControl*>(longitudinalToggle)) {
      QObject::connect(control, &AbstractControl::hideDescriptionEvent, [this]() {
        update();
      });
      QObject::connect(control, &AbstractControl::showDescriptionEvent, [this]() {
        update();
      });
    }
  }

  QSet<QString> forceUpdateKeys = {"HumanAcceleration", "LongitudinalTune"};
  for (const QString &key : forceUpdateKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, this, &FrogPilotLongitudinalPanel::updateToggles);
  }

  FrogPilotParamValueControl *aggressiveFollowToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveFollow"]);
  FrogPilotParamValueControl *aggressiveAccelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkAcceleration"]);
  FrogPilotParamValueControl *aggressiveDecelerationToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkDeceleration"]);
  FrogPilotParamValueControl *aggressiveDangerToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkDanger"]);
  FrogPilotParamValueControl *aggressiveSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkSpeed"]);
  FrogPilotParamValueControl *aggressiveSpeedDecreaseToggle = static_cast<FrogPilotParamValueControl*>(toggles["AggressiveJerkSpeedDecrease"]);
  ButtonControl *aggressiveResetButton = static_cast<ButtonControl*>(toggles["ResetAggressivePersonality"]);
  QObject::connect(aggressiveResetButton, &ButtonControl::clicked, [=]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the <b>Aggressive</b> personality?"), this)) {
      params.putFloat("AggressiveFollow", std::stof(params.getKeyDefaultValue("AggressiveFollow").value()));
      params.putFloat("AggressiveJerkAcceleration", std::stof(params.getKeyDefaultValue("AggressiveJerkAcceleration").value()));
      params.putFloat("AggressiveJerkDeceleration", std::stof(params.getKeyDefaultValue("AggressiveJerkDeceleration").value()));
      params.putFloat("AggressiveJerkDanger", std::stof(params.getKeyDefaultValue("AggressiveJerkDanger").value()));
      params.putFloat("AggressiveJerkSpeed", std::stof(params.getKeyDefaultValue("AggressiveJerkSpeed").value()));
      params.putFloat("AggressiveJerkSpeedDecrease", std::stof(params.getKeyDefaultValue("AggressiveJerkSpeedDecrease").value()));

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
  ButtonControl *standardResetButton = static_cast<ButtonControl*>(toggles["ResetStandardPersonality"]);
  QObject::connect(standardResetButton, &ButtonControl::clicked, [=]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the <b>Standard</b> personality?"), this)) {
      params.putFloat("StandardFollow", std::stof(params.getKeyDefaultValue("StandardFollow").value()));
      params.putFloat("StandardJerkAcceleration", std::stof(params.getKeyDefaultValue("StandardJerkAcceleration").value()));
      params.putFloat("StandardJerkDeceleration", std::stof(params.getKeyDefaultValue("StandardJerkDeceleration").value()));
      params.putFloat("StandardJerkDanger", std::stof(params.getKeyDefaultValue("StandardJerkDanger").value()));
      params.putFloat("StandardJerkSpeed", std::stof(params.getKeyDefaultValue("StandardJerkSpeed").value()));
      params.putFloat("StandardJerkSpeedDecrease", std::stof(params.getKeyDefaultValue("StandardJerkSpeedDecrease").value()));

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
  ButtonControl *relaxedResetButton = static_cast<ButtonControl*>(toggles["ResetRelaxedPersonality"]);
  QObject::connect(relaxedResetButton, &ButtonControl::clicked, [=]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your settings for the <b>Relaxed</b> personality?"), this)) {
      params.putFloat("RelaxedFollow", std::stof(params.getKeyDefaultValue("RelaxedFollow").value()));
      params.putFloat("RelaxedJerkAcceleration", std::stof(params.getKeyDefaultValue("RelaxedJerkAcceleration").value()));
      params.putFloat("RelaxedJerkDeceleration", std::stof(params.getKeyDefaultValue("RelaxedJerkDeceleration").value()));
      params.putFloat("RelaxedJerkDanger", std::stof(params.getKeyDefaultValue("RelaxedJerkDanger").value()));
      params.putFloat("RelaxedJerkSpeed", std::stof(params.getKeyDefaultValue("RelaxedJerkSpeed").value()));
      params.putFloat("RelaxedJerkSpeedDecrease", std::stof(params.getKeyDefaultValue("RelaxedJerkSpeedDecrease").value()));

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
    customPersonalityOpen = false;
    qolOpen = false;
    slcOpen = false;
    weatherOpen = false;

    openDescriptions(forceOpenDescriptions, toggles);
    longitudinalLayout->setCurrentWidget(longitudinalPanel);
  });
  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubSubPanel, [longitudinalLayout, customDrivingPersonalityPanel, qolPanel, speedLimitControllerPanel, this]() {
    openDescriptions(forceOpenDescriptions, toggles);

    if (customPersonalityOpen) {
      longitudinalLayout->setCurrentWidget(customDrivingPersonalityPanel);

      customPersonalityOpen = false;
    } else if (qolOpen) {
      longitudinalLayout->setCurrentWidget(qolPanel);

      qolOpen = false;
    } else if (slcOpen) {
      longitudinalLayout->setCurrentWidget(speedLimitControllerPanel);

      slcOpen = false;
    }
  });
  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubSubSubPanel, [longitudinalLayout, weatherPanel, this]() {
    openDescriptions(forceOpenDescriptions, toggles);

    if (weatherOpen) {
      longitudinalLayout->setCurrentWidget(weatherPanel);

      weatherOpen = false;
    }
  });
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, this, &FrogPilotLongitudinalPanel::updateMetric);
}

void FrogPilotLongitudinalPanel::showEvent(QShowEvent *event) {
  FrogPilotUIState &fs = *frogpilotUIState();

  calibratedLateralAccelerationLabel->setText(QString::number(params.getFloat("CalibratedLateralAcceleration"), 'f', 2) + tr(" m/s²"));
  calibrationProgressLabel->setText(QString::number(params.getFloat("CalibrationProgress"), 'f', 0) + "%");
  maxLateralAccelerationLabel->setText(QString::number(getMaxLateralAcceleration(params, parent), 'f', 2) + tr(" m/s²"));

  longitudinalActuatorDelayToggle->setTitle(QString(tr("Actuator Delay (Default: %1)")).arg(QString::number(parent->longitudinalActuatorDelay, 'f', 2)));
  startAccelToggle->setTitle(QString(tr("Start Acceleration (Default: %1)")).arg(QString::number(parent->startAccel, 'f', 2)));
  stopAccelToggle->setTitle(QString(tr("Stop Acceleration (Default: %1)")).arg(QString::number(parent->stopAccel, 'f', 2)));
  stoppingDecelRateToggle->setTitle(QString(tr("Stopping Rate (Default: %1)")).arg(QString::number(parent->stoppingDecelRate, 'f', 2)));
  vEgoStartingToggle->setTitle(QString(tr("Start Speed (Default: %1)")).arg(QString::number(parent->vEgoStarting, 'f', 2)));
  vEgoStoppingToggle->setTitle(QString(tr("Stop Speed (Default: %1)")).arg(QString::number(parent->vEgoStopping, 'f', 2)));

  bool keyExists = !params.get("WeatherToken").empty();
  weatherKeyControl->setText(0, keyExists ? tr("REMOVE") : tr("ADD"));
  weatherKeyControl->setVisibleButton(1, keyExists && fs.frogpilot_scene.online);

  updateToggles();
}

void FrogPilotLongitudinalPanel::updateMetric(bool metric, bool bootRun) {
  static bool previousMetric;
  if (metric != previousMetric && !bootRun) {
    double distanceConversion = metric ? FOOT_TO_METER : METER_TO_FOOT;
    double speedConversion = metric ? MILE_TO_KM : KM_TO_MILE;

    params.putInt("IncreasedStoppedDistance", std::lround(params.getInt("IncreasedStoppedDistance") * distanceConversion));
    params.putInt("IncreasedStoppedDistanceLowVisibility", std::lround(params.getInt("IncreasedStoppedDistanceLowVisibility") * distanceConversion));
    params.putInt("IncreasedStoppedDistanceRain", std::lround(params.getInt("IncreasedStoppedDistanceRain") * distanceConversion));
    params.putInt("IncreasedStoppedDistanceRainStorm", std::lround(params.getInt("IncreasedStoppedDistanceRainStorm") * distanceConversion));
    params.putInt("IncreasedStoppedDistanceSnow", std::lround(params.getInt("IncreasedStoppedDistanceSnow") * distanceConversion));

    params.putInt("CESignalSpeed", std::lround(params.getInt("CESignalSpeed") * speedConversion));
    params.putInt("CESpeed", std::lround(params.getInt("CESpeed") * speedConversion));
    params.putInt("CESpeedLead", std::lround(params.getInt("CESpeedLead") * speedConversion));
    params.putInt("CustomCruise", std::max(1L, std::lround(params.getInt("CustomCruise") * speedConversion)));
    params.putInt("CustomCruiseLong", std::max(1L, std::lround(params.getInt("CustomCruiseLong") * speedConversion)));
    params.putInt("Offset1", std::lround(params.getInt("Offset1") * speedConversion));
    params.putInt("Offset2", std::lround(params.getInt("Offset2") * speedConversion));
    params.putInt("Offset3", std::lround(params.getInt("Offset3") * speedConversion));
    params.putInt("Offset4", std::lround(params.getInt("Offset4") * speedConversion));
    params.putInt("Offset5", std::lround(params.getInt("Offset5") * speedConversion));
    params.putInt("Offset6", std::lround(params.getInt("Offset6") * speedConversion));
    params.putInt("Offset7", std::lround(params.getInt("Offset7") * speedConversion));
    params.putInt("SetSpeedOffset", std::lround(params.getInt("SetSpeedOffset") * speedConversion));
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

  FrogPilotDualParamValueControl *ceSpeedToggle = static_cast<FrogPilotDualParamValueControl*>(toggles["CESpeed"]);
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
  FrogPilotParamValueControl *increasedStoppedDistanceLowVisibilityToggle = static_cast<FrogPilotParamValueControl*>(toggles["IncreasedStoppedDistanceLowVisibility"]);
  FrogPilotParamValueControl *increasedStoppedDistanceRainToggle = static_cast<FrogPilotParamValueControl*>(toggles["IncreasedStoppedDistanceRain"]);
  FrogPilotParamValueControl *increasedStoppedDistanceRainStormToggle = static_cast<FrogPilotParamValueControl*>(toggles["IncreasedStoppedDistanceRainStorm"]);
  FrogPilotParamValueControl *increasedStoppedDistanceSnowToggle = static_cast<FrogPilotParamValueControl*>(toggles["IncreasedStoppedDistanceSnow"]);
  FrogPilotParamValueControl *setSpeedOffsetToggle = static_cast<FrogPilotParamValueControl*>(toggles["SetSpeedOffset"]);

  if (metric) {
    offset1Toggle->setTitle(tr("Speed Offset (0–29 km/h)"));
    offset2Toggle->setTitle(tr("Speed Offset (30–49 km/h)"));
    offset3Toggle->setTitle(tr("Speed Offset (50–59 km/h)"));
    offset4Toggle->setTitle(tr("Speed Offset (60–79 km/h)"));
    offset5Toggle->setTitle(tr("Speed Offset (80–99 km/h)"));
    offset6Toggle->setTitle(tr("Speed Offset (100–119 km/h)"));
    offset7Toggle->setTitle(tr("Speed Offset (120–140 km/h)"));

    offset1Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 0 and 29 km/h.</b>"));
    offset2Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 30 and 49 km/h.</b>"));
    offset3Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 50 and 59 km/h.</b>"));
    offset4Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 60 and 79 km/h.</b>"));
    offset5Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 80 and 99 km/h.</b>"));
    offset6Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 100 and 119 km/h.</b>"));
    offset7Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 120 and 140 km/h.</b>"));

    increasedStoppedDistanceToggle->updateControl(0, 3, metricDistanceLabels);
    increasedStoppedDistanceLowVisibilityToggle->updateControl(0, 3, metricDistanceLabels);
    increasedStoppedDistanceRainToggle->updateControl(0, 3, metricDistanceLabels);
    increasedStoppedDistanceRainStormToggle->updateControl(0, 3, metricDistanceLabels);
    increasedStoppedDistanceSnowToggle->updateControl(0, 3, metricDistanceLabels);

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
    setSpeedOffsetToggle->updateControl(0, 150, metricSpeedLabels);
  } else {
    offset1Toggle->setTitle(tr("Speed Offset (0–24 mph)"));
    offset2Toggle->setTitle(tr("Speed Offset (25–34 mph)"));
    offset3Toggle->setTitle(tr("Speed Offset (35–44 mph)"));
    offset4Toggle->setTitle(tr("Speed Offset (45–54 mph)"));
    offset5Toggle->setTitle(tr("Speed Offset (55–64 mph)"));
    offset6Toggle->setTitle(tr("Speed Offset (65–74 mph)"));
    offset7Toggle->setTitle(tr("Speed Offset (75–99 mph)"));

    offset1Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 0 and 24 mph.</b>"));
    offset2Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 25 and 34 mph.</b>"));
    offset3Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 35 and 44 mph.</b>"));
    offset4Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 45 and 54 mph.</b>"));
    offset5Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 55 and 64 mph.</b>"));
    offset6Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 65 and 74 mph.</b>"));
    offset7Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives between 75 and 99 mph.</b>"));

    increasedStoppedDistanceToggle->updateControl(0, 10, imperialDistanceLabels);
    increasedStoppedDistanceLowVisibilityToggle->updateControl(0, 10, imperialDistanceLabels);
    increasedStoppedDistanceRainToggle->updateControl(0, 10, imperialDistanceLabels);
    increasedStoppedDistanceRainStormToggle->updateControl(0, 10, imperialDistanceLabels);
    increasedStoppedDistanceSnowToggle->updateControl(0, 10, imperialDistanceLabels);

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

    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels[key].toDouble();

    if (key == "CEStopLights") {
      setVisible &= parent->tuningLevel < parent->frogpilotToggleLevels["CEModelStopTime"].toDouble();
    }

    else if (key == "CustomCruise" || key == "CustomCruiseLong" || key == "SetSpeedLimit" || key == "SetSpeedOffset") {
      setVisible &= !parent->hasPCMCruise;
    }

    else if (key == "HumanLaneChanges") {
      setVisible &= parent->hasRadar;
    }

    else if (key == "MapGears") {
      setVisible &= parent->isGM || parent->isHKGCanFd || parent->isToyota;
      setVisible &= !parent->isTSK;
    }

    else if (key == "ReverseCruise") {
      setVisible &= parent->isToyota;
    }

    else if (key == "SLCMapboxFiller") {
      setVisible &= !params.get("MapboxPublicKey").empty();
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
      }
    }
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
