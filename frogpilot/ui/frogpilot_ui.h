#pragma once

#include "cereal/messaging/messaging.h"
#include "selfdrive/ui/qt/network/wifi_manager.h"

#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

struct FrogPilotUIScene {
  bool always_on_lateral_active;
  bool downloading_update;
  bool enabled;
  bool frogpilot_panel_active;
  bool online;
  bool parked;
  bool reverse;
  bool sidebars_open;
  bool standstill;
  bool traffic_mode_enabled;
  bool wake_up_screen;

  int conditional_status;
  int driver_camera_timer;
  int started_timer;

  QJsonObject frogpilot_toggles;
};

class FrogPilotUIState : public QObject {
  Q_OBJECT

public:
  explicit FrogPilotUIState(QObject *parent = nullptr);

  void update();

  std::unique_ptr<SubMaster> sm;

  FrogPilotUIScene frogpilot_scene = {};

  Params params;
  Params params_memory{"", true};

  WifiManager *wifi;

signals:
  void themeUpdated();
};

FrogPilotUIState *frogpilotUIState();
