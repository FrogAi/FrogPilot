#pragma once

#include <set>

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotLateralPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotLateralPanel(FrogPilotSettingsWindow *parent);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateMetric(bool metric, bool bootRun);
  void updateState(const UIState &s);
  void updateToggles();

  bool hasAutoTune;
  bool hasNNFFLog;
  bool hasOpenpilotLongitudinal;
  bool isHKGCanFd;
  bool isHonda;
  bool isTorqueCar;
  bool started;

  float delayStock;
  float frictionStock;
  float latAccelStock;
  float kpStock;
  float steerRatioStock;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  std::set<QString> advancedLateralTuneKeys = {"ForceAutoTune", "ForceAutoTuneOff", "SteerDelay", "SteerFriction", "SteerLatAccel", "SteerKP", "SteerRatio"};
  std::set<QString> aolKeys = {"AlwaysOnLateralLKAS", "AlwaysOnLateralMain", "PauseAOLOnBrake"};
  std::set<QString> laneChangeKeys = {"LaneChangeTime", "LaneDetectionWidth", "MinimumLaneChangeSpeed", "NudgelessLaneChange", "OneLaneChange"};
  std::set<QString> lateralTuneKeys = {"NNFF", "NNFFLite", "TurnDesires"};
  std::set<QString> qolKeys = {"PauseLateralSpeed"};

  std::set<QString> parentKeys;

  FrogPilotParamValueButtonControl *steerDelayToggle;
  FrogPilotParamValueButtonControl *steerFrictionToggle;
  FrogPilotParamValueButtonControl *steerLatAccelToggle;
  FrogPilotParamValueButtonControl *steerKPToggle;
  FrogPilotParamValueButtonControl *steerRatioToggle;

  FrogPilotSettingsWindow *parent;

  QJsonObject frogpilotToggleLevels;

  Params params;
};
