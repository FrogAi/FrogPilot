#pragma once

#include "selfdrive/ui/qt/offroad/settings.h"

class FrogPilotSettingsWindow : public QFrame {
  Q_OBJECT

public:
  explicit FrogPilotSettingsWindow(SettingsWindow *parent);

  bool disableOpenpilotLongitudinal;
  bool hasAutoTune;
  bool hasBSM;
  bool hasDashSpeedLimits;
  bool hasExperimentalOpenpilotLongitudinal;
  bool hasNNFFLog;
  bool hasOpenpilotLongitudinal;
  bool hasPCMCruise;
  bool hasSNG;
  bool isGM;
  bool isGMPCMCruise;
  bool isHKGCanFd;
  bool isImpreza;
  bool isPIDCar;
  bool isSubaru;
  bool isToyota;
  bool isToyotaTuneSupported;
  bool isVolt;
  bool forcingAutoTune;
  bool liveValid;

  float steerFrictionStock;
  float steerKPStock;
  float steerLatAccelStock;
  float steerRatioStock;

signals:
  void closeMapBoxInstructions();
  void closeMapSelection();
  void closeParentToggle();
  void closeSubParentToggle();
  void closeSubSubParentToggle();
  void openMapBoxInstructions();
  void openMapSelection();
  void openPanel();
  void openParentToggle();
  void openSubParentToggle();
  void openSubSubParentToggle();
  void updateCarToggles();
  void updateMetric();

private:
  void addPanelControl(FrogPilotListWidget *list, QString &title, QString &desc, std::vector<QString> &button_labels, QString &icon, std::vector<QWidget*> &panels, bool isDrivingPanel, bool isNavigationPanel);
  void closePanel();
  void showEvent(QShowEvent *event) override;
  void updateCarVariables();
  void updatePanelVisibility();

  FrogPilotButtonsControl *drivingButton;
  FrogPilotButtonsControl *navigationButton;

  Params params;

  QStackedLayout *mainLayout;

  QWidget *frogpilotSettingsWidget;
};
