#pragma once

#include <iostream>
#include <memory>

#include <QObject>

#include "cereal/messaging/messaging.h"
#include "selfdrive/ui/qt/network/wifi_manager.h"

#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

struct RadarTrackData {
  QPointF calibrated_point;
};

struct FrogPilotUIScene {
  bool always_on_lateral_active;
  bool downloading_update;
  bool enabled;
  bool frogpilot_panel_active;
  bool map_open;
  bool online;
  bool parked;
  bool reverse;
  bool standstill;
  bool traffic_mode_enabled;
  bool use_stock_colors;
  bool wake_up_screen;

  float lane_width_left;
  float lane_width_right;

  int conditional_status;
  int driver_camera_timer;
  int model_length;
  int started_timer;

  std::vector<RadarTrackData> live_radar_tracks;

  QColor lane_lines_color;
  QColor lead_marker_color;
  QColor path_color;
  QColor path_edges_color;
  QColor sidebar_color1;
  QColor sidebar_color2;
  QColor sidebar_color3;

  QJsonObject frogpilot_toggles;

  QPolygonF track_adjacent_vertices[2];
  QPolygonF track_edge_vertices;
};

class FrogPilotUIState : public QObject {
  Q_OBJECT

public:
  explicit FrogPilotUIState(QObject *parent = nullptr);

  void update();

  std::unique_ptr<SubMaster> sm;

  FrogPilotUIScene frogpilot_scene;

  Params params_memory{"/dev/shm/params"};

  QJsonObject &frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  WifiManager *wifi;

signals:
  void reviewModel();
  void themeUpdated();
};

FrogPilotUIState *frogpilotUIState();

void update_theme(FrogPilotUIState *fs);
