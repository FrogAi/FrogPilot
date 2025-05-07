#pragma once

#include <set>

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotDevicePanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotDevicePanel(FrogPilotSettingsWindow *parent);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateState(const UIState &s);
  void updateToggles();

  bool started;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  std::set<QString> deviceManagementKeys = {"DeviceShutdown", "IncreaseThermalLimits", "LowVoltageShutdown", "NoLogging", "NoUploads", "OfflineMode", "UseKonikServer"};
  std::set<QString> screenKeys = {"ScreenBrightness", "ScreenBrightnessOnroad", "ScreenRecorder", "ScreenTimeout", "ScreenTimeoutOnroad"};

  std::set<QString> parentKeys;

  FrogPilotSettingsWindow *parent;

  QJsonObject frogpilotToggleLevels;
};
