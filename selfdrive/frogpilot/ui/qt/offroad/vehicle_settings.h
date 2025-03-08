#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotVehiclesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVehiclesPanel(FrogPilotSettingsWindow *parent);

signals:
  void openParentToggle();

private:
  void showEvent(QShowEvent *event) override;
  void updateState(const UIState &s);
  void updateToggles();

  bool started;

  std::map<QString, AbstractControl*> toggles;

  std::set<QString> gmKeys = {"ExperimentalGMTune", "LongPitch", "VoltSNG"};
  std::set<QString> hkgKeys = {"NewLongAPI"};
  std::set<QString> longitudinalKeys = {"ExperimentalGMTune", "FrogsGoMoosTweak", "LongPitch", "NewLongAPI", "SNGHack", "VoltSNG"};
  std::set<QString> toyotaKeys = {"ClusterOffset", "FrogsGoMoosTweak", "SNGHack", "ToyotaDoors"};

  FrogPilotSettingsWindow *parent;

  QMap<QString, QString> carModels;

  QStackedLayout *vehiclesLayout;

  ParamControl *disableOpenpilotLong;

  Params params;
  Params params_default{"/dev/shm/params_default"};
};
