#pragma once

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotUtilitiesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotUtilitiesPanel(FrogPilotSettingsWindow *parent);

private:
  FrogPilotSettingsWindow *parent;

  Params params;
  Params params_memory{"/dev/shm/params"};
};
