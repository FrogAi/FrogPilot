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
  QString processModelName(const QString &modelName);

  void hideSubToggles();
  void hideSubSubToggles();
  void hideToggles();
  void showEvent(QShowEvent *event) override;
  void startDownloadAllModels();
  void updateCalibrationDescription();
  void updateCarToggles();
  void updateMetric();
  void updateModelLabels();
  void updateState(const UIState &s);

  ButtonControl *deleteModelBtn;
  ButtonControl *downloadAllModelsBtn;
  ButtonControl *downloadModelBtn;
  ButtonControl *selectModelBtn;

  FrogPilotButtonsControl *manageDistanceIconsBtn;

  FrogPilotParamValueToggleControl *steerRatioToggle;

  LabelControl *downloadStatusLabel;

  std::set<QString> aggressivePersonalityKeys = {"PersonalityInfo", "AggressiveFollow", "AggressiveJerkAcceleration", "AggressiveJerkDanger", "AggressiveJerkSpeed", "ResetAggressivePersonality"};
  std::set<QString> aolKeys = {"AlwaysOnLateralLKAS", "AlwaysOnLateralMain", "HideAOLStatusBar", "PauseAOLOnBrake"};
  std::set<QString> conditionalExperimentalKeys = {"CESpeed", "CESpeedLead", "CECurves", "CELead", "CEModelStopTime", "CENavigation", "CESignal", "HideCEMStatusBar"};
  std::set<QString> customdrivingPersonalityKeys = {"AggressivePersonalityProfile", "RelaxedPersonalityProfile", "StandardPersonalityProfile", "TrafficPersonalityProfile"};
  std::set<QString> deviceManagementKeys = {"DeviceShutdown", "IncreaseThermalLimits", "LowVoltageShutdown", "NoLogging", "NoUploads", "OfflineMode"};
  std::set<QString> drivingPersonalityKeys = {"CustomPersonalities", "OnroadDistanceButton", "OnroadDistanceButtonButtons", "DownloadStatusLabel"};
  std::set<QString> experimentalModeActivationKeys = {"ExperimentalModeViaDistance", "ExperimentalModeViaLKAS", "ExperimentalModeViaTap"};
  std::set<QString> laneChangeKeys = {"LaneChangeTime", "LaneDetectionWidth", "MinimumLaneChangeSpeed", "NudgelessLaneChange", "OneLaneChange"};
  std::set<QString> lateralTuneKeys = {"ForceAutoTune", "NNFF", "NNFFLite", "SteerRatio", "TacoTune", "TurnDesires"};
  std::set<QString> longitudinalTuneKeys = {"AccelerationProfile", "DecelerationProfile", "HumanAcceleration", "HumanFollowing", "LeadDetectionThreshold", "StoppingDistance", "TrafficMode"};
  std::set<QString> modelManagementKeys = {"AutomaticallyUpdateModels", "ModelRandomizer", "DeleteModel", "DownloadModel", "DownloadAllModels", "SelectModel", "ResetCalibrations"};
  std::set<QString> modelRandomizerKeys = {"ManageBlacklistedModels", "ResetScores", "ReviewScores"};
  std::set<QString> mtscKeys = {"DisableMTSCSmoothing", "MTSCAggressiveness", "MTSCCurvatureCheck"};
  std::set<QString> qolKeys = {"CustomCruise", "CustomCruiseLong", "ForceStandstill", "MapGears", "PauseLateralSpeed", "ReverseCruise", "SetSpeedOffset"};
  std::set<QString> relaxedPersonalityKeys = {"PersonalityInfo", "RelaxedFollow", "RelaxedJerkAcceleration", "RelaxedJerkDanger", "RelaxedJerkSpeed", "ResetRelaxedPersonality"};
  std::set<QString> speedLimitControllerKeys = {"SLCControls", "SLCQOL", "SLCVisuals"};
  std::set<QString> speedLimitControllerControlsKeys = {"Offset1", "Offset2", "Offset3", "Offset4", "SLCFallback", "SLCOverride", "SLCPriority"};
  std::set<QString> speedLimitControllerQOLKeys = {"ForceMPHDashboard", "SetSpeedLimit", "SLCConfirmation", "SLCLookaheadHigher", "SLCLookaheadLower"};
  std::set<QString> speedLimitControllerVisualsKeys = {"ShowSLCOffset", "SpeedLimitChangedAlert", "UseVienna"};
  std::set<QString> standardPersonalityKeys = {"PersonalityInfo", "StandardFollow", "StandardJerkAcceleration", "StandardJerkDanger", "StandardJerkSpeed", "ResetStandardPersonality"};
  std::set<QString> trafficPersonalityKeys = {"PersonalityInfo", "TrafficFollow", "TrafficJerkAcceleration", "TrafficJerkDanger", "TrafficJerkSpeed", "ResetTrafficPersonality"};
  std::set<QString> visionTurnControlKeys = {"CurveSensitivity", "DisableVTSCSmoothing", "TurnAggressiveness"};

  std::map<std::string, AbstractControl*> toggles;

  QList<LabelControl*> labelControls;

  QDir modelDir{"/data/models/"};

  Params params;
  Params paramsMemory{"/dev/shm/params"};
  Params paramsStorage{"/persist/params"};

  bool allModelsDownloading;
  bool cancellingDownload;
  bool customPersonalitiesOpen;
  bool disableOpenpilotLongitudinal;
  bool drivingPersonalitiesOpen;
  bool hasAutoTune;
  bool hasCommaNNFFSupport;
  bool hasNNFFLog;
  bool hasOpenpilotLongitudinal;
  bool hasPCMCruise;
  bool hasDashSpeedLimits;
  bool haveModelsDownloaded;
  bool iconsDownloading;
  bool iconsDownloaded;
  bool isGM;
  bool isHKGCanFd;
  bool isMetric = params.getBool("IsMetric");
  bool isRelease;
  bool isToyota;
  bool modelDeleting;
  bool modelDownloading;
  bool modelManagementOpen;
  bool modelRandomizer;
  bool modelsDownloaded;
  bool onroadDistanceButton;
  bool slcOpen;
  bool started;
  bool themeDeleting;
  bool themeDownloading;

  float steerRatioStock;

  QStringList availableModelNames;
  QStringList availableModels;
  QStringList experimentalModels;
};
