#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotVehiclesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVehiclesPanel(FrogPilotSettingsWindow *parent);

private:
  FrogPilotSettingsWindow *parent;

  ButtonControl *selectMakeButton;
  ButtonControl *selectModelButton;

  ToggleControl *disableOpenpilotLong;

  std::set<QString> gmKeys = {
    "ExperimentalGMTune", "LongPitch", "NewLongAPIGM", "VoltSNG"
  };

  std::set<QString> hyundaiKeys = {
    "NewLongAPI"
  };

  std::set<QString> imprezaKeys = {
    "CrosstrekTorque"
  };

  std::set<QString> longitudinalKeys = {
    "ExperimentalGMTune", "LongPitch", "NewLongAPI", "NewLongAPIGM",
    "NewToyotaTune", "SNGHack", "VoltSNG"
  };

  std::set<QString> sngKeys = {
    "SNGHack"
  };

  std::set<QString> subaruKeys = {
    "CrosstrekTorque"
  };

  std::set<QString> toyotaKeys = {
    "ClusterOffset", "NewToyotaTune", "SNGHack", "ToyotaDoors"
  };

  std::set<QString> voltKeys = {
    "VoltSNG"
  };

  std::map<QString, AbstractControl*> toggles;

  QString carMake;
  QString carModel;

  QStringList models;

  QMap<QString, QString> carModels;

  Params params;

  bool disableOpenpilotLongitudinal;
  bool hasExperimentalOpenpilotLongitudinal;
  bool hasOpenpilotLongitudinal;
  bool hasSNG;
  bool isGMPCMCruise;
  bool isImpreza;
  bool isVolt;
  bool started;

  void setModels();
  void updateCarToggles();
  void updateState(const UIState &s);
  void updateToggles();
};
