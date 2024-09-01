#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotDevicePanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotDevicePanel(FrogPilotSettingsWindow *parent);

signals:
  void openParentToggle();

private:
  void hideToggles();
  void showToggles(const std::set<QString> &keys);
  void updateState(const UIState &s);

  std::set<QString> deviceManagementKeys = {
    "DeviceShutdown", "IncreaseThermalLimits", "LowVoltageShutdown",
    "NoLogging", "NoUploads", "OfflineMode"
  };

  std::set<QString> screenKeys = {
    "ScreenBrightness", "ScreenBrightnessOnroad", "ScreenRecorder",
    "ScreenTimeout", "ScreenTimeoutOnroad"
  };

  Params params;

  bool started;

  std::map<QString, AbstractControl*> toggles;
};
