#pragma once

#include <set>

#include <QStringList>

#include "selfdrive/frogpilot/ui/qt/widgets/frogpilot_controls.h"
#include "selfdrive/ui/qt/offroad/settings.h"
#include "selfdrive/ui/ui.h"

class FrogPilotVehiclesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVehiclesPanel(SettingsWindow *parent);

private:
  void hideToggles();
  void setModels();
  void updateCarToggles();
  void updateState(const UIState &s);
  void updateToggles();

  ButtonControl *selectMakeButton;
  ButtonControl *selectModelButton;

  ToggleControl *disableOpenpilotLong;

  QString carMake;
  QStringList models;

  std::set<QString> gmKeys = {"EVTable", "GasRegenCmd", "LongPitch"};
  std::set<QString> subaruKeys = {"CrosstrekTorque"};
  std::set<QString> toyotaKeys = {"LongitudinalTune", "SNGHack", "ToyotaDoors"};

  std::map<std::string, ParamControl*> toggles;

  Params params;
  Params paramsMemory{"/dev/shm/params"};

  bool hasExperimentalOpenpilotLongitudinal;
  bool hasOpenpilotLongitudinal;
  bool hasSNG;
  bool isEVCar;
  bool isGMTruck;
  bool isImpreza;
  bool started;
};
