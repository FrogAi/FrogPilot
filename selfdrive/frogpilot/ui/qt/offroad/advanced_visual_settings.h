#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotAdvancedVisualsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotAdvancedVisualsPanel(FrogPilotSettingsWindow *parent);

signals:
  void openParentToggle();

private:
  void hideToggles();
  void showToggles(const std::set<QString> &keys);
  void updateCarToggles();
  void updateMetric();

  std::set<QString> advancedCustomOnroadUIKeys = {
    "CameraView", "HideLeadMarker", "HideSpeed",
    "HideUIElements", "ShowStoppingPoint", "WheelSpeed"
  };

  std::set<QString> developerUIKeys = {
    "BorderMetrics", "FPSCounter", "LateralMetrics",
    "LongitudinalMetrics", "NumericalTemp",
    "SidebarMetrics", "UseSI"
  };

  std::set<QString> modelUIKeys = {
    "LaneLinesWidth", "PathEdgeWidth", "PathWidth",
    "RoadEdgesWidth", "UnlimitedLength"
  };

  FrogPilotButtonToggleControl *borderMetricsBtn;
  FrogPilotButtonToggleControl *lateralMetricsBtn;

  FrogPilotSettingsWindow *parent;

  Params params;

  bool disableOpenpilotLongitudinal;
  bool hasAutoTune;
  bool hasBSM;
  bool hasOpenpilotLongitudinal;
  bool isMetric = params.getBool("IsMetric");

  std::map<QString, AbstractControl*> toggles;
};
