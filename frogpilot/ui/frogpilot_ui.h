#pragma once

#include <memory>

#include "cereal/messaging/messaging.h"
#include "selfdrive/ui/qt/network/wifi_manager.h"

#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

struct FrogPilotUIScene {
  bool always_on_lateral_active = false;
  bool downloading_update = false;
  bool driver_camera_active = false;
  bool enabled = false;
  bool frogpilot_panel_active = false;
  bool map_open = false;
  bool online = false;
  bool parked = false;
  bool reverse = false;
  bool standstill = false;
  bool traffic_mode_enabled = false;
  bool wake_up_screen = false;

  float lane_width_left = 0;
  float lane_width_right = 0;

  int conditional_status = 0;
  int model_length = 0;

  std::vector<QPointF> live_radar_tracks;

  QJsonObject frogpilot_toggles;

  QPointF lead_vertices[2];

  QPolygonF track_adjacent_vertices[2];
  QPolygonF track_edge_vertices;
};

class FrogPilotUIState : public QObject {
  Q_OBJECT

public:
  explicit FrogPilotUIState(QObject *parent = nullptr);

  void cancelModelDownload();
  void cancelThemeDownload();
  void downloadAllModels();
  void downloadModels(const QStringList &models);
  void downloadTheme(const QString &component, const QStringList &themes);
  void experimentalModePressed();
  void flashPanda();
  void reportIssue(const QString &report);
  void requestAccountLink();
  void requestAccountUrl(const QString &purpose);
  void runUpdateChecks();
  void screenRecorderEvent(cereal::FrogPilotCarEvent::EventName event);
  void setDistanceButtonPressed(bool pressed);
  void setTethering(int mode);
  void speedLimitAccepted();
  void testAlert(const QString &alert, float volume);
  void unlinkAccount();
  void update();
  void updateToggles();

  std::unique_ptr<SubMaster> sm;

  FrogPilotUIScene frogpilot_scene = {};

  Params params_memory{"/dev/shm/params"};

  QJsonObject &frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  uint64_t account_link_request_time = 0;
  uint64_t account_url_request_time = 0;
  uint64_t download_model_request_time = 0;
  uint64_t download_theme_request_time = 0;
  uint64_t flash_panda_request_time = 0;
  uint64_t issue_report_request_time = 0;

  WifiManager *wifi = nullptr;

signals:
  void cameraFrameReceived();
  void statsSaved();
  void themeUpdated();
  void togglesUpdated();

private:
  bool distance_button_pressed = false;

  int driver_camera_timer = 0;
  int tethering_mode = 0;

  std::unique_ptr<PubMaster> pm;
};

FrogPilotUIState *frogpilotUIState();

bool isOpenpilotSteering();

QString formatShortTime(const QTime &time);
