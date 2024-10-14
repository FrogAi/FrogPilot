#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotVehiclesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVehiclesPanel(FrogPilotSettingsWindow *parent);

private:
  void setModels();
  void updateCarToggles();
  void updateState(const UIState &s);
  void updateToggles();

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
    "SNGHack", "VoltSNG"
  };

  std::set<QString> sngKeys = {
    "SNGHack"
  };

  std::set<QString> subaruKeys = {
    "CrosstrekTorque"
  };

  std::set<QString> toyotaKeys = {
    "ClusterOffset", "FrogsGoMoosTweak", "NewToyotaTune", "SNGHack",
    "ToyotaDoors"
  };

  std::set<QString> toyotaTuneKeys = {
    "NewToyotaTune"
  };

  std::set<QString> voltKeys = {
    "VoltSNG"
  };

  ButtonControl *selectMakeButton;
  ButtonControl *selectModelButton;

  FrogPilotSettingsWindow *parent;

  QString carMake;
  QString carModel;

  QStringList models;

  QMap<QString, QString> carModels;

  Params params;

  ToggleControl *disableOpenpilotLong;

  bool disableOpenpilotLongitudinal;
  bool hasExperimentalOpenpilotLongitudinal;
  bool hasOpenpilotLongitudinal;
  bool hasSNG;
  bool isGMPCMCruise;
  bool isImpreza;
  bool isToyotaTuneSupported;
  bool isVolt;
  bool started;

  std::map<QString, AbstractControl*> toggles;
};
