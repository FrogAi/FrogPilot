#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotVisualsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVisualsPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

signals:
  void openSubPanel();
  void openSubSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateMetric(bool metric, bool bootRun);
  void updateToggles();

  bool forceOpenDescriptions;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> advancedCustomOnroadUIKeys = {"HideAlerts", "HideLeadMarker", "HideMaxSpeed", "HideSpeed", "HideSpeedLimit", "WheelSpeed"};
  QSet<QString> customOnroadUIKeys = {"AccelerationPath", "AdjacentPath", "BlindSpotPath", "Compass", "OnroadDistanceButton", "PedalsOnUI", "RotatingWheel"};
  QSet<QString> modelUIKeys = {"DynamicPathWidth", "LaneLinesWidth", "PathEdgeWidth", "PathWidth", "RoadEdgesWidth"};
  QSet<QString> navigationUIKeys = {"RoadNameUI", "ShowSpeedLimits", "SLCMapboxFiller", "UseVienna", "VisionSpeedLimitDetection"};
  QSet<QString> qualityOfLifeKeys = {"CameraView", "DriverCamera", "StoppedTimer"};

  QSet<QString> parentKeys;

  FrogPilotSettingsWindow *parent;

  Params params;
};
