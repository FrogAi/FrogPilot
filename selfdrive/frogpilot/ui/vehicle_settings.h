#pragma once

#include <set>

#include <QStringList>

#include "selfdrive/frogpilot/ui/frogpilot_ui_functions.h"
#include "selfdrive/ui/qt/offroad/settings.h"
#include "selfdrive/ui/ui.h"

class FrogPilotVehiclesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVehiclesPanel(SettingsWindow *parent);

private:
  void setModels();
  void setToggles();
  void updateState(const UIState &s);
  void updateToggles();

  ButtonControl *selectMakeButton;
  ButtonControl *selectModelButton;

  QString carMake;
  QStringList models;

  std::map<std::string, ParamControl*> toggles;

  std::set<QString> gmKeys = {"GasRegenCmd", "LongPitch", "LowerVolt"};
  std::set<QString> subaruKeys = {"CrosstrekTorque"};
  std::set<QString> toyotaKeys = {"LockDoors", "LongitudinalTune"};

  Params params;
  Params paramsMemory{"/dev/shm/params"};

  bool started = false;
};
