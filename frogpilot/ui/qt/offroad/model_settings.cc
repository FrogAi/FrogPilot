#include "frogpilot/ui/qt/offroad/model_settings.h"

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
    {"AutomaticallyDownloadModels", tr("Automatically Download New Models"), tr("<b>Download new driving models on their own as they are released, so they are ready when you want to try one.</b><br><br>This runs whenever the device is on Wi-Fi or another unmetered connection, including while you are driving. It also grabs every model that is not already on the device, not just newly released ones, so anything you removed with \"Delete Driving Models\" comes back."), ""},
    {"DeleteModel", tr("Delete Driving Models"), tr("<b>Remove driving models you have downloaded to free up storage.</b><br><br>\"DELETE\" lets you pick which ones, \"DELETE ALL\" removes the rest. The model you are currently using and the one FrogPilot ships with are always kept. Turn \"Automatically Download New Models\" off first, or anything you delete is downloaded again within the hour whenever the device is on Wi-Fi or another unmetered connection."), ""},
    {"DownloadModel", tr("Download Driving Models"), tr("<b>Download driving models onto the device so you can switch to them.</b><br><br>\"DOWNLOAD\" lets you pick which ones, \"DOWNLOAD ALL\" fetches everything. Your car has to be parked and online, and models are large, so this can take a while."), ""},
    {"ManageBlacklistedModels", tr("Manage Model Blacklist"), tr("<b>Stop the \"Model Randomizer\" from picking driving models you did not like.</b><br><br>Blocking a model here has no effect on choosing it yourself under \"Select Driving Model\"."), ""},
    {"ManageScores", tr("Manage Model Ratings"), tr("<b>See how you rated each driving model and how many drives you gave it, or wipe those ratings and start fresh.</b><br><br>These are for your own comparison. The \"Model Randomizer\" picks at random and does not favor your higher-rated models."), ""},
    {"ModelRandomizer", tr("Model Randomizer"), tr("<b>Picks a different driving model for you at the start of every drive, then asks how it went when you park, so you can work out which one you like best.</b><br><br>It only chooses from models you have downloaded and have not blacklisted, and it only asks for a rating after drives longer than 15 minutes. Your ratings are saved under \"Manage Model Ratings\" for you to compare."), ""},
    {"SelectModel", tr("Select Driving Model"), tr("<b>Choose which driving model does the driving.</b><br><br>The model is the part of openpilot that decides how to steer, speed up, and slow down, so switching it changes how the car feels. Only models you have downloaded are listed, and changing it while driving asks you to reboot."), ""}
  };

  for (const auto &[param, title, desc, icon] : modelToggles) {
    AbstractControl *modelToggle = nullptr;

    if (param == "DeleteModel") {
      deleteModelButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DELETE ALL")});
      QObject::connect(deleteModelButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QMap<QString, QString> deletableModels = getDeletableModels();

        QStringList modelsToDelete;
        if (id == 0) {
          QStringList selections = FrogPilotMultiOptionDialog::getSelections(tr("Choose driving models to delete"), deletableModels.values(), tr("Delete"), this);
          if (selections.isEmpty()) {
            return;
          }

          QString confirmation;
          if (selections.size() == 1) {
            confirmation = tr("Are you sure you want to delete the \"%1\" model?").arg(selections.first());
          } else {
            confirmation = tr("Are you sure you want to delete these %1 models?").arg(selections.size());
          }

          if (!ConfirmationDialog::confirm(confirmation, tr("Delete"), this)) {
            return;
          }

          for (const QString &selection : selections) {
            modelsToDelete.append(deletableModels.key(selection));
          }
        } else if (ConfirmationDialog::confirm(tr("Delete every downloaded driving model except the one you are using and the one FrogPilot ships with?"), tr("Delete"), this)) {
          modelsToDelete = deletableModels.keys();
        } else {
          return;
        }

        for (const QString &file : modelDir.entryList(QDir::Files)) {
          for (const QString &modelKey : modelsToDelete) {
            if (modelFileMatches(file, modelKey)) {
              QFile::remove(modelDir.filePath(file));
              break;
            }
          }
        }

        frogpilotUIState()->updateToggles();

        allModelsDownloaded = false;
        noModelsDownloaded = getDeletableModels().isEmpty();
        deleteModelButton->setEnabled(!(allModelsDownloading || modelDownloading || noModelsDownloaded));
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
          QStringList downloadableModels = getDownloadableModels();
          allModelsDownloaded = downloadableModels.isEmpty();

          QStringList modelsToDownload = FrogPilotMultiOptionDialog::getSelections(tr("Choose driving models to download"), downloadableModels, tr("Download"), this);
          if (modelsToDownload.isEmpty()) {
            return;
          }

          QStringList modelKeys;
          for (const QString &modelToDownload : modelsToDownload) {
            modelKeys.append(modelFileToNameMap.key(modelToDownload));
          }

          frogpilotUIState()->downloadModels(modelKeys);

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

                frogpilotUIState()->updateToggles();
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
            ConfirmationDialog::alert(tr("You have not blacklisted any driving models."), this);
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

          frogpilotUIState()->updateToggles();

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
      bool parked = !uiState()->scene.started || fs.frogpilot_scene.parked || frogsGoMoo();

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
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotModelPanel::updateState);
}

void FrogPilotModelPanel::showEvent(QShowEvent *event) {
  FrogPilotUIState &fs = *frogpilotUIState();
  UIState &s = *uiState();

  QStringList availableModels = QString::fromStdString(params.get("AvailableModels")).split(",");
  availableModelNames = QString::fromStdString(params.get("AvailableModelNames")).split(",");

  modelFileToNameMap.clear();
  modelFileToNameMapProcessed.clear();
  for (int i = 0; i < qMin(availableModels.size(), availableModelNames.size()); ++i) {
    if (availableModels[i].isEmpty()) {
      continue;
    }

    modelFileToNameMap.insert(availableModels[i], availableModelNames[i]);
    modelFileToNameMapProcessed.insert(availableModels[i], processModelName(availableModelNames[i]));
  }

  const QString defaultModelKey = normalizeModelKey(QString::fromStdString(params_default.get("Model")));
  if (!modelFileToNameMap.contains(defaultModelKey)) {
    const QString defaultModelName = "Dark Souls (Default) 👀📡";
    modelFileToNameMap.insert(defaultModelKey, defaultModelName);
    modelFileToNameMapProcessed.insert(defaultModelKey, processModelName(defaultModelName));
  }

  availableModelNames = modelFileToNameMap.values();
  availableModelNames.sort();

  allModelsDownloaded = getDownloadableModels().isEmpty();

  QMap<QString, QString> deletableModels = getDeletableModels();
  noModelsDownloaded = deletableModels.isEmpty();

  QString modelKey = normalizeModelKey(QString::fromStdString(params.get("Model")));
  if (!modelDir.exists(modelKey + ".thneed") && !hasAllTinygradFiles(modelDir, modelKey)) {
    modelKey = defaultModelKey;
  }
  currentModel = modelFileToNameMap.value(modelKey);
  selectModelButton->setValue(currentModel);

  bool parked = !s.scene.started || fs.frogpilot_scene.parked || frogsGoMoo();

  deleteModelButton->setEnabled(!(allModelsDownloading || modelDownloading || noModelsDownloaded));

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

QStringList FrogPilotModelPanel::getDownloadableModels() {
  QStringList downloadableModels = availableModelNames;
  for (const QString &modelKey : modelFileToNameMap.keys()) {
    if (modelDir.exists(modelKey + ".thneed") || hasAllTinygradFiles(modelDir, modelKey)) {
      downloadableModels.removeAll(modelFileToNameMap.value(modelKey));
    }
  }
  return downloadableModels;
}

void FrogPilotModelPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible()) {
    return;
  }

  const cereal::FrogPilotProcessState::Reader &frogpilotProcessState = (*fs.sm)["frogpilotProcessState"].getFrogpilotProcessState();

  bool parked = !s.scene.started || fs.frogpilot_scene.parked || frogsGoMoo();

  if (frogpilotProcessState.getDownloadingModels() && !allModelsDownloading && !modelDownloading) {
    allModelsDownloading = true;
  }

  bool downloading = allModelsDownloading || modelDownloading;

  if (downloading && !finalizingDownload) {
    if (fs.download_model_request_time <= frogpilotProcessState.getModelDownloadRequestTime()) {
      QString progress = QString::fromStdString(frogpilotProcessState.getModelDownloadProgress());
      bool finished = !frogpilotProcessState.getDownloadingModels();

      if (progress == "Downloading...") {
        downloadModelButton->setValue(tr("Downloading..."));
      } else if (progress == "Downloaded!") {
        downloadModelButton->setValue(tr("Downloaded!"));
      } else if (progress == "All models downloaded!") {
        downloadModelButton->setValue(tr("All models downloaded!"));
      } else if (progress.contains("cancelled", Qt::CaseInsensitive)) {
        downloadModelButton->setValue(tr("Download cancelled..."));
      } else if (progress.contains("offline", Qt::CaseInsensitive)) {
        downloadModelButton->setValue(tr("GitHub and GitLab are offline..."));
      } else if (finished || progress.contains("failed", Qt::CaseInsensitive)) {
        downloadModelButton->setValue(tr("Download failed..."));
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

          allModelsDownloaded = getDownloadableModels().isEmpty();

          noModelsDownloaded = getDeletableModels().isEmpty();

          downloadModelButton->setValue("");
        });
      }
    }
  } else if (!downloading) {
    downloadModelButton->setValue(fs.frogpilot_scene.online ? (parked ? "" : tr("Not parked")) : tr("Offline..."));
  }

  deleteModelButton->setEnabled(!(downloading || noModelsDownloaded));

  downloadModelButton->setText(0, modelDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  downloadModelButton->setText(1, allModelsDownloading ? tr("CANCEL") : tr("DOWNLOAD ALL"));

  downloadModelButton->setEnabledButtons(0, !cancellingDownload && !finalizingDownload && (modelDownloading || (!allModelsDownloaded && !allModelsDownloading && fs.frogpilot_scene.online && parked)));
  downloadModelButton->setEnabledButtons(1, !cancellingDownload && !finalizingDownload && (allModelsDownloading || (!allModelsDownloaded && !modelDownloading && fs.frogpilot_scene.online && parked)));

  downloadModelButton->setVisibleButton(0, !allModelsDownloading);
  downloadModelButton->setVisibleButton(1, !modelDownloading);

  parent->keepScreenOn = downloading;
}

void FrogPilotModelPanel::updateModelLabels(FrogPilotListWidget *labelsList) {
  labelsList->clear();

  QJsonObject modelDrivesAndScores = QJsonDocument::fromJson(QString::fromStdString(params.get("ModelDrivesAndScores")).toUtf8()).object();

  for (const QString &modelName : availableModelNames) {
    QJsonObject modelData = modelDrivesAndScores.value(processModelName(modelName)).toObject();

    int drives = modelData.value("Drives").toInt(0);
    int score = qRound(modelData.value("Score").toDouble());

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
