#pragma once

#include <set>

#include "selfdrive/ui/qt/offroad/settings.h"
#include "selfdrive/ui/ui.h"

class FrogPilotVisualsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotVisualsPanel(SettingsWindow *parent);

signals:
  void openParentToggle();
  void openSubParentToggle();

private:
  void hideSubToggles();
  void hideToggles();
  void showEvent(QShowEvent *event) override;
  void updateCarToggles();
  void updateMetric();
  void updateState(const UIState &s);

  FrogPilotButtonsControl *manageCustomColorsBtn;
  FrogPilotButtonsControl *manageCustomIconsBtn;
  FrogPilotButtonsControl *manageCustomSignalsBtn;
  FrogPilotButtonsControl *manageCustomSoundsBtn;
  FrogPilotButtonsControl *manageWheelIconsBtn;

  LabelControl *downloadStatusLabel;

  std::set<QString> alertVolumeControlKeys = {"DisengageVolume", "EngageVolume", "PromptDistractedVolume", "PromptVolume", "RefuseVolume", "WarningImmediateVolume", "WarningSoftVolume"};
  std::set<QString> bonusContentKeys = {"GoatScream", "HolidayThemes", "PersonalizeOpenpilot", "RandomEvents"};
  std::set<QString> customAlertsKeys = {"GreenLightAlert", "LeadDepartingAlert", "LoudBlindspotAlert"};
  std::set<QString> customOnroadUIKeys = {"Compass", "CustomPaths", "PedalsOnUI", "RoadNameUI", "RotatingWheel", "ShowStoppingPoint"};
  std::set<QString> developerUIKeys = {"BorderMetrics", "FPSCounter", "LateralMetrics", "LongitudinalMetrics", "NumericalTemp", "SidebarMetrics", "UseSI"};
  std::set<QString> modelUIKeys = {"DynamicPathWidth", "HideLeadMarker", "LaneLinesWidth", "PathEdgeWidth", "PathWidth", "RoadEdgesWidth", "UnlimitedLength"};
  std::set<QString> personalizeOpenpilotKeys = {"CustomColors", "CustomIcons", "CustomSignals", "CustomSounds", "DownloadStatusLabel", "StartupAlert", "WheelIcon"};
  std::set<QString> qolKeys = {"BigMap", "CameraView", "DriverCamera", "FullMap", "HideSpeed", "MapStyle", "StoppedTimer", "WheelSpeed"};
  std::set<QString> screenKeys = {"HideUIElements", "ScreenBrightness", "ScreenBrightnessOnroad", "ScreenRecorder", "ScreenTimeout", "ScreenTimeoutOnroad", "StandbyMode"};

  std::map<std::string, AbstractControl*> toggles;

  Params params;
  Params paramsMemory{"/dev/shm/params"};

  bool cancellingDownload;
  bool colorDownloading;
  bool colorsDownloaded;
  bool disableOpenpilotLongitudinal;
  bool hasAutoTune;
  bool hasBSM;
  bool hasOpenpilotLongitudinal;
  bool iconDownloading;
  bool iconsDownloaded;
  bool isMetric = params.getBool("IsMetric");
  bool isRelease;
  bool personalizeOpenpilotOpen;
  bool signalDownloading;
  bool signalsDownloaded;
  bool soundDownloading;
  bool soundsDownloaded;
  bool started;
  bool themeDeleting;
  bool themeDownloading;
  bool wheelDownloading;
  bool wheelsDownloaded;
};
