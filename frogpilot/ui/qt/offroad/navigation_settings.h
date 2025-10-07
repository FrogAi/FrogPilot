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
  bool updatingLimits;

  ButtonControl *publicMapboxKeyControl;
  ButtonControl *secretMapboxKeyControl;
  ButtonControl *setupButton;

  FrogPilotButtonControl *updateSpeedLimitsToggle;

  FrogPilotSettingsWindow *parent;

  LabelControl *ipLabel;

  Params params;
  Params params_cache{"", true};
  Params params_memory{"", false, true};

  QLabel *imageLabel;

  QStackedLayout *primelessLayout;
};
