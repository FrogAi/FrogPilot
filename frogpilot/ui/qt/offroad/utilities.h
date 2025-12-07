#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

extern const std::set<std::string> excluded_keys;

class FrogPilotUtilitiesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotUtilitiesPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

private:
  bool actionRunning = false;

  Params params;
};
