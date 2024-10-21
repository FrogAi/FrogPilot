#include "selfdrive/frogpilot/ui/qt/offroad/model_settings.h"

FrogPilotModelPanel::FrogPilotModelPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> modelToggles {
    {"AutomaticallyUpdateModels", tr("Automatically Update and Download Models"), tr("Automatically downloads new models and updates them if needed."), ""},

    {"ModelRandomizer", tr("Model Randomizer"), tr("Randomly selects a model each drive and brings up a prompt at the end of the drive to review the model if it's longer than 15 minutes to help find your preferred model."), ""},
    {"ManageBlacklistedModels", tr("Manage Model Blacklist"), tr("Controls which models are blacklisted and won't be used for future drives."), ""},
    {"ResetScores", tr("Reset Model Scores"), tr("Clears the ratings you've given to the driving models."), ""},
    {"ReviewScores", tr("Review Model Scores"), tr("Displays the ratings you've assigned to the driving models."), ""},

    {"DeleteModel", tr("Delete Model"), tr("Removes the selected driving model from your device."), ""},
    {"DownloadModel", tr("Download Model"), tr("Downloads the selected driving model."), ""},
    {"DownloadAllModels", tr("Download All Models"), tr("Downloads all undownloaded driving models."), ""},
    {"SelectModel", tr("Select Model"), tr("Selects which model openpilot uses to drive."), ""},
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
      FrogPilotButtonsControl *blacklistBtn = new FrogPilotButtonsControl(title, desc, {tr("ADD"), tr("REMOVE")});
      QObject::connect(blacklistBtn, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList blacklistedModels = QString::fromStdString(params.get("BlacklistedModels")).split(",", QString::SkipEmptyParts);
        QStringList selectableModels = availableModelNames;

        for (QString &model : blacklistedModels) {
          selectableModels.removeAll(model);
          if (model.contains("(Default)")) {
            blacklistedModels.move(blacklistedModels.indexOf(model), 0);
          }
        }

        if (id == 0) {
          if (selectableModels.size() == 1) {
            FrogPilotConfirmationDialog::toggleAlert(tr("There's no more models to blacklist! The only available model is \"%1\"!").arg(selectableModels.first()), tr("OK"), this);
          } else {
            QString selectedModel = MultiOptionDialog::getSelection(tr("Select a model to add to the blacklist"), selectableModels, "", this);
            if (!selectedModel.isEmpty()) {
              if (ConfirmationDialog::confirm(tr("Are you sure you want to add the '%1' model to the blacklist?").arg(selectedModel), tr("Add"), this)) {
                if (!blacklistedModels.contains(selectedModel)) {
                  blacklistedModels.append(selectedModel);
                  params.putNonBlocking("BlacklistedModels", blacklistedModels.join(",").toStdString());
                  paramsStorage.putNonBlocking("BlacklistedModels", blacklistedModels.join(",").toStdString());
                }
              }
            }
          }
        } else if (id == 1) {
          QString selectedModel = MultiOptionDialog::getSelection(tr("Select a model to remove from the blacklist"), blacklistedModels, "", this);
          if (!selectedModel.isEmpty()) {
            if (ConfirmationDialog::confirm(tr("Are you sure you want to remove the '%1' model from the blacklist?").arg(selectedModel), tr("Remove"), this)) {
              if (blacklistedModels.contains(selectedModel)) {
                blacklistedModels.removeAll(selectedModel);
                params.putNonBlocking("BlacklistedModels", blacklistedModels.join(",").toStdString());
                paramsStorage.putNonBlocking("BlacklistedModels", blacklistedModels.join(",").toStdString());
              }
            }
          }
        }
      });
      modelToggle = blacklistBtn;
    } else if (param == "ResetScores") {
      ButtonControl *resetScoresBtn = new ButtonControl(title, tr("RESET"), desc);
      QObject::connect(resetScoresBtn, &ButtonControl::clicked, [this]() {
        if (FrogPilotConfirmationDialog::yesorno(tr("Reset all model scores?"), this)) {
          for (const QString &model : availableModelNames) {
            QString cleanedModel = processModelName(model);
            params.remove(QString("%1Drives").arg(cleanedModel).toStdString());
            paramsStorage.remove(QString("%1Drives").arg(cleanedModel).toStdString());
            params.remove(QString("%1Score").arg(cleanedModel).toStdString());
            paramsStorage.remove(QString("%1Score").arg(cleanedModel).toStdString());
          }
          updateModelLabels();
        }
      });
      modelToggle = reinterpret_cast<AbstractControl*>(resetScoresBtn);
    } else if (param == "ReviewScores") {
      ButtonControl *reviewScoresBtn = new ButtonControl(title, tr("VIEW"), desc);
      QObject::connect(reviewScoresBtn, &ButtonControl::clicked, [this]() {
        openSubParentToggle();

        for (LabelControl *label : labelControls) {
          label->setVisible(true);
        }

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(false);
        }
      });
      modelToggle = reinterpret_cast<AbstractControl*>(reviewScoresBtn);
    } else if (param == "DeleteModel") {
      deleteModelBtn = new ButtonControl(title, tr("DELETE"), desc);
      QObject::connect(deleteModelBtn, &ButtonControl::clicked, [this]() {
        QStringList deletableModels, existingModels = modelDir.entryList({"*.thneed"}, QDir::Files);
        QMap<QString, QString> labelToFileMap;
        QString currentModel = QString::fromStdString(params.get("Model")) + ".thneed";

        for (int i = 0; i < availableModels.size(); ++i) {
          QString modelFile = availableModels[i] + ".thneed";
          if (existingModels.contains(modelFile) && modelFile != currentModel && !availableModelNames[i].contains("(Default)")) {
            deletableModels.append(availableModelNames[i]);
            labelToFileMap[availableModelNames[i]] = modelFile;
          }
        }

        QString selectedModel = MultiOptionDialog::getSelection(tr("Select a model to delete"), deletableModels, "", this);
        if (!selectedModel.isEmpty()) {
          if (ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' model?").arg(selectedModel), tr("Delete"), this)) {
            std::thread([=]() {
              modelDeleting = true;
              modelsDownloaded = false;
              update();

              params.putBoolNonBlocking("ModelsDownloaded", false);
              deleteModelBtn->setValue(tr("Deleting..."));

              QFile::remove(modelDir.absoluteFilePath(labelToFileMap[selectedModel]));
              deleteModelBtn->setValue(tr("Deleted!"));

              util::sleep_for(1000);
              deleteModelBtn->setValue("");
              modelDeleting = false;

              QStringList modelFiles = modelDir.entryList({"*.thneed"}, QDir::Files);
              modelFiles.removeAll(currentModel);

              haveModelsDownloaded = modelFiles.size() > 1;
              update();
            }).detach();
          }
        }
      });
      modelToggle = reinterpret_cast<AbstractControl*>(deleteModelBtn);
    } else if (param == "DownloadModel") {
      downloadModelBtn = new ButtonControl(title, tr("DOWNLOAD"), desc);
      QObject::connect(downloadModelBtn, &ButtonControl::clicked, [this]() {
        if (downloadModelBtn->text() == tr("CANCEL")) {
          paramsMemory.remove("ModelToDownload");
          paramsMemory.putBool("CancelModelDownload", true);
          cancellingDownload = true;
        } else {
          QMap<QString, QString> labelToModelMap;
          QStringList existingModels = modelDir.entryList({"*.thneed"}, QDir::Files);
          QStringList downloadableModels;

          for (int i = 0; i < availableModels.size(); ++i) {
            QString modelFile = availableModels[i] + ".thneed";
            if (!existingModels.contains(modelFile) && !availableModelNames[i].contains("(Default)")) {
              downloadableModels.append(availableModelNames[i]);
              labelToModelMap.insert(availableModelNames[i], availableModels[i]);
            }
          }

          QString modelToDownload = MultiOptionDialog::getSelection(tr("Select a driving model to download"), downloadableModels, "", this);
          if (!modelToDownload.isEmpty()) {
            modelDownloading = true;
            paramsMemory.put("ModelToDownload", labelToModelMap.value(modelToDownload).toStdString());
            paramsMemory.put("ModelDownloadProgress", "0%");

            downloadModelBtn->setValue(tr("Downloading %1...").arg(modelToDownload.remove(QRegularExpression("[🗺️👀📡]")).trimmed()));

            QTimer *progressTimer = new QTimer(this);
            progressTimer->setInterval(100);

            QObject::connect(progressTimer, &QTimer::timeout, this, [=]() {
              QString progress = QString::fromStdString(paramsMemory.get("ModelDownloadProgress"));
              bool downloadComplete = progress.contains(QRegularExpression("downloaded", QRegularExpression::CaseInsensitiveOption));
              bool downloadFailed = progress.contains(QRegularExpression("cancelled|exists|failed|offline", QRegularExpression::CaseInsensitiveOption));

              if (!progress.isEmpty() && progress != "0%") {
                downloadModelBtn->setValue(progress);
              }

              if (downloadComplete || downloadFailed) {
                bool lastModelDownloaded = downloadComplete;

                if (downloadComplete) {
                  haveModelsDownloaded = true;
                  update();
                }

                if (downloadComplete) {
                  for (const QString &model : availableModels) {
                    if (!QFile::exists(modelDir.filePath(model + ".thneed"))) {
                      lastModelDownloaded = false;
                      break;
                    }
                  }
                }

                downloadModelBtn->setValue(progress);

                paramsMemory.remove("CancelModelDownload");
                paramsMemory.remove("ModelDownloadProgress");

                progressTimer->stop();
                progressTimer->deleteLater();

                QTimer::singleShot(2000, this, [=]() {
                  cancellingDownload = false;
                  modelDownloading = false;

                  downloadModelBtn->setValue("");

                  if (lastModelDownloaded) {
                    modelsDownloaded = true;
                    update();

                    params.putBoolNonBlocking("ModelsDownloaded", modelsDownloaded);
                  }
                });
              }
            });
            progressTimer->start();
          }
        }
      });
      modelToggle = reinterpret_cast<AbstractControl*>(downloadModelBtn);
    } else if (param == "DownloadAllModels") {
      downloadAllModelsBtn = new ButtonControl(title, tr("DOWNLOAD"), desc);
      QObject::connect(downloadAllModelsBtn, &ButtonControl::clicked, [this]() {
        if (downloadAllModelsBtn->text() == tr("CANCEL")) {
          paramsMemory.remove("DownloadAllModels");
          paramsMemory.putBool("CancelModelDownload", true);
          cancellingDownload = true;
        } else {
          startDownloadAllModels();
        }
      });
      modelToggle = reinterpret_cast<AbstractControl*>(downloadAllModelsBtn);
    } else if (param == "SelectModel") {
      selectModelBtn = new ButtonControl(title, tr("SELECT"), desc);
      QObject::connect(selectModelBtn, &ButtonControl::clicked, [this]() {
        QSet<QString> modelFilesBaseNames = QSet<QString>::fromList(modelDir.entryList({"*.thneed"}, QDir::Files).replaceInStrings(QRegExp("\\.thneed$"), ""));
        QStringList selectableModels;

        for (int i = 0; i < availableModels.size(); ++i) {
          if (modelFilesBaseNames.contains(availableModels[i]) || availableModelNames[i].contains("(Default)")) {
            selectableModels.append(availableModelNames[i]);
          }
        }

        QString modelToSelect = MultiOptionDialog::getSelection(tr("Select a model - 🗺️ = Navigation | 📡 = Radar | 👀 = VOACC"), selectableModels, "", this);
        if (!modelToSelect.isEmpty()) {
          selectModelBtn->setValue(modelToSelect);
          int modelIndex = availableModelNames.indexOf(modelToSelect);

          params.put("Model", availableModels.at(modelIndex).toStdString());
          params.put("ModelName", modelToSelect.toStdString());

          if (experimentalModels.contains(availableModels.at(modelIndex))) {
            FrogPilotConfirmationDialog::toggleAlert(tr("WARNING: This is a very experimental model and may drive dangerously!"), tr("I understand the risks."), this);
          }

          if (started) {
            if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
              Hardware::reboot();
            }
          }

          updateFrogPilotToggles();
        }
      });
      selectModelBtn->setValue(QString::fromStdString(params.get("ModelName")));
      modelToggle = reinterpret_cast<AbstractControl*>(selectModelBtn);

    } else {
      modelToggle = new ParamControl(param, title, desc, icon);
    }

    addItem(modelToggle);
    toggles[param] = modelToggle;

    makeConnections(modelToggle);

    if (FrogPilotParamManageControl *frogPilotManageToggle = qobject_cast<FrogPilotParamManageControl*>(modelToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotParamManageControl::manageButtonClicked, this, &FrogPilotModelPanel::openParentToggle);
    }

    QObject::connect(modelToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  QObject::connect(static_cast<ToggleControl*>(toggles["ModelRandomizer"]), &ToggleControl::toggleFlipped, [this](bool state) {
    modelRandomizer = state;
    if (state && !modelsDownloaded) {
      if (FrogPilotConfirmationDialog::yesorno(tr("The 'Model Randomizer' only works with downloaded models. Do you want to download all the driving models?"), this)) {
        startDownloadAllModels();
      }
    }
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, this, &FrogPilotModelPanel::hideToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubParentToggle, this, &FrogPilotModelPanel::hideSubToggles);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotModelPanel::updateState);
}

void FrogPilotModelPanel::showEvent(QShowEvent *event) {
  QString currentModel = QString::fromStdString(params.get("Model")) + ".thneed";

  availableModelNames = QString::fromStdString(params.get("AvailableModelsNames")).split(",");
  availableModels = QString::fromStdString(params.get("AvailableModels")).split(",");
  experimentalModels = QString::fromStdString(params.get("ExperimentalModels")).split(",");

  modelRandomizer = params.getBool("ModelRandomizer");
  modelsDownloaded = params.getBool("ModelsDownloaded");

  QStringList modelFiles = modelDir.entryList({"*.thneed"}, QDir::Files);
  modelFiles.removeAll(currentModel);
  haveModelsDownloaded = modelFiles.size() > 1;
}

void FrogPilotModelPanel::updateState(const UIState &s) {
  if (!isVisible()) return;

  uiState()->scene.keep_screen_on = modelDownloading;

  downloadAllModelsBtn->setText(modelDownloading && allModelsDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  downloadModelBtn->setText(modelDownloading && !allModelsDownloading ? tr("CANCEL") : tr("DOWNLOAD"));

  deleteModelBtn->setEnabled(!modelDeleting && !modelDownloading);
  downloadAllModelsBtn->setEnabled(s.scene.online && !cancellingDownload && !modelDeleting && (!modelDownloading || allModelsDownloading) && !modelsDownloaded);
  downloadModelBtn->setEnabled(s.scene.online && !cancellingDownload && !modelDeleting && !allModelsDownloading && !modelsDownloaded);
  selectModelBtn->setEnabled(!modelDeleting && !modelDownloading && !modelRandomizer);

  started = s.scene.started;
}

void FrogPilotModelPanel::startDownloadAllModels() {
  allModelsDownloading = true;
  modelDownloading = true;

  paramsMemory.putBool("DownloadAllModels", true);

  downloadAllModelsBtn->setValue(tr("Downloading models..."));

  QTimer *progressTimer = new QTimer(this);
  progressTimer->setInterval(100);

  QObject::connect(progressTimer, &QTimer::timeout, this, [=]() {
    QString progress = QString::fromStdString(paramsMemory.get("ModelDownloadProgress"));
    bool downloadComplete = progress.contains(QRegularExpression("All models downloaded!", QRegularExpression::CaseInsensitiveOption));
    bool downloadFailed = progress.contains(QRegularExpression("cancelled|exists|failed|offline", QRegularExpression::CaseInsensitiveOption));

    if (!progress.isEmpty() && progress != "0%") {
      downloadAllModelsBtn->setValue(progress);
    }

    if (downloadComplete || downloadFailed) {
      if (downloadComplete) {
        haveModelsDownloaded = true;
        update();
      }

      downloadAllModelsBtn->setValue(progress);

      paramsMemory.remove("CancelModelDownload");
      paramsMemory.remove("ModelDownloadProgress");

      progressTimer->stop();
      progressTimer->deleteLater();

      QTimer::singleShot(2000, this, [=]() {
        cancellingDownload = false;
        modelDownloading = false;

        paramsMemory.remove("DownloadAllModels");

        downloadAllModelsBtn->setValue("");
      });
    }
  });
  progressTimer->start();
}

void FrogPilotModelPanel::updateModelLabels() {
  QVector<QPair<QString, int>> modelScores;
  for (QString &model : availableModelNames) {
    QString cleanedModel = processModelName(model);
    int score = params.getInt((cleanedModel + "Score").toStdString());

    if (model.contains("(Default)")) {
      modelScores.prepend(qMakePair(model, score));
    } else {
      modelScores.append(qMakePair(model, score));
    }
  }

  labelControls.clear();

  for (QPair<QString, int> &pair : modelScores) {
    QString scoreDisplay = pair.second == 0 ? "N/A" : QString::number(pair.second) + "%";
    LabelControl *labelControl = new LabelControl(pair.first, scoreDisplay, "");
    labelControls.append(labelControl);
    addItem(labelControl);
  }

  for (LabelControl *label : labelControls) {
    label->setVisible(false);
  }
}

void FrogPilotModelPanel::showToggles(const std::set<QString> &keys) {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(keys.find(key) != keys.end());
  }

  setUpdatesEnabled(true);
  update();
}

void FrogPilotModelPanel::hideToggles() {
  setUpdatesEnabled(false);

  modelRandomizerOpen = false;

  for (LabelControl *label : labelControls) {
    label->setVisible(false);
  }

  for (auto &[key, toggle] : toggles) {
    bool subToggles = modelRandomizerKeys.find(key) != modelRandomizerKeys.end();

    toggle->setVisible(!subToggles);
  }

  setUpdatesEnabled(true);
  update();
}

void FrogPilotModelPanel::hideSubToggles() {
  setUpdatesEnabled(false);

  if (modelRandomizerOpen) {
    for (LabelControl *label : labelControls) {
      label->setVisible(false);
    }

    for (auto &[key, toggle] : toggles) {
      toggle->setVisible(modelRandomizerKeys.find(key) != modelRandomizerKeys.end());
    }
  }

  setUpdatesEnabled(true);
  update();
}
