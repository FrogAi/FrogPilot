#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotDataPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotDataPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

signals:
  void openSubPanel();

private:
  void updateStatsLabels(FrogPilotListWidget *labelsList);

  bool forceOpenDescriptions;
  bool isMetric = false;

  FrogPilotSettingsWindow *parent;

  Params params;
};
