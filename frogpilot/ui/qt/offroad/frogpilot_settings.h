#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "selfdrive/ui/qt/offroad/settings.h"
#include "selfdrive/ui/qt/widgets/scrollview.h"

class QNetworkAccessManager;

class FrogPilotSettingsWindow : public QFrame {
  Q_OBJECT

public:
  explicit FrogPilotSettingsWindow(SettingsWindow *parent);

  void updateTuningLevel();
  void updateVariables();

  bool canUsePedal = false;
  bool carDetected = false;
  bool canUseSDSU = false;
  bool forceOpenDescriptions = false;
  bool hasAutoTune = true;
  bool hasBSM = true;
  bool hasDashSpeedLimits = true;
  bool hasExperimentalOpenpilotLongitudinal = false;
  bool hasNNFFLog = true;
  bool hasOpenpilotLongitudinal = true;
  bool hasPCMCruise = false;
  bool hasPedal = false;
  bool hasRadar = true;
  bool hasSDSU = false;
  bool hasSNG = false;
  bool hasZSS = false;
  bool isAngleCar = false;
  bool isGM = true;
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
  bool openpilotLongitudinalControlDisabled = false;
  bool useLkasForAOL = false;

  float friction = 0;
  float latAccelFactor = 0;
  float longitudinalActuatorDelay = 0;
  float maxLateralAccel = 0;
  float startAccel = 0;
  float steerActuatorDelay = 0;
  float steerKp = 0;
  float steerRatio = 0;
  float stopAccel = 0;
  float stoppingDecelRate = 0;
  float vEgoStarting = 0;
  float vEgoStopping = 0;

  int tuningLevel = 0;

  QJsonObject frogpilotToggleLevels;

  std::string carFingerprint;

signals:
  void closeSubPanel();
  void closeSubSubPanel();
  void closeSubSubSubPanel();
  void openPanel();
  void openSubPanel();
  void openSubSubPanel();
  void openSubSubSubPanel();
  void updateMetric(bool metric, bool bootRun=false);

private:
  void closePanel();
  void createPanelButtons(FrogPilotListWidget *list);
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void updateState();

  bool panelOpen = false;

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
  Params params_memory{"/dev/shm/params"};

  QStackedLayout *mainLayout;

  ScrollView *frogpilotPanel;
};
