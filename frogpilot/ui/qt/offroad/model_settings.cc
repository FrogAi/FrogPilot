#include "frogpilot/ui/qt/offroad/model_settings.h"

const QStringList tinygradSuffixes = {
  "_driving_policy_metadata.pkl",
  "_driving_policy_tinygrad.pkl",
  "_driving_vision_metadata.pkl",
  "_driving_vision_tinygrad.pkl"
};

bool hasAllTinygradFiles(const QDir &modelDir, const QString &modelKey) {
  for (const QString &suffix : tinygradSuffixes) {
    if (!modelDir.exists(modelKey + suffix)) {
      return false;
    }
  }
  return true;
}

bool modelFileMatches(const QString &file, const QString &modelKey) {
  if (file == modelKey + ".thneed") {
    return true;
  }

  for (const QString &suffix : tinygradSuffixes) {
    if (file == modelKey + suffix) {
      return true;
    }
  }
  return false;
}

QString normalizeModelKey(QString key) {
  key = key.toLower();
  if (key.endsWith("_default")) {
    key.chop(QString("_default").size());
  }
  return key;
}

FrogPilotModelPanel::FrogPilotModelPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

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
    {"DeleteModel", tr("Delete Driving Models"), tr("<b>Remove driving models you have downloaded to free up storage.</b><br><br>\"DELETE\" picks one, \"DELETE ALL\" removes the rest. The model you are currently using and the one FrogPilot ships with are always kept. Turn \"Automatically Download New Models\" off first, or anything you delete is downloaded again within the hour."), ""},
    {"DownloadModel", tr("Download Driving Models"), tr("<b>Download driving models onto the device so you can switch to them.</b><br><br>\"DOWNLOAD\" picks one, \"DOWNLOAD ALL\" fetches everything. Your car has to be parked and online, and models are large, so this can take a while."), ""},
    {"ModelRandomizer", tr("Model Randomizer"), tr("<b>Picks a different driving model for you at the start of every drive, then asks how it went when you park, so you can work out which one you like best.</b><br><br>It only chooses from models you have downloaded and have not blacklisted, and it only asks for a rating after drives longer than 15 minutes. Your ratings are saved under \"Manage Model Ratings\" for you to compare."), ""},
    {"ManageBlacklistedModels", tr("Manage Model Blacklist"), tr("<b>Stop the \"Model Randomizer\" from picking driving models you did not get on with.</b><br><br>Blocking a model here has no effect on choosing it yourself under \"Select Driving Model\"."), ""},
    {"ManageScores", tr("Manage Model Ratings"), tr("<b>See how you rated each driving model and how many drives you gave it, or wipe those ratings and start fresh.</b><br><br>These are for your own comparison. The \"Model Randomizer\" picks at random and does not favour your higher-rated models."), ""},
    {"SelectModel", tr("Select Driving Model"), tr("<b>Choose which driving model does the driving.</b><br><br>The model is the part of openpilot that decides how to steer, speed up, and slow down, so switching it changes how the car feels. Only models you have downloaded are listed, and changing it while driving asks you to reboot."), ""}
  };

  for (const auto &[param, title, desc, icon] : modelToggles) {
    AbstractControl *modelToggle;

    if (param == "DeleteModel") {
      deleteModelButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DELETE ALL")});
      QObject::connect(deleteModelButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QMap<QString, QString> deletableModels = getDeletableModels();
        noModelsDownloaded = deletableModels.isEmpty();

        if (id == 0) {
          QString modelToDelete = MultiOptionDialog::getSelection(tr("Select a driving model to delete"), deletableModels.values(), "", this);
          if (!modelToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the \"%1\" model?").arg(modelToDelete), tr("Delete"), this)) {
            QString modelFile = deletableModels.key(modelToDelete);
            for (const QString &file : modelDir.entryList(QDir::Files)) {
              if (modelFileMatches(file, modelFile)) {
                QFile::remove(modelDir.filePath(file));
              }
            }

            allModelsDownloaded = false;
            deletableModels.remove(modelFile);
            noModelsDownloaded = deletableModels.isEmpty();
            deleteModelButton->setButtonsEnabled(!(allModelsDownloading || modelDownloading || noModelsDownloaded));
          }
        } else if (id == 1) {
          if (ConfirmationDialog::confirm(tr("Delete every downloaded driving model except the one you are using and the one FrogPilot ships with?"), tr("Delete"), this)) {
            for (const QString &file : modelDir.entryList(QDir::Files)) {
              for (const QString &modelKey : deletableModels.keys()) {
                if (modelFileMatches(file, modelKey)) {
                  QFile::remove(modelDir.filePath(file));
                  break;
                }
              }
            }

            allModelsDownloaded = false;
            noModelsDownloaded = true;
          }
        }
      });
      modelToggle = deleteModelButton;
    } else if (param == "DownloadModel") {
      downloadModelButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DOWNLOAD"), tr("DOWNLOAD ALL")});
      QObject::connect(downloadModelButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        if (id == 0) {
          if (modelDownloading) {
            params_memory.putBool("CancelModelDownload", true);

            cancellingDownload = true;
          } else {
            QStringList downloadableModels = availableModelNames;
            for (const QString &modelKey : modelFileToNameMap.keys()) {
              QString modelName = modelFileToNameMap.value(modelKey);
              if (modelDir.exists(modelKey + ".thneed") || hasAllTinygradFiles(modelDir, modelKey)) {
                downloadableModels.removeAll(modelName);
              }
            }
            allModelsDownloaded = downloadableModels.isEmpty();

            QString modelToDownload = MultiOptionDialog::getSelection(tr("Select a driving model to download"), downloadableModels, "", this);
            if (!modelToDownload.isEmpty()) {
              params_memory.put("ModelToDownload", modelFileToNameMap.key(modelToDownload).toStdString());
              params_memory.put("ModelDownloadProgress", "Downloading...");

              downloadModelButton->setText(0, tr("CANCEL"));

              downloadModelButton->setValue(tr("Downloading..."));

              downloadModelButton->setVisibleButton(1, false);

              modelDownloading = true;
            }
          }
        } else if (id == 1) {
          if (allModelsDownloading) {
            params_memory.putBool("CancelModelDownload", true);

            cancellingDownload = true;
          } else {
            params_memory.putBool("DownloadAllModels", true);
            params_memory.put("ModelDownloadProgress", "Downloading...");

            downloadModelButton->setText(1, tr("CANCEL"));

            downloadModelButton->setValue(tr("Downloading..."));

            downloadModelButton->setVisibleButton(0, false);

            allModelsDownloading = true;
          }
        }
      });
      modelToggle = downloadModelButton;
    } else if (param == "ManageBlacklistedModels") {
      FrogPilotButtonsControl *blacklistButton = new FrogPilotButtonsControl(title, desc, icon, {tr("ADD"), tr("REMOVE"), tr("REMOVE ALL")});
      QObject::connect(blacklistButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList blacklistedModels = QString::fromStdString(params.get("BlacklistedModels")).split(",");
        blacklistedModels.removeAll("");

        if (id == 0) {
          QStringList blacklistableModels;
          for (const QString &model : modelFileToNameMapProcessed.keys()) {
            if (!blacklistedModels.contains(model) && (modelDir.exists(model + ".thneed") || hasAllTinygradFiles(modelDir, model))) {
              blacklistableModels.append(modelFileToNameMapProcessed.value(model));
            }
          }

          if (blacklistableModels.isEmpty()) {
            ConfirmationDialog::alert(tr("There are no driving models available to blacklist."), this);
          } else if (blacklistableModels.size() == 1) {
            ConfirmationDialog::alert(tr("There are no more driving models to blacklist. The only available model is \"%1\"!").arg(blacklistableModels.first()), this);
          } else {
            QString modelToBlacklist = MultiOptionDialog::getSelection(tr("Select a driving model to add to the blacklist"), blacklistableModels, "", this);
            if (!modelToBlacklist.isEmpty()) {
              if (ConfirmationDialog::confirm(tr("Are you sure you want to add the \"%1\" model to the blacklist?").arg(modelToBlacklist), tr("Add"), this)) {
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

          if (whitelistableModels.isEmpty()) {
            ConfirmationDialog::alert(tr("You have not blocked any driving models."), this);
            return;
          }

          QString modelToWhitelist = MultiOptionDialog::getSelection(tr("Select a driving model to remove from the blacklist"), whitelistableModels, "", this);
          if (!modelToWhitelist.isEmpty()) {
            if (ConfirmationDialog::confirm(tr("Are you sure you want to remove the \"%1\" model from the blacklist?").arg(modelToWhitelist), tr("Remove"), this)) {
              blacklistedModels.removeAll(modelFileToNameMapProcessed.key(modelToWhitelist));

              params.put("BlacklistedModels", blacklistedModels.join(",").toStdString());
            }
          }
        } else if (id == 2) {
          if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to remove all of your blacklisted driving models?"), this)) {
            params.remove("BlacklistedModels");
            params_cache.remove("BlacklistedModels");
          }
        }
      });
      modelToggle = blacklistButton;
    } else if (param == "ManageScores") {
      FrogPilotButtonsControl *manageScoresButton = new FrogPilotButtonsControl(title, desc, icon, {tr("RESET"), tr("VIEW")});
      QObject::connect(manageScoresButton, &FrogPilotButtonsControl::buttonClicked, [modelLayout, modelLabelsList, modelLabelsPanel, this](int id) {
        if (id == 0) {
          if (FrogPilotConfirmationDialog::yesorno(tr("Reset how many drives and what rating each driving model has? Your drives themselves are not touched."), this)) {
            params.remove("ModelDrivesAndScores");
            params_cache.remove("ModelDrivesAndScores");
          }
        } else if (id == 1) {
          openSubPanel();

          updateModelLabels(modelLabelsList);

          modelLayout->setCurrentWidget(modelLabelsPanel);
        }
      });
      modelToggle = manageScoresButton;
    } else if (param == "SelectModel") {
      selectModelButton = new ButtonControl(title, tr("SELECT"), desc);
      QObject::connect(selectModelButton, &ButtonControl::clicked, [this]() {
        QStringList selectableModels;
        for (const QString &modelKey : modelFileToNameMap.keys()) {
          QString modelName = modelFileToNameMap.value(modelKey);
          if (modelName.contains("(Default)")) {
            continue;
          }

          if (modelDir.exists(modelKey + ".thneed") || hasAllTinygradFiles(modelDir, modelKey)) {
            selectableModels.append(modelName);
          }
        }
        selectableModels.sort();
        selectableModels.prepend(modelFileToNameMap.value(normalizeModelKey(QString::fromStdString(params_default.get("Model")))));

        QString modelToSelect = MultiOptionDialog::getSelection(tr("Select a Model 🗺️ = Navigation | 📡 = Radar | 👀 = VOACC"), selectableModels, currentModel, this);
        if (!modelToSelect.isEmpty()) {
          currentModel = modelToSelect;

          params.put("Model", modelFileToNameMap.key(modelToSelect).toStdString());

          updateFrogPilotToggles();

          if (uiState()->scene.started) {
            if (FrogPilotConfirmationDialog::toggleReboot(this)) {
              Hardware::reboot();
            }
          }
          selectModelButton->setValue(modelToSelect);

          QMap<QString, QString> deletableModels = getDeletableModels();
          noModelsDownloaded = deletableModels.isEmpty();
        }
      });
      modelToggle = selectModelButton;

    } else {
      modelToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = modelToggle;

    modelList->addItem(modelToggle);

    QObject::connect(modelToggle, &AbstractControl::hideDescriptionEvent, [this]() {
      update();
    });
    QObject::connect(modelToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  openDescriptions(forceOpenDescriptions, toggles);

  QObject::connect(static_cast<ToggleControl*>(toggles["ModelRandomizer"]), &ToggleControl::toggleFlipped, [this](bool state) {
    updateToggles();

    if (state && !allModelsDownloaded && !allModelsDownloading && !modelDownloading) {
      FrogPilotUIState &fs = *frogpilotUIState();
      bool parked = !uiState()->scene.started || fs.frogpilot_scene.parked || fs.frogpilot_toggles.value("frogs_go_moo").toBool();

      if (!fs.frogpilot_scene.online || !parked) {
        ConfirmationDialog::alert(tr("The \"Model Randomizer\" only picks from models you have downloaded. Park your car and connect to the internet to download them."), this);
      } else if (FrogPilotConfirmationDialog::yesorno(tr("The \"Model Randomizer\" only picks from models you have downloaded. Download every model now?"), this)) {
        params_memory.putBool("DownloadAllModels", true);
        params_memory.put("ModelDownloadProgress", "Downloading...");

        downloadModelButton->setValue(tr("Downloading..."));

        allModelsDownloading = true;
      }
    }
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [modelLayout, modelPanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    modelLayout->setCurrentWidget(modelPanel);
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotModelPanel::updateState);
}

void FrogPilotModelPanel::showEvent(QShowEvent *event) {
  FrogPilotUIState &fs = *frogpilotUIState();
  UIState &s = *uiState();

  allModelsDownloading = params_memory.getBool("DownloadAllModels");
  modelDownloading = !allModelsDownloading && !params_memory.get("ModelDownloadProgress").empty();

  QStringList availableModels = QString::fromStdString(params.get("AvailableModels")).split(",");
  availableModelNames = QString::fromStdString(params.get("AvailableModelNames")).split(",");

  modelFileToNameMap.clear();
  modelFileToNameMapProcessed.clear();
  for (int i = 0; i < qMin(availableModels.size(), availableModelNames.size()); ++i) {
    modelFileToNameMap.insert(availableModels[i], availableModelNames[i]);
    modelFileToNameMapProcessed.insert(availableModels[i], processModelName(availableModelNames[i]));
  }
  availableModelNames = modelFileToNameMap.values();
  availableModelNames.sort();

  QStringList downloadableModels = availableModelNames;
  for (const QString &modelKey : modelFileToNameMap.keys()) {
    QString modelName = modelFileToNameMap.value(modelKey);
    if (modelDir.exists(modelKey + ".thneed") || hasAllTinygradFiles(modelDir, modelKey)) {
      downloadableModels.removeAll(modelName);
    }
  }
  allModelsDownloaded = downloadableModels.isEmpty();

  QMap<QString, QString> deletableModels = getDeletableModels();
  noModelsDownloaded = deletableModels.isEmpty();

  QString modelKey = normalizeModelKey(QString::fromStdString(params.get("Model")));
  if (!modelDir.exists(modelKey + ".thneed") && !hasAllTinygradFiles(modelDir, modelKey)) {
    modelKey = normalizeModelKey(QString::fromStdString(params_default.get("Model")));
  }
  currentModel = modelFileToNameMap.value(modelKey);
  selectModelButton->setValue(currentModel);

  bool parked = !s.scene.started || fs.frogpilot_scene.parked || fs.frogpilot_toggles.value("frogs_go_moo").toBool();

  deleteModelButton->setButtonsEnabled(!(allModelsDownloading || modelDownloading || noModelsDownloaded));

  downloadModelButton->setEnabledButtons(0, !cancellingDownload && !finalizingDownload && (modelDownloading || (!allModelsDownloaded && !allModelsDownloading && fs.frogpilot_scene.online && parked)));
  downloadModelButton->setEnabledButtons(1, !cancellingDownload && !finalizingDownload && (allModelsDownloading || (!allModelsDownloaded && !modelDownloading && fs.frogpilot_scene.online && parked)));

  downloadModelButton->setValue(fs.frogpilot_scene.online ? (parked ? "" : tr("Not parked")) : tr("Offline..."));

  updateToggles();
}

QMap<QString, QString> FrogPilotModelPanel::getDeletableModels() {
  const QSet<QString> protectedModels = {
    normalizeModelKey(QString::fromStdString(params.get("Model"))),
    normalizeModelKey(QString::fromStdString(params_default.get("Model"))),
    normalizeModelKey(frogpilotUIState()->frogpilot_toggles.value("model").toString())
  };

  const QStringList files = modelDir.entryList(QDir::Files);

  QMap<QString, QString> deletableModels;

  for (QMap<QString, QString>::const_iterator model = modelFileToNameMapProcessed.cbegin(); model != modelFileToNameMapProcessed.cend(); ++model) {
    if (protectedModels.contains(model.key())) {
      continue;
    }

    for (const QString &file : files) {
      if (modelFileMatches(file, model.key())) {
        deletableModels.insert(model.key(), model.value());
        break;
      }
    }
  }

  return deletableModels;
}

void FrogPilotModelPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible() || finalizingDownload) {
    return;
  }

  bool parked = !s.scene.started || fs.frogpilot_scene.parked || fs.frogpilot_toggles.value("frogs_go_moo").toBool();

  if (allModelsDownloading || modelDownloading) {
    QString progress = QString::fromStdString(params_memory.get("ModelDownloadProgress"));
    bool downloadFailed = progress.contains(QRegularExpression("cancelled|exists|failed|missing|offline", QRegularExpression::CaseInsensitiveOption)) && (!allModelsDownloading || !params_memory.getBool("DownloadAllModels"));

     {
      QString translatedProgress;
      if (progress == "Downloading...") {
        translatedProgress = tr("Downloading...");
      } else if (progress == "Downloaded!") {
        translatedProgress = tr("Downloaded!");
      } else if (progress == "All models downloaded!") {
        translatedProgress = tr("All models downloaded!");
      } else if (progress.contains("cancelled", Qt::CaseInsensitive)) {
        translatedProgress = tr("Download cancelled...");
      } else if (progress.contains("failed", Qt::CaseInsensitive)) {
        translatedProgress = tr("Download failed...");
      } else if (progress.contains("offline", Qt::CaseInsensitive)) {
        translatedProgress = tr("GitHub and GitLab are offline...");
      } else if (progress == "Repository unavailable") {
        translatedProgress = tr("Repository unavailable");
      } else {
        translatedProgress = progress;
      }
      downloadModelButton->setValue(translatedProgress);
    }

    if (progress == "All models downloaded!" || (progress == "Downloaded!" && !allModelsDownloading) || downloadFailed) {
      finalizingDownload = true;

      QTimer::singleShot(2500, this, [progress, this]() {
        allModelsDownloading = false;
        cancellingDownload = false;
        finalizingDownload = false;
        modelDownloading = false;

        QStringList downloadableModels = availableModelNames;
        for (const QString &modelKey : modelFileToNameMap.keys()) {
          QString modelName = modelFileToNameMap.value(modelKey);
          if (modelDir.exists(modelKey + ".thneed") || hasAllTinygradFiles(modelDir, modelKey)) {
            downloadableModels.removeAll(modelName);
          }
        }
        allModelsDownloaded = downloadableModels.isEmpty();

        QMap<QString, QString> deletableModels = getDeletableModels();
        noModelsDownloaded = deletableModels.isEmpty();

        params_memory.remove("CancelModelDownload");
        params_memory.remove("ModelDownloadProgress");

        downloadModelButton->setValue("");
      });
    }
  } else {
    downloadModelButton->setValue(fs.frogpilot_scene.online ? (parked ? "" : tr("Not parked")) : tr("Offline..."));
  }

  deleteModelButton->setButtonsEnabled(!(allModelsDownloading || modelDownloading || noModelsDownloaded));

  downloadModelButton->setText(0, modelDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  downloadModelButton->setText(1, allModelsDownloading ? tr("CANCEL") : tr("DOWNLOAD ALL"));

  downloadModelButton->setEnabledButtons(0, !cancellingDownload && !finalizingDownload && (modelDownloading || (!allModelsDownloaded && !allModelsDownloading && fs.frogpilot_scene.online && parked)));
  downloadModelButton->setEnabledButtons(1, !cancellingDownload && !finalizingDownload && (allModelsDownloading || (!allModelsDownloaded && !modelDownloading && fs.frogpilot_scene.online && parked)));

  downloadModelButton->setVisibleButton(0, !allModelsDownloading);
  downloadModelButton->setVisibleButton(1, !modelDownloading);

  parent->keepScreenOn = allModelsDownloading || modelDownloading;
}

void FrogPilotModelPanel::updateModelLabels(FrogPilotListWidget *labelsList) {
  labelsList->clear();

  QJsonObject modelDrivesAndScores = QJsonDocument::fromJson(QString::fromStdString(params.get("ModelDrivesAndScores")).toUtf8()).object();

  for (const QString &modelName : availableModelNames) {
    QJsonObject modelData = modelDrivesAndScores.value(processModelName(modelName)).toObject();

    int drives = modelData.value("Drives").toInt(0);
    int score = modelData.value("Score").toInt(0);

    QString drivesDisplay = drives == 1 ? tr("%1 Drive").arg(drives) : drives > 0 ? tr("%1 Drives").arg(drives) : tr("N/A");
    QString scoreDisplay = drives > 0 ? tr("Score: %1%").arg(score) : tr("N/A");

    QString labelTitle = processModelName(modelName);
    QString labelText = QString("%1 (%2)").arg(scoreDisplay, drivesDisplay);

    LabelControl *labelControl = new LabelControl(labelTitle, labelText, "", this);
    labelsList->addItem(labelControl);
  }
}

void FrogPilotModelPanel::updateToggles() {
  for (auto &[key, toggle] : toggles) {
    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels[key].toDouble();

    if (key == "ManageBlacklistedModels" || key == "ManageScores") {
      setVisible &= (parent->tuningLevel >= parent->frogpilotToggleLevels["ModelRandomizer"].toDouble() && params.getBool("ModelRandomizer"));
    }

    else if (key == "SelectModel") {
      setVisible &= !(parent->tuningLevel >= parent->frogpilotToggleLevels["ModelRandomizer"].toDouble() && params.getBool("ModelRandomizer"));
    }

    toggle->setVisible(setVisible);
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
