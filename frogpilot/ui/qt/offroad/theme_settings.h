#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotThemesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotThemesPanel(FrogPilotSettingsWindow *parent);

protected:
  void showEvent(QShowEvent *event) override;

signals:
  void openSubPanel();

private:
  void updateState(const UIState &s, const FrogPilotUIState &fs);
  void updateToggles();

  bool cancellingDownload;
  bool colorDownloading;
  bool colorsDownloaded;
  bool distanceIconDownloading;
  bool distanceIconsDownloaded;
  bool finalizingDownload;
  bool forceOpenDescriptions;
  bool iconDownloading;
  bool iconsDownloaded;
  bool randomThemes;
  bool signalDownloading;
  bool signalsDownloaded;
  bool soundDownloading;
  bool soundsDownloaded;
  bool themeDownloading;
  bool wheelDownloading;
  bool wheelsDownloaded;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  QSet<QString> customThemeKeys = {"CustomColors", "CustomDistanceIcons", "CustomIcons", "CustomSignals", "CustomSounds", "DownloadStatusLabel", "WheelIcon"};

  QSet<QString> parentKeys;

  FrogPilotButtonsControl *manageCustomColorsButton;
  FrogPilotButtonsControl *manageCustomIconsButton;
  FrogPilotButtonsControl *manageCustomSignalsButton;
  FrogPilotButtonsControl *manageCustomSoundsButton;
  FrogPilotButtonsControl *manageDistanceIconsButton;
  FrogPilotButtonsControl *manageWheelIconsButton;

  FrogPilotSettingsWindow *parent;

  LabelControl *downloadStatusLabel;

  QDir themePacksDirectory{"/data/themes/theme_packs/"};
  QDir wheelsDirectory{"/data/themes/steering_wheels/"};

  QJsonObject frogpilotToggleLevels;

  QString colorSchemeToDownload;
  QString distanceIconPackToDownload;
  QString iconPackToDownload;
  QString signalAnimationToDownload;
  QString soundPackToDownload;
  QString wheelToDownload;

  Params params;
  Params params_memory{"/dev/shm/params"};
};
