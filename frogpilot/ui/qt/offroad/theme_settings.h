#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

struct ThemeAsset {
  QDir directory;

  QString subFolder;
  QString component;
  QString downloadableParam;
  QString deleteTitle;
  QString deleteOneConfirmation;
  QString deleteSeveralConfirmation;
  QString downloadTitle;
  QString selectTitle;

  QStringList excludedBuiltinThemes;

  bool downloaded = false;
  bool downloading = false;

  FrogPilotButtonsControl *button;
};

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
  void updateState(const UIState &s, const FrogPilotUIState &fs);
  void updateThemeSelections(bool randomThemesEnabled);
  void updateToggles();

  bool cancellingDownload = false;
  bool finalizingDownload = false;
  bool forceOpenDescriptions;
  bool randomThemes = false;
  bool themeDownloading = false;

  std::map<QString, AbstractControl*> toggles;

  std::map<QString, ThemeAsset> themeAssets;

  QSet<QString> customThemeKeys = {"ColorScheme", "DistanceIconPack", "DownloadStatusLabel", "IconPack", "SignalAnimation", "SoundPack", "WheelIcon"};

  QSet<QString> parentKeys;

  FrogPilotButtonsControl *startupAlertButton;

  FrogPilotSettingsWindow *parent;

  LabelControl *downloadStatusLabel;

  QDir themePacksDirectory{"/data/themes/theme_packs/"};
  QDir wheelsDirectory{"/data/themes/steering_wheels/"};
};
