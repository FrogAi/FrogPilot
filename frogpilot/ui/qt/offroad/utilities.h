#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotUtilitiesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotUtilitiesPanel(FrogPilotSettingsWindow *parent);

protected:
  void showEvent(QShowEvent *event) override;

private:
  ButtonControl *pondButton;

  FrogPilotSettingsWindow *parent;

  Params params;
  Params params_memory{"/dev/shm/params"};

  QNetworkAccessManager *networkManager;

  QTimer *pairingPollTimer;
};
