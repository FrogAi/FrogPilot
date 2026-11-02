#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotDataPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotDataPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

signals:
  void openSubPanel();

private:
  void showEvent(QShowEvent *event) override;
  void updateState(const UIState &s, const FrogPilotUIState &fs);
  void updateStatsLabels(FrogPilotListWidget *labelsList);

  Params params;

  FrogPilotButtonsControl *accountButton;
};
