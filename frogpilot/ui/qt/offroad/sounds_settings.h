#pragma once

#include <QProcess>

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotSoundsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotSoundsPanel(FrogPilotSettingsWindow *parent);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void playSound(const QString &alert, int volume);
  void updateToggles();

  bool forceOpenDescriptions = false;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> alertVolumeControlKeys {"DisengageVolume", "EngageVolume", "PromptDistractedVolume", "PromptVolume", "RefuseVolume", "WarningImmediateVolume", "WarningSoftVolume"};
  QSet<QString> customAlertsKeys {"GoatScream", "GreenLightAlert", "LeadDepartingAlert", "LoudBlindspotAlert", "SpeedLimitChangedAlert"};

  QSet<QString> parentKeys;

  FrogPilotSettingsWindow *parent;

  QJsonObject frogpilotToggleLevels;

  Params params;
  Params params_memory{"/dev/shm/params"};

  QProcess *soundPlayerProcess = nullptr;
};
