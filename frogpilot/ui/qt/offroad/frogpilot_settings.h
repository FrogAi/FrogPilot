#pragma once

#include "selfdrive/ui/qt/offroad/settings.h"
#include "selfdrive/ui/qt/widgets/scrollview.h"

class FrogPilotSettingsWindow : public QFrame {
  Q_OBJECT

public:
  explicit FrogPilotSettingsWindow(SettingsWindow *parent);

  void confirmTuningLevel(QWidget *dialogParent);
  void updateTuningLevel();
  void updateVariables();

  bool canDisableOpenpilotLong = true;
  bool canUseDSUBypass = false;
  bool canUsePedal = false;
  bool carDetected = false;
  bool canUseSDSU = false;
  bool hasAlphaLongitudinal = false;
  bool hasAutoTune = true;
  bool hasBSM = true;
  bool hasDashSpeedLimits = true;
  bool hasLKASButton = true;
  bool hasNNFFLog = true;
  bool hasOpenpilotLongitudinal = true;
  bool hasPCMCruise = false;
  bool hasPedal = false;
  bool hasRadar = true;
  bool hasSDSU = false;
  bool hasSNG = false;
  bool hasZSS = false;
  bool isAngleCar = false;
  bool isFrogsGoMoo = ::isFrogsGoMoo();
  bool isGM = true;
  bool isGMCCOnly = false;
  bool isHKG = true;
  bool isHKGCanFd = true;
  bool isHondaNidec = false;
  bool isSubaru = false;
  bool isTorqueCar = false;
  bool isToyota = true;
  bool isTSK = false;
  bool isVolt = true;
  bool keepScreenOn = false;
  bool lkasAllowedForAOL = false;
  bool openpilotLongitudinalControlDisabled = false;

  float friction = 0.0f;
  float latAccelFactor = 0.0f;
  float longitudinalActuatorDelay = 0.0f;
  float startAccel = 0.0f;
  float steerActuatorDelay = 0.0f;
  float steerKp = 0.0f;
  float steerRatio = 0.0f;
  float stopAccel = 0.0f;
  float stoppingDecelRate = 0.0f;
  float vEgoStarting = 0.0f;
  float vEgoStopping = 0.0f;

  int activeOperations = 0;
  int tuningLevel = 0;

  std::string carFingerprint;

  QJsonObject frogpilotToggleLevels;

signals:
  void closeSubPanel();
  void closeSubSubPanel();
  void closeSubSubSubPanel();
  void openPanel();
  void openSubPanel();
  void openSubSubPanel();
  void openSubSubSubPanel();
  void tuningLevelChanged(int level);
  void updateMetric(bool metric, bool bootRun=false);

private:
  void closePanel();
  void createPanelButtons(FrogPilotListWidget *list);
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void updateState();

  bool forceOpenDescriptions = false;
  bool panelOpen = false;

  FrogPilotButtonsControl *drivingPanelButtons = nullptr;
  FrogPilotButtonsControl *navigationPanelButtons = nullptr;
  FrogPilotButtonsControl *soundPanelButtons = nullptr;
  FrogPilotButtonsControl *systemPanelButtons = nullptr;
  FrogPilotButtonsControl *themePanelButtons = nullptr;
  FrogPilotButtonsControl *togglePreset = nullptr;
  FrogPilotButtonsControl *vehiclePanelButtons = nullptr;

  ParamWatcher *carParamsWatcher = nullptr;

  Params params;

  QJsonObject shownDescriptions;

  QStackedLayout *mainLayout;

  ScrollView *frogpilotPanel;
};
