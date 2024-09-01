#pragma once

#include "selfdrive/frogpilot/navigation/ui/navigation_functions.h"
#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotMapsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotMapsPanel(FrogPilotSettingsWindow *parent);

signals:
  void openMapSelection();

private:
  void cancelDownload();
  void displayMapButtons(bool visible = true);
  void downloadMaps();
  void finalizeDownload();
  void handleDownloadError();
  void hideEvent(QHideEvent *event) override;
  void resetDownloadLabels();
  void resetDownloadState();
  void showEvent(QShowEvent *event);
  void updateDownloadLabels(int downloadedFiles, int totalFiles, qint64 remainingTime, qint64 elapsedMilliseconds);
  void updateDownloadStatusLabels();
  void updateState(const UIState &s);

  ButtonControl *downloadMapsButton;
  ButtonControl *removeMapsButton;

  ButtonParamControl *preferredSchedule;

  FrogPilotButtonsControl *selectMapsButton;

  LabelControl *africaLabel;
  LabelControl *antarcticaLabel;
  LabelControl *asiaLabel;
  LabelControl *downloadETA;
  LabelControl *downloadStatus;
  LabelControl *downloadTimeElapsed;
  LabelControl *europeLabel;
  LabelControl *lastMapsDownload;
  LabelControl *mapsSize;
  LabelControl *midwestLabel;
  LabelControl *northAmericaLabel;
  LabelControl *northeastLabel;
  LabelControl *oceaniaLabel;
  LabelControl *southAmericaLabel;
  LabelControl *southLabel;
  LabelControl *territoriesLabel;
  LabelControl *westLabel;

  MapSelectionControl *africaMaps;
  MapSelectionControl *antarcticaMaps;
  MapSelectionControl *asiaMaps;
  MapSelectionControl *europeMaps;
  MapSelectionControl *midwestMaps;
  MapSelectionControl *northAmericaMaps;
  MapSelectionControl *northeastMaps;
  MapSelectionControl *oceaniaMaps;
  MapSelectionControl *southAmericaMaps;
  MapSelectionControl *southMaps;
  MapSelectionControl *territoriesMaps;
  MapSelectionControl *westMaps;

  Params params;
  Params paramsMemory{"/dev/shm/params"};

  bool countriesOpen;
  bool downloadActive;

  qint64 startTime;

  QString mapsFolderPath = "/data/media/0/osm/offline";
  QString mapsSelected;
};
