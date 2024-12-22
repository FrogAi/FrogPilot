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
  void updateState(const UIState &s);
  void updateToggles();

  ButtonControl *selectMakeButton;
  ButtonControl *selectModelButton;

  FrogPilotSettingsWindow *parent;

  QJsonObject frogpilotToggleLevels;

  QString carMake;
  QString carModel;

  QStringList carModels;

  ParamControl *forceFingerprint;

  Params params;
  Params params_default{"/data/params_default"};

  ToggleControl *disableOpenpilotLong;

  bool disableOpenpilotLongitudinal = params.getBool("DisableOpenpilotLongitudinal");
  bool hasExperimentalOpenpilotLongitudinal;
  bool hasOpenpilotLongitudinal;
  bool hasSNG;
  bool isBolt;
  bool isGM;
  bool isHKG;
  bool isImpreza;
  bool isSubaru;
  bool isToyota;
  bool isVolt;
  bool started;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  std::set<QString> gmKeys = {"ExperimentalGMTune", "LongPitch", "NewLongAPIGM", "VoltSNG"};
  std::set<QString> hyundaiKeys = {"NewLongAPI"};
  std::set<QString> imprezaKeys = {"CrosstrekTorque"};
  std::set<QString> longitudinalKeys = {"ExperimentalGMTune", "FrogsGoMoosTweak", "LongPitch", "NewLongAPI", "NewLongAPIGM", "SNGHack", "VoltSNG"};
  std::set<QString> sngKeys = {"SNGHack"};
  std::set<QString> subaruKeys = {"CrosstrekTorque"};
  std::set<QString> toyotaKeys = {"ClusterOffset", "FrogsGoMoosTweak", "SNGHack", "ToyotaDoors"};
  std::set<QString> voltKeys = {"VoltSNG"};
};
