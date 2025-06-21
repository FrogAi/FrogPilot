#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotNavigationPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotNavigationPanel(FrogPilotSettingsWindow *parent);

signals:
  void closeSubSubPanel();
  void openSubSubPanel();

protected:
  void hideEvent(QHideEvent *event);
  void showEvent(QShowEvent *event) override;

private:
  void createMapboxKeyControl(ButtonControl *&control, const QString &label, const std::string &paramKey, const QString &prefix, FrogPilotListWidget *list);
  void mousePressEvent(QMouseEvent *event);
  void updateButtons();
  void updateState(const UIState &s, const FrogPilotUIState &fs);
  void updateStep();

  bool mapboxPublicKeySet;
  bool mapboxSecretKeySet;
  bool setupCompleted;
  bool updatingLimits;

  ButtonControl *amapKeyControl1;
  ButtonControl *amapKeyControl2;
  ButtonControl *googleKeyControl;
  ButtonControl *publicMapboxKeyControl;
  ButtonControl *secretMapboxKeyControl;

  FrogPilotButtonControl *updateSpeedLimitsToggle;

  FrogPilotSettingsWindow *parent;

  LabelControl *ipLabel;

  Params params;
  Params params_cache{"/cache/params"};
  Params params_memory{"/dev/shm/params"};

  QLabel *imageLabel;

  QStackedLayout *primelessLayout;
};
