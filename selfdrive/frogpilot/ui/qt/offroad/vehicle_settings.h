#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotVehiclesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVehiclesPanel(FrogPilotSettingsWindow *parent);

private:
  void setModels();
  void showEvent(QShowEvent *event) override;
  void updateCarToggles();
  void updateState(const UIState &s);
  void updateToggles();

  std::set<QString> gmKeys = {
    "ExperimentalGMTune", "LongPitch", "NewLongAPIGM",
    "VoltSNG"
  };

  std::set<QString> hyundaiKeys = {
    "NewLongAPI"
  };

  std::set<QString> imprezaKeys = {
    "CrosstrekTorque"
  };

  std::set<QString> longitudinalKeys = {
    "ExperimentalGMTune", "LongPitch", "NewLongAPI",
    "NewLongAPIGM", "SNGHack", "VoltSNG"
  };

  std::set<QString> sngKeys = {
    "SNGHack"
  };

  std::set<QString> subaruKeys = {
    "CrosstrekTorque"
  };

  std::set<QString> toyotaKeys = {
    "ClusterOffset", "FrogsGoMoosTweak", "SNGHack",
    "ToyotaDoors"
  };

  std::set<QString> voltKeys = {
    "VoltSNG"
  };

  ButtonControl *selectMakeButton;
  ButtonControl *selectModelButton;

  FrogPilotSettingsWindow *parent;

  QMap<QString, QString> carModels;

  QString carMake;
  QString carModel;

  QStringList models;

  ParamControl *forceFingerprint;

  Params params;

  ToggleControl *disableOpenpilotLong;

  bool disableOpenpilotLongitudinal;
  bool hasExperimentalOpenpilotLongitudinal;
  bool hasOpenpilotLongitudinal;
  bool hasSNG;
  bool isBolt;
  bool isGMPCMCruise;
  bool isImpreza;
  bool isVolt;
  bool started;

  int customizationLevel;

  std::map<QString, AbstractControl*> toggles;
};
