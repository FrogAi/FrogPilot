#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotLateralPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotLateralPanel(FrogPilotSettingsWindow *parent);

signals:
  void openParentToggle();

private:
  void hideToggles();
  void showEvent(QShowEvent *event) override;
  void showToggles(const std::set<QString> &keys);
  void updateMetric(bool metric, bool bootRun);
  void updateState(const UIState &s);

  bool hasAutoTune;
  bool hasNNFFLog;
  bool isPIDCar;
  bool isSubaru;
  bool liveValid;
  bool started;

  float frictionStock;
  float latAccelStock;
  float kpStock;
  float steerRatioStock;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  std::set<QString> advancedLateralTuneKeys = {"ForceAutoTune", "ForceAutoTuneOff", "SteerFriction", "SteerLatAccel", "SteerKP", "SteerRatio"};
  std::set<QString> aolKeys = {"AlwaysOnLateralLKAS", "AlwaysOnLateralMain", "PauseAOLOnBrake"};
  std::set<QString> laneChangeKeys = {"LaneChangeTime", "LaneDetectionWidth", "MinimumLaneChangeSpeed", "NudgelessLaneChange", "OneLaneChange"};
  std::set<QString> lateralTuneKeys = {"NNFF", "NNFFLite", "TurnDesires"};
  std::set<QString> qolKeys = {"PauseLateralSpeed"};

  FrogPilotParamValueButtonControl *steerFrictionToggle;
  FrogPilotParamValueButtonControl *steerLatAccelToggle;
  FrogPilotParamValueButtonControl *steerKPToggle;
  FrogPilotParamValueButtonControl *steerRatioToggle;

  FrogPilotSettingsWindow *parent;

  QJsonObject frogpilotToggleLevels;

  Params params;
};
