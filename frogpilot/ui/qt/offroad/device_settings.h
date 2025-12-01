#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotDevicePanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotDevicePanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateToggles();

  bool developmentBranch;
  bool forceOpenDescriptions;
  bool vettingBranch;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> deviceManagementKeys = {"DeviceShutdown", "HigherBitrate", "IncreaseThermalLimits", "LowVoltageShutdown", "NoLogging", "NoUploads", "UseKonikServer"};
  QSet<QString> screenKeys = {"InstantReplay", "ScreenBrightness", "ScreenBrightnessOnroad", "ScreenRecorder", "ScreenTimeout", "ScreenTimeoutOnroad", "StandbyMode"};

  QSet<QString> parentKeys;

  FrogPilotSettingsWindow *parent;
};
