#pragma once

#include <vector>

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"
#include "frogpilot/ui/qt/widgets/navigation_functions.h"

class FrogPilotMapsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotMapsPanel(FrogPilotSettingsWindow *parent);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void cancelDownload();
  void refreshMapInfo();
  void startDownload();
  void updateDownloadLabels(const std::string &osmDownloadProgress);
  void updateState(const UIState &s, const FrogPilotUIState &fs);

  bool cancellingDownload = false;
  bool forceOpenDescriptions = false;
  bool hasMapsSelected = false;
  bool removingMaps = false;
  bool wasDownloadingMaps = false;

  int initialDownloadedFiles = -1;
  int previousDownloadedFiles = 0;

  ButtonControl *downloadMapsButton;
  ButtonControl *removeMapsButton;
  ButtonControl *resetMapdButton;

  ButtonParamControl *preferredSchedule;

  FrogPilotButtonsControl *selectMaps;

  FrogPilotSettingsWindow *parent;

  LabelControl *downloadETA;
  LabelControl *downloadStatus;
  LabelControl *downloadTimeElapsed;
  LabelControl *lastMapsDownload;
  LabelControl *mapsSize;

  std::vector<MapSelectionControl *> mapSelectionControls;

  Params params;
  Params params_memory{"/dev/shm/params"};

  QDateTime startTime;

  QDir mapsFolderPath{"/data/media/0/osm/offline"};

  QElapsedTimer elapsedTime;
};
