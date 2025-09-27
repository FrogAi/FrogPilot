#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotNavigationPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotNavigationPanel(FrogPilotSettingsWindow *parent);

signals:
  void closeSubPanel();
  void openSubPanel();

protected:
  void hideEvent(QHideEvent *event);
  void showEvent(QShowEvent *event) override;

private:
  void createKeyControl(ButtonControl *&control, const QString &label, const std::string &paramKey, const QString &prefix, const int &minLength, FrogPilotListWidget *list);
  void mousePressEvent(QMouseEvent *event);
  void updateButtons();
  void updateState(const UIState &s, const FrogPilotUIState &fs);
  void updateStep();

  bool forceOpenDescriptions;
  bool mapboxPublicKeySet;
  bool mapboxSecretKeySet;
  bool setupCompleted;
  bool updatingLimits;

  ButtonControl *amapKeyControl1;
  ButtonControl *amapKeyControl2;
  ButtonControl *publicMapboxKeyControl;
  ButtonControl *secretMapboxKeyControl;
  ButtonControl *setupButton;

  FrogPilotButtonControl *updateSpeedLimitsToggle;

  FrogPilotButtonsControl *searchInput;

  FrogPilotSettingsWindow *parent;

  LabelControl *ipLabel;

  Params params;
  Params params_cache{"/cache/params"};
  Params params_memory{"/dev/shm/params"};

  QLabel *imageLabel;

  QStackedLayout *primelessLayout;
};
