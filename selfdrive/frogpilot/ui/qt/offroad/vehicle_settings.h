#pragma once

#include <set>

#include <QStringList>

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

  ButtonControl *selectMakeButton;
  ButtonControl *selectModelButton;

  ToggleControl *disableOpenpilotLong;

  QString carMake;
  QString carModel;

  QStringList models;

  std::set<QString> gmKeys = {"GasRegenCmd", "LongPitch"};
  std::set<QString> subaruKeys = {"CrosstrekTorque"};
  std::set<QString> toyotaKeys = {"ClusterOffset", "ToyotaTune", "SNGHack", "ToyotaDoors"};

  std::map<std::string, AbstractControl*> toggles;

  Params params;

  bool hasExperimentalOpenpilotLongitudinal;
  bool hasOpenpilotLongitudinal;
  bool hasSNG;
  bool isGMTruck;
  bool isImpreza;
  bool started;
};
