#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotUtilitiesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotUtilitiesPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

private:
  void submitReport(QJsonObject report);
  void updateState(const UIState &s, const FrogPilotUIState &fs);

  bool actionRunning = false;

  ButtonControl *reportIssueButton;

  Params params;
};
