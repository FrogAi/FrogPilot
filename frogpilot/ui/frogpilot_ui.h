#pragma once

#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

struct FrogPilotUIScene {
  bool downloading_update;
  bool frogpilot_panel_active;
  bool online;
  bool parked;
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
};

FrogPilotUIState *frogpilotUIState();
