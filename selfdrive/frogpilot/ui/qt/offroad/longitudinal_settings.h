#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotLongitudinalPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotLongitudinalPanel(FrogPilotSettingsWindow *parent);

signals:
  void openParentToggle();
  void openSubParentToggle();

private:
  void hideSubToggles();
  void hideToggles();
  void showEvent(QShowEvent *event) override;
  void showToggles(const std::set<QString> &keys);
  void updateMetric(bool metric, bool bootRun);

  bool customPersonalityOpen;
  bool hasDashSpeedLimits;
  bool hasPCMCruise;
  bool isGM;
  bool isHKGCanFd;
  bool isSubaru;
  bool isToyota;
  bool slcOpen;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  std::set<QString> aggressivePersonalityKeys = {"AggressiveFollow", "AggressiveJerkAcceleration", "AggressiveJerkDeceleration", "AggressiveJerkDanger", "AggressiveJerkSpeed", "AggressiveJerkSpeedDecrease", "ResetAggressivePersonality"};
  std::set<QString> conditionalExperimentalKeys = {"CESpeed", "CESpeedLead", "CECurves", "CELead", "CEModelStopTime", "CENavigation", "CESignalSpeed", "ShowCEMStatus"};
  std::set<QString> curveSpeedKeys = {"CurveDetectionMethod", "CurveSensitivity", "HideCSCUI", "MTSCCurvatureCheck", "TurnAggressiveness"};
  std::set<QString> customDrivingPersonalityKeys = {"AggressivePersonalityProfile", "RelaxedPersonalityProfile", "StandardPersonalityProfile", "TrafficPersonalityProfile"};
  std::set<QString> experimentalModeActivationKeys = {"ExperimentalModeViaDistance", "ExperimentalModeViaLKAS", "ExperimentalModeViaTap"};
  std::set<QString> longitudinalTuneKeys = {"AccelerationProfile", "DecelerationProfile", "HumanAcceleration", "HumanFollowing", "LeadDetectionThreshold", "MaxDesiredAcceleration", "TacoTune"};
  std::set<QString> qolKeys = {"CustomCruise", "CustomCruiseLong", "ForceStandstill", "ForceStops", "IncreasedStoppedDistance", "MapGears", "ReverseCruise", "SetSpeedOffset"};
  std::set<QString> relaxedPersonalityKeys = {"RelaxedFollow", "RelaxedJerkAcceleration", "RelaxedJerkDeceleration", "RelaxedJerkDanger", "RelaxedJerkSpeed", "RelaxedJerkSpeedDecrease", "ResetRelaxedPersonality"};
  std::set<QString> speedLimitControllerKeys = {"SLCOffsets", "SLCFallback", "SLCOverride", "SLCPriority", "SLCQOL", "SLCVisuals"};
  std::set<QString> speedLimitControllerOffsetsKeys = {"Offset1", "Offset2", "Offset3", "Offset4"};
  std::set<QString> speedLimitControllerQOLKeys = {"ForceMPHDashboard", "SetSpeedLimit", "SLCConfirmation", "SLCLookaheadHigher", "SLCLookaheadLower"};
  std::set<QString> speedLimitControllerVisualKeys = {"ShowSLCOffset", "SpeedLimitSources"};
  std::set<QString> standardPersonalityKeys = {"StandardFollow", "StandardJerkAcceleration", "StandardJerkDeceleration", "StandardJerkDanger", "StandardJerkSpeed", "StandardJerkSpeedDecrease", "ResetStandardPersonality"};
  std::set<QString> trafficPersonalityKeys = {"TrafficFollow", "TrafficJerkAcceleration", "TrafficJerkDeceleration", "TrafficJerkDanger", "TrafficJerkSpeed", "TrafficJerkSpeedDecrease", "ResetTrafficPersonality"};

  FrogPilotButtonsControl *curveDetectionToggle;

  FrogPilotSettingsWindow *parent;

  QJsonObject frogpilotToggleLevels;

  Params params;
  Params params_default{"/dev/shm/params_default"};
};
