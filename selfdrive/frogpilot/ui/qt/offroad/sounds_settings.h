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
  void hideToggles();
  void showEvent(QShowEvent *event) override;
  void showToggles(const std::set<QString> &keys);
  void updateCarToggles();

  std::set<QString> alertVolumeControlKeys = {
    "DisengageVolume", "EngageVolume", "PromptDistractedVolume",
    "PromptVolume", "RefuseVolume", "WarningImmediateVolume",
    "WarningSoftVolume"
  };

  std::set<QString> customAlertsKeys = {
    "GoatScream", "GreenLightAlert", "LeadDepartingAlert",
    "LoudBlindspotAlert", "SpeedLimitChangedAlert"
  };

  FrogPilotSettingsWindow *parent;

  Params params;

  bool hasBSM;
  bool hasOpenpilotLongitudinal;

  int customizationLevel;

  std::map<QString, AbstractControl*> toggles;
};
