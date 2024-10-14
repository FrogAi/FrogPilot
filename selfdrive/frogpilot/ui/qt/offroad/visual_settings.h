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
  void hideToggles();
  void showToggles(const std::set<QString> &keys);
  void updateCarToggles();

  std::set<QString> customOnroadUIKeys = {
    "Compass", "CustomPaths", "DynamicPathWidth",
    "PedalsOnUI", "RoadNameUI", "RotatingWheel"
  };

  std::set<QString> qolKeys = {
    "BigMap", "DriverCamera", "MapStyle",
    "StandbyMode", "StoppedTimer"
  };

  FrogPilotSettingsWindow *parent;

  FrogPilotButtonToggleControl *customPathsBtn;

  Params params;

  bool hasBSM;

  std::map<QString, AbstractControl*> toggles;
};
