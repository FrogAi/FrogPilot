#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotVisualsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVisualsPanel(FrogPilotSettingsWindow *parent);

signals:
  void openParentToggle();

private:
  FrogPilotSettingsWindow *parent;

  FrogPilotButtonToggleControl *customPathsBtn;

  std::set<QString> customOnroadUIKeys = {
    "Compass", "CustomPaths", "DynamicPathWidth",
    "PedalsOnUI", "RoadNameUI", "RotatingWheel"
  };

  std::set<QString> qolKeys = {
    "BigMap", "DriverCamera", "MapStyle",
    "StandbyMode", "StoppedTimer"
  };

  std::map<QString, AbstractControl*> toggles;

  Params params;

  bool hasBSM;

  void hideToggles();
  void showToggles(const std::set<QString> &keys);
  void updateCarToggles();
};
