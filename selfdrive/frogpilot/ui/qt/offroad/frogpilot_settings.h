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
  bool isSubaru;
  bool isToyota;
  bool isVolt;
  bool forcingAutoTune;
  bool liveValid;

  float steerFrictionStock;
  float steerKPStock;
  float steerLatAccelStock;
  float steerRatioStock;

signals:
  void closeParentToggle();
  void closeSubParentToggle();
  void closeSubSubParentToggle();
  void openPanel();
  void openParentToggle();
  void openSubParentToggle();
  void openSubSubParentToggle();
  void updateCarToggles();
  void updateMetric();

private:
  FrogPilotButtonsControl *drivingButton;

  Params params;

  QStackedLayout *mainLayout;

  QWidget *frogpilotSettingsWidget;

  void addPanelControl(FrogPilotListWidget *list, const QString &title, const QString &desc, const std::vector<QString> &button_labels, const QString &icon, const std::vector<QWidget*> &panels, const bool isDrivingPanel = false);
  void closePanel();
  void showEvent(QShowEvent *event) override;
  void updateCarVariables();
  void updatePanelVisibility();
};
