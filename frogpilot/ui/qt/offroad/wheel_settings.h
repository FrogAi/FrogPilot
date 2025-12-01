#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotWheelPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotWheelPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

protected:
  void showEvent(QShowEvent *event) override;

private:
  QMap<int, QString> buttonFunctions();

  void updateButtonValues();
  void updateToggles();

  bool forceOpenDescriptions = false;

  std::map<QString, ButtonControl*> toggles;

  FrogPilotSettingsWindow *parent;

  Params params;
};
