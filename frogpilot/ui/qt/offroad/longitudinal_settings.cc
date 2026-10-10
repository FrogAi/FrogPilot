#include "frogpilot/ui/qt/offroad/longitudinal_settings.h"

namespace {
bool validInteger(const QJsonValue &value) {
  return value.isDouble() && value.toDouble() == std::floor(value.toDouble());
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
    {"LongitudinalTune", tr("Acceleration and Braking"), tr("<b>Change how openpilot speeds up and slows down.</b>"), "../../frogpilot/assets/toggle_icons/icon_longitudinal_tune.svg"},
    {"AccelerationProfile", tr("Acceleration Profile"), tr("<b>How quickly openpilot speeds up.</b><br><br>\"Standard\" is openpilot's normal acceleration, \"Eco\" is gentle and efficient, \"Sport\" is firmer and more responsive, and \"Sport+\" accelerates as hard as your car allows. None of these apply while \"Experimental Mode\" is running, including when \"Conditional Experimental Mode\" switches it on for you."), ""},
    {"DecelerationProfile", tr("Deceleration Profile"), tr("<b>How gently openpilot slows down when the road ahead is clear.</b><br><br>\"Standard\" brakes at full strength, \"Eco\" brakes about half as hard so you coast more, and \"Eco+\" just lets off the gas and coasts, so it slows quicker uphill and barely slows on a steep downhill until the road levels out. Whenever there is a car ahead, or while \"Experimental Mode\" is running, openpilot goes back to full-strength braking so it can still stop in time."), ""},
    {"HumanAcceleration", tr("Human-Like Acceleration"), tr("<b>openpilot builds speed more like a person, holding back while your set speed is low and easing off as you close in on it.</b><br><br>At a 25 mph set speed you get roughly half the acceleration you otherwise would. On cars with a fixed launch push, this replaces that push with the same smooth target openpilot uses everywhere else, and the \"Start Acceleration\" row disappears while this is on."), ""},
    {"HumanFollowing", tr("Human-Like Following"), tr("<b>openpilot follows the car ahead more naturally, reading where that car is headed so it eases off and starts braking earlier and softer instead of reacting late.</b><br><br>On the default driving model this only works while openpilot can clearly see the car ahead."), ""},
    {"HumanLaneChanges", tr("Human-Like Lane Changes"), tr("<b>openpilot watches the cars in the lane it is moving into and adjusts its speed for them during a lane change.</b><br><br>Without this it only reacts to the car directly ahead until the move is finished. It only takes over while openpilot is already tracking a car ahead, so with a clear lane in front of you nothing changes."), ""},
    {"LeadDetectionThreshold", tr("Lead Detection Sensitivity"), tr("<b>How sure openpilot has to be that something in front of you is really a car before it starts following it and braking for it.</b><br><br>Lower numbers pick up cars sooner and farther away, with more chances of reacting to something that is not a moving car. Higher numbers wait for a clearer look, so there are fewer false alarms but openpilot notices the car ahead later.<br><br>Default: 35%."), ""},
    {"TacoTune", tr("\"Taco Bell Run\" Turn Speed Hack"), tr("<b>openpilot slows down more for left and right turns, using the trick comma built for their 2022 \"Taco Bell Run\" drive.</b><br><br>It does nothing while \"Curve Speed Controller\" is on. Otherwise it never switches off, so it is still working on fast highway curves. What stops at about 45 mph is the ramp: openpilot allows more cornering force the faster you go, up to that speed, then holds it flat. That makes the slowdown most noticeable in slow, tight turns."), ""},

    {"AdvancedLongitudinalTune", tr("Advanced Longitudinal Tuning"), tr("<b>Hand-set the acceleration and braking numbers openpilot normally takes from your car.</b>"), "../../frogpilot/assets/toggle_icons/icon_advanced_longitudinal_tune.svg"},
    {"LongitudinalActuatorDelay", tr("Actuator Delay"), tr("<b>How long your car takes to respond after openpilot presses the gas or brake.</b><br><br>Raise it if your car feels slow to react. Lower it if it feels too eager or overshoots."), ""},
    {"MaxDesiredAcceleration", tr("Maximum Acceleration"), tr("<b>The hardest openpilot is ever allowed to accelerate, no matter which profile you pick.</b><br><br>Lower it for a calmer car everywhere. This caps acceleration only, never braking."), ""},
    {"StartAccel", tr("Start Acceleration"), tr("<b>How hard openpilot pulls away in the first moment after a stop.</b><br><br>Raise it for quicker takeoffs. Lower it for gentler ones. Only some cars use a fixed launch push like this, so on most cars this row does nothing."), ""},
    {"VEgoStarting", tr("Start Speed"), tr("<b>The speed that marks the end of pulling away from a stop, which sets how long openpilot holds its takeoff acceleration.</b><br><br>Raise it to keep that takeoff push going up to a higher speed. Lower it to hand back to normal gas and brake control almost as soon as the wheels turn, which makes the start softer. Only some cars have a separate takeoff stage at all, so on most cars this row does nothing."), ""},
    {"StopAccel", tr("Stop Acceleration"), tr("<b>How hard openpilot holds the brakes as your car finishes stopping and while it sits still.</b><br><br>The number is negative because it is braking, so -2.00 holds harder than -0.50. Go more negative if your car creeps or rolls back on a hill, and closer to zero if the last moment of the stop feels too abrupt."), ""},
    {"StoppingDecelRate", tr("Stopping Rate"), tr("<b>How quickly openpilot builds up brake pressure as it comes to a stop.</b><br><br>Raise it for shorter, firmer stops. Lower it for longer, smoother ones."), ""},
    {"VEgoStopping", tr("Stop Speed"), tr("<b>When the speed openpilot plans to be doing over the next second drops below this, it treats your car as stopped and switches to holding the brakes.</b><br><br>Raise it to settle into the stop earlier and more smoothly. Lower it to keep normal braking going longer, at the risk of rolling past your mark."), ""},

    {"ConditionalExperimental", tr("Conditional Experimental Mode"), tr("<b>Automatically switch to \"Experimental Mode\" when set conditions are met.</b> Allows the model to handle challenging situations with smarter decision making."), "../../frogpilot/assets/toggle_icons/icon_conditional.svg"},
    {"CESpeed", tr("Below"), tr("<b>Switch to \"Experimental Mode\" below this speed when there is no car ahead of you.</b><br><br>It helps openpilot handle slow, fiddly situations more smoothly."), ""},
    {"CECurves", tr("Curve Detected Ahead"), tr("<b>Switch to \"Experimental Mode\" when openpilot sees a curve coming up.</b><br><br>The model picks its own speed for the curve instead of holding your set speed."), ""},
    {"CEStopLights", tr("\"Detected\" Stop Lights/Signs"), tr("<b>Switch to \"Experimental Mode\" for a predicted stop, except while following a detected lead.</b><br><br>It keeps checking for a possible stop behind that lead and can trigger once the lead is no longer tracked. \"Traffic Mode\" turns this condition off.<br><br><i><b>Disclaimer</b>: openpilot does not explicitly detect traffic lights or stop signs. In \"Experimental Mode\", openpilot makes end-to-end driving decisions from camera input, which means it may stop even when there's no clear reason!</i>"), ""},
    {"CELead", tr("Lead Detected Ahead"), tr("<b>Switch to \"Experimental Mode\" when you need to brake for the car ahead.</b><br><br>\"Slower Lead\" covers a car that is slowing down or much slower than you, and \"Stopped Lead\" covers a car that is stopped or about to stop. Gently catching up to a slightly slower car stays in normal driving. Both start off, so pick at least one with the buttons on this row or nothing happens."), ""},
    {"CEModelStopTime", tr("Stop Detection Sensitivity"), tr("<b>Adjust when a predicted stop can switch to \"Experimental Mode\". Higher values can trigger earlier; lower values are less sensitive.</b><br><br>The seconds value is a sensitivity setting, not an exact countdown to a stop. While following a detected lead, it keeps checking for a possible stop and can trigger once that lead is no longer tracked. \"Traffic Mode\" turns this condition off.<br><br><i><b>Disclaimer</b>: openpilot does not explicitly detect traffic lights or stop signs. In \"Experimental Mode\", openpilot makes end-to-end driving decisions from camera input, which means it may stop even when there's no clear reason!</i>"), ""},
    {"CESignalSpeed", tr("Turn Signal Below"), tr("<b>Switch to \"Experimental Mode\" when you signal below the set speed. Turn on \"Not For Detected Lanes\" to suppress this when an adjacent lane is detected as you start signalling.</b><br><br>With the button off, any signal below the set speed can trigger it. With it on, the space beside you must be narrower than the \"Minimum Lane Width\" under \"Lane Changes\" in the \"STEERING\" panel when you start signalling, or you must be below the \"Minimum Lane Change Speed\". That width starts at zero, so raise it to use lane detection. Lane estimates can miss an adjacent lane."), ""},
    {"ShowCEMStatus", tr("Status Widget"), tr("<b>Show which condition switched \"Experimental Mode\" on, right on the driving screen.</b>"), ""},

    {"CurveSpeedController", tr("Curve Speed Controller"), tr("<b>openpilot slows down on its own for curves ahead, and you pick how fast it takes them with \"Curve Speed Profile\".</b><br><br>It comes set to \"Adaptive\", which learns how you prefer to take curves."), "../../frogpilot/assets/toggle_icons/icon_speed_map.svg"},
    {"CalibratedLateralAcceleration", tr("Calibrated Lateral Acceleration"), tr("<b>How hard you corner, learned from your own driving.</b><br><br>The \"Adaptive\" profile uses this to take curves the way you do, but never harder than your steering has proven it can hold. A higher number means carrying more speed through curves. Lower means taking them gentler."), ""},
    {"CalibrationProgress", tr("Calibration Progress"), tr("<b>How much of your own cornering openpilot has learned from.</b><br><br>This only grows while you're the one controlling the speed, so it fills up as you drive curves yourself. At 100% openpilot has gathered enough of your cornering to match the way you take curves."), ""},
    {"CurveSpeedProfile", tr("Curve Speed Profile"), tr("<b>How fast openpilot takes curves.</b><br><br>\"Gentle\" and \"Standard\" hold to a fixed, relaxed pace, \"Sport\" uses your car's maximum configured or learned cornering limit, and \"Adaptive\" learns how you prefer to take curves."), ""},
    {"MaxLateralAcceleration", tr("Maximum Lateral Acceleration"), tr("<b>How fast the \"Sport\" profile is allowed to take curves.</b><br><br>This is also the maximum for every other curve speed profile. openpilot learns the limit from your car when possible and otherwise uses the value configured for it."), ""},
    {"ResetCurveData", tr("Reset Curve Data"), tr("<b>Throw away everything openpilot has learned about how you take curves and start over.</b><br><br>\"Adaptive\" goes back to its starting value and relearns as you drive. Only available while the car is off."), ""},
    {"ShowCSCStatus", tr("Status Widget"), tr("<b>Show the speed openpilot is aiming for through the curve, right on the driving screen.</b><br><br>It also shows a \"Training...\" note while openpilot is learning from the way you take a curve yourself."), ""},

    {"CustomPersonalities", tr("Driving Personalities"), tr("<b>Change what Aggressive, Standard and Relaxed actually do, so they match how you like to drive.</b>"), "../../frogpilot/assets/toggle_icons/icon_personality.svg"},

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

    {"QOLLongitudinal", tr("Quality of Life"), tr("<b>Smaller changes to how openpilot handles the gas and brake.</b>"), "../../frogpilot/assets/toggle_icons/icon_quality_of_life.svg"},
    {"CustomCruise", tr("Cruise Interval"), tr("<b>How much your set speed moves with each tap of the + or - cruise button.</b><br><br>Set it to 1 to land on any speed exactly, or higher to get where you are going in fewer taps. When this is 5 or another multiple of 5, a tap from a set speed in between only goes as far as the next multiple of it, so with 5 a set speed of 63 becomes 65 or 60."), ""},
    {"CustomCruiseLong", tr("Cruise Interval (Hold)"), tr("<b>How much your set speed moves while you hold the + or - cruise button down.</b><br><br>The default is 5, against 1 for a single tap. When this is 5 or another multiple of 5, the first step from a set speed in between only goes as far as the next multiple of it, so with 5 a set speed of 63 becomes 65 or 60."), ""},
    {"ForceStops", tr("Force Stop at \"Detected\" Stop Lights/Signs"), tr("<b>openpilot comes to a full stop whenever it thinks it sees a red light or stop sign, whether or not \"Experimental Mode\" is running.</b><br><br>It only kicks in when openpilot is not already tracking a car ahead, so behind a queue at a light your normal following does the stopping instead. Touching the gas cancels a forced stop. \"Traffic Mode\" turns this off.<br><br><i><b>Heads up</b>: openpilot never actually reads traffic lights or stop signs. It decides from what the camera sees, so it can stop when there is no reason to.</i>"), ""},
    {"IncreasedStoppedDistance", tr("Increase Stopped Distance by:"), tr("<b>Adds a set amount of extra room between you and the car ahead, and keeps that room at every speed, not just when you are stopped.</b><br><br>You notice it most at red lights, where a few feet stops openpilot creeping up close. While moving, that same room means openpilot starts slowing a little sooner. \"Traffic Mode\" ignores this setting."), ""},
    {"MapGears", tr("Map Accel/Decel to Gears"), tr("<b>Lets your car's \"Eco\" and \"Sport\" gear modes take over how openpilot speeds up, how it slows down, or both.</b><br><br>Pick \"Acceleration\", \"Deceleration\" or both with the buttons on this row, since neither starts on and nothing changes until you do.<br><br>\"Eco\" gear makes openpilot accelerate gently and \"Sport\" gear makes it accelerate firmly. For braking, \"Eco\" gear halves how hard openpilot can brake and \"Sport\" gear brakes at full strength. The braking change only applies when there is no car ahead."), ""},
    {"SetSpeedOffset", tr("Offset Set Speed by:"), tr("<b>Adds an extra amount on top of the \"Cruise Interval (Hold)\" step, but only when you press and hold the + cruise button.</b><br><br>The - button does not mirror it. Holding - takes the normal hold step down and then moves down by one more \"Cruise Interval (Hold)\" amount minus this offset, so with a hold interval of 5 and 5 chosen here you just get the normal hold step down, as you also do with any offset above the hold interval. A quick tap is never affected, and 0 turns this off."), ""},
    {"ReverseCruise", tr("Reverse Cruise Increase"), tr("<b>Every tap of the + cruise button raises your set speed by 5 instead of 1, the same amount you already get from holding it.</b><br><br>Leave it off when you want to land on an exact speed like 63."), ""},
    {"WeatherPresets", tr("Weather Condition Offsets"), tr("<b>openpilot drives more cautiously on its own when the weather turns bad.</b><br><br>It checks the current weather where you are and applies whichever set of adjustments below matches. Every one of those adjustments starts at 0, so a weather icon shows up on the driving screen but nothing about how openpilot drives changes until you open the sets below and put in your own numbers."), ""},

    {"LowVisibilityOffsets", tr("Low Visibility"), tr("<b>How openpilot drives when fog or haze cuts your visibility.</b><br><br>These add to your normal settings rather than replacing them."), ""},
    {"IncreaseFollowingLowVisibility", tr("Increase Following Distance by:"), tr("<b>Adds extra seconds of space between you and the car ahead in low visibility.</b><br><br>Your total gap never goes past 3.00 seconds, so this only adds what is left below that."), ""},
    {"IncreasedStoppedDistanceLowVisibility", tr("Increase Stopped Distance by:"), tr("<b>Adds extra room between you and the car ahead in low visibility, at every speed and not just when stopped.</b><br><br>This stacks on top of the same setting under \"Quality of Life\". \"Traffic Mode\" ignores both."), ""},
    {"ReduceAccelerationLowVisibility", tr("Reduce Acceleration by:"), tr("<b>Holds openpilot back from accelerating as hard in low visibility.</b><br><br>This does not apply while \"Experimental Mode\" is running."), ""},
    {"ReduceLateralAccelerationLowVisibility", tr("Reduce Cornering Force by:"), tr("<b>Eases off how hard openpilot corners in low visibility.</b><br><br>Only does anything while \"Curve Speed Controller\" is on. Raise it for gentler, safer cornering on a slippery road. Curve speed drops by less than this number, because cornering force rises with the square of speed."), ""},

    {"RainOffsets", tr("Rain"), tr("<b>How openpilot drives in the rain.</b><br><br>These add to your normal settings rather than replacing them."), ""},
    {"IncreaseFollowingRain", tr("Increase Following Distance by:"), tr("<b>Adds extra seconds of space between you and the car ahead in rain.</b><br><br>Your total gap never goes past 3.00 seconds, so this only adds what is left below that."), ""},
    {"IncreasedStoppedDistanceRain", tr("Increase Stopped Distance by:"), tr("<b>Adds extra room between you and the car ahead in rain, at every speed and not just when stopped.</b><br><br>This stacks on top of the same setting under \"Quality of Life\". \"Traffic Mode\" ignores both."), ""},
    {"ReduceAccelerationRain", tr("Reduce Acceleration by:"), tr("<b>Holds openpilot back from accelerating as hard in rain.</b><br><br>Raise it for softer, more controlled pickup on a slippery road. This does not apply while \"Experimental Mode\" is running."), ""},
    {"ReduceLateralAccelerationRain", tr("Reduce Cornering Force by:"), tr("<b>Eases off how hard openpilot corners in rain.</b><br><br>Only does anything while \"Curve Speed Controller\" is on. Raise it for gentler, safer cornering on a slippery road. Curve speed drops by less than this number, because cornering force rises with the square of speed."), ""},

    {"RainStormOffsets", tr("Rainstorms"), tr("<b>How openpilot drives in heavy rain.</b><br><br>These add to your normal settings rather than replacing them."), ""},
    {"IncreaseFollowingRainStorm", tr("Increase Following Distance by:"), tr("<b>Adds extra seconds of space between you and the car ahead in a rainstorm.</b><br><br>Your total gap never goes past 3.00 seconds, so this only adds what is left below that."), ""},
    {"IncreasedStoppedDistanceRainStorm", tr("Increase Stopped Distance by:"), tr("<b>Adds extra room between you and the car ahead in a rainstorm, at every speed and not just when stopped.</b><br><br>This stacks on top of the same setting under \"Quality of Life\". \"Traffic Mode\" ignores both."), ""},
    {"ReduceAccelerationRainStorm", tr("Reduce Acceleration by:"), tr("<b>Holds openpilot back from accelerating as hard in a rainstorm.</b><br><br>Raise it for softer, more controlled pickup on a slippery road. This does not apply while \"Experimental Mode\" is running."), ""},
    {"ReduceLateralAccelerationRainStorm", tr("Reduce Cornering Force by:"), tr("<b>Eases off how hard openpilot corners in a rainstorm.</b><br><br>Only does anything while \"Curve Speed Controller\" is on. Raise it for gentler, safer cornering on a slippery road. Curve speed drops by less than this number, because cornering force rises with the square of speed."), ""},

    {"SnowOffsets", tr("Snow"), tr("<b>How openpilot drives in snow.</b><br><br>These add to your normal settings rather than replacing them."), ""},
    {"IncreaseFollowingSnow", tr("Increase Following Distance by:"), tr("<b>Adds extra seconds of space between you and the car ahead in snow.</b><br><br>Your total gap never goes past 3.00 seconds, so this only adds what is left below that."), ""},
    {"IncreasedStoppedDistanceSnow", tr("Increase Stopped Distance by:"), tr("<b>Adds extra room between you and the car ahead in snow, at every speed and not just when stopped.</b><br><br>This stacks on top of the same setting under \"Quality of Life\". \"Traffic Mode\" ignores both."), ""},
    {"ReduceAccelerationSnow", tr("Reduce Acceleration by:"), tr("<b>Holds openpilot back from accelerating as hard in snow.</b><br><br>Raise it for softer, more controlled pickup on a slippery road. This does not apply while \"Experimental Mode\" is running."), ""},
    {"ReduceLateralAccelerationSnow", tr("Reduce Cornering Force by:"), tr("<b>Eases off how hard openpilot corners in snow.</b><br><br>Only does anything while \"Curve Speed Controller\" is on. Raise it for gentler, safer cornering on a slippery road. Curve speed drops by less than this number, because cornering force rises with the square of speed."), ""},

    {"SetWeatherKey", tr("Set Your Own Key"), tr("<b>Use your own \"OpenWeatherMap\" key for weather requests every minute.</b><br><br>Requires a One Call API 4.0 subscription. Without a working personal key, FrogPilot tries shared weather with a 15-minute request interval. Your saved key is kept."), ""},

    {"SpeedLimitController", tr("Speed Limit Controller"), tr("<b>Hold openpilot's max speed to the posted speed limit.</b><br><br>The limit comes from your downloaded maps, Mapbox, speed limit signs read by the road camera, or, on supported Ford, Genesis, Hyundai, Kia, Lexus and Toyota models, your dashboard."), "../../frogpilot/assets/toggle_icons/icon_speed_limit.svg"},
    {"SLCFallback", tr("Fallback Speed"), tr("<b>The speed used by \"Speed Limit Controller\" when no speed limit is found.</b><br><br>- <b>Set Speed</b>: Use the cruise set speed<br>- <b>Experimental Mode</b>: Let openpilot pick the speed from what the camera sees, never going above your set speed<br>- <b>Previous Limit</b>: Keep using the last confirmed limit"), ""},
    {"SLCOverride", tr("Override Speed"), tr("<b>The speed used by \"Speed Limit Controller\" after you manually drive faster than the posted limit.</b><br><br>- <b>None</b>: Go back to the posted limit as soon as you are off the gas<br>- <b>Set With Gas Pedal</b>: Use the highest speed reached while pressing the gas<br>- <b>Max Set Speed</b>: Use the cruise set speed<br><br>Overrides clear when openpilot disengages."), ""},
    {"SLCQOL", tr("Speed Limit Changes"), tr("<b>Smaller changes to how \"Speed Limit Controller\" behaves.</b>"), ""},
    {"SLCConfirmation", tr("Confirm New Speed Limits"), tr("<b>Ask before changing to a new speed limit, with the \"Lower Limits\" and \"Higher Limits\" buttons choosing which changes need your approval.</b><br><br>Neither button starts on, so pick at least one or openpilot keeps accepting every new limit without asking. To accept, tap the flashing widget on the driving screen or press the Cruise Increase button. To deny, press Cruise Decrease or ignore it for 30 seconds."), ""},
    {"SLCLookaheadHigher", tr("Higher Limit Lookahead Time"), tr("<b>How far ahead openpilot looks for a higher speed limit coming up.</b><br><br>This reads from your downloaded map data."), ""},
    {"SLCLookaheadLower", tr("Lower Limit Lookahead Time"), tr("<b>How far ahead openpilot looks for a lower speed limit coming up.</b><br><br>This reads from your downloaded map data."), ""},
    {"SetSpeedLimit", tr("Match Speed Limit on Engage"), tr("<b>Engaging openpilot sets your max speed to the current speed limit with your \"Speed Limit Offsets\" added on top.</b><br><br>The offsets do not start at zero, so set them all to 0 if you want the max speed to land on the posted number. This only happens when openpilot has no set speed to go back to, since engaging with the Resume or + button brings back your last set speed instead."), ""},
    {"SLCMapboxFiller", tr("Use Mapbox as Fallback"), tr("<b>Fall back to Mapbox for the speed limit when none of your chosen sources have one.</b><br><br>Needs your Public Mapbox Key and a working internet connection."), ""},
    {"SLCPriority", tr("Speed Limit Source Priority"), tr("<b>Choose which sources openpilot checks for the speed limit and in what order, or have it always use the highest or lowest limit being reported.</b><br><br>Pick up to three sources and openpilot uses the first one that currently has a limit. \"Camera\" is openpilot reading speed limit signs with the road camera. \"Highest\" and \"Lowest\" ignore the order and take the fastest or slowest limit any source reports, so one wrong map entry can hold you well below the posted limit."), ""},
    {"SLCOffsets", tr("Speed Limit Offsets"), tr("<b>Drive a set amount above or below the posted speed limit.</b><br><br>Each speed range below gets its own offset."), ""},
    {"Offset1", tr("Speed Offset (0–24 mph)"), tr("<b>How far above or below the posted limit openpilot drives on roads posted between 0 and 24 mph.</b>"), ""},
    {"Offset2", tr("Speed Offset (25–34 mph)"), tr("<b>How far above or below the posted limit openpilot drives on roads posted between 25 and 34 mph.</b>"), ""},
    {"Offset3", tr("Speed Offset (35–44 mph)"), tr("<b>How far above or below the posted limit openpilot drives on roads posted between 35 and 44 mph.</b>"), ""},
    {"Offset4", tr("Speed Offset (45–54 mph)"), tr("<b>How far above or below the posted limit openpilot drives on roads posted between 45 and 54 mph.</b>"), ""},
    {"Offset5", tr("Speed Offset (55–64 mph)"), tr("<b>How far above or below the posted limit openpilot drives on roads posted between 55 and 64 mph.</b>"), ""},
    {"Offset6", tr("Speed Offset (65–74 mph)"), tr("<b>How far above or below the posted limit openpilot drives on roads posted between 65 and 74 mph.</b>"), ""},
    {"Offset7", tr("Speed Offset (75–99 mph)"), tr("<b>How far above or below the posted limit openpilot drives on roads posted between 75 and 99 mph.</b>"), ""},
    {"SLCVisuals", tr("Visual Settings"), tr("<b>Change how \"Speed Limit Controller\" appears on the driving screen.</b>"), ""},
    {"ShowSLCOffset", tr("Show Speed Limit Offset"), tr("<b>The speed limit sign on the driving screen shows the posted limit with your offset printed underneath it.</b><br><br>With this off, the offset is added into the number on the sign instead, so a 65 mph road with a +10 offset shows 75 rather than 65."), ""},
    {"SpeedLimitSources", tr("Show Speed Limit Sources"), tr("<b>Show every speed limit source and what each one currently reports, on the driving screen.</b><br><br>Useful for working out which source to trust before setting your priority order."), ""}
  };

  const QString second = tr(" second");
  const QString seconds = tr(" seconds");

  std::map<float, QString> followTimeLabels;
  for (float i = 0; i <= 3; i += 0.01) {
    followTimeLabels[i] = std::lround(i / 0.01) == 1 / 0.01 ? QString::number(i, 'f', 2) + second : QString::number(i, 'f', 2) + seconds;
  }

  for (const auto &[param, title, desc, icon] : longitudinalToggles) {
    QWidget *longitudinalToggle;

    if (param == "AdvancedLongitudinalTune") {
      FrogPilotManageControl *advancedLongitudinalTuneToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(advancedLongitudinalTuneToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, advancedLongitudinalTunePanel]() {
        longitudinalLayout->setCurrentWidget(advancedLongitudinalTunePanel);
      });
      longitudinalToggle = advancedLongitudinalTuneToggle;
    } else if (param == "LongitudinalActuatorDelay") {
      longitudinalActuatorDelayToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 1, tr(" seconds"), {{1, "1.00" + tr(" second")}}, 0.01);
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
      stoppingDecelRateToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.001, 12, tr(" m/s²/s"), std::map<float, QString>(), 0.001, true);
      longitudinalToggle = stoppingDecelRateToggle;
    } else if (param == "VEgoStopping") {
      vEgoStoppingToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0.01, 1, tr(" m/s"), std::map<float, QString>(), 0.01);
      longitudinalToggle = vEgoStoppingToggle;

    } else if (param == "ConditionalExperimental") {
      FrogPilotManageControl *conditionalExperimentalToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(conditionalExperimentalToggle, &FrogPilotManageControl::manageButtonClicked, [this, longitudinalLayout, conditionalExperimentalPanel]() {
        longitudinalLayout->setCurrentWidget(conditionalExperimentalPanel);

        if (forceOpenDescriptions) {
          ceSpeedToggle->showDescription();
          ceSpeedLeadToggle->showDescription();
        }
      });
      longitudinalToggle = conditionalExperimentalToggle;
    } else if (param == "CESpeed") {
      ceSpeedToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr(" mph"), std::map<float, QString>(), 1, true);
      ceSpeedLeadToggle = new FrogPilotParamValueControl("CESpeedLead", tr("With Lead"), tr("<b>Switch to \"Experimental Mode\" below this speed when openpilot is tracking a car ahead of you.</b><br><br>This replaces the \"Below\" value whenever there is a car ahead. It switches on once you are moving at walking pace or faster, and can then stay on as you crawl or stop behind that car."), icon, 0, 99, tr(" mph"), std::map<float, QString>(), 1, true);
      QHBoxLayout *conditionalSpeedsLayout = new QHBoxLayout();
      conditionalSpeedsLayout->setContentsMargins(0, 0, 0, 0);
      conditionalSpeedsLayout->setSpacing(20);
      conditionalSpeedsLayout->addWidget(ceSpeedToggle);
      conditionalSpeedsLayout->addWidget(ceSpeedLeadToggle);
      longitudinalToggle = new LayoutWidget(conditionalSpeedsLayout);
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
      for (int i = 0; i <= 9; ++i) {
        stopTimeLabels[i] = i == 0 ? tr("Off") : i == 1 ? QString::number(i) + second : QString::number(i) + seconds;
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
      calibrationProgressLabel = new LabelControl(title, "", desc);
      longitudinalToggle = calibrationProgressLabel;
    } else if (param == "CalibratedLateralAcceleration") {
      calibratedLateralAccelerationLabel = new LabelControl(title, "", desc);
      longitudinalToggle = calibratedLateralAccelerationLabel;
    } else if (param == "MaxLateralAcceleration") {
      maxLateralAccelerationLabel = new LabelControl(title, "", desc);
      longitudinalToggle = maxLateralAccelerationLabel;
    } else if (param == "ResetCurveData") {
      ButtonControl *resetCurveDataButton = new ButtonControl(title, tr("RESET"), desc);
      QObject::connect(resetCurveDataButton, &ButtonControl::clicked, [this]() {
        if (uiState()->scene.started) {
          ConfirmationDialog::alert(tr("Curve data can't be reset while the car is on. Turn the car off and try again."), this);
          return;
        }
        if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your curvature data?"), this)) {
          params.putFloat("CalibratedLateralAcceleration", std::stof(params.getKeyDefaultValue("CalibratedLateralAcceleration").value()));
          params.remove("CalibrationProgress");
          params.remove("CurvatureData");
          params.remove("MaxLateralAcceleration");

          frogpilotUIState()->updateToggles();

          updateCurveLabels();
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
      longitudinalToggle = new ButtonControl(title, tr("RESET"), desc);
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
      longitudinalToggle = new ButtonParamControl(param, title, desc, icon, accelerationProfiles);
    } else if (param == "DecelerationProfile") {
      std::vector<QString> decelerationProfiles{tr("Standard"), tr("Eco"), tr("Eco+")};
      longitudinalToggle = new ButtonParamControl(param, title, desc, icon, decelerationProfiles);
    } else if (param == "LeadDetectionThreshold") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 25, 50, "%");

    } else if (param == "QOLLongitudinal") {
      FrogPilotManageControl *qolLongitudinalToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(qolLongitudinalToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, qolPanel]() {
        longitudinalLayout->setCurrentWidget(qolPanel);
      });
      longitudinalToggle = qolLongitudinalToggle;
    } else if (param == "CustomCruise" || param == "CustomCruiseLong") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 99, tr(" mph"));
    } else if (param == "IncreasedStoppedDistance" || param == "IncreasedStoppedDistanceLowVisibility" || param == "IncreasedStoppedDistanceRain" || param == "IncreasedStoppedDistanceRainStorm" || param == "IncreasedStoppedDistanceSnow") {
      FrogPilotParamValueControl *increasedStoppedDistanceToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, tr(" feet"));
      increasedStoppedDistanceToggle->setPrecision(1);
      longitudinalToggle = increasedStoppedDistanceToggle;
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
              weatherKeyControl->setValue("");
              weatherKeyControl->setVisibleButton(1, false);
            }
          } else {
            int keyLength = 32;
            QString newKey = InputDialog::getText(tr("Enter your \"OpenWeatherMap\" key"), this, "", false, -1, "", keyLength).trimmed();
            if (!newKey.isEmpty()) {
              params.put("WeatherToken", newKey.toStdString());

              weatherKeyControl->setText(0, tr("REMOVE"));
              weatherKeyControl->setVisibleButton(1, frogpilotUIState()->frogpilot_scene.online);

              weatherKeyControl->setValue(tr("Testing..."));
              testWeatherKey(newKey);
            }
          }
        } else if (id == 1) {
          weatherKeyControl->setValue(tr("Testing..."));
          testWeatherKey(QString::fromStdString(params.get("WeatherToken")).trimmed());
        }
      });
      longitudinalToggle = weatherKeyControl;
    } else if (param == "LowVisibilityOffsets") {
      ButtonControl *manageLowVisibilityOffsetsButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(manageLowVisibilityOffsetsButton, &ButtonControl::clicked, [longitudinalLayout, weatherLowVisibilityPanel, this]() {
        openSubSubSubPanel();

        longitudinalLayout->setCurrentWidget(weatherLowVisibilityPanel);

        weatherOpen = true;
      });
      longitudinalToggle = manageLowVisibilityOffsetsButton;
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
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 3, QString(), followTimeLabels, 0.01, true);
    } else if (param == "ReduceAccelerationLowVisibility" || param == "ReduceAccelerationRain" || param == "ReduceAccelerationRainStorm" || param == "ReduceAccelerationSnow" || param == "ReduceLateralAccelerationLowVisibility" || param == "ReduceLateralAccelerationRain" || param == "ReduceLateralAccelerationRainStorm" || param == "ReduceLateralAccelerationSnow") {
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, "%", std::map<float, QString>(), 1);

    } else if (param == "SpeedLimitController") {
      FrogPilotManageControl *speedLimitControllerToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(speedLimitControllerToggle, &FrogPilotManageControl::manageButtonClicked, [longitudinalLayout, speedLimitControllerPanel]() {
        longitudinalLayout->setCurrentWidget(speedLimitControllerPanel);
      });
      longitudinalToggle = speedLimitControllerToggle;
    } else if (param == "SLCFallback") {
      std::vector<QString> fallbackOptions{tr("Set Speed"), tr("Experimental Mode"), tr("Previous Limit")};
      longitudinalToggle = new ButtonParamControl(param, title, desc, icon, fallbackOptions);
    } else if (param == "SLCOverride") {
      std::vector<QString> overrideOptions{tr("None"), tr("Set With Gas Pedal"), tr("Max Set Speed")};
      longitudinalToggle = new ButtonParamControl(param, title, desc, icon, overrideOptions);
    } else if (param == "SLCPriority") {
      slcPriorityButton = new ButtonControl(title, tr("SELECT"), desc);
      primaryPriorities = QStringList{tr("Camera"), tr("Dashboard"), tr("Map Data"), tr("Highest"), tr("Lowest")};
      QStringList otherPriorities = {tr("None"), tr("Camera"), tr("Dashboard"), tr("Map Data")};
      translatedPriorities = QStringList{tr("None"), tr("Camera"), tr("Dashboard"), tr("Map Data"), tr("Highest"), tr("Lowest")};
      canonicalPriorities = QStringList{"None", "Camera", "Dashboard", "Map Data", "Highest", "Lowest"};
      QStringList priorityPrompts = {tr("Select your primary priority"), tr("Select your secondary priority"), tr("Select your tertiary priority")};

      QObject::connect(slcPriorityButton, &ButtonControl::clicked, [=]() {
        QStringList selectedPriorities;

        for (int i = 1; i <= 3; ++i) {
          QStringList availablePriorities = i == 1 ? primaryPriorities : otherPriorities;
          for (const QString &selected : selectedPriorities) {
            availablePriorities.removeAll(selected);
          }

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

          const int selectionIndex = translatedPriorities.indexOf(selection);
          params.put(QString("SLCPriority%1").arg(i).toStdString(), canonicalPriorities[selectionIndex].toStdString());
          if (selection == tr("None") || selection == tr("Lowest") || selection == tr("Highest")) {
            break;
          }
        }

        if (!selectedPriorities.isEmpty()) {
          for (int i = selectedPriorities.size() + 1; i <= 3; ++i) {
            params.put(QString("SLCPriority%1").arg(i).toStdString(), std::string("None"));
          }
        }

        updateSLCPriorityValue();
      });

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
      longitudinalToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 30, tr(" seconds"), {{1, "1" + tr(" second")}});
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
  }

  QSet<QString> forceUpdateKeys = {"HumanAcceleration", "LongitudinalTune"};
  for (const QString &key : forceUpdateKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, this, &FrogPilotLongitudinalPanel::updateToggles);
  }

  const std::vector<std::pair<QString, QString>> personalityResets {
    {"Aggressive", tr("Are you sure you want to completely reset your settings for the <b>Aggressive</b> personality?")},
    {"Standard", tr("Are you sure you want to completely reset your settings for the <b>Standard</b> personality?")},
    {"Relaxed", tr("Are you sure you want to completely reset your settings for the <b>Relaxed</b> personality?")}
  };
  for (const std::pair<QString, QString> &reset : personalityResets) {
    QObject::connect(static_cast<ButtonControl*>(toggles["Reset" + reset.first + "Personality"]), &ButtonControl::clicked, [reset, this]() {
      if (FrogPilotConfirmationDialog::yesorno(reset.second, this)) {
        const QStringList keySuffixes = {"Follow", "JerkAcceleration", "JerkDeceleration", "JerkDanger", "JerkSpeed", "JerkSpeedDecrease"};
        for (const QString &keySuffix : keySuffixes) {
          QString key = reset.first + keySuffix;
          params.putFloat(key.toStdString(), std::stof(params.getKeyDefaultValue(key.toStdString()).value()));

          static_cast<FrogPilotParamValueControl*>(toggles[key])->refresh();
        }
      }
    });
  }

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
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::togglesUpdated, this, &FrogPilotLongitudinalPanel::updateCurveLabels);
}

void FrogPilotLongitudinalPanel::showEvent(QShowEvent *event) {
  FrogPilotUIState &fs = *frogpilotUIState();

  updateCurveLabels();

  if (parent->carDetected) {
    longitudinalActuatorDelayToggle->setTitle(QString(tr("Actuator Delay (Default: %1)")).arg(QString::number(parent->longitudinalActuatorDelay, 'f', 2)));
    startAccelToggle->setTitle(QString(tr("Start Acceleration (Default: %1)")).arg(QString::number(parent->startAccel, 'f', 2)));
    stopAccelToggle->setTitle(QString(tr("Stop Acceleration (Default: %1)")).arg(QString::number(parent->stopAccel, 'f', 2)));
    stoppingDecelRateToggle->setTitle(QString(tr("Stopping Rate (Default: %1)")).arg(QString::number(parent->stoppingDecelRate, 'f', 2)));
    vEgoStartingToggle->setTitle(QString(tr("Start Speed (Default: %1)")).arg(QString::number(parent->vEgoStarting, 'f', 2)));
    vEgoStoppingToggle->setTitle(QString(tr("Stop Speed (Default: %1)")).arg(QString::number(parent->vEgoStopping, 'f', 2)));
  }

  bool keyExists = !params.get("WeatherToken").empty();
  weatherKeyControl->setText(0, keyExists ? tr("REMOVE") : tr("ADD"));
  weatherKeyControl->setVisibleButton(1, keyExists && fs.frogpilot_scene.online);

  updateSLCPriorityValue();

  updateToggles();
}

void FrogPilotLongitudinalPanel::testWeatherKey(const QString &key) {
  QString url = QString("https://api.openweathermap.org/data/4.0/onecall/current?lat=42.4293&lon=-83.9850&appid=%1").arg(key);

  QNetworkRequest request(url);
  QNetworkReply *reply = networkManager->get(request);
  QObject::connect(reply, &QNetworkReply::finished, this, [=]() {
    reply->deleteLater();

    if (QString::fromStdString(params.get("WeatherToken")).trimmed() != key) {
      return;
    }

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    weatherKeyControl->setValue("");

    if (!isVisible()) {
      return;
    }

    QString message;
    if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
      QJsonObject data = QJsonDocument::fromJson(reply->readAll()).object().value("data").toArray().at(0).toObject();

      bool validSunData = !data.contains("sunrise") || validInteger(data.value("sunrise"));
      validSunData &= !data.contains("sunset") || validInteger(data.value("sunset"));
      QJsonValue weatherId = data.value("weather").toArray().at(0).toObject().value("id");
      if (validSunData && validInteger(weatherId)) {
        ConfirmationDialog::alert(tr("Key is valid for OpenWeatherMap One Call API 4.0!"), this);
        return;
      }
      message = tr("OpenWeatherMap returned incomplete weather data. Try again later.");
    } else if (status == 401 || status == 403) {
      message = tr("OpenWeatherMap did not accept this key for One Call API 4.0. Check the key and make sure it has a One Call API 4.0 subscription. "
                   "Until then, FrogPilot uses shared weather, updated every 15 minutes.");
    } else if (status == 429) {
      message = tr("OpenWeatherMap's request limit was reached. Try again later.");
    } else if (status != 0) {
      message = tr("OpenWeatherMap request failed (HTTP %1).").arg(status);
    } else {
      message = tr("Could not reach OpenWeatherMap. Check your connection and try again.");
    }

    ConfirmationDialog::alert(message, this);
  });
}

void FrogPilotLongitudinalPanel::updateCurveLabels() {
  float maxLateralAcceleration = frogpilotUIState()->frogpilot_scene.frogpilot_toggles.value("maxLateralAccel").toDouble();

  calibratedLateralAccelerationLabel->setText(QString::number(std::min(params.getFloat("CalibratedLateralAcceleration"), maxLateralAcceleration), 'f', 2) + tr(" m/s²"));
  calibrationProgressLabel->setText(QString::number(params.getFloat("CalibrationProgress"), 'f', 0) + "%");
  maxLateralAccelerationLabel->setText(QString::number(maxLateralAcceleration, 'f', 2) + tr(" m/s²"));
}

void FrogPilotLongitudinalPanel::updateMetric(bool metric, bool bootRun) {
  static bool previousMetric;
  if (metric != previousMetric && !bootRun) {
    double distanceConversion = metric ? FOOT_TO_METER : METER_TO_FOOT;
    double speedConversion = metric ? MILE_TO_KM : KM_TO_MILE;

    params.putFloat("IncreasedStoppedDistance", params.getFloat("IncreasedStoppedDistance") * distanceConversion);
    params.putFloat("IncreasedStoppedDistanceLowVisibility", params.getFloat("IncreasedStoppedDistanceLowVisibility") * distanceConversion);
    params.putFloat("IncreasedStoppedDistanceRain", params.getFloat("IncreasedStoppedDistanceRain") * distanceConversion);
    params.putFloat("IncreasedStoppedDistanceRainStorm", params.getFloat("IncreasedStoppedDistanceRainStorm") * distanceConversion);
    params.putFloat("IncreasedStoppedDistanceSnow", params.getFloat("IncreasedStoppedDistanceSnow") * distanceConversion);

    params.putFloat("CESignalSpeed", std::lround(params.getFloat("CESignalSpeed") * speedConversion));
    params.putFloat("CESpeed", std::lround(params.getFloat("CESpeed") * speedConversion));
    params.putFloat("CESpeedLead", std::lround(params.getFloat("CESpeedLead") * speedConversion));
    params.putFloat("Offset1", std::lround(params.getFloat("Offset1") * speedConversion));
    params.putFloat("Offset2", std::lround(params.getFloat("Offset2") * speedConversion));
    params.putFloat("Offset3", std::lround(params.getFloat("Offset3") * speedConversion));
    params.putFloat("Offset4", std::lround(params.getFloat("Offset4") * speedConversion));
    params.putFloat("Offset5", std::lround(params.getFloat("Offset5") * speedConversion));
    params.putFloat("Offset6", std::lround(params.getFloat("Offset6") * speedConversion));
    params.putFloat("Offset7", std::lround(params.getFloat("Offset7") * speedConversion));
    params.putFloat("SetSpeedOffset", std::lround(params.getFloat("SetSpeedOffset") * speedConversion));
  }
  previousMetric = metric;

  static std::map<float, QString> imperialDistanceLabels;
  static std::map<float, QString> metricDistanceLabels;
  static std::map<float, QString> speedLabels;

  static bool labelsInitialized = false;
  if (!labelsInitialized) {
    const QString foot = tr(" foot");
    const QString feet = tr(" feet");
    const QString meter = tr(" meter");
    const QString meters = tr(" meters");

    for (int i = 0; i <= 100; ++i) {
      float distance = i / 10.0f;
      imperialDistanceLabels[distance] = i == 0 ? tr("Off") : i == 10 ? QString::number(distance, 'f', 1) + foot : QString::number(distance, 'f', 1) + feet;
    }

    for (int i = 0; i <= 30; ++i) {
      float distance = i / 10.0f;
      metricDistanceLabels[distance] = i == 0 ? tr("Off") : i == 10 ? QString::number(distance, 'f', 1) + meter : QString::number(distance, 'f', 1) + meters;
    }

    speedLabels[0] = tr("Off");

    labelsInitialized = true;
  }

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

    offset1Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 0 and 29 km/h.</b>"));
    offset2Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 30 and 49 km/h.</b>"));
    offset3Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 50 and 59 km/h.</b>"));
    offset4Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 60 and 79 km/h.</b>"));
    offset5Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 80 and 99 km/h.</b>"));
    offset6Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 100 and 119 km/h.</b>"));
    offset7Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 120 and 140 km/h.</b>"));

    increasedStoppedDistanceToggle->updateControl(0, 3, metricDistanceLabels);
    increasedStoppedDistanceLowVisibilityToggle->updateControl(0, 3, metricDistanceLabels);
    increasedStoppedDistanceRainToggle->updateControl(0, 3, metricDistanceLabels);
    increasedStoppedDistanceRainStormToggle->updateControl(0, 3, metricDistanceLabels);
    increasedStoppedDistanceSnowToggle->updateControl(0, 3, metricDistanceLabels);

    ceSignal->updateControl(0, 150, speedLabels, tr(" km/h"));
    ceSpeedLeadToggle->updateControl(0, 150, speedLabels, tr(" km/h"));
    ceSpeedToggle->updateControl(0, 150, speedLabels, tr(" km/h"));
    customCruiseToggle->updateControl(1, 150, speedLabels, tr(" km/h"));
    customCruiseLongToggle->updateControl(1, 150, speedLabels, tr(" km/h"));
    offset1Toggle->updateControl(-150, 150, speedLabels, tr(" km/h"));
    offset2Toggle->updateControl(-150, 150, speedLabels, tr(" km/h"));
    offset3Toggle->updateControl(-150, 150, speedLabels, tr(" km/h"));
    offset4Toggle->updateControl(-150, 150, speedLabels, tr(" km/h"));
    offset5Toggle->updateControl(-150, 150, speedLabels, tr(" km/h"));
    offset6Toggle->updateControl(-150, 150, speedLabels, tr(" km/h"));
    offset7Toggle->updateControl(-150, 150, speedLabels, tr(" km/h"));
    setSpeedOffsetToggle->updateControl(0, 150, speedLabels, tr(" km/h"));
  } else {
    offset1Toggle->setTitle(tr("Speed Offset (0–24 mph)"));
    offset2Toggle->setTitle(tr("Speed Offset (25–34 mph)"));
    offset3Toggle->setTitle(tr("Speed Offset (35–44 mph)"));
    offset4Toggle->setTitle(tr("Speed Offset (45–54 mph)"));
    offset5Toggle->setTitle(tr("Speed Offset (55–64 mph)"));
    offset6Toggle->setTitle(tr("Speed Offset (65–74 mph)"));
    offset7Toggle->setTitle(tr("Speed Offset (75–99 mph)"));

    offset1Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 0 and 24 mph.</b>"));
    offset2Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 25 and 34 mph.</b>"));
    offset3Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 35 and 44 mph.</b>"));
    offset4Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 45 and 54 mph.</b>"));
    offset5Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 55 and 64 mph.</b>"));
    offset6Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 65 and 74 mph.</b>"));
    offset7Toggle->setDescription(tr("<b>How far above or below the posted limit openpilot drives on roads posted between 75 and 99 mph.</b>"));

    increasedStoppedDistanceToggle->updateControl(0, 10, imperialDistanceLabels);
    increasedStoppedDistanceLowVisibilityToggle->updateControl(0, 10, imperialDistanceLabels);
    increasedStoppedDistanceRainToggle->updateControl(0, 10, imperialDistanceLabels);
    increasedStoppedDistanceRainStormToggle->updateControl(0, 10, imperialDistanceLabels);
    increasedStoppedDistanceSnowToggle->updateControl(0, 10, imperialDistanceLabels);

    ceSignal->updateControl(0, 99, speedLabels, tr(" mph"));
    ceSpeedLeadToggle->updateControl(0, 99, speedLabels, tr(" mph"));
    ceSpeedToggle->updateControl(0, 99, speedLabels, tr(" mph"));
    customCruiseToggle->updateControl(1, 99, speedLabels, tr(" mph"));
    customCruiseLongToggle->updateControl(1, 99, speedLabels, tr(" mph"));
    offset1Toggle->updateControl(-99, 99, speedLabels, tr(" mph"));
    offset2Toggle->updateControl(-99, 99, speedLabels, tr(" mph"));
    offset3Toggle->updateControl(-99, 99, speedLabels, tr(" mph"));
    offset4Toggle->updateControl(-99, 99, speedLabels, tr(" mph"));
    offset5Toggle->updateControl(-99, 99, speedLabels, tr(" mph"));
    offset6Toggle->updateControl(-99, 99, speedLabels, tr(" mph"));
    offset7Toggle->updateControl(-99, 99, speedLabels, tr(" mph"));
    setSpeedOffsetToggle->updateControl(0, 99, speedLabels, tr(" mph"));
  }
}

void FrogPilotLongitudinalPanel::updateSLCPriorityValue() {
  QStringList selectedPriorities;
  for (int i = 1; i <= 3; ++i) {
    QString priority = QString::fromStdString(params.get(QString("SLCPriority%1").arg(i).toStdString()));
    const int storedIndex = canonicalPriorities.indexOf(priority);
    if (storedIndex >= 0) {
      priority = translatedPriorities[storedIndex];
    }
    if (!parent->hasDashSpeedLimits && priority == tr("Dashboard")) {
      continue;
    }
    if (primaryPriorities.contains(priority)) {
      selectedPriorities.append(priority);
    }
    if (priority == tr("Highest") || priority == tr("Lowest")) {
      break;
    }
  }
  slcPriorityButton->setValue(selectedPriorities.join(", "));
}

void FrogPilotLongitudinalPanel::updateToggles() {
  bool frogsGoMoosTweak = parent->isToyota && params.getBool("FrogsGoMoosTweak");

  QSet<QString> visibleParents;

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      continue;
    }

    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels.value(key).toDouble();

    if (key == "CEStopLights") {
      setVisible &= parent->tuningLevel < parent->frogpilotToggleLevels.value("CEModelStopTime").toDouble();
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
      setVisible &= parent->hasPCMCruise;
      setVisible &= parent->isToyota;
    }

    else if (key == "SLCMapboxFiller") {
      setVisible &= !params.get("MapboxPublicKey").empty();
    }

    else if (key == "StartAccel") {
      setVisible &= !(params.getBool("LongitudinalTune") && params.getBool("HumanAcceleration"));
    }

    else if (key == "StoppingDecelRate" || key == "VEgoStarting" || key == "VEgoStopping") {
      setVisible &= !frogsGoMoosTweak;
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (advancedLongitudinalTuneKeys.contains(key)) {
        visibleParents.insert("AdvancedLongitudinalTune");
      } else if (aggressivePersonalityKeys.contains(key)) {
        toggles["AggressivePersonalityProfile"]->setVisible(true);
      } else if (conditionalExperimentalKeys.contains(key)) {
        visibleParents.insert("ConditionalExperimental");
      } else if (curveSpeedKeys.contains(key)) {
        visibleParents.insert("CurveSpeedController");
      } else if (customDrivingPersonalityKeys.contains(key)) {
        visibleParents.insert("CustomPersonalities");
      } else if (longitudinalTuneKeys.contains(key)) {
        visibleParents.insert("LongitudinalTune");
      } else if (qolKeys.contains(key)) {
        visibleParents.insert("QOLLongitudinal");
      } else if (relaxedPersonalityKeys.contains(key)) {
        toggles["RelaxedPersonalityProfile"]->setVisible(true);
      } else if (speedLimitControllerKeys.contains(key)) {
        visibleParents.insert("SpeedLimitController");
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

  for (const QString &key : parentKeys) {
    toggles[key]->setVisible(visibleParents.contains(key));
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
