#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

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
  bool hasAutoTune = true;
  bool hasBSM = true;
  bool hasDashSpeedLimits = true;
  bool hasDistanceButton = true;
  bool hasLKASButton = true;
  bool hasNNFFLog = true;
  bool hasOldLongAPI = false;
  bool hasOpenpilotLongitudinal = true;
  bool hasPCMCruise = false;
  bool hasPedal = false;
  bool hasRadar = true;
  bool hasSDSU = false;
  bool hasSNG = false;
  bool hasStartingState = true;
  bool hasZSS = false;
  bool isAngleCar = false;
  bool isGM = true;
  bool isGMCCOnly = false;
  bool isHKG = true;
  bool isHKGCanFd = true;
  bool isHonda = true;
  bool isHondaNidec = true;
  bool isSubaru = false;
  bool isTorqueCar = false;
  bool isToyota = true;
  bool isTSK = false;
  bool isVolt = true;
  bool keepScreenOn = false;
  bool lkasAllowedForAOL = false;
  bool subaruSNG = false;

  float friction = 0;
  float latAccelFactor = 0;
  float longitudinalActuatorDelay = 0;
  float startAccel = 0;
  float steerActuatorDelay = 0;
  float steerKp = 0;
  float steerRatio = 0;
  float stopAccel = 0;
  float stoppingDecelRate = 0;
  float vEgoStarting = 0;
  float vEgoStopping = 0;

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
  bool isMetric = false;
  bool panelOpen = false;
  bool useHigherBitrate = false;
  bool useKonikServer = false;

  std::string carMake;

  FrogPilotButtonsControl *drivingPanelButtons = nullptr;
  FrogPilotButtonsControl *navigationPanelButtons = nullptr;
  FrogPilotButtonsControl *soundPanelButtons = nullptr;
  FrogPilotButtonsControl *systemPanelButtons = nullptr;
  FrogPilotButtonsControl *themePanelButtons = nullptr;
  FrogPilotButtonsControl *togglePreset = nullptr;
  FrogPilotButtonsControl *vehiclePanelButtons = nullptr;

  ParamWatcher *carParamsWatcher = nullptr;

  Params params;
  Params params_default{"/dev/shm/params_default"};
  Params params_memory{"/dev/shm/params"};

  QJsonObject shownDescriptions;

  QStackedLayout *mainLayout = nullptr;

  ScrollView *frogpilotPanel = nullptr;
};
