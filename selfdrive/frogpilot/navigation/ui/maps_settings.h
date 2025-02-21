#pragma once

#include "selfdrive/frogpilot/navigation/ui/navigation_functions.h"
#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotMapsPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotMapsPanel(FrogPilotSettingsWindow *parent);

signals:
  void openMapSelection();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void cancelDownload();
  void startDownload();
  void updateDownloadLabels(std::string &osmDownloadProgress);
  void updateState(const UIState &s);

  bool cancellingDownload;
  bool hasMapsSelected;

  ButtonControl *downloadMapsButton;
  ButtonControl *removeMapsButton;

  FrogPilotSettingsWindow *parent;

  LabelControl *downloadETA;
  LabelControl *downloadStatus;
  LabelControl *downloadTimeElapsed;
  LabelControl *lastMapsDownload;
  LabelControl *mapsSize;

  Params params;
  Params params_memory{"/dev/shm/params"};

  QDateTime startTime;

  QElapsedTimer elapsedTime;

  QString mapsFolderPath;

  QStackedLayout *mapsLayout;
};
