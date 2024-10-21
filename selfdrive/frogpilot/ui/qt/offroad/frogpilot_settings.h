#pragma once

#include "selfdrive/ui/qt/offroad/settings.h"

class FrogPilotSettingsWindow : public QFrame {
  Q_OBJECT

public:
  explicit FrogPilotSettingsWindow(SettingsWindow *parent);

  bool disableOpenpilotLongitudinal = false;
  bool forcingAutoTune = false;
  bool hasAutoTune = true;
  bool hasBSM = true;
  bool hasDashSpeedLimits = true;
  bool hasExperimentalOpenpilotLongitudinal = false;
  bool hasNNFFLog = true;
  bool hasOpenpilotLongitudinal = true;
  bool hasPCMCruise = true;
  bool hasRadar = true;
  bool hasSNG = false;
  bool isBolt = false;
  bool isGM = true;
  bool isGMPCMCruise = false;
  bool isHKGCanFd = true;
  bool isImpreza = true;
  bool isPIDCar = false;
  bool isSubaru = true;
  bool isToyota = true;
  bool isVolt = true;
  bool liveValid = false;

  float steerFrictionStock;
  float steerKPStock;
  float steerLatAccelStock;
  float steerRatioStock;

  int customizationLevel;

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
  void updateCarToggles();
  void updateMetric();

private:
  void addPanelControl(FrogPilotListWidget *list, QString &title, QString &desc, std::vector<QString> &button_labels, QString &icon, std::vector<QWidget*> &panels, QString &currentPanel);
  void closePanel();
  void showEvent(QShowEvent *event) override;
  void updateCarVariables();
  void updatePanelVisibility();

  FrogPilotButtonsControl *drivingButton;
  FrogPilotButtonsControl *navigationButton;
  FrogPilotButtonsControl *systemButton;

  Params params;

  QStackedLayout *mainLayout;

  QWidget *frogpilotSettingsWidget;
};
