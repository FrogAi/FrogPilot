#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotVisualsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVisualsPanel(FrogPilotSettingsWindow *parent);

signals:
  void openParentToggle();
  void openSubParentToggle();

private:
  void hideSubToggles();
  void hideToggles();
  void showEvent(QShowEvent *event) override;
  void showToggles(const std::set<QString> &keys);
  void updateMetric(bool metric, bool bootRun);

  bool developerUIOpen;
  bool hasAutoTune;
  bool hasBSM;
  bool hasOpenpilotLongitudinal;
  bool hasRadar;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  std::set<QString> accessibilityKeys = {"CameraView", "DriverCamera", "OnroadDistanceButton", "StandbyMode", "StoppedTimer"};
  std::set<QString> advancedCustomOnroadUIKeys = {"HideAlerts", "HideLeadMarker", "HideMapIcon", "HideMaxSpeed", "HideSpeed", "HideSpeedLimit", "WheelSpeed"};
  std::set<QString> customOnroadUIKeys = {"AccelerationPath", "AdjacentPath", "BlindSpotPath", "Compass", "PedalsOnUI", "RotatingWheel"};
  std::set<QString> developerMetricKeys = {"BorderMetrics", "FPSCounter", "LateralMetrics", "LongitudinalMetrics", "NumericalTemp", "SidebarMetrics", "UseSI"};
  std::set<QString> developerUIKeys = {"DeveloperMetrics", "DeveloperWidgets"};
  std::set<QString> developerWidgetKeys = {"AdjacentLeadsUI", "RadarTracksUI", "ShowStoppingPoint"};
  std::set<QString> modelUIKeys = {"DynamicPathWidth", "LaneLinesWidth", "PathEdgeWidth", "PathWidth", "RoadEdgesWidth", "UnlimitedLength"};
  std::set<QString> navigationUIKeys = {"BigMap", "MapStyle", "RoadNameUI", "ShowSpeedLimits", "UseVienna"};

  FrogPilotButtonToggleControl *borderMetricsBtn;
  FrogPilotButtonToggleControl *lateralMetricsBtn;

  FrogPilotSettingsWindow *parent;

  Params params;

  QJsonObject frogpilotToggleLevels;
};
