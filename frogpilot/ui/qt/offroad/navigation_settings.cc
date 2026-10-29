#include "frogpilot/ui/qt/offroad/navigation_settings.h"

FrogPilotNavigationPanel::FrogPilotNavigationPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  networkManager = new QNetworkAccessManager(this);

  navigationLayout = new QStackedLayout();
  addItem(navigationLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);
  ipLabel = new LabelControl(tr("Manage Your Settings At"), tr("Offline..."), tr("<b>Open this address in a browser on the same Wi-Fi to reach \"The Pond\", where you manage your drives, recordings, themes, and more from a phone or computer.</b>"));
  settingsList->addItem(ipLabel);

  pairingToggle = new ParamControl("ThePondPairing", tr("Pairing Code"), tr("<b>Ask for a code before a browser can use \"The Pond\".</b><br><br>Without a code, any device on the same Wi-Fi or Tailscale network can open your drives, camera footage, logs, location, and saved places, and can change your settings, unlock your doors, reboot the device, switch branches, run any command on the device from its shell, restore a backup that replaces its software, or sign the device into a Tailscale account. The code shows next to the address above. Turning this off signs every browser out, and turning it back on makes a new code."), "");
  QObject::connect(pairingToggle, &ToggleControl::toggleFlipped, [this](bool state) {
    if (state) {
      params.put("ThePondPairingCode", newPairingCode().toStdString());
    } else {
      params.remove("ThePondPairedBrowsers");
      params.remove("ThePondPairingCode");
    }
  });
  settingsList->addItem(pairingToggle);

  publicMapboxKeyControl = new FrogPilotButtonsControl(tr("Public Mapbox Key"), tr("<b>Lets openpilot get speed limits from Mapbox.</b><br><br>You make this key yourself on Mapbox's website. \"Mapbox Setup Instructions\" walks you through it."), "", {tr("ADD"), tr("TEST")});
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

          if (uiState()->scene.started && FrogPilotConfirmationDialog::toggleReboot(this)) {
            FrogPilotConfirmationDialog::softReboot(this);
          }
        }
      }
    } else {
      testMapboxKey(publicMapboxKeyControl, "MapboxPublicKey", "https://api.mapbox.com/geocoding/v5/mapbox.places/mapbox.json?access_token=%1");
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
            FrogPilotConfirmationDialog::softReboot(this);
          }
        }
      }
    } else {
      testMapboxKey(secretMapboxKeyControl, "MapboxSecretKey", "https://api.mapbox.com/directions/v5/mapbox/driving/-73.989,40.733;-74,40.733?access_token=%1");
    }
  });
  settingsList->addItem(secretMapboxKeyControl);

  setupButton = new ButtonControl(tr("Mapbox Setup Instructions"), tr("VIEW"), tr("<b>Walks you through getting your own free Mapbox keys.</b><br><br>The guide only shows the steps for where you are in setup, so it changes as you add each key. Tap the instructions to come back here."), this);
  QObject::connect(setupButton, &ButtonControl::clicked, [this]() {
    emit openSubPanel();

    updateStep();

    navigationLayout->setCurrentIndex(1);
  });
  settingsList->addItem(setupButton);

  speedLimitFillerToggle = new ParamControl("SpeedLimitFiller", tr("Speed Limit Filler"), tr("<b>Learns speed limits from the signs the camera reads on roads where your downloaded maps have no limit or the wrong one.</b><br><br>Each learned limit is saved for that road and direction and used in place of the map's limit the next time you drive there, and it is forgotten once the map matches the signs. It also keeps a cropped photo of the sign each learned limit came from. Roads with conditional limits, such as ones that change with the time of day or the weather, are skipped. Needs downloaded maps. With \"Share FrogPilot Data\" on, the learned limits, their sign photos, and the roads where the signs matched your map are sent to FrogPilot while parked on unmetered Wi-Fi or Ethernet and used to improve speed limits for everyone using FrogPilot."), "");
  settingsList->addItem(speedLimitFillerToggle);

  ScrollView *settingsPanel = new ScrollView(settingsList, this);
  navigationLayout->addWidget(settingsPanel);

  imageLabel = new QLabel(this);

  ScrollView *instructionsPanel = new ScrollView(imageLabel, this);
  navigationLayout->addWidget(instructionsPanel);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [this]() {
    navigationLayout->setCurrentIndex(0);
    imageLabel->clear();

    if (forceOpenDescriptions) {
      ipLabel->showDescription();
      pairingToggle->showDescription();
      publicMapboxKeyControl->showDescription();
      secretMapboxKeyControl->showDescription();
      setupButton->showDescription();
      speedLimitFillerToggle->showDescription();
    }
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotNavigationPanel::updateState);
}

void FrogPilotNavigationPanel::showEvent(QShowEvent *event) {
  if (forceOpenDescriptions) {
    ipLabel->showDescription();
    pairingToggle->showDescription();
    publicMapboxKeyControl->showDescription();
    secretMapboxKeyControl->showDescription();
    setupButton->showDescription();
    speedLimitFillerToggle->showDescription();
  }

  updateButtons();
  pairingToggle->setVisible(parent->tuningLevel >= parent->frogpilotToggleLevels.value("ThePondPairing").toDouble());
  speedLimitFillerToggle->setVisible(parent->tuningLevel >= parent->frogpilotToggleLevels.value("SpeedLimitFiller").toDouble());
}

void FrogPilotNavigationPanel::hideEvent(QHideEvent *event) {
  navigationLayout->setCurrentIndex(0);
  imageLabel->clear();
}

void FrogPilotNavigationPanel::mousePressEvent(QMouseEvent *event) {
  if (navigationLayout->currentIndex() == 1) {
    emit closeSubPanel();
  }
}

void FrogPilotNavigationPanel::testMapboxKey(FrogPilotButtonsControl *control, const std::string &paramKey, const QString &urlTemplate) {
  control->setValue(tr("Testing..."));

  QString key = QString::fromStdString(params.get(paramKey));
  QString url = urlTemplate.arg(key);

  QNetworkRequest request(url);
  QNetworkReply *reply = networkManager->get(request);
  connect(reply, &QNetworkReply::finished, [=]() {
    control->setValue("");

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

void FrogPilotNavigationPanel::updateButtons() {
  mapboxPublicKeySet = QString::fromStdString(params.get("MapboxPublicKey")).startsWith("pk");
  mapboxSecretKeySet = QString::fromStdString(params.get("MapboxSecretKey")).startsWith("sk");

  publicMapboxKeyControl->setText(0, mapboxPublicKeySet ? tr("REMOVE") : tr("ADD"));
  secretMapboxKeyControl->setText(0, mapboxSecretKeySet ? tr("REMOVE") : tr("ADD"));

  updateNetworkState(*frogpilotUIState());
}

void FrogPilotNavigationPanel::updateNetworkState(const FrogPilotUIState &fs) {
  const FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;

  QString ipAddress = fs.wifi->ipv4_address;
  if (ipAddress.isEmpty()) {
    ipLabel->setText(tr("Offline..."));
  } else {
    QString address = QString("%1:%2").arg(ipAddress).arg(8082);
    if (params.getBool("ThePondPairing")) {
      address += tr(" (Code %1)").arg(QString::fromStdString(params.get("ThePondPairingCode")));
    }
    ipLabel->setText(address);
  }

  publicMapboxKeyControl->setVisibleButton(1, mapboxPublicKeySet && frogpilot_scene.online);
  secretMapboxKeyControl->setVisibleButton(1, mapboxSecretKeySet && frogpilot_scene.online);
}

void FrogPilotNavigationPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible() || s.sm->frame % (UI_FREQ / 2) != 0) {
    return;
  }

  updateNetworkState(fs);
  parent->keepScreenOn = navigationLayout->currentIndex() == 1;
}

void FrogPilotNavigationPanel::updateStep() {
  QString step;
  if (mapboxPublicKeySet) {
    step = "../../frogpilot/navigation/navigation_training/public_key_set.svg";
  } else {
    step = "../../frogpilot/navigation/navigation_training/no_keys_set.svg";
  }

  QPixmap pixmap;
  pixmap.load(step);
  imageLabel->setPixmap(pixmap);
}
