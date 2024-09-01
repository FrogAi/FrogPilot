#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotSoundsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotSoundsPanel(FrogPilotSettingsWindow *parent);

signals:
  void openParentToggle();

private:
  FrogPilotSettingsWindow *parent;

  std::set<QString> alertVolumeControlKeys = {
    "DisengageVolume", "EngageVolume", "PromptDistractedVolume",
    "PromptVolume", "RefuseVolume", "WarningImmediateVolume",
    "WarningSoftVolume"
  };

  std::set<QString> customAlertsKeys = {
    "GoatScream", "GreenLightAlert", "LeadDepartingAlert",
    "LoudBlindspotAlert", "SpeedLimitChangedAlert"
  };

  std::map<QString, AbstractControl*> toggles;

  Params params;

  bool hasBSM;
  bool hasOpenpilotLongitudinal;

  void hideToggles();
  void showToggles(const std::set<QString> &keys);
  void updateCarToggles();
};
