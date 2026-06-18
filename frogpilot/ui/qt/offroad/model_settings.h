#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotModelPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotModelPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

signals:
  void openSubPanel();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void updateModelLabels(FrogPilotListWidget *labelsList);
  void updateState(const UIState &s, const FrogPilotUIState &fs);
  void updateToggles();

  bool allModelsDownloaded = false;
  bool allModelsDownloading = false;
  bool cancellingDownload = false;
  bool finalizingDownload = false;
  bool forceOpenDescriptions;
  bool modelDownloading = false;
  bool noModelsDownloaded = false;
  bool tinygradUpdate = false;
  bool updatingTinygrad = false;

  std::map<QString, AbstractControl*> toggles;

  ButtonControl *selectModelButton;

  FrogPilotButtonsControl *deleteModelButton;
  FrogPilotButtonsControl *downloadModelButton;
  FrogPilotButtonsControl *updateTinygradButton;

  FrogPilotSettingsWindow *parent;

  Params params;
  Params params_memory{"", true};

  QDir modelDir{"/data/models/"};

  QMap<QString, QString> modelFileToNameMap;
  QMap<QString, QString> modelFileToNameMapProcessed;

  QString currentModel;
  QString defaultModel;

  QStringList availableModelNames;
};
