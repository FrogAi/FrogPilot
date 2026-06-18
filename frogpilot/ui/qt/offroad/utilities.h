#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotUtilitiesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotUtilitiesPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

private:
  bool forceOpenDescriptions;

  FrogPilotSettingsWindow *parent;

  Params params;
  Params params_memory{"", true};

  std::set<std::string> excluded_keys = {
    "AvailableModels", "AvailableModelNames", "FrogPilotStats",
    "GithubSshKeys", "GithubUsername", "MapBoxRequests",
    "ModelDrivesAndScores", "SpeedLimits",
    "SpeedLimitsFiltered", "UpdaterAvailableBranches",
  };
};
