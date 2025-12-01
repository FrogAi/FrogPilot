#pragma once

#include "cereal/messaging/messaging.h"
#include "selfdrive/ui/qt/network/wifi_manager.h"

#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

struct FrogPilotUIScene {
  bool frogpilot_panel_active;
  bool online;
  bool parked;
  bool reverse;
  bool standstill;
};

class FrogPilotUIState : public QObject {
  Q_OBJECT

public:
  explicit FrogPilotUIState(QObject *parent = nullptr);

  void update();

  std::unique_ptr<SubMaster> sm;

  FrogPilotUIScene frogpilot_scene = {};

  uint64_t download_maps_request_time = 0;
  uint64_t download_theme_request_time = 0;

  WifiManager *wifi;

signals:
  void cameraFrameReceived();
};

FrogPilotUIState *frogpilotUIState();
