#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/frogpilot_ui_functions.h"
#include "selfdrive/ui/qt/offroad/settings.h"
#include "selfdrive/ui/ui.h"

class FrogPilotVisualsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVisualsPanel(SettingsWindow *parent);

signals:
  void closeParentToggle();
  void closeSubParentToggle();
  void openParentToggle();
  void openSubParentToggle();

private:
  void hideEvent(QHideEvent *event);
  void hideSubToggles();
  void hideSubSubToggles();
  void parentToggleClicked();
  void subParentToggleClicked();
  void updateMetric();
  void updateState(const UIState &s);
  void updateToggles();

  std::set<QString> alertVolumeControlKeys = {"EngageVolume", "DisengageVolume", "RefuseVolume", "PromptVolume", "PromptDistractedVolume", "WarningSoftVolume", "WarningImmediateVolume"};
  std::set<QString> customAlertsKeys = {"GreenLightAlert"};
  std::set<QString> customOnroadUIKeys = {"AccelerationPath", "AdjacentPath", "BlindSpotPath", "FPSCounter", "LeadInfo"};
  std::set<QString> customThemeKeys = {"CustomColors", "CustomIcons", "CustomSignals", "CustomSounds"};
  std::set<QString> modelUIKeys = {"DynamicPathWidth", "LaneLinesWidth", "PathEdgeWidth", "PathWidth", "RoadEdgesWidth", "UnlimitedLength"};
  std::set<QString> qolKeys = {"DriveStats", "FullMap", "HideSpeed"};

  std::map<std::string, ParamControl*> toggles;

  Params params;
  Params paramsMemory{"/dev/shm/params"};

  bool isMetric = params.getBool("IsMetric");
  bool started = false;
};
