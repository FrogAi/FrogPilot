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
  bool forceOpenDescriptions = false;
  bool modelDownloading = false;
  bool noModelsDownloaded = false;

  std::map<QString, AbstractControl*> toggles;

  ButtonControl *selectModelButton = nullptr;

  FrogPilotButtonsControl *deleteModelButton = nullptr;
  FrogPilotButtonsControl *downloadModelButton = nullptr;

  FrogPilotSettingsWindow *parent = nullptr;

  Params params;
  Params params_default{"/dev/shm/params_default"};

  QDir modelDir{"/data/models/"};

  QMap<QString, QString> getDeletableModels();
  QMap<QString, QString> modelFileToNameMap;
  QMap<QString, QString> modelFileToNameMapProcessed;

  QString currentModel;

  QStringList availableModelNames;
  QStringList getDownloadableModels();
};
