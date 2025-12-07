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
  QString modelId(const QString &modelName);
  QString modelName(const QString &modelId);
  QStringList deletableModels();
  QStringList modelNames(const QStringList &modelIds);
  void refreshModels();
  void updateModelLabels(FrogPilotListWidget *labelsList);
  void updateState(const UIState &s, const FrogPilotUIState &fs);
  void updateToggles();

  bool allModelsDownloading = false;
  bool cancellingDownload = false;
  bool finalizingDownload = false;
  bool forceOpenDescriptions;
  bool hasDeletableModels = false;
  bool modelDownloading = false;

  std::map<QString, AbstractControl*> toggles;

  ButtonControl *selectModelButton;

  FrogPilotButtonsControl *deleteModelButton;
  FrogPilotButtonsControl *downloadModelButton;

  FrogPilotSettingsWindow *parent;

  Params params;

  QDir modelsDir;

  QMap<QString, QString> availableModels;

  QString defaultModel;
  QString defaultModelName;
  QString selectedModelName;

  QStringList downloadedModels;
};
