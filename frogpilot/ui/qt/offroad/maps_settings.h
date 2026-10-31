#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"
#include "frogpilot/ui/qt/widgets/navigation_functions.h"

class FrogPilotMapsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotMapsPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void cancelDownload();
  void refreshMapInfo();
  void startDownload();
  void updateDownloadLabels(const std::string &osmDownloadProgress, bool downloadingMaps);
  void updateMapsSelected();
  void updateState(const UIState &s, const FrogPilotUIState &fs);

  bool cancellingDownload = false;
  bool forceOpenDescriptions = false;
  bool hasMapsSelected = false;
  bool removingMaps = false;
  bool wasDownloadingMaps = false;

  int initialDownloadedFiles = -1;
  int previousDownloadedFiles = 0;

  ButtonControl *downloadMapsButton = nullptr;
  ButtonControl *removeMapsButton = nullptr;
  ButtonControl *resetMapdButton = nullptr;

  ButtonParamControl *preferredSchedule = nullptr;

  FrogPilotButtonsControl *selectMaps = nullptr;

  FrogPilotSettingsWindow *parent = nullptr;

  LabelControl *downloadETA = nullptr;
  LabelControl *downloadStatus = nullptr;
  LabelControl *downloadTimeElapsed = nullptr;
  LabelControl *lastMapsDownload = nullptr;
  LabelControl *mapsSize = nullptr;

  std::vector<MapSelectionControl *> mapSelectionControls;

  Params params;
  Params params_memory{"/dev/shm/params"};

  QDateTime startTime;

  QDir mapsFolderPath{"/data/media/0/osm/offline"};

  QElapsedTimer elapsedTime;
};
