#pragma once

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

  bool forceOpenDescriptions;
  bool hasAutoTune;
  bool hasNNFFLog;
  bool hasOpenpilotLongitudinal;
  bool isAngleCar;
  bool isHKGCanFd;
  bool isTorqueCar;
  bool started;

  float friction;
  float latAccelFactor;
  float steerActuatorDelay;
  float steerKp;
  float steerRatio;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> advancedLateralTuneKeys = {"ForceAutoTune", "ForceAutoTuneOff", "ForceTorqueController", "SteerDelay", "SteerFriction", "SteerLatAccel", "SteerKP", "SteerRatio"};
  QSet<QString> aolKeys = {"AlwaysOnLateralLKAS", "AlwaysOnLateralMain", "PauseAOLOnBrake"};
  QSet<QString> laneChangeKeys = {"LaneChangeTime", "LaneDetectionWidth", "MinimumLaneChangeSpeed", "NudgelessLaneChange", "OneLaneChange"};
  QSet<QString> lateralTuneKeys = {"NNFF", "NNFFLite", "TurnDesires"};
  QSet<QString> qolKeys = {"PauseLateralSpeed"};

  QSet<QString> parentKeys;

  FrogPilotParamValueButtonControl *steerDelayToggle;
  FrogPilotParamValueButtonControl *steerFrictionToggle;
  FrogPilotParamValueButtonControl *steerLatAccelToggle;
  FrogPilotParamValueButtonControl *steerKPToggle;
  FrogPilotParamValueButtonControl *steerRatioToggle;

  FrogPilotSettingsWindow *parent;

  QJsonObject frogpilotToggleLevels;

  Params params;
};
