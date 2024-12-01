#pragma once

#include <set>

#include "selfdrive/frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotModelPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotModelPanel(FrogPilotSettingsWindow *parent);

signals:
  void openParentToggle();
  void openSubParentToggle();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void hideSubToggles();
  void hideToggles();
  void showToggles(const std::set<QString> &keys);
  void updateModelLabels();
  void updateState(const UIState &s);

  ButtonControl *selectModelBtn;

  FrogPilotButtonsControl *deleteModelBtn;
  FrogPilotButtonsControl *downloadModelBtn;

  FrogPilotSettingsWindow *parent;

  Params params;
  Params params_default{"/data/params_default"};
  Params params_memory{"/dev/shm/params"};
  Params params_storage{"/persist/params"};

  QDir modelDir{"/data/models/"};

  QJsonObject frogpilotToggleLevels;

  QList<LabelControl*> labelControls;

  QMap<QString, QString> modelFileToNameMap;
  QMap<QString, QString> modelFileToNameMapProcessed;

  QStringList availableModels;
  QStringList availableModelNames;

  bool allModelsDownloaded;
  bool allModelsDownloading;
  bool cancellingDownload;
  bool finalizingDownload;
  bool modelDownloading;
  bool modelRandomizerOpen;
  bool noModelsDownloaded;
  bool started;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  std::set<QString> modelRandomizerKeys = {"ManageBlacklistedModels", "ResetScores", "ReviewScores"};
};
