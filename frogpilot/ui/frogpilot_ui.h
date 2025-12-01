#pragma once

#include "cereal/messaging/messaging.h"
#include "selfdrive/ui/qt/network/wifi_manager.h"

#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

struct FrogPilotUIScene {
  bool always_on_lateral_active;
  bool downloading_update;
  bool frogpilot_panel_active;
  bool online;
  bool parked;
  bool reverse;
  bool standstill;
  bool traffic_mode_enabled;

  int conditional_status;

  QJsonObject frogpilot_toggles;
};

class FrogPilotUIState : public QObject {
  Q_OBJECT

public:
  explicit FrogPilotUIState(QObject *parent = nullptr);

  void cancelMapsDownload();
  void cancelModelDownload();
  void cancelThemeDownload();
  void downloadAllModels();
  void downloadMaps();
  void downloadModels(const QStringList &models);
  void downloadTheme(const QString &component, const QStringList &themes);
  void experimentalModePressed();
  void flashPanda();
  void reportIssue(const QString &report);
  void runUpdateChecks();
  void screenRecorderEvent(cereal::FrogPilotOnroadEvent::EventName event);
  void setDistanceButtonPressed(bool pressed);
  void speedLimitAccepted();
  void testAlert(const QString &alert);
  void update();
  void updateToggles();

  std::unique_ptr<SubMaster> sm;

  FrogPilotUIScene frogpilot_scene = {};

  uint64_t download_maps_request_time = 0;
  uint64_t download_model_request_time = 0;
  uint64_t download_theme_request_time = 0;
  uint64_t flash_panda_request_time = 0;
  uint64_t issue_report_request_time = 0;

  WifiManager *wifi;

signals:
  void cameraFrameReceived();
  void statsSaved();
  void themeUpdated();
  void togglesUpdated();

private:
  bool distance_button_pressed = false;

  std::unique_ptr<PubMaster> pm;
};

FrogPilotUIState *frogpilotUIState();
