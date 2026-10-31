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

  bool allDownloaded = false;
  bool downloading = false;

  FrogPilotButtonsControl *button;
};

class FrogPilotThemesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotThemesPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

protected:
  void showEvent(QShowEvent *event) override;

signals:
  void openSubPanel();

private:
  void updateStartupAlert();
  void updateState(const UIState &s, const FrogPilotUIState &fs);
  void updateThemeSelections(bool randomThemesEnabled);
  void updateToggles();

  bool cancellingDownload = false;
  bool finalizingDownload = false;
  bool forceOpenDescriptions = false;
  bool randomThemes = false;
  bool themeDownloading = false;

  std::map<QString, AbstractControl*> toggles;

  std::map<QString, ThemeAsset> themeAssets;

  QSet<QString> customThemeKeys = {"ColorScheme", "DistanceIconPack", "DownloadStatusLabel", "IconPack", "SignalAnimation", "SoundPack", "WheelIcon"};

  QSet<QString> parentKeys;

  FrogPilotButtonsControl *startupAlertButton = nullptr;

  FrogPilotSettingsWindow *parent = nullptr;

  LabelControl *downloadStatusLabel = nullptr;

  QDir themePacksDirectory{"/data/themes/theme_packs/"};
  QDir wheelsDirectory{"/data/themes/steering_wheels/"};

  Params params;
  Params params_default{"/dev/shm/params_default"};
};
