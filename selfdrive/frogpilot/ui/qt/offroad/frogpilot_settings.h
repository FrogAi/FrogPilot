#pragma once

#include "selfdrive/ui/qt/offroad/settings.h"
#include "selfdrive/ui/qt/widgets/scrollview.h"

class FrogPilotSettingsWindow : public QFrame {
  Q_OBJECT

public:
  explicit FrogPilotSettingsWindow(SettingsWindow *parent);

  void updateVariables();

  bool hasAutoTune = true;
  bool hasBSM = true;
  bool hasDashSpeedLimits = true;
  bool hasExperimentalOpenpilotLongitudinal = false;
  bool hasNNFFLog = true;
  bool hasOpenpilotLongitudinal = true;
  bool hasPCMCruise = false;
  bool hasRadar = true;
  bool hasSNG = false;
  bool isBolt = false;
  bool isGM = true;
  bool isHKG = true;
  bool isHKGCanFd = true;
  bool isPIDCar = false;
  bool isSubaru = false;
  bool isToyota = true;
  bool isVolt = true;
  bool keepScreenOn = false;
  bool liveValid = false;

  float frictionStock;
  float kpStock;
  float latAccelStock;
  float steerRatioStock;

  int tuningLevel;

  QJsonObject frogpilotToggleLevels;

signals:
  void closeMapBoxInstructions();
  void closeMapSelection();
  void closeParentToggle();
  void closeSubParentToggle();
  void openMapBoxInstructions();
  void openMapSelection();
  void openPanel();
  void openParentToggle();
  void openSubParentToggle();
  void updateMetric(bool metric, bool bootRun=false);

private:
  void closePanel();
  void createPanelButtons(FrogPilotListWidget *list);
  void hideEvent(QHideEvent *event) override;
  void updateState();

  bool panelOpen;

  FrogPilotButtonsControl *drivingPanelButtons;
  FrogPilotButtonsControl *navigationPanelButtons;
  FrogPilotButtonsControl *systemPanelButtons;

  Params params;
  Params params_memory{"/dev/shm/params"};
  Params paramsTracking{"/cache/tracking"};

  QStackedLayout *mainLayout;

  ScrollView *frogpilotPanel;
};
