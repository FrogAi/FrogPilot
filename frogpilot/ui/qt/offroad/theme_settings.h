#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotThemesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotThemesPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateStartupAlert();
  void updateToggles();

  bool forceOpenDescriptions = false;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> customThemeKeys = {"ColorScheme", "DistanceIconPack", "DownloadStatusLabel", "IconPack", "SignalAnimation", "SoundPack", "WheelIcon"};

  QSet<QString> parentKeys;

  FrogPilotButtonsControl *startupAlertButton;

  FrogPilotSettingsWindow *parent;
};
