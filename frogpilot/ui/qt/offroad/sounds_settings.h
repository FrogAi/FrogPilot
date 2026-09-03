#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotSoundsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotSoundsPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void playSound(const QString &path, float volume);
  void testSound(const QString &key);
  void updateToggles();

  bool forceOpenDescriptions;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> alertVolumeControlKeys {"DisengageVolume", "EngageVolume", "PromptDistractedVolume", "PromptVolume", "RefuseVolume", "WarningImmediateVolume", "WarningSoftVolume"};
  QSet<QString> customAlertsKeys {"GoatScream", "GreenLightAlert", "LeadDepartingAlert", "LoudBlindspotAlert", "SpeedLimitChangedAlert"};

  QSet<QString> parentKeys;

  FrogPilotSettingsWindow *parent;

  Params params;
  Params params_memory{"", true};

  QProcess *soundPlayerProcess;
};
