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
  void showEvent(QShowEvent *event) override;
  void showToggles(const std::set<QString> &keys);
  void updateCarToggles();
  void updateMetric();

  std::set<QString> accessibilityKeys = {
    "CameraView", "DriverCamera", "OnroadDistanceButton",
    "StandbyMode", "StoppedTimer"
  };

  std::set<QString> advancedCustomOnroadUIKeys = {
    "HideAlerts", "HideLeadMarker", "HideMapIcon",
    "HideMaxSpeed", "HideSpeed", "HideSpeedLimit",
    "WheelSpeed"
  };

  std::set<QString> customOnroadUIKeys = {
    "AccelerationPath", "AdjacentPath", "BlindSpotPath",
    "Compass", "PedalsOnUI", "RotatingWheel"
  };

  std::set<QString> developerUIKeys = {
    "BorderMetrics", "FPSCounter", "LateralMetrics",
    "LongitudinalMetrics", "NumericalTemp",
    "SidebarMetrics", "UseSI"
  };

  std::set<QString> modelUIKeys = {
    "DynamicPathWidth", "LaneLinesWidth", "PathEdgeWidth",
    "PathWidth", "RoadEdgesWidth", "ShowStoppingPoint",
    "UnlimitedLength"
  };

  std::set<QString> navigationUIKeys = {
    "BigMap", "MapStyle", "RoadNameUI",
    "ShowSLCOffset", "UseVienna"
  };

  FrogPilotButtonToggleControl *borderMetricsBtn;
  FrogPilotButtonToggleControl *lateralMetricsBtn;
  FrogPilotButtonToggleControl *longitudinalMetricsBtn;

  FrogPilotSettingsWindow *parent;

  Params params;

  bool disableOpenpilotLongitudinal;
  bool hasAutoTune;
  bool hasBSM;
  bool hasOpenpilotLongitudinal;
  bool hasRadar;
  bool isMetric = params.getBool("IsMetric");

  int customizationLevel;

  std::map<QString, AbstractControl*> toggles;
};
