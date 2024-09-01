#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotAdvancedDrivingPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotAdvancedDrivingPanel(FrogPilotSettingsWindow *parent);

signals:
  void openParentToggle();
  void openSubParentToggle();
  void openSubSubParentToggle();

protected:
  void showEvent(QShowEvent *event) override;

private:
  FrogPilotSettingsWindow *parent;

  ButtonControl *deleteModelBtn;
  ButtonControl *downloadAllModelsBtn;
  ButtonControl *downloadModelBtn;
  ButtonControl *selectModelBtn;

  FrogPilotParamValueButtonControl *steerFrictionToggle;
  FrogPilotParamValueButtonControl *steerLatAccelToggle;
  FrogPilotParamValueButtonControl *steerKPToggle;
  FrogPilotParamValueButtonControl *steerRatioToggle;

  std::set<QString> aggressivePersonalityKeys = {
    "AggressiveFollow", "AggressiveJerkAcceleration", "AggressiveJerkDeceleration",
    "AggressiveJerkDanger", "AggressiveJerkSpeed", "AggressiveJerkSpeedDecrease",
    "ResetAggressivePersonality"
  };

  std::set<QString> customDrivingPersonalityKeys = {
    "AggressivePersonalityProfile", "RelaxedPersonalityProfile",
    "StandardPersonalityProfile", "TrafficPersonalityProfile"
  };

  std::set<QString> lateralTuneKeys = {
    "ForceAutoTune", "ForceAutoTuneOff", "SteerFriction", "SteerLatAccel",
    "SteerKP", "SteerRatio", "TacoTune", "TurnDesires"
  };

  std::set<QString> longitudinalTuneKeys = {
    "LeadDetectionThreshold", "MaxDesiredAcceleration"
  };

  std::set<QString> modelManagementKeys = {
    "AutomaticallyUpdateModels", "DeleteModel", "DownloadModel", "DownloadAllModels",
    "ModelRandomizer", "ResetCalibrations", "SelectModel"
  };

  std::set<QString> modelRandomizerKeys = {
    "ManageBlacklistedModels", "ResetScores", "ReviewScores"
  };

  std::set<QString> qolKeys = {
    "ForceStandstill", "ForceStops", "SetSpeedOffset"
  };

  std::set<QString> relaxedPersonalityKeys = {
    "RelaxedFollow", "RelaxedJerkAcceleration", "RelaxedJerkDeceleration",
    "RelaxedJerkDanger", "RelaxedJerkSpeed", "RelaxedJerkSpeedDecrease",
    "ResetRelaxedPersonality"
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

  std::map<QString, AbstractControl*> toggles;

  QStringList availableModelNames;
  QStringList availableModels;
  QStringList experimentalModels;

  QList<LabelControl*> labelControls;

  QDir modelDir{"/data/models/"};

  Params params;
  Params paramsMemory{"/dev/shm/params"};
  Params paramsStorage{"/persist/params"};

  bool allModelsDownloading;
  bool cancellingDownload;
  bool customPersonalityOpen;
  bool disableOpenpilotLongitudinal;
  bool hasAutoTune;
  bool hasNNFFLog;
  bool hasOpenpilotLongitudinal;
  bool hasPCMCruise;
  bool haveModelsDownloaded;
  bool isMetric = params.getBool("IsMetric");
  bool liveValid;
  bool modelDeleting;
  bool modelDownloading;
  bool modelManagement;
  bool modelManagementOpen;
  bool modelRandomizer;
  bool modelsDownloaded;
  bool started;

  float steerFrictionStock;
  float steerLatAccelStock;
  float steerKPStock;
  float steerRatioStock;

  void hideSubToggles();
  void hideSubSubToggles();
  void hideToggles();
  void showToggles(const std::set<QString> &keys);

  void startDownloadAllModels();
  void updateCalibrationDescription();
  void updateCarToggles();
  void updateMetric();
  void updateModelLabels();
  void updateState(const UIState &s);
};
