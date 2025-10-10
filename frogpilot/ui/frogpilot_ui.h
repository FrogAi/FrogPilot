#pragma once

#include "selfdrive/ui/qt/network/wifi_manager.h"

#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

struct FrogPilotUIScene {
  bool always_on_lateral_active;
  bool downloading_update;
  bool frogpilot_panel_active;
  bool online;
  bool parked;
  bool reverse;
  bool use_stock_colors;

  float lane_width_left;
  float lane_width_right;

  int conditional_status;
  int driver_camera_timer;
  int model_length;

  QColor lane_lines_color;
  QColor lead_marker_color;
  QColor path_color;
  QColor path_edges_color;
  QColor sidebar_color1;
  QColor sidebar_color2;
  QColor sidebar_color3;
};

class FrogPilotUIState : public QObject {
  Q_OBJECT

public:
  explicit FrogPilotUIState(QObject *parent = nullptr);

  void update();

  std::unique_ptr<SubMaster> sm;

  FrogPilotUIScene frogpilot_scene;

  Params params_memory{"", false, true};

  QJsonObject &frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  WifiManager *wifi;
};

FrogPilotUIState *frogpilotUIState();

void update_theme(FrogPilotUIState *fs);
