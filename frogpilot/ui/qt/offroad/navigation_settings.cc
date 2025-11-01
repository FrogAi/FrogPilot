#include "frogpilot/ui/qt/offroad/navigation_settings.h"

FrogPilotNavigationPanel::FrogPilotNavigationPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  networkManager = new QNetworkAccessManager(this);

  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

  primelessLayout = new QStackedLayout();
  addItem(primelessLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);
  ipLabel = new LabelControl(tr("Manage Your Settings At"), tr("Offline..."));
  settingsList->addItem(ipLabel);

  std::vector<QString> searchOptions{tr("Mapbox"), tr("Amap")};
  searchInput = new FrogPilotButtonsControl(tr("Destination Search Provider"),
                                            tr("<b>The search provider used for destination queries</b> in \"Navigate on Openpilot\". "
                                               "Options include Mapbox (recommended) and Amap."),
                                               "", searchOptions, true);
  QObject::connect(searchInput, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    amapKeyControl1->setVisible(id == 1);
    amapKeyControl2->setVisible(id == 1);
    publicMapboxKeyControl->setVisible(id == 0);
    secretMapboxKeyControl->setVisible(id == 0);
    setupButton->setVisible(id == 0);

    params.putInt("SearchInput", id);

    update();
  });
  searchInput->setCheckedButton(params.getInt("SearchInput"));
  settingsList->addItem(searchInput);

  createKeyControl(amapKeyControl1, tr("Amap Key #1"), "AMapKey1", "", 39, settingsList);
  createKeyControl(amapKeyControl2, tr("Amap Key #2"), "AMapKey2", "", 39, settingsList);

  publicMapboxKeyControl = new FrogPilotButtonsControl(tr("Public Mapbox Key"), tr("<b>Manage your Public Mapbox Key.</b>"), "", {tr("ADD"), tr("TEST")});
  QObject::connect(publicMapboxKeyControl, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 0) {
      if (mapboxPublicKeySet) {
        if (FrogPilotConfirmationDialog::yesorno(tr("Remove your Public Mapbox Key?"), this)) {
          params.remove("MapboxPublicKey");
          params_cache.remove("MapboxPublicKey");

          updateButtons();
        }
      } else {
        QString key = InputDialog::getText(tr("Enter your Public Mapbox Key"), this).trimmed();
        if (!key.isEmpty()) {
          if (!key.startsWith("pk.")) {
            key = "pk." + key;
          }
          if (key.length() >= 80) {
            params.put("MapboxPublicKey", key.toStdString());

            updateButtons();
          } else {
            ConfirmationDialog::alert(tr("Inputted key is invalid or too short!"), this);
          }
        }
      }
    } else {
      publicMapboxKeyControl->setValue(tr("Testing..."));

      QString key = QString::fromStdString(params.get("MapboxPublicKey"));
      QString url = QString("https://api.mapbox.com/geocoding/v5/mapbox.places/mapbox.json?access_token=%1").arg(key);

      QNetworkRequest request(url);
      QNetworkReply *reply = networkManager->get(request);
      connect(reply, &QNetworkReply::finished, [=]() {
        publicMapboxKeyControl->setValue("");

        QString message;
        if (reply->error() == QNetworkReply::NoError) {
          message = tr("Key is valid!");
        } else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
          message = tr("Key is invalid!");
        } else {
          message = tr("An error occurred: %1").arg(reply->errorString());
        }
        ConfirmationDialog::alert(message, this);
        reply->deleteLater();
      });
    }
  });
  settingsList->addItem(publicMapboxKeyControl);

  secretMapboxKeyControl = new FrogPilotButtonsControl(tr("Secret Mapbox Key"), tr("<b>Manage your Secret Mapbox Key.</b>"), "", {tr("ADD"), tr("TEST")});
  QObject::connect(secretMapboxKeyControl, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 0) {
      if (mapboxSecretKeySet) {
        if (FrogPilotConfirmationDialog::yesorno(tr("Remove your Secret Mapbox Key?"), this)) {
          params.remove("MapboxSecretKey");
          params_cache.remove("MapboxSecretKey");

          updateButtons();
        }
      } else {
        QString key = InputDialog::getText(tr("Enter your Secret Mapbox Key"), this).trimmed();
        if (!key.isEmpty()) {
          if (!key.startsWith("sk.")) {
            key = "sk." + key;
          }
          if (key.length() >= 80) {
            params.put("MapboxSecretKey", key.toStdString());

            updateButtons();
          } else {
            ConfirmationDialog::alert(tr("Inputted key is invalid or too short!"), this);
          }
        }
      }
    } else {
      secretMapboxKeyControl->setValue(tr("Testing..."));

      QString key = QString::fromStdString(params.get("MapboxSecretKey"));
      QString url = QString("https://api.mapbox.com/directions/v5/mapbox/driving/-73.989,40.733;-74,40.733?access_token=%1").arg(key);

      QNetworkRequest request(url);
      QNetworkReply *reply = networkManager->get(request);
      connect(reply, &QNetworkReply::finished, [=]() {
        secretMapboxKeyControl->setValue("");

        QString message;
        if (reply->error() == QNetworkReply::NoError) {
          message = tr("Key is valid!");
        } else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
          message = tr("Key is invalid!");
        } else {
          message = tr("An error occurred: %1").arg(reply->errorString());
        }
        ConfirmationDialog::alert(message, this);
        reply->deleteLater();
      });
    }
  });
  settingsList->addItem(secretMapboxKeyControl);

  setupButton = new ButtonControl(tr("Mapbox Setup Instructions"), tr("VIEW"), tr("<b>Instructions on how to set up Mapbox</b> for \"Primeless Navigation\"."), this);
  QObject::connect(setupButton, &ButtonControl::clicked, [this]() {
    openSubPanel();

    updateStep();

    primelessLayout->setCurrentIndex(1);
  });
  settingsList->addItem(setupButton);

  std::vector<QString> filterButtonNames{tr("CANCEL"), tr("Manually Update Speed Limits")};
  updateSpeedLimitsToggle = new FrogPilotButtonControl("SpeedLimitFiller", tr("Speed Limit Filler"),
                                                    tr("<b>Automatically collect missing or incorrect speed limits while you drive</b> using speeds limits sourced from your dashboard (if supported), "
                                                       "Mapbox, and \"Navigate on openpilot\".<br><br>"
                                                       "When you're parked and connected to Wi-Fi, FrogPilot will automatically processes this data into a file "
                                                       "to be used with the tool located at \"SpeedLimitFiller.frogpilot.download\".<br><br>"
                                                       "You can download this file from \"The Pond\" in the \"Download Speed Limits\" menu.<br><br>"
                                                       "Need a step-by-step guide? Visit <b>#speed-limit-filler</b> in the FrogPilot Discord!"),
                                                       "", filterButtonNames);
  QObject::connect(updateSpeedLimitsToggle, &FrogPilotButtonControl::buttonClicked, [this](int id) {
    if (id == 0) {
      if (FrogPilotConfirmationDialog::yesorno(tr("Cancel the speed-limit update?"), this)) {
        updatingLimits = false;

        updateSpeedLimitsToggle->setEnabledButton(0, false);
        updateSpeedLimitsToggle->setValue(tr("Cancelled..."));

        params_memory.remove("UpdateSpeedLimits");

        QTimer::singleShot(2500, [this]() {
          updateSpeedLimitsToggle->clearCheckedButtons(true);
          updateSpeedLimitsToggle->setEnabledButton(0, true);
          updateSpeedLimitsToggle->setValue("");
          updateSpeedLimitsToggle->setVisibleButton(0, false);
          updateSpeedLimitsToggle->setVisibleButton(1, true);

          params_memory.remove("UpdateSpeedLimitsStatus");
        });
      }
    } else if (id == 1) {
      QJsonObject overpassRequests = QJsonDocument::fromJson(QString::fromStdString(params.get("OverpassRequests")).toUtf8()).object();

      int totalRequests = overpassRequests.value("total_requests").toInt(0);
      int maxRequests = overpassRequests.value("max_requests").toInt(10000);
      int savedDay = overpassRequests.value("day").toInt(QDate::currentDate().day());

      int currentDay = QDate::currentDate().day();

      if (savedDay != currentDay) {
        totalRequests = 0;
      }

      if (totalRequests >= maxRequests) {
        QTime now = QTime::currentTime();

        int secondsUntilMidnight = (24 * 3600) - (now.hour() * 3600 + now.minute() * 60 + now.second());
        int hours = secondsUntilMidnight / 3600;
        int minutes = (secondsUntilMidnight % 3600) / 60;

        ConfirmationDialog::alert(QString(tr("You've hit today's request limit.\n\nIt will reset in %1 hours and %2 minutes.")).arg(hours).arg(minutes), this);

        updateSpeedLimitsToggle->clearCheckedButtons(true);
        return;
      }

      updateSpeedLimitsToggle->setVisibleButton(0, true);
      updateSpeedLimitsToggle->setVisibleButton(1, false);

      if (FrogPilotConfirmationDialog::yesorno(tr("This process takes a while. It's recommended to start when you're done driving and connected to stable Wi-Fi. Continue?"), this)) {
        updatingLimits = true;

        updateSpeedLimitsToggle->setValue("Calculating...");

        params_memory.put("UpdateSpeedLimitsStatus", "Calculating...");
        params_memory.putBool("UpdateSpeedLimits", true);
      } else {
        updateSpeedLimitsToggle->setVisibleButton(0, false);
        updateSpeedLimitsToggle->setVisibleButton(1, true);

        updateSpeedLimitsToggle->clearCheckedButtons(true);
      }
    }
  });
  updateSpeedLimitsToggle->setVisibleButton(0, false);
  settingsList->addItem(updateSpeedLimitsToggle);

  ScrollView *settingsPanel = new ScrollView(settingsList, this);
  primelessLayout->addWidget(settingsPanel);

  imageLabel = new QLabel(this);

  ScrollView *instructionsPanel = new ScrollView(imageLabel, this);
  primelessLayout->addWidget(instructionsPanel);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [this]() {
    primelessLayout->setCurrentIndex(0);

    if (forceOpenDescriptions) {
      amapKeyControl1->showDescription();
      amapKeyControl2->showDescription();
      publicMapboxKeyControl->showDescription();
      searchInput->showDescription();
      secretMapboxKeyControl->showDescription();
      setupButton->showDescription();
      updateSpeedLimitsToggle->showDescription();
    }
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotNavigationPanel::updateState);
}

void FrogPilotNavigationPanel::showEvent(QShowEvent *event) {
  if (forceOpenDescriptions) {
    amapKeyControl1->showDescription();
    amapKeyControl2->showDescription();
    publicMapboxKeyControl->showDescription();
    searchInput->showDescription();
    secretMapboxKeyControl->showDescription();
    setupButton->showDescription();
    updateSpeedLimitsToggle->showDescription();
  }

  FrogPilotUIState &fs = *frogpilotUIState();
  UIState &s = *uiState();

  FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;

  QString ipAddress = fs.wifi->getIp4Address();
  ipLabel->setText(ipAddress.isEmpty() ? tr("Offline...") : QString("%1:8082").arg(ipAddress));

  updateButtons();

  setupCompleted = mapboxPublicKeySet && mapboxSecretKeySet;
  updatingLimits = !params_memory.get("UpdateSpeedLimitsStatus").empty() && QString::fromStdString(params_memory.get("UpdateSpeedLimitsStatus")) != "Completed!";

  bool parked = !s.scene.started || fs.frogpilot_scene.parked || fs.frogpilot_toggles.value("frogs_go_moo").toBool();

  int selectedSearchInput = params.getInt("SearchInput");

  amapKeyControl1->setVisible(selectedSearchInput == 1);
  amapKeyControl2->setVisible(selectedSearchInput == 1);
  publicMapboxKeyControl->setVisible(selectedSearchInput == 0);
  secretMapboxKeyControl->setVisible(selectedSearchInput == 0);
  setupButton->setVisible(selectedSearchInput == 0);

  updateSpeedLimitsToggle->setVisibleButton(0, updatingLimits);
  updateSpeedLimitsToggle->setVisibleButton(1, !updatingLimits);

  if (updatingLimits) {
    updateSpeedLimitsToggle->setValue(QString::fromStdString(params_memory.get("UpdateSpeedLimitsStatus")));
  } else {
    updateSpeedLimitsToggle->setEnabledButton(1, frogpilot_scene.online && util::system_time_valid() && parked);
    updateSpeedLimitsToggle->setValue(frogpilot_scene.online ? (parked ? "" : "Not parked") : tr("Offline..."));
    updateSpeedLimitsToggle->setVisible(parent->tuningLevel >= parent->frogpilotToggleLevels["SpeedLimitFiller"].toDouble());
  }
}

void FrogPilotNavigationPanel::hideEvent(QHideEvent *event) {
  primelessLayout->setCurrentIndex(0);
}

void FrogPilotNavigationPanel::mousePressEvent(QMouseEvent *event) {
  if (primelessLayout->currentIndex() == 1) {
    closeSubPanel();

    primelessLayout->setCurrentIndex(0);

    if (forceOpenDescriptions) {
      amapKeyControl1->showDescription();
      amapKeyControl2->showDescription();
      publicMapboxKeyControl->showDescription();
      searchInput->showDescription();
      secretMapboxKeyControl->showDescription();
      setupButton->showDescription();
      updateSpeedLimitsToggle->showDescription();
    }
  }
}

void FrogPilotNavigationPanel::createKeyControl(ButtonControl *&control, const QString &label, const std::string &paramKey, const QString &prefix, const int &minLength, FrogPilotListWidget *list) {
  control = new ButtonControl(label, "", tr("<b>Manage your \"%1\".</b>").arg(label));
  QObject::connect(control, &ButtonControl::clicked, [=] {
    if (control->text() == tr("ADD")) {
      QString key = InputDialog::getText(tr("Enter your %1").arg(label), this).trimmed();

      if (!key.startsWith(prefix)) {
        key = prefix + key;
      }

      if (key.length() >= minLength) {
        params.put(paramKey, key.toStdString());
      } else {
        ConfirmationDialog::alert(tr("Inputted key is invalid or too short!"), this);
      }
    } else {
      if (FrogPilotConfirmationDialog::yesorno(tr("Remove your %1?").arg(label), this)) {
        control->setText(tr("ADD"));

        params.remove(paramKey);
        params_cache.remove(paramKey);

        setupCompleted = false;
      }
    }
  });
  control->setText(QString::fromStdString(params.get(paramKey)).startsWith(prefix) ? tr("REMOVE") : tr("ADD"));
  list->addItem(control);
}

void FrogPilotNavigationPanel::updateButtons() {
  FrogPilotUIState &fs = *frogpilotUIState();

  amapKeyControl1->setText(params.get("AMapKey1").empty() ? tr("ADD") : tr("REMOVE"));
  amapKeyControl2->setText(params.get("AMapKey2").empty() ? tr("ADD") : tr("REMOVE"));

  mapboxPublicKeySet = QString::fromStdString(params.get("MapboxPublicKey")).startsWith("pk");
  mapboxSecretKeySet = QString::fromStdString(params.get("MapboxSecretKey")).startsWith("sk");

  publicMapboxKeyControl->setText(0, mapboxPublicKeySet ? tr("REMOVE") : tr("ADD"));
  publicMapboxKeyControl->setVisibleButton(1, mapboxPublicKeySet && fs.frogpilot_scene.online);
  secretMapboxKeyControl->setText(0, mapboxSecretKeySet ? tr("REMOVE") : tr("ADD"));
  secretMapboxKeyControl->setVisibleButton(1, mapboxSecretKeySet && fs.frogpilot_scene.online);
}

void FrogPilotNavigationPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible() || s.sm->frame % (UI_FREQ / 2) != 0) {
    return;
  }

  updateButtons();
  updateStep();

  bool parked = !s.scene.started || fs.frogpilot_scene.parked || fs.frogpilot_toggles.value("frogs_go_moo").toBool();

  if (updatingLimits) {
    if (QString::fromStdString(params_memory.get("UpdateSpeedLimitsStatus")) == "Completed!") {
      updatingLimits = false;

      updateSpeedLimitsToggle->setValue(tr("Completed!"));

      QTimer::singleShot(2500, [this]() {
        updateSpeedLimitsToggle->clearCheckedButtons(true);
        updateSpeedLimitsToggle->setValue("");
        updateSpeedLimitsToggle->setVisibleButton(0, false);
        updateSpeedLimitsToggle->setVisibleButton(1, true);

        params_memory.remove("UpdateSpeedLimitsStatus");
      });
    } else {
      updateSpeedLimitsToggle->setValue(QString::fromStdString(params_memory.get("UpdateSpeedLimitsStatus")));
    }
  } else {
    updateSpeedLimitsToggle->setEnabledButton(1, fs.frogpilot_scene.online && util::system_time_valid() && parked);
    updateSpeedLimitsToggle->setValue(fs.frogpilot_scene.online ? (parked ? "" : "Not parked") : tr("Offline..."));
  }

  parent->keepScreenOn = primelessLayout->currentIndex() == 1 || updatingLimits;
}

void FrogPilotNavigationPanel::updateStep() {
  QString currentStep;
  if (setupCompleted) {
    currentStep = "../../frogpilot/navigation/navigation_training/setup_completed.png";
  } else if (mapboxPublicKeySet && mapboxSecretKeySet) {
    currentStep = "../../frogpilot/navigation/navigation_training/both_keys_set.png";
  } else if (mapboxPublicKeySet) {
    currentStep = "../../frogpilot/navigation/navigation_training/public_key_set.png";
  } else {
    currentStep = "../../frogpilot/navigation/navigation_training/no_keys_set.png";
  }

  QPixmap pixmap;
  pixmap.load(currentStep);
  imageLabel->setPixmap(pixmap.scaledToWidth(1500, Qt::SmoothTransformation));

  update();
}
