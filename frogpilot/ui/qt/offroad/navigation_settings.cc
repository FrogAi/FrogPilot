#include "frogpilot/ui/qt/offroad/navigation_settings.h"

FrogPilotNavigationPanel::FrogPilotNavigationPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  networkManager = new QNetworkAccessManager(this);

  primelessLayout = new QStackedLayout();
  addItem(primelessLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);
  ipLabel = new LabelControl(tr("Manage Your Settings At"), tr("Offline..."), tr("<b>Open this address in a browser on the same Wi-Fi to reach \"The Pond\", where you search for destinations and send them to your car.</b>"));
  settingsList->addItem(ipLabel);

  publicMapboxKeyControl = new FrogPilotButtonsControl(tr("Public Mapbox Key"), tr("<b>Lets you search for a destination and preview the route without paying for comma's subscription.</b><br><br>You make this key yourself on Mapbox's website. Navigation stays locked until both this and the \"Secret Mapbox Key\" are set, so add both. \"Mapbox Setup Instructions\" walks you through it."), "", {tr("ADD"), tr("TEST")});
  QObject::connect(publicMapboxKeyControl, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 0) {
      if (mapboxPublicKeySet) {
        if (FrogPilotConfirmationDialog::yesorno(tr("Remove your Public Mapbox Key?"), this)) {
          params.remove("MapboxPublicKey");

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

  secretMapboxKeyControl = new FrogPilotButtonsControl(tr("Secret Mapbox Key"), tr("<b>Draws the map itself on your driving screen, alongside what the \"Public Mapbox Key\" does for searching.</b><br><br>Keep this one to yourself, since it can be used to run up charges on your Mapbox account. After you add it you are asked whether to reboot now or later, and it does not take effect until the device has actually rebooted."), "", {tr("ADD"), tr("TEST")});
  QObject::connect(secretMapboxKeyControl, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 0) {
      if (mapboxSecretKeySet) {
        if (FrogPilotConfirmationDialog::yesorno(tr("Remove your Secret Mapbox Key?"), this)) {
          params.remove("MapboxSecretKey");

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
                                                    tr("<b>Spot missing or outdated OpenStreetMap speed limits while you drive.</b><br><br>"
                                                       "FrogPilot compares the speed limits it sees with your downloaded maps and saves possible corrections for you to review later. This makes it quick and easy to improve speed-limit data for future drives and everyone who uses OpenStreetMap.<br><br>"
                                                       "Downloaded maps are required. Saved suggestions may reveal which roads you drove. Nothing is submitted automatically, so review each suggestion before making an OpenStreetMap edit.<br><br>"
                                                       "Need a step-by-step guide? Visit <b>#speed-limit-filler</b> in the FrogPilot Discord!"),
                                                       "");
  settingsList->addItem(updateSpeedLimitsToggle);

  ScrollView *settingsPanel = new ScrollView(settingsList, this);
  primelessLayout->addWidget(settingsPanel);

  imageLabel = new QLabel(this);

  ScrollView *instructionsPanel = new ScrollView(imageLabel, this);
  primelessLayout->addWidget(instructionsPanel);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [this]() {
    primelessLayout->setCurrentIndex(0);

    if (forceOpenDescriptions) {
      ipLabel->showDescription();
      publicMapboxKeyControl->showDescription();
      secretMapboxKeyControl->showDescription();
      setupButton->showDescription();
      updateSpeedLimitsToggle->showDescription();
    }
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotNavigationPanel::updateState);
}

void FrogPilotNavigationPanel::showEvent(QShowEvent *event) {
  if (forceOpenDescriptions) {
    ipLabel->showDescription();
    publicMapboxKeyControl->showDescription();
    secretMapboxKeyControl->showDescription();
    setupButton->showDescription();
    updateSpeedLimitsToggle->showDescription();
  }

  updateButtons();
  updateSpeedLimitsToggle->setVisible(parent->tuningLevel >= parent->frogpilotToggleLevels["SpeedLimitFiller"].toDouble());
}

void FrogPilotNavigationPanel::hideEvent(QHideEvent *event) {
  primelessLayout->setCurrentIndex(0);
}

void FrogPilotNavigationPanel::mousePressEvent(QMouseEvent *event) {
  if (primelessLayout->currentIndex() == 1) {
    closeSubPanel();

    primelessLayout->setCurrentIndex(0);

    if (forceOpenDescriptions) {
      ipLabel->showDescription();
      publicMapboxKeyControl->showDescription();
      secretMapboxKeyControl->showDescription();
      setupButton->showDescription();
      updateSpeedLimitsToggle->showDescription();
    }
  }
}

void FrogPilotNavigationPanel::updateButtons() {
  FrogPilotUIState &fs = *frogpilotUIState();
  FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;

  QString ipAddress = fs.wifi->getIp4Address();
  ipLabel->setText(ipAddress.isEmpty() ? tr("Offline...") : QString("%1:8082").arg(ipAddress));

  mapboxPublicKeySet = QString::fromStdString(params.get("MapboxPublicKey")).startsWith("pk");
  mapboxSecretKeySet = QString::fromStdString(params.get("MapboxSecretKey")).startsWith("sk");

  publicMapboxKeyControl->setText(0, mapboxPublicKeySet ? tr("REMOVE") : tr("ADD"));
  publicMapboxKeyControl->setVisibleButton(1, mapboxPublicKeySet && frogpilot_scene.online);
  secretMapboxKeyControl->setText(0, mapboxSecretKeySet ? tr("REMOVE") : tr("ADD"));
  secretMapboxKeyControl->setVisibleButton(1, mapboxSecretKeySet && frogpilot_scene.online);
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
  if (mapboxPublicKeySet) {
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
