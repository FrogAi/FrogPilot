#pragma once

#include "cereal/messaging/messaging.h"
#include "selfdrive/ui/qt/network/wifi_manager.h"

#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

struct FrogPilotUIScene {
  bool always_on_lateral_active = false;
  bool downloading_update = false;
  bool frogpilot_panel_active = false;
  bool online = false;
  bool parked = false;
  bool reverse = false;
  bool standstill = false;

  int conditional_status = 0;

  QJsonObject frogpilot_toggles;
};

class FrogPilotUIState : public QObject {
  Q_OBJECT

public:
  explicit FrogPilotUIState(QObject *parent = nullptr);

  void createSettingsBackup(const QString &name);
  void experimentalModePressed();
  void restoreSettingsBackup(const QString &name);
  void runUpdateChecks();
  void testAlert(const QString &alert, float volume);
  void update();
  void updateToggles();

  std::unique_ptr<SubMaster> sm;

  FrogPilotUIScene frogpilot_scene = {};

  uint64_t settings_backup_request_time = 0;

  WifiManager *wifi;

signals:
  void cameraFrameReceived();
  void togglesUpdated();

private:
  std::unique_ptr<PubMaster> pm;
};

FrogPilotUIState *frogpilotUIState();
