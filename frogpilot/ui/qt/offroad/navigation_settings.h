#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotNavigationPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotNavigationPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

signals:
  void closeSubPanel();
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  bool forceOpenDescriptions = false;

  ButtonControl *setupButton;

  ParamControl *pairingToggle;
  ParamControl *speedLimitFillerToggle;

  FrogPilotButtonsControl *publicMapboxKeyControl;
  FrogPilotButtonsControl *secretMapboxKeyControl;

  FrogPilotSettingsWindow *parent;

  LabelControl *ipLabel;

  QStackedLayout *navigationLayout;
};
