#pragma once

#include <set>

#include "selfdrive/ui/qt/offroad/settings.h"
#include "selfdrive/ui/ui.h"

class FrogPilotControlsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotControlsPanel(SettingsWindow *parent);

signals:
  void openParentToggle();
  void openSubParentToggle();
  void openSubSubParentToggle();

private:
  void hideSubToggles();
  void hideSubSubToggles();
  void hideToggles();
  void showEvent(QShowEvent *event, const UIState &s);
  void updateCarToggles();
  void updateMetric();
  void updateState(const UIState &s);

  ButtonControl *deleteModelBtn;
  ButtonControl *downloadModelBtn;
  ButtonControl *personalitiesInfoBtn;
  ButtonControl *selectModelBtn;

  FrogPilotDualParamControl *conditionalSpeedsImperial;
  FrogPilotDualParamControl *conditionalSpeedsMetric;

  FrogPilotParamManageControl *modelManagerToggle;

  FrogPilotParamValueToggleControl *steerRatioToggle;

  std::set<QString> aggressivePersonalityKeys = {"AggressiveFollow", "AggressiveJerkAcceleration", "AggressiveJerkSpeed", "ResetAggressivePersonality"};
  std::set<QString> aolKeys = {"AlwaysOnLateralMain", "HideAOLStatusBar", "PauseAOLOnBrake"};
  std::set<QString> conditionalExperimentalKeys = {"CECurves", "CECurvesLead", "CENavigation", "CESignal", "CESlowerLead", "CEStopLights", "HideCEMStatusBar"};
  std::set<QString> deviceManagementKeys = {"DeviceShutdown", "IncreaseThermalLimits", "LowVoltageShutdown", "NoLogging", "NoUploads", "OfflineMode"};
  std::set<QString> customdrivingPersonalityKeys = {"TrafficPersonalityProfile", "AggressivePersonalityProfile", "StandardPersonalityProfile", "RelaxedPersonalityProfile"};
  std::set<QString> drivingPersonalityKeys = {"CustomPersonalities", "OnroadDistanceButton"};
  std::set<QString> experimentalModeActivationKeys = {"ExperimentalModeViaDistance", "ExperimentalModeViaLKAS", "ExperimentalModeViaTap"};
  std::set<QString> laneChangeKeys = {"LaneChangeTime", "LaneDetectionWidth", "MinimumLaneChangeSpeed", "NudgelessLaneChange", "OneLaneChange"};
  std::set<QString> lateralTuneKeys = {"ForceAutoTune", "NNFF", "NNFFLite", "SteerRatio", "TacoTune", "TurnDesires"};
  std::set<QString> longitudinalTuneKeys = {"AccelerationProfile", "AggressiveAcceleration", "DecelerationProfile", "LeadDetectionThreshold", "SmoothBraking", "StoppingDistance", "TrafficMode"};
  std::set<QString> mtscKeys = {"DisableMTSCSmoothing", "MTSCAggressiveness", "MTSCCurvatureCheck"};
  std::set<QString> qolKeys = {"CustomCruise", "CustomCruiseLong", "MapGears", "PauseLateralSpeed", "ReverseCruise", "SetSpeedOffset"};
  std::set<QString> relaxedPersonalityKeys = {"RelaxedFollow", "RelaxedJerkAcceleration", "RelaxedJerkSpeed", "ResetRelaxedPersonality"};
  std::set<QString> speedLimitControllerKeys = {"SLCControls", "SLCQOL", "SLCVisuals"};
  std::set<QString> speedLimitControllerControlsKeys = {"Offset1", "Offset2", "Offset3", "Offset4", "SLCFallback", "SLCOverride", "SLCPriority"};
  std::set<QString> speedLimitControllerQOLKeys = {"ForceMPHDashboard", "SetSpeedLimit", "SLCConfirmation", "SLCLookaheadHigher", "SLCLookaheadLower"};
  std::set<QString> speedLimitControllerVisualsKeys = {"ShowSLCOffset", "SpeedLimitChangedAlert", "UseVienna"};
  std::set<QString> standardPersonalityKeys = {"StandardFollow", "StandardJerkAcceleration", "StandardJerkSpeed", "ResetStandardPersonality"};
  std::set<QString> trafficPersonalityKeys = {"TrafficFollow", "TrafficJerkAcceleration", "TrafficJerkSpeed", "ResetTrafficPersonality"};
  std::set<QString> tuningKeys = {"kiV1", "kiV2", "kiV3", "kiV4", "kpV1", "kpV2", "kpV3", "kpV4"};
  std::set<QString> visionTurnControlKeys = {"CurveSensitivity", "DisableVTSCSmoothing", "TurnAggressiveness"};

  std::map<std::string, AbstractControl*> toggles;

  Params params;
  Params paramsMemory{"/dev/shm/params"};

  bool customPersonalitiesOpen;
  bool hasAutoTune;
  bool hasCommaNNFFSupport;
  bool hasNNFFLog;
  bool hasOpenpilotLongitudinal;
  bool hasPCMCruise;
  bool hasDashSpeedLimits;
  bool isGM;
  bool isHKGCanFd;
  bool isMetric = params.getBool("IsMetric");
  bool isRelease;
  bool isToyota;
  bool slcOpen;
  bool started;

  float steerRatioStock;
};
