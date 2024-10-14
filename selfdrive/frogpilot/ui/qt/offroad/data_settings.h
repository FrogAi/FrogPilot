#pragma once

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotDataPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotDataPanel(FrogPilotSettingsWindow *parent);

private:
  Params params;
};
