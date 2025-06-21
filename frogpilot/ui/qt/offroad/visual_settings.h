#pragma once

#include <set>

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotVisualsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVisualsPanel(FrogPilotSettingsWindow *parent);

signals:
  void openSubPanel();
  void openSubSubPanel();

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
  std::set<QString> developerMetricKeys = {"AdjacentPathMetrics", "BorderMetrics", "FPSCounter", "LeadInfo", "NumericalTemp", "SidebarMetrics", "UseSI"};
  std::set<QString> developerSidebarKeys = {"DeveloperSidebarMetric1", "DeveloperSidebarMetric2", "DeveloperSidebarMetric3", "DeveloperSidebarMetric4", "DeveloperSidebarMetric5", "DeveloperSidebarMetric6", "DeveloperSidebarMetric7"};
  std::set<QString> developerUIKeys = {"DeveloperMetrics", "DeveloperSidebar", "DeveloperWidgets"};
  std::set<QString> developerWidgetKeys = {"AdjacentLeadsUI", "RadarTracksUI", "ShowStoppingPoint"};
  std::set<QString> modelUIKeys = {"DynamicPathWidth", "LaneLinesWidth", "PathEdgeWidth", "PathWidth", "RoadEdgesWidth", "UnlimitedLength"};
  std::set<QString> navigationUIKeys = {"BigMap", "MapStyle", "RoadNameUI", "ShowSpeedLimits", "SLCMapboxFiller", "UseVienna"};
  std::set<QString> qualityOfLifeKeys = {"CameraView", "DriverCamera", "StandbyMode", "StoppedTimer"};

  std::set<QString> parentKeys;

  FrogPilotButtonToggleControl *borderMetricsBtn;

  FrogPilotSettingsWindow *parent;

  Params params;
  Params params_cache{"/cache/params"};

  QJsonObject frogpilotToggleLevels;
};
