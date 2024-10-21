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
  void updateCarToggles();
  void updateMetric();

  std::set<QString> aggressivePersonalityKeys = {
    "AggressiveFollow", "AggressiveJerkAcceleration", "AggressiveJerkDeceleration",
    "AggressiveJerkDanger", "AggressiveJerkSpeed", "AggressiveJerkSpeedDecrease",
    "ResetAggressivePersonality"
  };

  std::set<QString> conditionalExperimentalKeys = {
    "CESpeed", "CESpeedLead", "CECurves",
    "CELead", "CEModelStopTime", "CENavigation",
    "CESignalSpeed", "HideCEMStatusBar"
  };

  std::set<QString> curveSpeedKeys = {
    "CurveDetectionMethod", "CurveSensitivity", "DisableCurveSpeedSmoothing",
    "MTSCCurvatureCheck", "TurnAggressiveness"
  };

  std::set<QString> customDrivingPersonalityKeys = {
    "AggressivePersonalityProfile", "RelaxedPersonalityProfile", "StandardPersonalityProfile",
    "TrafficPersonalityProfile"
  };

  std::set<QString> experimentalModeActivationKeys = {
    "ExperimentalModeViaDistance", "ExperimentalModeViaLKAS", "ExperimentalModeViaTap"
  };

  std::set<QString> longitudinalTuneKeys = {
    "AccelerationProfile", "DecelerationProfile", "HumanAcceleration",
    "HumanFollowing", "LeadDetectionThreshold", "MaxDesiredAcceleration",
    "TacoTune"
  };

  std::set<QString> qolKeys = {
    "CustomCruise", "CustomCruiseLong", "ForceStandstill",
    "ForceStops", "IncreasedStoppedDistance", "MapGears",
    "ReverseCruise", "SetSpeedOffset"
  };

  std::set<QString> relaxedPersonalityKeys = {
    "RelaxedFollow", "RelaxedJerkAcceleration", "RelaxedJerkDeceleration",
    "RelaxedJerkDanger", "RelaxedJerkSpeed", "RelaxedJerkSpeedDecrease",
    "ResetRelaxedPersonality"
  };

  std::set<QString> speedLimitControllerKeys = {
    "SLCConfirmation", "SLCOffsets", "SLCFallback",
    "SLCOverride", "SLCPriority", "SLCQOL"
  };

  std::set<QString> speedLimitControllerOffsetsKeys = {
    "Offset1", "Offset2", "Offset3", "Offset4"
  };

  std::set<QString> speedLimitControllerQOLKeys = {
    "ForceMPHDashboard", "SetSpeedLimit", "SLCLookaheadHigher",
    "SLCLookaheadLower"
  };

  std::set<QString> standardPersonalityKeys = {
    "StandardFollow", "StandardJerkAcceleration", "StandardJerkDeceleration",
    "StandardJerkDanger", "StandardJerkSpeed", "StandardJerkSpeedDecrease",
    "ResetStandardPersonality"
  };

  std::set<QString> trafficPersonalityKeys = {
    "TrafficFollow", "TrafficJerkAcceleration", "TrafficJerkDeceleration",
    "TrafficJerkDanger", "TrafficJerkSpeed", "TrafficJerkSpeedDecrease",
    "ResetTrafficPersonality"
  };

  FrogPilotSettingsWindow *parent;

  FrogPilotButtonsControl *curveDetectionBtn;

  Params params;

  bool customPersonalityOpen;
  bool disableOpenpilotLongitudinal;
  bool hasPCMCruise;
  bool hasDashSpeedLimits;
  bool isGM;
  bool isHKGCanFd;
  bool isMetric = params.getBool("IsMetric");
  bool isSubaru;
  bool isToyota;
  bool slcOpen;

  int customizationLevel;

  std::map<QString, AbstractControl*> toggles;
};
