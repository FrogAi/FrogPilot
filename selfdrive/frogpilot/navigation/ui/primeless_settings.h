#pragma once

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotPrimelessPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotPrimelessPanel(FrogPilotSettingsWindow *parent);

signals:
  void closeMapBoxInstructions();
  void openMapBoxInstructions();

protected:
  void hideEvent(QHideEvent *event);
  void showEvent(QShowEvent *event) override;

private:
  void createMapboxKeyControl(ButtonControl *&control, const QString &label, const std::string &paramKey, const QString &prefix, FrogPilotListWidget *list);
  void mousePressEvent(QMouseEvent *event);
  void updateButtons();
  void updateState(const UIState &s);
  void updateStep();

  bool mapboxPublicKeySet;
  bool mapboxSecretKeySet;
  bool setupCompleted;

  ButtonControl *amapKeyControl1;
  ButtonControl *amapKeyControl2;
  ButtonControl *googleKeyControl;
  ButtonControl *publicMapboxKeyControl;
  ButtonControl *secretMapboxKeyControl;

  FrogPilotSettingsWindow *parent;

  LabelControl *ipLabel;

  Params params;
  Params params_cache{"/cache/params"};

  QLabel *imageLabel;

  QStackedLayout *primelessLayout;
};
