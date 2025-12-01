#include "frogpilot/ui/qt/offroad/model_settings.h"

FrogPilotModelPanel::FrogPilotModelPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  QStackedLayout *modelLayout = new QStackedLayout();
  addItem(modelLayout);

  FrogPilotListWidget *modelList = new FrogPilotListWidget(this);

  ScrollView *modelPanel = new ScrollView(modelList, this);

  modelLayout->addWidget(modelPanel);

  FrogPilotListWidget *modelLabelsList = new FrogPilotListWidget(this);

  ScrollView *modelLabelsPanel = new ScrollView(modelLabelsList, this);

  modelLayout->addWidget(modelLabelsPanel);

  const std::vector<std::tuple<QString, QString, QString, QString>> modelToggles {
    {"AutomaticallyDownloadModels", tr("Automatically Download New Models"), tr("<b>Download new driving models on their own as they are released, so they are ready when you want to try one.</b><br><br>This runs whenever the device is online, including while you are driving. It also grabs every model that is not already on the device, not just newly released ones, so anything you removed with \"Delete Driving Models\" comes back."), ""},
    {"DeleteModel", tr("Delete Driving Models"), tr("<b>Remove driving models you have downloaded to free up storage.</b><br><br>\"DELETE\" lets you pick which ones, \"DELETE ALL\" removes the rest. The model you are currently using and the one FrogPilot ships with are always kept. Turn \"Automatically Download New Models\" off first, or anything you delete is downloaded again within the hour."), ""},
    {"DownloadModel", tr("Download Driving Models"), tr("<b>Download driving models onto the device so you can switch to them.</b><br><br>\"DOWNLOAD\" lets you pick which ones, \"DOWNLOAD ALL\" fetches everything. Your car has to be parked and online, and models are large, so this can take a while."), ""},
    {"ManageBlacklistedModels", tr("Manage Model Blacklist"), tr("<b>Stop the \"Model Randomizer\" from picking driving models you did not get on with.</b><br><br>Blocking a model here has no effect on choosing it yourself under \"Select Driving Model\"."), ""},
    {"ManageScores", tr("Manage Model Ratings"), tr("<b>See how you rated each driving model and how many drives you gave it, or wipe those ratings and start fresh.</b><br><br>These are for your own comparison. The \"Model Randomizer\" picks at random and does not favour your higher-rated models."), ""},
    {"ModelRandomizer", tr("Model Randomizer"), tr("<b>Picks a different driving model for you at the start of every drive, then asks how it went when you park, so you can work out which one you like best.</b><br><br>It only chooses from models you have downloaded and have not blacklisted, and it only asks for a rating after drives longer than 15 minutes. Your ratings are saved under \"Manage Model Ratings\" for you to compare."), ""},
    {"SelectModel", tr("Select Driving Model"), tr("<b>Choose which driving model does the driving.</b><br><br>The model is the part of openpilot that decides how to steer, speed up, and slow down, so switching it changes how the car feels. Only models you have downloaded are listed, and changing it while driving asks you to reboot."), ""}
  };

  for (const auto &[param, title, desc, icon] : modelToggles) {
    AbstractControl *modelToggle;

    if (param == "DeleteModel") {
      deleteModelButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DELETE ALL")});
      QObject::connect(deleteModelButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        if (id == 0) {
          QStringList modelsToDelete = FrogPilotMultiOptionDialog::getSelections(tr("Choose driving models to delete"), modelNames(deletableModels()), tr("Delete"), this);
          if (!modelsToDelete.isEmpty()) {
            QString confirmation;
            if (modelsToDelete.size() == 1) {
              confirmation = tr("Are you sure you want to delete the \"%1\" model?").arg(modelsToDelete.first());
            } else {
              confirmation = tr("Are you sure you want to delete these %1 models?").arg(modelsToDelete.size());
            }

            if (ConfirmationDialog::confirm(confirmation, tr("Delete"), this)) {
              for (const QString &modelToDelete : modelsToDelete) {
                QDir(modelsDir.filePath(modelId(modelToDelete))).removeRecursively();
              }
            }
          }
        } else if (ConfirmationDialog::confirm(tr("Delete every downloaded driving model except the one you are using and the one FrogPilot ships with?"), tr("Delete"), this)) {
          for (const QString &modelId : deletableModels()) {
            QDir(modelsDir.filePath(modelId)).removeRecursively();
          }
        }

        frogpilotUIState()->updateToggles();

        refreshModels();
      });
      modelToggle = deleteModelButton;
    } else if (param == "DownloadModel") {
      downloadModelButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DOWNLOAD"), tr("DOWNLOAD ALL")});
      QObject::connect(downloadModelButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        if (allModelsDownloading || modelDownloading) {
          frogpilotUIState()->cancelModelDownload();

          cancellingDownload = true;
          return;
        }

        if (id == 0) {
          QStringList missingModels;
          for (const QString &modelId : availableModels.keys()) {
            if (!downloadedModels.contains(modelId)) {
              missingModels.append(availableModels.value(modelId));
            }
          }

          QStringList modelsToDownload = FrogPilotMultiOptionDialog::getSelections(tr("Choose driving models to download"), missingModels, tr("Download"), this);
          if (modelsToDownload.isEmpty()) {
            return;
          }

          QStringList modelIds;
          for (const QString &modelToDownload : modelsToDownload) {
            modelIds.append(availableModels.key(modelToDownload));
          }

          frogpilotUIState()->downloadModels(modelIds);

          modelDownloading = true;
        } else {
          frogpilotUIState()->downloadAllModels();

          allModelsDownloading = true;
        }

        downloadModelButton->setValue(tr("Downloading..."));
      });
      modelToggle = downloadModelButton;
    } else if (param == "ManageBlacklistedModels") {
      FrogPilotButtonsControl *blacklistButton = new FrogPilotButtonsControl(title, desc, icon, {tr("ADD"), tr("REMOVE"), tr("REMOVE ALL")});
      QObject::connect(blacklistButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList blacklistedModels = QString::fromStdString(params.get("BlacklistedModels")).split(",", QString::SkipEmptyParts);

        if (id == 0) {
          QStringList blacklistableModels;
          for (const QString &modelId : QStringList{defaultModel} + downloadedModels) {
            if (!blacklistedModels.contains(modelId)) {
              blacklistableModels.append(modelId);
            }
          }

          if (blacklistableModels.isEmpty()) {
            ConfirmationDialog::alert(tr("There are no driving models available to blacklist."), this);
          } else if (blacklistableModels.size() == 1) {
            ConfirmationDialog::alert(tr("There are no more driving models to blacklist. The only available model is \"%1\"!").arg(modelName(blacklistableModels.first())), this);
          } else {
            QString modelToBlacklist = MultiOptionDialog::getSelection(tr("Select a driving model to add to the blacklist"), modelNames(blacklistableModels), "", this);
            if (!modelToBlacklist.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to add the \"%1\" model to the blacklist?").arg(modelToBlacklist), tr("Add"), this)) {
              blacklistedModels.append(modelId(modelToBlacklist));

              params.put("BlacklistedModels", blacklistedModels.join(",").toStdString());

              frogpilotUIState()->updateToggles();
            }
          }
        } else if (id == 1) {
          QStringList whitelistableModels = modelNames(blacklistedModels);
          whitelistableModels.sort();

          if (whitelistableModels.isEmpty()) {
            ConfirmationDialog::alert(tr("You have not blocked any driving models."), this);
            return;
          }

          QString modelToWhitelist = MultiOptionDialog::getSelection(tr("Select a driving model to remove from the blacklist"), whitelistableModels, "", this);
          if (!modelToWhitelist.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to remove the \"%1\" model from the blacklist?").arg(modelToWhitelist), tr("Remove"), this)) {
            blacklistedModels.removeAll(modelId(modelToWhitelist));

            params.put("BlacklistedModels", blacklistedModels.join(",").toStdString());
          }
        } else if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to remove all of your blacklisted driving models?"), this)) {
          params.remove("BlacklistedModels");
        }
      });
      modelToggle = blacklistButton;
    } else if (param == "ManageScores") {
      FrogPilotButtonsControl *manageScoresButton = new FrogPilotButtonsControl(title, desc, icon, {tr("RESET"), tr("VIEW")});
      QObject::connect(manageScoresButton, &FrogPilotButtonsControl::buttonClicked, [modelLayout, modelLabelsList, modelLabelsPanel, this](int id) {
        if (id == 0) {
          if (FrogPilotConfirmationDialog::yesorno(tr("Reset how many drives and what rating each driving model has? Your drives themselves are not touched."), this)) {
            params.remove("ModelDrivesAndScores");
          }
        } else {
          openSubPanel();

          updateModelLabels(modelLabelsList);

          modelLayout->setCurrentWidget(modelLabelsPanel);
        }
      });
      modelToggle = manageScoresButton;
    } else if (param == "SelectModel") {
      selectModelButton = new ButtonControl(title, tr("SELECT"), desc);
      QObject::connect(selectModelButton, &ButtonControl::clicked, [this]() {
        QString defaultModelDisplay = defaultModelName + " (Default)";

        QStringList models = modelNames(downloadedModels);
        models.sort();
        models.prepend(defaultModelDisplay);

        QString modelToSelect = MultiOptionDialog::getSelection(tr("Select a Model"), models, selectedModelName, this);
        if (modelToSelect.isEmpty()) {
          return;
        }

        params.put("DrivingModel", modelId(modelToSelect == defaultModelDisplay ? defaultModelName : modelToSelect).toStdString());

        frogpilotUIState()->updateToggles();

        if (uiState()->scene.started && FrogPilotConfirmationDialog::toggleReboot(this)) {
          FrogPilotConfirmationDialog::softReboot(this);
        }

        refreshModels();
      });
      modelToggle = selectModelButton;
    } else {
      modelToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = modelToggle;

    modelList->addItem(modelToggle);
  }

  openDescriptions(forceOpenDescriptions, toggles);

  QObject::connect(static_cast<ToggleControl*>(toggles["ModelRandomizer"]), &ToggleControl::toggleFlipped, [this](bool state) {
    updateToggles();

    if (downloadedModels.size() < availableModels.size() && !allModelsDownloading && !modelDownloading && state) {
      FrogPilotUIState &fs = *frogpilotUIState();
      bool parked = !uiState()->scene.started || fs.frogpilot_scene.parked || this->parent->isFrogsGoMoo;

      if (!fs.frogpilot_scene.online || !parked) {
        ConfirmationDialog::alert(tr("The \"Model Randomizer\" only picks from models you have downloaded. Park your car and connect to the internet to download them."), this);
      } else if (FrogPilotConfirmationDialog::yesorno(tr("The \"Model Randomizer\" only picks from models you have downloaded. Download every model now?"), this)) {
        frogpilotUIState()->downloadAllModels();

        allModelsDownloading = true;

        downloadModelButton->setValue(tr("Downloading..."));
      }
    }
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [modelLayout, modelPanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    modelLayout->setCurrentWidget(modelPanel);
  });
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::togglesUpdated, this, &FrogPilotModelPanel::refreshModels);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotModelPanel::updateState);
}

QString FrogPilotModelPanel::modelId(const QString &modelName) {
  return modelName == defaultModelName ? defaultModel : availableModels.key(modelName);
}

QString FrogPilotModelPanel::modelName(const QString &modelId) {
  return modelId == defaultModel ? defaultModelName : availableModels.value(modelId);
}

QStringList FrogPilotModelPanel::deletableModels() {
  QStringList models = downloadedModels;
  models.removeAll(QString::fromStdString(params.get("DrivingModel")));
  models.removeAll(frogpilotUIState()->frogpilot_scene.frogpilot_toggles.value("model").toString());
  return models;
}

QStringList FrogPilotModelPanel::modelNames(const QStringList &modelIds) {
  QStringList names;
  for (const QString &modelId : modelIds) {
    if (modelId == defaultModel || availableModels.contains(modelId)) {
      names.append(modelName(modelId));
    }
  }
  return names;
}

void FrogPilotModelPanel::refreshModels() {
  const QJsonObject &frogpilot_toggles = frogpilotUIState()->frogpilot_scene.frogpilot_toggles;

  defaultModel = frogpilot_toggles.value("default_model").toString();
  defaultModelName = frogpilot_toggles.value("default_model_name").toString();
  modelsDir.setPath(frogpilot_toggles.value("models_path").toString());

  availableModels.clear();
  const QJsonObject models = frogpilot_toggles.value("available_models").toObject();
  for (const QString &modelId : models.keys()) {
    availableModels.insert(modelId, models.value(modelId).toString());
  }

  downloadedModels.clear();
  for (const QString &modelId : availableModels.keys()) {
    if (modelsDir.exists(modelId)) {
      downloadedModels.append(modelId);
    }
  }

  hasDeletableModels = !deletableModels().isEmpty();

  QString selectedModel = QString::fromStdString(params.get("DrivingModel"));
  selectedModelName = downloadedModels.contains(selectedModel) ? availableModels.value(selectedModel) : defaultModelName + " (Default)";
  selectModelButton->setValue(selectedModelName);
}

void FrogPilotModelPanel::showEvent(QShowEvent *event) {
  refreshModels();

  updateToggles();
}

void FrogPilotModelPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible()) {
    return;
  }

  const FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;

  const cereal::FrogPilotProcessState::Reader &frogpilotProcessState = (*fs.sm)["frogpilotProcessState"].getFrogpilotProcessState();

  if (frogpilotProcessState.getDownloadingModels() && !allModelsDownloading && !modelDownloading) {
    allModelsDownloading = true;
  }

  bool downloading = allModelsDownloading || modelDownloading;
  bool parked = !s.scene.started || frogpilot_scene.parked || parent->isFrogsGoMoo;

  if (downloading && !finalizingDownload) {
    if (fs.download_model_request_time <= frogpilotProcessState.getModelDownloadRequestTime()) {
      QString progress = QString::fromStdString(frogpilotProcessState.getModelDownloadProgress());
      bool finished = !frogpilotProcessState.getDownloadingModels();

      if (progress == "Downloaded!") {
        downloadModelButton->setValue(tr("Downloaded!"));
      } else if (progress == "All models downloaded!") {
        downloadModelButton->setValue(tr("All models downloaded!"));
      } else if (progress.contains("cancelled", Qt::CaseInsensitive)) {
        downloadModelButton->setValue(tr("Download cancelled..."));
      } else if (progress.contains("offline", Qt::CaseInsensitive)) {
        downloadModelButton->setValue(tr("GitHub and GitLab are offline..."));
      } else if (finished || progress.contains("failed", Qt::CaseInsensitive)) {
        downloadModelButton->setValue(tr("Download failed..."));
      } else if (progress == "Downloading...") {
        downloadModelButton->setValue(tr("Downloading..."));
      } else {
        downloadModelButton->setValue(progress);
      }

      if (finished) {
        finalizingDownload = true;

        QTimer::singleShot(2500, this, [this]() {
          allModelsDownloading = false;
          cancellingDownload = false;
          finalizingDownload = false;
          modelDownloading = false;

          refreshModels();
        });
      }
    }
  } else if (!downloading) {
    downloadModelButton->setValue(frogpilot_scene.online ? (parked ? "" : tr("Not parked")) : tr("Offline..."));
  }

  bool canCancel = !cancellingDownload && !finalizingDownload;
  bool canDownload = !downloading && downloadedModels.size() < availableModels.size() && frogpilot_scene.online && parked;

  deleteModelButton->setEnabled(!downloading && hasDeletableModels);

  downloadModelButton->setText(0, modelDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  downloadModelButton->setText(1, allModelsDownloading ? tr("CANCEL") : tr("DOWNLOAD ALL"));

  downloadModelButton->setEnabledButtons(0, modelDownloading ? canCancel : canDownload);
  downloadModelButton->setEnabledButtons(1, allModelsDownloading ? canCancel : canDownload);

  downloadModelButton->setVisibleButton(0, !allModelsDownloading);
  downloadModelButton->setVisibleButton(1, !modelDownloading);

  parent->keepScreenOn = downloading;
}

void FrogPilotModelPanel::updateModelLabels(FrogPilotListWidget *labelsList) {
  labelsList->clear();

  QJsonObject modelDrivesAndScores = QJsonDocument::fromJson(QString::fromStdString(params.get("ModelDrivesAndScores")).toUtf8()).object();

  QStringList models = availableModels.values();
  models.append(defaultModelName);
  models.sort();

  for (const QString &model : models) {
    QJsonObject modelData = modelDrivesAndScores.value(model).toObject();

    int drives = modelData.value("Drives").toInt(0);
    int score = modelData.value("Score").toInt(0);

    QString drivesDisplay = drives == 1 ? tr("%1 Drive").arg(drives) : drives > 0 ? tr("%1 Drives").arg(drives) : tr("N/A");
    QString scoreDisplay = drives > 0 ? tr("Score: %1%").arg(score) : tr("N/A");

    labelsList->addItem(new LabelControl(model, QString("%1 (%2)").arg(scoreDisplay, drivesDisplay), "", this));
  }
}

void FrogPilotModelPanel::updateToggles() {
  for (auto &[key, toggle] : toggles) {
    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels.value(key).toDouble();

    if (key == "ManageBlacklistedModels" || key == "ManageScores") {
      setVisible &= (parent->tuningLevel >= parent->frogpilotToggleLevels.value("ModelRandomizer").toDouble() && params.getBool("ModelRandomizer"));
    }

    else if (key == "SelectModel") {
      setVisible &= !(parent->tuningLevel >= parent->frogpilotToggleLevels.value("ModelRandomizer").toDouble() && params.getBool("ModelRandomizer"));
    }

    toggle->setVisible(setVisible);
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
