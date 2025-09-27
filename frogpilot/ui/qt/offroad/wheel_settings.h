#pragma once

#include <set>

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotWheelPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotWheelPanel(FrogPilotSettingsWindow *parent);

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateToggles();

  bool hasOpenpilotLongitudinal;
  bool isSubaru;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  FrogPilotSettingsWindow *parent;

  Params params;

  QJsonObject frogpilotToggleLevels;
};
