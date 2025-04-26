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

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateMetric(bool metric, bool bootRun);
  void updateToggles();

  bool developerUIOpen;
  bool hasAutoTune;
  bool hasBSM;
  bool hasOpenpilotLongitudinal;
  bool hasRadar;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  std::set<QString> advancedCustomOnroadUIKeys = {"HideAlerts", "HideLeadMarker", "HideMapIcon", "HideMaxSpeed", "HideSpeed", "HideSpeedLimit", "WheelSpeed"};
  std::set<QString> customOnroadUIKeys = {"AccelerationPath", "AdjacentPath", "BlindSpotPath", "Compass", "OnroadDistanceButton", "PedalsOnUI", "RotatingWheel"};
  std::set<QString> developerMetricKeys = {"BorderMetrics", "FPSCounter", "LateralMetrics", "LongitudinalMetrics", "NumericalTemp", "SidebarMetrics", "UseSI"};
  std::set<QString> developerUIKeys = {"DeveloperMetrics", "DeveloperWidgets"};
  std::set<QString> developerWidgetKeys = {"AdjacentLeadsUI", "RadarTracksUI", "ShowStoppingPoint"};
  std::set<QString> modelUIKeys = {"DynamicPathWidth", "LaneLinesWidth", "PathEdgeWidth", "PathWidth", "RoadEdgesWidth", "UnlimitedLength"};
  std::set<QString> navigationUIKeys = {"BigMap", "MapStyle", "RoadNameUI", "ShowSpeedLimits", "UseVienna"};
  std::set<QString> qualityOfLifeKeys = {"CameraView", "DriverCamera", "StandbyMode", "StoppedTimer"};

  std::set<QString> parentKeys;

  FrogPilotButtonToggleControl *borderMetricsBtn;
  FrogPilotButtonToggleControl *lateralMetricsBtn;
  FrogPilotButtonToggleControl *longitudinalMetricsBtn;

  FrogPilotSettingsWindow *parent;

  Params params;

  QJsonObject frogpilotToggleLevels;
};
