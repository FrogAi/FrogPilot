#pragma once

#include "cereal/messaging/messaging.h"
#include "selfdrive/ui/qt/network/wifi_manager.h"

#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

struct FrogPilotUIScene {
  bool frogpilot_panel_active = false;
  bool online = false;
  bool parked = false;
  bool reverse = false;
  bool standstill = false;

  QJsonObject frogpilot_toggles;
};

class FrogPilotUIState : public QObject {
  Q_OBJECT

public:
  explicit FrogPilotUIState(QObject *parent = nullptr);

  void runUpdateChecks();
  void testAlert(const QString &alert, float volume);
  void update();
  void updateToggles();

  std::unique_ptr<SubMaster> sm;

  FrogPilotUIScene frogpilot_scene = {};

  WifiManager *wifi;

signals:
  void cameraFrameReceived();
  void togglesUpdated();

private:
  std::unique_ptr<PubMaster> pm;
};

FrogPilotUIState *frogpilotUIState();
