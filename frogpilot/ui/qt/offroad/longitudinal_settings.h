#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotLongitudinalPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotLongitudinalPanel(FrogPilotSettingsWindow *parent);

signals:
  void openSubPanel();
  void openSubSubPanel();
  void openSubSubSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateMetric(bool metric, bool bootRun);
  void updateToggles();

  bool customPersonalityOpen;
  bool forceOpenDescriptions;
  bool qolOpen;
  bool slcOpen;
  bool weatherOpen;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> advancedLongitudinalTuneKeys = {"LongitudinalActuatorDelay", "MaxDesiredAcceleration", "StartAccel", "StopAccel", "StoppingDecelRate", "VEgoStarting", "VEgoStopping"};
  QSet<QString> aggressivePersonalityKeys = {"AggressiveFollow", "AggressiveJerkAcceleration", "AggressiveJerkDeceleration", "AggressiveJerkDanger", "AggressiveJerkSpeed", "AggressiveJerkSpeedDecrease", "ResetAggressivePersonality"};
  QSet<QString> conditionalExperimentalKeys = {"CESpeed", "CESpeedLead", "CECurves", "CELead", "CEModelStopTime", "CENavigation", "CESignalSpeed", "CEStopLights", "ShowCEMStatus"};
  QSet<QString> curveSpeedKeys = {"CalibratedLateralAcceleration", "CalibrationProgress", "ResetCurveData", "ShowCSCStatus"};
  QSet<QString> customDrivingPersonalityKeys = {"AggressivePersonalityProfile", "RelaxedPersonalityProfile", "StandardPersonalityProfile", "TrafficPersonalityProfile"};
  QSet<QString> longitudinalTuneKeys = {"AccelerationProfile", "DecelerationProfile", "HumanAcceleration", "HumanFollowing", "HumanLaneChanges", "LeadDetectionThreshold", "TacoTune"};
  QSet<QString> qolKeys = {"CustomCruise", "CustomCruiseLong", "ForceStops", "IncreasedStoppedDistance", "MapGears", "ReverseCruise", "SetSpeedOffset", "WeatherPresets"};
  QSet<QString> relaxedPersonalityKeys = {"RelaxedFollow", "RelaxedJerkAcceleration", "RelaxedJerkDeceleration", "RelaxedJerkDanger", "RelaxedJerkSpeed", "RelaxedJerkSpeedDecrease", "ResetRelaxedPersonality"};
  QSet<QString> speedLimitControllerKeys = {"SLCOffsets", "SLCFallback", "SLCOverride", "SLCPriority", "SLCQOL", "SLCVisuals"};
  QSet<QString> speedLimitControllerOffsetsKeys = {"Offset1", "Offset2", "Offset3", "Offset4", "Offset5", "Offset6", "Offset7"};
  QSet<QString> speedLimitControllerQOLKeys = {"ForceMPHDashboard", "SetSpeedLimit", "SLCConfirmation", "SLCLookaheadHigher", "SLCLookaheadLower", "SLCMapboxFiller"};
  QSet<QString> speedLimitControllerVisualKeys = {"ShowSLCOffset", "SpeedLimitSources"};
  QSet<QString> standardPersonalityKeys = {"StandardFollow", "StandardJerkAcceleration", "StandardJerkDeceleration", "StandardJerkDanger", "StandardJerkSpeed", "StandardJerkSpeedDecrease", "ResetStandardPersonality"};
  QSet<QString> trafficPersonalityKeys = {"TrafficFollow", "TrafficJerkAcceleration", "TrafficJerkDeceleration", "TrafficJerkDanger", "TrafficJerkSpeed", "TrafficJerkSpeedDecrease", "ResetTrafficPersonality"};
  QSet<QString> weatherKeys = {"LowVisibilityOffsets", "RainOffsets", "RainStormOffsets", "SetWeatherKey", "SnowOffsets"};
  QSet<QString> weatherLowVisibilityKeys = {"IncreaseFollowingLowVisibility", "IncreasedStoppedDistanceLowVisibility", "ReduceAccelerationLowVisibility", "ReduceLateralAccelerationLowVisibility"};
  QSet<QString> weatherRainKeys = {"IncreaseFollowingRain", "IncreasedStoppedDistanceRain", "ReduceAccelerationRain", "ReduceLateralAccelerationRain"};
  QSet<QString> weatherRainStormKeys = {"IncreaseFollowingRainStorm", "IncreasedStoppedDistanceRainStorm", "ReduceAccelerationRainStorm", "ReduceLateralAccelerationRainStorm"};
  QSet<QString> weatherSnowKeys = {"IncreaseFollowingSnow", "IncreasedStoppedDistanceSnow", "ReduceAccelerationSnow", "ReduceLateralAccelerationSnow"};

  QSet<QString> parentKeys;

  FrogPilotButtonsControl *weatherKeyControl;

  FrogPilotParamValueControl *longitudinalActuatorDelayToggle;
  FrogPilotParamValueControl *startAccelToggle;
  FrogPilotParamValueControl *stopAccelToggle;
  FrogPilotParamValueControl *stoppingDecelRateToggle;
  FrogPilotParamValueControl *vEgoStartingToggle;
  FrogPilotParamValueControl *vEgoStoppingToggle;

  FrogPilotSettingsWindow *parent;

  LabelControl *calibratedLateralAccelerationLabel;
  LabelControl *calibrationProgressLabel;

  QJsonObject frogpilotToggleLevels;

  Params params;
  Params params_cache{"/cache/params"};
  Params params_default{"/dev/shm/params_default"};
  Params params_memory{"/dev/shm/params"};

  QNetworkAccessManager *networkManager;
};
