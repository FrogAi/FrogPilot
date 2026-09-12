#pragma once

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
  void updateCarLabels();
  void updateToggles();

  bool forceOpenDescriptions = false;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> gmKeys = {"ExperimentalGMTune", "LongPitch", "VoltSNG"};
  QSet<QString> hkgKeys = {"NewLongAPI", "TacoTuneHacks"};
  QSet<QString> hondaKeys = {"HondaAltTune", "HondaLowSpeedPedal", "HondaMaxBrake"};
  QSet<QString> longitudinalKeys = {"ExperimentalGMTune", "FrogsGoMoosTweak", "HondaAltTune", "HondaMaxBrake", "HondaLowSpeedPedal", "LongPitch", "NewLongAPI", "SNGHack", "VoltSNG"};
  QSet<QString> subaruKeys = {"SubaruSNG"};
  QSet<QString> toyotaKeys = {"ClusterOffset", "FrogsGoMoosTweak", "LockDoorsTimer", "SNGHack", "ToyotaDoors", "ToyotaDSUBypass"};
  QSet<QString> vehicleInfoKeys = {"BlindSpotSupport", "HardwareDetected", "OpenpilotLongitudinal", "PedalSupport", "RadarSupport", "SDSUSupport", "SNGSupport"};

  QSet<QString> parentKeys;

  ButtonControl *selectMakeButton;
  ButtonControl *selectModelButton;

  FrogPilotSettingsWindow *parent;

  ParamControl *disableOpenpilotLong;
  ParamControl *forceFingerprint;

  Params params;
  Params params_default{"/dev/shm/params_default"};

  QJsonObject frogpilotToggleLevels;

  QMap<QString, QString> carModels;
};
