#pragma once

#include <set>

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotVehiclesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVehiclesPanel(FrogPilotSettingsWindow *parent);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateState(const UIState &s);
  void updateToggles();

  bool hasExperimentalOpenpilotLongitudinal;
  bool hasOpenpilotLongitudinal;
  bool hasSNG;
  bool isC3;
  bool isGM;
  bool isHKG;
  bool isToyota;
  bool isVolt;
  bool openpilotLongitudinalControlDisabled;
  bool started;
  bool tacoHacksAllowed;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  std::set<QString> gmKeys = {"ExperimentalGMTune", "LongPitch", "VoltSNG"};
  std::set<QString> hkgKeys = {"NewLongAPI", "TacoTuneHacks"};
  std::set<QString> longitudinalKeys = {"ExperimentalGMTune", "FrogsGoMoosTweak", "LongPitch", "NewLongAPI", "SNGHack", "VoltSNG"};
  std::set<QString> toyotaKeys = {"ClusterOffset", "FrogsGoMoosTweak", "LockDoorsTimer", "SNGHack", "ToyotaDoors"};

  std::set<QString> parentKeys;

  FrogPilotSettingsWindow *parent;

  QJsonObject frogpilotToggleLevels;

  QMap<QString, QString> carModels;

  ParamControl *disableOpenpilotLong;

  Params params;
  Params params_default{"/dev/shm/params_default"};
};
