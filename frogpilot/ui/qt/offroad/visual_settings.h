#pragma once

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
  bool forceOpenDescriptions;
  bool hasAutoTune;
  bool hasBSM;
  bool hasOpenpilotLongitudinal;
  bool hasRadar;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> advancedCustomOnroadUIKeys = {"HideAlerts", "HideLeadMarker", "HideMapIcon", "HideMaxSpeed", "HideSpeed", "HideSpeedLimit", "WheelSpeed"};
  QSet<QString> customOnroadUIKeys = {"AccelerationPath", "AdjacentPath", "BlindSpotPath", "Compass", "OnroadDistanceButton", "PedalsOnUI", "RotatingWheel"};
  QSet<QString> developerMetricKeys = {"AdjacentPathMetrics", "BorderMetrics", "FPSCounter", "LeadInfo", "NumericalTemp", "SidebarMetrics", "UseSI"};
  QSet<QString> developerSidebarKeys = {"DeveloperSidebarMetric1", "DeveloperSidebarMetric2", "DeveloperSidebarMetric3", "DeveloperSidebarMetric4", "DeveloperSidebarMetric5", "DeveloperSidebarMetric6", "DeveloperSidebarMetric7"};
  QSet<QString> developerUIKeys = {"DeveloperMetrics", "DeveloperSidebar", "DeveloperWidgets"};
  QSet<QString> developerWidgetKeys = {"AdjacentLeadsUI", "RadarTracksUI", "ShowStoppingPoint"};
  QSet<QString> modelUIKeys = {"DynamicPathWidth", "LaneLinesWidth", "PathEdgeWidth", "PathWidth", "RoadEdgesWidth", "UnlimitedLength"};
  QSet<QString> navigationUIKeys = {"BigMap", "MapStyle", "RoadNameUI", "ShowSpeedLimits", "SLCMapboxFiller", "UseVienna"};
  QSet<QString> qualityOfLifeKeys = {"CameraView", "DriverCamera", "StoppedTimer"};

  QSet<QString> parentKeys;

  std::vector<QString> sidebarMetricsToggles;

  FrogPilotButtonsControl *sidebarMetricsToggle;

  FrogPilotButtonToggleControl *borderMetricsButton;

  FrogPilotSettingsWindow *parent;

  Params params;

  QJsonObject frogpilotToggleLevels;
};
