#pragma once

#include <set>

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotModelPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotModelPanel(FrogPilotSettingsWindow *parent);

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

  ButtonControl *selectModelButton;

  FrogPilotButtonsControl *deleteModelButton;
  FrogPilotButtonsControl *downloadModelButton;

  FrogPilotSettingsWindow *parent;

  Params params;
  Params params_cache{"/cache/params"};
  Params params_default{"/dev/shm/params_default"};
  Params params_memory{"/dev/shm/params"};

  QDir modelDir{"/data/models/"};

  QMap<QString, QString> getDeletableModels();
  QMap<QString, QString> modelFileToNameMap;
  QMap<QString, QString> modelFileToNameMapProcessed;

  QString currentModel;

  QStringList availableModelNames;
};
