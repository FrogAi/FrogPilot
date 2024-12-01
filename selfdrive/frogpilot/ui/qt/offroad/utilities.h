#pragma once

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class UtilitiesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit UtilitiesPanel(FrogPilotSettingsWindow *parent);

private:
  FrogPilotButtonsControl *forceStartedBtn;

  Params params;
  Params params_memory{"/dev/shm/params"};
};
