#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotThemesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotThemesPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

protected:
  void showEvent(QShowEvent *event) override;

signals:
  void openSubPanel();

private:
  void updateState(const UIState &s, const FrogPilotUIState &fs);
  void updateToggles();

  bool cancellingDownload = false;
  bool colorDownloading = false;
  bool colorsDownloaded = false;
  bool distanceIconDownloading = false;
  bool distanceIconsDownloaded = false;
  bool finalizingDownload = false;
  bool forceOpenDescriptions;
  bool iconDownloading = false;
  bool iconsDownloaded = false;
  bool randomThemes = false;
  bool signalDownloading = false;
  bool signalsDownloaded = false;
  bool soundDownloading = false;
  bool soundsDownloaded = false;
  bool themeDownloading = false;
  bool wheelDownloading = false;
  bool wheelsDownloaded = false;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> customThemeKeys = {"ColorScheme", "DistanceIconPack", "DownloadStatusLabel", "IconPack", "SignalAnimation", "SoundPack", "WheelIcon"};

  QSet<QString> parentKeys;

  FrogPilotButtonsControl *manageColorSchemeButton;
  FrogPilotButtonsControl *manageDistanceIconPackButton;
  FrogPilotButtonsControl *manageIconPackButton;
  FrogPilotButtonsControl *manageSignalAnimationButton;
  FrogPilotButtonsControl *manageSoundPackButton;
  FrogPilotButtonsControl *manageWheelIconsButton;

  FrogPilotSettingsWindow *parent;

  LabelControl *downloadStatusLabel;

  QDir themePacksDirectory{"/data/themes/theme_packs/"};
  QDir wheelsDirectory{"/data/themes/steering_wheels/"};

  QString colorSchemeToDownload;
  QString distanceIconPackToDownload;
  QString iconPackToDownload;
  QString signalAnimationToDownload;
  QString soundPackToDownload;
  QString wheelToDownload;

  Params params;
  Params params_memory{"", true};
};
