#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotDataPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotDataPanel(FrogPilotSettingsWindow *parent);

signals:
  void openSubPanel();

private:
  void updateStatsLabels(FrogPilotListWidget *labelsList);

  bool isMetric;

  FrogPilotSettingsWindow *parent;

  Params params;
  Params params_cache{"/cache/params"};
};
