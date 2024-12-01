#include "selfdrive/frogpilot/ui/qt/offroad/model_settings.h"

FrogPilotModelPanel::FrogPilotModelPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> modelToggles {
    {"AutomaticallyUpdateModels", tr("Automatically Update and Download Models"), tr("Automatically downloads new models and updates existing ones if needed."), ""},

    {"ModelRandomizer", tr("Model Randomizer"), tr("Randomly selects a model each drive and brings up a model review prompt at the end to help find your preferred model."), ""},
    {"ManageBlacklistedModels", tr("Manage Model Blacklist"), tr("Manage the blacklisted models that aren't being used with 'Model Randomizer'."), ""},
    {"ResetScores", tr("Reset Model Scores"), tr("Clear the ratings you've given to the driving models."), ""},
    {"ReviewScores", tr("Review Model Scores"), tr("View the ratings you've assigned to the driving models."), ""},

    {"DeleteModel", tr("Delete Model"), tr("Delete driving models from your device."), ""},
    {"DownloadModel", tr("Download Model"), tr("Download new driving models."), ""},
    {"SelectModel", tr("Select Model"), tr("Select your preferred driving model."), ""},
  };

  for (const auto &[param, title, desc, icon] : modelToggles) {
    AbstractControl *modelToggle;

    if (param == "ModelRandomizer") {
      FrogPilotParamManageControl *modelRandomizerToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(modelRandomizerToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        modelRandomizerOpen = true;
        showToggles(modelRandomizerKeys);
        updateModelLabels();
      });
      modelToggle = modelRandomizerToggle;
    } else if (param == "ManageBlacklistedModels") {
      FrogPilotButtonsControl *blacklistBtn = new FrogPilotButtonsControl(title, desc, {tr("ADD"), tr("REMOVE"), tr("REMOVE ALL")});
      QObject::connect(blacklistBtn, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList blacklistedModels = QString::fromStdString(params.get("BlacklistedModels")).split(",");
        blacklistedModels.removeAll("");

        if (id == 0) {
          QStringList blacklistableModels;
          for (const QString &model : modelFileToNameMapProcessed.keys()) {
            if (!blacklistedModels.contains(model)) {
              blacklistableModels.append(modelFileToNameMapProcessed.value(model));
            }
          }

          if (blacklistableModels.size() <= 1) {
            FrogPilotConfirmationDialog::toggleAlert(tr("There are no more models to blacklist! The only available model is \"%1\"!").arg(blacklistableModels.first()), tr("OK"), this);
          } else {
            QString modelToBlacklist = MultiOptionDialog::getSelection(tr("Select a model to add to the blacklist"), blacklistableModels, "", this);
            if (!modelToBlacklist.isEmpty()) {
              if (ConfirmationDialog::confirm(tr("Are you sure you want to add the '%1' model to the blacklist?").arg(modelToBlacklist), tr("Add"), this)) {
                blacklistedModels.append(modelFileToNameMapProcessed.key(modelToBlacklist));
                params.put("BlacklistedModels", blacklistedModels.join(",").toStdString());
              }
            }
          }
        } else if (id == 1) {
          QStringList whitelistableModels;
          for (const QString &model : blacklistedModels) {
            QString modelName = modelFileToNameMapProcessed.value(model);
            if (!modelName.isEmpty()) {
              whitelistableModels.append(modelName);
            }
          }
          whitelistableModels.sort();

          QString modelToWhitelist = MultiOptionDialog::getSelection(tr("Select a model to remove from the blacklist"), whitelistableModels, "", this);
          if (!modelToWhitelist.isEmpty()) {
            if (ConfirmationDialog::confirm(tr("Are you sure you want to remove the '%1' model from the blacklist?").arg(modelToWhitelist), tr("Remove"), this)) {
              blacklistedModels.removeAll(modelFileToNameMapProcessed.key(modelToWhitelist));
              params.put("BlacklistedModels", blacklistedModels.join(",").toStdString());
            }
          }
        } else if (id == 2) {
          if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to remove all of your blacklisted models?"), this)) {
            params.remove("BlacklistedModels");
            params_storage.remove("BlacklistedModels");
          }
        }
      });
      modelToggle = blacklistBtn;
    } else if (param == "ResetScores") {
      ButtonControl *resetScoresBtn = new ButtonControl(title, tr("RESET"), desc);
      QObject::connect(resetScoresBtn, &ButtonControl::clicked, [this]() {
        if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to reset all of your model drives and scores?"), this)) {
          params.remove("ModelDrivesAndScores");
          params_storage.remove("ModelDrivesAndScores");
          updateModelLabels();
        }
      });
      modelToggle = resetScoresBtn;
    } else if (param == "ReviewScores") {
      ButtonControl *reviewScoresBtn = new ButtonControl(title, tr("REVIEW"), desc);
      QObject::connect(reviewScoresBtn, &ButtonControl::clicked, [this]() {
        openSubParentToggle();

        for (LabelControl *labels : labelControls) {
          labels->setVisible(true);
        }

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(false);
        }
      });
      modelToggle = reviewScoresBtn;

    } else if (param == "DeleteModel") {
      deleteModelBtn = new FrogPilotButtonsControl(title, desc, {tr("DELETE"), tr("DELETE ALL")});
      QObject::connect(deleteModelBtn, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList deletableModels;
        for (const QString &file : modelDir.entryList(QDir::Files)) {
          QString modelName = modelFileToNameMapProcessed.value(QFileInfo(file).baseName());
          if (!modelName.isEmpty()) {
            deletableModels.append(modelName);
          }
        }
        deletableModels.removeAll(processModelName(QString::fromStdString(params.get("ModelName"))));
        deletableModels.removeAll(processModelName(QString::fromStdString(params_default.get("ModelName"))));

        if (id == 0) {
          QString modelToDelete = MultiOptionDialog::getSelection(tr("Select a driving model to delete"), deletableModels, "", this);
          if (!modelToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' model?").arg(modelToDelete), tr("Delete"), this)) {
            QString modelFile = modelFileToNameMapProcessed.key(modelToDelete);
            for (const QString &file : modelDir.entryList(QDir::Files)) {
              if (QFileInfo(file).baseName() == modelFile) {
                QFile::remove(modelDir.filePath(file));
                break;
              }
            }
            allModelsDownloaded = false;
          }
        } else if (id == 1) {
          if (ConfirmationDialog::confirm(tr("Are you sure you want to delete all of your downloaded driving models?"), tr("Delete"), this)) {
            for (const QString &file : modelDir.entryList(QDir::Files)) {
              QString modelName = modelFileToNameMapProcessed.value(QFileInfo(file).baseName());
              if (deletableModels.contains(modelName)) {
                QFile::remove(modelDir.filePath(file));
              }
            }
            allModelsDownloaded = false;
            noModelsDownloaded = true;
          }
        }
      });
      modelToggle = deleteModelBtn;
    } else if (param == "DownloadModel") {
      downloadModelBtn = new FrogPilotButtonsControl(title, desc, {tr("DOWNLOAD"), tr("DOWNLOAD ALL")});
      QObject::connect(downloadModelBtn, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        if (id == 0) {
          if (modelDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelModelDownload", true);
          } else {
            QStringList downloadableModels = availableModelNames;
            for (const QString &file : modelDir.entryList(QDir::Files)) {
              downloadableModels.removeAll(modelFileToNameMap.value(QFileInfo(file).baseName()));
            }

            QString modelToDownload = MultiOptionDialog::getSelection(tr("Select a driving model to download"), downloadableModels, "", this);
            if (!modelToDownload.isEmpty()) {
              modelDownloading = true;

              params_memory.put("ModelToDownload", modelFileToNameMap.key(modelToDownload).toStdString());
              params_memory.put("ModelDownloadProgress", "Downloading...");

              downloadModelBtn->setValue("Downloading...");
            }
          }
        } else if (id == 1) {
          if (allModelsDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelModelDownload", true);
          } else {
            allModelsDownloading = true;

            params_memory.putBool("DownloadAllModels", true);
            params_memory.put("ModelDownloadProgress", "Downloading...");

            downloadModelBtn->setValue("Downloading...");
          }
        }
      });
      modelToggle = downloadModelBtn;
    } else if (param == "SelectModel") {
      selectModelBtn = new ButtonControl(title, tr("SELECT"), desc);
      QObject::connect(selectModelBtn, &ButtonControl::clicked, [this]() {
        QStringList selectableModels;
        for (const QString &file : modelDir.entryList(QDir::Files)) {
          QString modelName = modelFileToNameMap.value(QFileInfo(file).baseName());
          if (!modelName.isEmpty() && !modelName.contains("(Default)")) {
            selectableModels.append(modelName);
          }
        }
        selectableModels.prepend(QString::fromStdString(params_default.get("ModelName")));

        QString modelToSelect = MultiOptionDialog::getSelection(tr("Select a model - 🗺️ = Navigation | 📡 = Radar | 👀 = VOACC"), selectableModels, QString::fromStdString(params.get("ModelName")), this);
        if (!modelToSelect.isEmpty()) {
          params.put("Model", modelFileToNameMap.key(modelToSelect).toStdString());
          params.put("ModelName", modelToSelect.toStdString());

          if (started) {
            if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
              Hardware::reboot();
            }
          }
          selectModelBtn->setValue(modelToSelect);
        }
      });
      selectModelBtn->setValue(QString::fromStdString(params.get("ModelName")));
      modelToggle = selectModelBtn;

    } else {
      modelToggle = new ParamControl(param, title, desc, icon);
    }

    addItem(modelToggle);
    toggles[param] = modelToggle;

    if (FrogPilotParamManageControl *frogPilotManageToggle = qobject_cast<FrogPilotParamManageControl*>(modelToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotParamManageControl::manageButtonClicked, this, &FrogPilotModelPanel::openParentToggle);
    }

    QObject::connect(modelToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  QObject::connect(static_cast<ToggleControl*>(toggles["ModelRandomizer"]), &ToggleControl::toggleFlipped, [this](bool state) {
    if (state && !allModelsDownloaded) {
      if (FrogPilotConfirmationDialog::yesorno(tr("The 'Model Randomizer' only works with downloaded models. Do you want to download all the driving models?"), this)) {
        allModelsDownloading = true;

        params_memory.putBool("DownloadAllModels", true);
        params_memory.put("ModelDownloadProgress", "Downloading...");

        downloadModelBtn->setValue("Downloading...");
      }
    }
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, this, &FrogPilotModelPanel::hideToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubParentToggle, this, &FrogPilotModelPanel::hideSubToggles);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotModelPanel::updateState);
}

void FrogPilotModelPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  tuningLevel = parent->tuningLevel;

  availableModels = QString::fromStdString(params.get("AvailableModels")).split(",");
  availableModels.sort();
  availableModelNames = QString::fromStdString(params.get("AvailableModelNames")).split(",");
  availableModelNames.sort();

  int size = qMin(availableModels.size(), availableModelNames.size());
  for (int i = 0; i < size; ++i) {
    modelFileToNameMap.insert(availableModels[i], availableModelNames[i]);
    modelFileToNameMapProcessed.insert(availableModels[i], processModelName(availableModelNames[i]));
  }

  QStringList downloadableModels = availableModelNames;
  for (const QString &file : modelDir.entryList(QDir::Files)) {
    downloadableModels.removeAll(modelFileToNameMap.value(QFileInfo(file).baseName()));
  }
  allModelsDownloaded = downloadableModels.isEmpty();

  QStringList deletableModels;
  for (const QString &file : modelDir.entryList(QDir::Files)) {
    QString modelName = modelFileToNameMapProcessed.value(QFileInfo(file).baseName());
    if (!modelName.isEmpty()) {
      deletableModels.append(modelName);
    }
  }
  deletableModels.removeAll(processModelName(QString::fromStdString(params.get("ModelName"))));
  deletableModels.removeAll(processModelName(QString::fromStdString(params_default.get("ModelName"))));
  noModelsDownloaded = deletableModels.isEmpty();

  hideToggles();
}

void FrogPilotModelPanel::updateState(const UIState &s) {
  if (!isVisible() || finalizingDownload) return;

  if (allModelsDownloading || modelDownloading) {
    QString progress = QString::fromStdString(params_memory.get("ModelDownloadProgress"));
    bool downloadFailed = progress.contains(QRegularExpression("cancelled|exists|failed|offline", QRegularExpression::CaseInsensitiveOption));

    if (progress != "Downloading...") {
      downloadModelBtn->setValue(progress);
    }

    if (progress == "All models downloaded!" && allModelsDownloading || progress == "Downloaded!" && modelDownloading || downloadFailed) {
      finalizingDownload = true;

      QTimer::singleShot(2500, [this, progress]() {
        allModelsDownloaded = progress == "All models downloaded!";
        allModelsDownloading = false;
        cancellingDownload = false;
        finalizingDownload = false;
        modelDownloading = false;
        noModelsDownloaded = false;

        params_memory.remove("CancelModelDownload");
        params_memory.remove("DownloadAllModels");
        params_memory.remove("ModelDownloadProgress");
        params_memory.remove("ModelToDownload");

        downloadModelBtn->setEnabled(true);
        downloadModelBtn->setValue("");
      });
    }
  }

  bool parked = !started || s.scene.parked || s.scene.frogs_go_moo;

  deleteModelBtn->setEnabled(!(allModelsDownloading || modelDownloading || noModelsDownloaded));

  downloadModelBtn->setText(0, modelDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  downloadModelBtn->setText(1, allModelsDownloading ? tr("CANCEL") : tr("DOWNLOAD ALL"));

  downloadModelBtn->setEnabledButtons(0, !allModelsDownloaded && !allModelsDownloading && !cancellingDownload && s.scene.online && parked);
  downloadModelBtn->setEnabledButtons(1, !allModelsDownloaded && !modelDownloading && !cancellingDownload && s.scene.online && parked);

  downloadModelBtn->setVisibleButton(0, !allModelsDownloading);
  downloadModelBtn->setVisibleButton(1, !modelDownloading);

  started = s.scene.started;
}

void FrogPilotModelPanel::updateModelLabels() {
  QString modelDrivesAndScoresJson = QString::fromStdString(params.get("ModelDrivesAndScores"));
  QJsonDocument jsonDoc = QJsonDocument::fromJson(modelDrivesAndScoresJson.toUtf8());
  QJsonObject modelDrivesAndScores = jsonDoc.object();

  qDeleteAll(labelControls);
  labelControls.clear();

  for (const QString &modelName : availableModelNames) {
    QJsonObject modelData = modelDrivesAndScores.value(processModelName(modelName)).toObject();

    int drives = modelData.value("Drives").toInt(0);
    int score = modelData.value("Score").toInt(0);

    QString drivesDisplay = drives == 1 ? QString("%1 Drive") : drives > 0 ? QString("%1 Drives").arg(drives) : "N/A";
    QString scoreDisplay = score > 0 ? QString("Score: %1%").arg(score) : "N/A";

    QString labelTitle = QStringLiteral("%1").arg(processModelName(modelName));
    QString labelText = QStringLiteral("%1 (%2)").arg(scoreDisplay, drivesDisplay);

    LabelControl *labelControl = new LabelControl(labelTitle, labelText, "", this);
    labelControls.append(labelControl);
    addItem(labelControl);
  }

  for (LabelControl *labels : labelControls) {
    labels->setVisible(false);
  }
}

void FrogPilotModelPanel::showToggles(const std::set<QString> &keys) {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(keys.find(key) != keys.end() && tuningLevel >= frogpilotToggleLevels[key].toDouble());
  }

  setUpdatesEnabled(true);
  update();
}

void FrogPilotModelPanel::hideToggles() {
  setUpdatesEnabled(false);

  modelRandomizerOpen = false;

  for (auto &[key, toggle] : toggles) {
    bool subToggles = modelRandomizerKeys.find(key) != modelRandomizerKeys.end();

    toggle->setVisible(!subToggles && tuningLevel >= frogpilotToggleLevels[key].toDouble());
  }

  for (LabelControl *labels : labelControls) {
    labels->setVisible(false);
  }

  setUpdatesEnabled(true);
  update();
}

void FrogPilotModelPanel::hideSubToggles() {
  setUpdatesEnabled(false);

  if (modelRandomizerOpen) {
    for (auto &[key, toggle] : toggles) {
      toggle->setVisible(modelRandomizerKeys.find(key) != modelRandomizerKeys.end());
    }

    for (LabelControl *labels : labelControls) {
      labels->setVisible(false);
    }
  }

  setUpdatesEnabled(true);
  update();
}
