#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotVehiclesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVehiclesPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateCarLabels();
  void updateToggles();

  bool forceOpenDescriptions = false;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> gmKeys = {"LongPitch", "VoltSNG"};
  QSet<QString> hkgKeys = {"NewLongAPI", "TacoTuneHacks"};
  QSet<QString> hondaKeys = {"HondaAltTune", "HondaLowSpeedPedal", "HondaMaxBrake"};
  QSet<QString> longitudinalKeys = {"FrogsGoMoosTweak", "HondaAltTune", "HondaMaxBrake", "HondaLowSpeedPedal", "LongPitch", "NewLongAPI", "SNGHack", "VoltSNG"};
  QSet<QString> subaruKeys = {"SubaruSNG"};
  QSet<QString> toyotaKeys = {"ClusterOffset", "FrogsGoMoosTweak", "LockDoorsTimer", "SNGHack", "ToyotaDoors", "ToyotaDSUBypass"};
  QSet<QString> vehicleInfoKeys = {"BlindSpotSupport", "HardwareDetected", "OpenpilotLongitudinal", "PedalSupport", "RadarSupport", "SDSUSupport", "SNGSupport"};

  QSet<QString> parentKeys;

  ButtonControl *selectMakeButton = nullptr;
  ButtonControl *selectModelButton = nullptr;

  FrogPilotSettingsWindow *parent = nullptr;

  ParamControl *disableOpenpilotLong = nullptr;
  ParamControl *forceFingerprint = nullptr;

  Params params;
  Params params_default{"/dev/shm/params_default"};

  QMap<QString, QString> carModels;
};
