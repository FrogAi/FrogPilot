#pragma once

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
  void startDownload();
  void updateDownloadLabels(std::string &osmDownloadProgress);
  void updateState(const UIState &s, const FrogPilotUIState &fs);

  bool cancellingDownload;
  bool forceOpenDescriptions;
  bool hasMapsSelected;

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

  Params params;
  Params params_memory{"/dev/shm/params"};

  QDateTime startTime;

  QDir mapsFolderPath{"/data/media/0/osm/offline"};

  QElapsedTimer elapsedTime;
};
