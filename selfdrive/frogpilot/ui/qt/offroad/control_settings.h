#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/widgets/frogpilot_controls.h"
#include "selfdrive/ui/qt/offroad/settings.h"
#include "selfdrive/ui/ui.h"

class FrogPilotControlsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotControlsPanel(SettingsWindow *parent);

signals:
  void openParentToggle();
  void openSubParentToggle();

private:
  void hideSubToggles();
  void hideToggles();
  void showEvent(QShowEvent *event, const UIState &s);
  void updateCarToggles();
  void updateMetric();
  void updateState();
  void updateToggles();

  std::set<QString> aolKeys = {};
  std::set<QString> conditionalExperimentalKeys = {};
  std::set<QString> deviceManagementKeys = {};
  std::set<QString> experimentalModeActivationKeys = {};
  std::set<QString> laneChangeKeys = {};
  std::set<QString> lateralTuneKeys = {};
  std::set<QString> longitudinalTuneKeys = {"AccelerationProfile", "AggressiveAcceleration", "DecelerationProfile"};
  std::set<QString> mtscKeys = {};
  std::set<QString> qolKeys = {};
  std::set<QString> speedLimitControllerKeys = {};
  std::set<QString> speedLimitControllerControlsKeys = {};
  std::set<QString> speedLimitControllerQOLKeys = {};
  std::set<QString> speedLimitControllerVisualsKeys = {};
  std::set<QString> visionTurnControlKeys = {};

  std::map<std::string, ParamControl*> toggles;

  Params params;
  Params paramsMemory{"/dev/shm/params"};

  bool hasAutoTune;
  bool hasCommaNNFFSupport;
  bool hasNNFFLog;
  bool hasOpenpilotLongitudinal;
  bool hasPCMCruise;
  bool hasDashSpeedLimits;
  bool isMetric = params.getBool("IsMetric");
  bool isStaging;
  bool isToyota;
  bool online;
  bool started;
};
