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
  FrogPilotSettingsWindow *parent;

  FrogPilotButtonsControl *curveDetectionBtn;
  FrogPilotButtonsControl *slcConfirmationBtn;

  std::set<QString> conditionalExperimentalKeys = {
    "CESpeed", "CESpeedLead", "CECurves", "CELead",
    "CEModelStopTime", "CENavigation", "CESignalSpeed",
    "HideCEMStatusBar"
  };

  std::set<QString> curveSpeedKeys = {
    "CurveDetectionMethod", "CurveSensitivity",
    "DisableCurveSpeedSmoothing", "MTSCCurvatureCheck",
    "TurnAggressiveness"
  };

  std::set<QString> experimentalModeActivationKeys = {
    "ExperimentalModeViaDistance", "ExperimentalModeViaLKAS",
    "ExperimentalModeViaTap"
  };

  std::set<QString> longitudinalTuneKeys = {
    "AccelerationProfile", "DecelerationProfile",
    "HumanAcceleration", "HumanFollowing", "IncreasedStoppedDistance"
  };

  std::set<QString> qolKeys = {
    "CustomCruise", "CustomCruiseLong", "MapGears",
    "OnroadDistanceButton", "ReverseCruise"
  };

  std::set<QString> speedLimitControllerKeys = {
    "SLCConfirmation", "SLCOffsets", "SLCFallback", "SLCOverride",
    "SLCPriority", "SLCQOL", "SLCVisuals"
  };

  std::set<QString> speedLimitControllerOffsetsKeys = {
    "Offset1", "Offset2", "Offset3", "Offset4"
  };

  std::set<QString> speedLimitControllerQOLKeys = {
    "ForceMPHDashboard", "SetSpeedLimit", "SLCLookaheadHigher",
    "SLCLookaheadLower"
  };

  std::set<QString> speedLimitControllerVisualsKeys = {
    "ShowSLCOffset", "UseVienna"
  };

  std::map<QString, AbstractControl*> toggles;

  Params params;

  bool hasPCMCruise;
  bool hasDashSpeedLimits;
  bool isGM;
  bool isHKGCanFd;
  bool isMetric = params.getBool("IsMetric");
  bool isSubaru;
  bool isToyota;
  bool slcOpen;

  void hideSubToggles();
  void hideToggles();
  void showToggles(const std::set<QString> &keys);
  void updateCarToggles();
  void updateMetric();
};
