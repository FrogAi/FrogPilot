#pragma once

#include <set>

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotSoundsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotSoundsPanel(FrogPilotSettingsWindow *parent);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateState(const UIState &s);
  void updateToggles();

  bool hasBSM;
  bool hasOpenpilotLongitudinal;
  bool started;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  std::set<QString> alertVolumeControlKeys = {"DisengageVolume", "EngageVolume", "PromptDistractedVolume", "PromptVolume", "RefuseVolume", "WarningImmediateVolume", "WarningSoftVolume"};
  std::set<QString> customAlertsKeys = {"GoatScream", "GreenLightAlert", "LeadDepartingAlert", "LoudBlindspotAlert", "SpeedLimitChangedAlert"};

  std::set<QString> parentKeys;

  FrogPilotSettingsWindow *parent;

  QJsonObject frogpilotToggleLevels;

  Params params;
  Params params_memory{"/dev/shm/params"};
};
