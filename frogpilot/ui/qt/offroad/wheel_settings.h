#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotWheelPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotWheelPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateToggles();
  void updateButtonValues();

  bool forceOpenDescriptions;

  std::map<QString, AbstractControl*> toggles;

  FrogPilotSettingsWindow *parent;

  Params params;

  QMap<int, QString> buttonFunctions();
};
