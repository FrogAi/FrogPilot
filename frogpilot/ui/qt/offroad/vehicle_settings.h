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
  void updateToggles();

  bool forceOpenDescriptions;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> gmKeys = {"LongPitch", "VoltSNG"};
  QSet<QString> hkgKeys = {"TacoTuneHacks"};
  QSet<QString> hondaKeys = {"HondaAltTune", "HondaMaxBrake"};
  QSet<QString> longitudinalKeys = {"FrogsGoMoosTweak", "HondaAltTune", "HondaMaxBrake", "LongPitch", "SNGHack", "VoltSNG"};
  QSet<QString> subaruKeys = {"SubaruSNG"};
  QSet<QString> toyotaKeys = {"ClusterOffset", "FrogsGoMoosTweak", "LockDoorsTimer", "SNGHack", "ToyotaDSUBypass", "ToyotaDoors"};
  QSet<QString> vehicleInfoKeys = {"BlindSpotSupport", "HardwareDetected", "OpenpilotLongitudinal", "PedalSupport", "RadarSupport", "SDSUSupport", "SNGSupport"};

  QSet<QString> parentKeys;

  FrogPilotSettingsWindow *parent;

  ParamControl *disableOpenpilotLong;

  Params params;
};
