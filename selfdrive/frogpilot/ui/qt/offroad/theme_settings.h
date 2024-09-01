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
  FrogPilotSettingsWindow *parent;

  FrogPilotButtonsControl *manageCustomColorsBtn;
  FrogPilotButtonsControl *manageCustomIconsBtn;
  FrogPilotButtonsControl *manageCustomSignalsBtn;
  FrogPilotButtonsControl *manageCustomSoundsBtn;
  FrogPilotButtonsControl *manageDistanceIconsBtn;
  FrogPilotButtonsControl *manageWheelIconsBtn;

  LabelControl *downloadStatusLabel;

  std::set<QString> customThemeKeys = {
    "CustomColors", "CustomDistanceIcon", "CustomIcons",
    "CustomSignals", "CustomSounds", "DownloadStatusLabel",
    "WheelIcon"
  };

  std::map<QString, AbstractControl*> toggles;

  Params params;
  Params paramsMemory{"/dev/shm/params"};

  bool cancellingDownload;
  bool colorDownloading;
  bool colorsDownloaded;
  bool disableOpenpilotLongitudinal;
  bool distanceIconDownloading;
  bool distanceIconsDownloaded;
  bool hasOpenpilotLongitudinal;
  bool iconDownloading;
  bool iconsDownloaded;
  bool personalizeOpenpilotOpen;
  bool signalDownloading;
  bool signalsDownloaded;
  bool soundDownloading;
  bool soundsDownloaded;
  bool themeDeleting;
  bool themeDownloading;
  bool wheelDownloading;
  bool wheelsDownloaded;

  void hideToggles();
  void showToggles(const std::set<QString> &keys);
  void updateCarToggles();
  void updateState(const UIState &s);
};
