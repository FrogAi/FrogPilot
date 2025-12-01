#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotModelPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotModelPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateToggles();

  bool forceOpenDescriptions = false;

  std::map<QString, AbstractControl*> toggles;

  FrogPilotSettingsWindow *parent;
};
