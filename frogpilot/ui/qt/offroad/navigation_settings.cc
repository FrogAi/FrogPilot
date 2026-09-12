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
  ipLabel = new LabelControl(tr("Manage Your Settings At"), tr("Offline..."), tr("<b>Open this address in a browser on the same Wi-Fi to reach \"The Pond\", where you search for destinations and send them to your car.</b>"));
  settingsList->addItem(ipLabel);

  std::vector<QString> searchOptions{tr("Mapbox"), tr("Amap")};
  searchInput = new FrogPilotButtonsControl(tr("Destination Search Provider"),
                                            tr("<b>Switches this menu between your Mapbox keys and your Amap keys.</b><br><br>"
                                               "Picking \"Amap\" just swaps which rows this menu shows you. \"The Pond\" still needs both Mapbox keys before it will show its navigation page at all, so set those either way. Destination searching itself happens in \"The Pond\", not here."),
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
  settingsList->addItem(searchInput);

  createKeyControl(amapKeyControl1, tr("Amap Key #1"), "AMapKey1", "", 39, settingsList);
  createKeyControl(amapKeyControl2, tr("Amap Key #2"), "AMapKey2", "", 39, settingsList);

  publicMapboxKeyControl = new FrogPilotButtonsControl(tr("Public Mapbox Key"), tr("<b>Lets you search for a destination and preview the route without paying for comma's subscription.</b><br><br>You make this key yourself on Mapbox's website. Navigation stays locked until both this and the \"Secret Mapbox Key\" are set, so add both. \"Mapbox Setup Instructions\" walks you through it."), "", {tr("ADD"), tr("TEST")});
  QObject::connect(publicMapboxKeyControl, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 0) {
      if (mapboxPublicKeySet) {
        if (FrogPilotConfirmationDialog::yesorno(tr("Remove your Public Mapbox Key?"), this)) {
          params.remove("MapboxPublicKey");
          params_cache.remove("MapboxPublicKey");

          updateButtons();
        }
      } else {
        int minKeyLength = 80;
        QString key = InputDialog::getText(tr("Enter your Public Mapbox Key"), this, "", false, minKeyLength).trimmed();
        if (!key.isEmpty()) {
          if (key.startsWith("sk.")) {
            ConfirmationDialog::alert(tr("That's your Secret Mapbox Key. Enter your Public Mapbox Key."), this);
            return;
          }
          if (!key.startsWith("pk.")) {
            key = "pk." + key;
          }
          params.put("MapboxPublicKey", key.toStdString());
          updateButtons();
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
          message = tr("An error occurred: %1").arg(QString(reply->errorString()).replace(key, tr("[key hidden]")));
        }

        if (isVisible()) {
          ConfirmationDialog::alert(message, this);
        }
        reply->deleteLater();
      });
    }
  });
  settingsList->addItem(publicMapboxKeyControl);

  secretMapboxKeyControl = new FrogPilotButtonsControl(tr("Secret Mapbox Key"), tr("<b>Draws the map itself on your driving screen, alongside what the \"Public Mapbox Key\" does for searching.</b><br><br>Keep this one to yourself, since it can be used to run up charges on your Mapbox account. After you add it you are asked whether to reboot, and the map only appears once the device has actually rebooted."), "", {tr("ADD"), tr("TEST")});
  QObject::connect(secretMapboxKeyControl, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 0) {
      if (mapboxSecretKeySet) {
        if (FrogPilotConfirmationDialog::yesorno(tr("Remove your Secret Mapbox Key?"), this)) {
          params.remove("MapboxSecretKey");
          params_cache.remove("MapboxSecretKey");

          updateButtons();
        }
      } else {
        int minKeyLength = 80;
        QString key = InputDialog::getText(tr("Enter your Secret Mapbox Key"), this, "", false, minKeyLength).trimmed();
        if (!key.isEmpty()) {
          if (key.startsWith("pk.")) {
            ConfirmationDialog::alert(tr("That's your Public Mapbox Key. Enter your Secret Mapbox Key."), this);
            return;
          }
          if (!key.startsWith("sk.")) {
            key = "sk." + key;
          }
          params.put("MapboxSecretKey", key.toStdString());
          updateButtons();

          if (FrogPilotConfirmationDialog::toggleReboot(this)) {
            Hardware::reboot();
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
          message = tr("An error occurred: %1").arg(QString(reply->errorString()).replace(key, tr("[key hidden]")));
        }

        if (isVisible()) {
          ConfirmationDialog::alert(message, this);
        }
        reply->deleteLater();
      });
    }
  });
  settingsList->addItem(secretMapboxKeyControl);

  setupButton = new ButtonControl(tr("Mapbox Setup Instructions"), tr("VIEW"), tr("<b>Walks you through getting your own free Mapbox keys so navigation works without comma's subscription.</b><br><br>The guide only shows the steps for where you are in setup, so it changes as you add each key. Tap the instructions to come back here."), this);
  QObject::connect(setupButton, &ButtonControl::clicked, [this]() {
    openSubPanel();

    updateStep();

    primelessLayout->setCurrentIndex(1);
  });
  settingsList->addItem(setupButton);

  updateSpeedLimitsToggle = new ParamControl("SpeedLimitFiller", tr("Speed Limit Filler"),
                                                    tr("<b>Collect missing or incorrect speed limits automatically while you drive.</b><br><br>"
                                                       "FrogPilot compares speed limits from your dashboard, where supported, Mapbox, and \"Navigate on openpilot\". Downloaded maps are required because "
                                                       "FrogPilot uses their OSM way IDs to identify each road.<br><br>You can download the results from \"The Pond\" in "
                                                       "the \"Download Speed Limits\" menu and load them into the Speed Limit Filler website. Review every proposed edit before submitting it to OSM.<br><br>"
                                                       "Need a step-by-step guide? "
                                                       "Visit <b>#speed-limit-filler</b> in the FrogPilot Discord!"),
                                                       "");
  settingsList->addItem(updateSpeedLimitsToggle);

  ScrollView *settingsPanel = new ScrollView(settingsList, this);
  primelessLayout->addWidget(settingsPanel);

  imageLabel = new QLabel(this);

  ScrollView *instructionsPanel = new ScrollView(imageLabel, this);
  primelessLayout->addWidget(instructionsPanel);

  toggles["AMapKey1"] = amapKeyControl1;
  toggles["AMapKey2"] = amapKeyControl2;
  toggles["ManageSettingsAt"] = ipLabel;
  toggles["MapboxPublicKey"] = publicMapboxKeyControl;
  toggles["MapboxSecretKey"] = secretMapboxKeyControl;
  toggles["MapboxSetup"] = setupButton;
  toggles["SearchInput"] = searchInput;
  toggles["SpeedLimitFiller"] = updateSpeedLimitsToggle;

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [this]() {
    primelessLayout->setCurrentIndex(0);

    openDescriptions(forceOpenDescriptions, toggles);
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotNavigationPanel::updateState);
}

void FrogPilotNavigationPanel::showEvent(QShowEvent *event) {
  openDescriptions(forceOpenDescriptions, toggles);

  updateButtons();

  setupCompleted = mapboxPublicKeySet && mapboxSecretKeySet;

  int selectedSearchInput = params.getInt("SearchInput");

  searchInput->setCheckedButton(selectedSearchInput);

  amapKeyControl1->setVisible(selectedSearchInput == 1);
  amapKeyControl2->setVisible(selectedSearchInput == 1);
  publicMapboxKeyControl->setVisible(selectedSearchInput == 0);
  secretMapboxKeyControl->setVisible(selectedSearchInput == 0);
  setupButton->setVisible(selectedSearchInput == 0);

  updateSpeedLimitsToggle->setVisible(parent->tuningLevel >= parent->frogpilotToggleLevels["SpeedLimitFiller"].toDouble());
}

void FrogPilotNavigationPanel::hideEvent(QHideEvent *event) {
  primelessLayout->setCurrentIndex(0);
}

void FrogPilotNavigationPanel::mousePressEvent(QMouseEvent *event) {
  if (primelessLayout->currentIndex() == 1) {
    closeSubPanel();

    primelessLayout->setCurrentIndex(0);

    openDescriptions(forceOpenDescriptions, toggles);
  }
}

void FrogPilotNavigationPanel::createKeyControl(ButtonControl *&control, const QString &label, const std::string &paramKey, const QString &prefix, const int &minLength, FrogPilotListWidget *list) {
  control = new ButtonControl(label, "", tr("<b>Your %1, stored for Amap destination searching in \"The Pond\".</b><br><br>FrogPilot does not actually send searches to Amap yet, so \"The Pond\" only checks that the keys are filled in. Set both Mapbox keys if you want destination search to work.").arg(label));
  QObject::connect(control, &ButtonControl::clicked, [=] {
    if (control->text() == tr("ADD")) {
      QString key = InputDialog::getText(tr("Enter your %1").arg(label), this, "", false, minLength).trimmed();
      if (!key.isEmpty()) {
        if (!key.startsWith(prefix)) {
          key = prefix + key;
        }
        params.put(paramKey, key.toStdString());
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

  QString ipAddress = fs.wifi->getIp4Address();
  ipLabel->setText(ipAddress.isEmpty() ? tr("Offline...") : QString("%1:8082").arg(ipAddress));

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

  parent->keepScreenOn = primelessLayout->currentIndex() == 1;
}

void FrogPilotNavigationPanel::updateStep() {
  QString step;
  if (setupCompleted) {
    step = "../../frogpilot/navigation/navigation_training/setup_completed.png";
  } else if (mapboxPublicKeySet && mapboxSecretKeySet) {
    step = "../../frogpilot/navigation/navigation_training/both_keys_set.png";
  } else if (mapboxPublicKeySet) {
    step = "../../frogpilot/navigation/navigation_training/public_key_set.png";
  } else {
    step = "../../frogpilot/navigation/navigation_training/no_keys_set.png";
  }

  if (step == currentStep) {
    return;
  }
  currentStep = step;

  QPixmap pixmap;
  pixmap.load(currentStep);
  imageLabel->setPixmap(pixmap.scaledToWidth(1500, Qt::SmoothTransformation));

  update();
}
