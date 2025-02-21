#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotThemesPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotThemesPanel(FrogPilotSettingsWindow *parent);

protected:
  void showEvent(QShowEvent *event) override;

signals:
  void openParentToggle();

private:
  void hideToggles();
  void showToggles(const std::set<QString> &keys);
  void updateState(const UIState &s);

  bool cancellingDownload;
  bool colorDownloading;
  bool colorsDownloaded;
  bool distanceIconDownloading;
  bool distanceIconsDownloaded;
  bool finalizingDownload;
  bool iconDownloading;
  bool iconsDownloaded;
  bool signalDownloading;
  bool signalsDownloaded;
  bool soundDownloading;
  bool soundsDownloaded;
  bool themeDownloading;
  bool wheelDownloading;
  bool wheelsDownloaded;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  std::set<QString> customThemeKeys = {"CustomColors", "CustomDistanceIcons", "CustomIcons", "CustomSignals", "CustomSounds", "DownloadStatusLabel", "WheelIcon"};

  FrogPilotButtonsControl *manageCustomColorsBtn;
  FrogPilotButtonsControl *manageCustomIconsBtn;
  FrogPilotButtonsControl *manageCustomSignalsBtn;
  FrogPilotButtonsControl *manageCustomSoundsBtn;
  FrogPilotButtonsControl *manageDistanceIconsBtn;
  FrogPilotButtonsControl *manageWheelIconsBtn;

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
