#include "frogpilot/ui/qt/offroad/navigation_settings.h"

#include <QRandomGenerator>

FrogPilotNavigationPanel::FrogPilotNavigationPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  networkManager = new QNetworkAccessManager(this);

  forceOpenDescriptions = forceOpen;

  primelessLayout = new QStackedLayout();
  addItem(primelessLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);
  amapKeyControl = new ButtonControl(tr("Amap Key"), tr("ADD"), tr("<b>Lets you search for a destination with Amap when \"Destination Search Provider\" is set to Amap.</b><br><br>This is a Web Service key you make yourself in the console on Amap's website, lbs.amap.com. Add a key to your app there and choose \"Web Service\" (Web 服务) as its service platform, since other kinds of Amap keys don't work here."), this);
  QObject::connect(amapKeyControl, &ButtonControl::clicked, [this]() {
    if (params.get("AMapKey1").empty()) {
      QString key = InputDialog::getText(tr("Enter your Amap Key"), this).trimmed();
      if (!key.isEmpty()) {
        params.put("AMapKey1", key.toStdString());
      }
    } else if (FrogPilotConfirmationDialog::yesorno(tr("Remove your Amap Key?"), this)) {
      params.remove("AMapKey1");
    }

    updateButtons();
  });
  settingsList->addItem(amapKeyControl);

  searchProviderControl = new FrogPilotButtonsControl(tr("Destination Search Provider"), tr("<b>Chooses who finds the places you search for on the \"Navigation\" page in \"The Pond\".</b><br><br>Mapbox is the default. Amap is for searching places in China and needs your own \"Amap Key\". The route and the map on your driving screen come from Mapbox either way, so navigation needs both Mapbox keys with either choice."), "", {tr("Mapbox"), tr("Amap")}, true);
  QObject::connect(searchProviderControl, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    params.putInt("SearchInput", id);

    updateButtons();
  });
  settingsList->addItem(searchProviderControl);

  ipLabel = new LabelControl(tr("Manage Your Settings At"), tr("Offline..."), tr("<b>Open this address in a browser on the same Wi-Fi to reach \"The Pond\", where you manage your drives, recordings, themes, navigation, and more from a phone or computer.</b>"));
  settingsList->addItem(ipLabel);

  pairingToggle = new ParamControl("ThePondPairing", tr("Pairing Code"), tr("<b>Ask for a code before a browser can use \"The Pond\".</b><br><br>Without a code, any device on the same Wi-Fi or Tailscale network can open your drives, camera footage, logs, location, and saved places, and can change your settings, unlock your doors, reboot the device, or sign the device into a Tailscale account. The code shows next to the address above. Turning this off signs every browser out, and turning it back on makes a new code."), "");
  QObject::connect(pairingToggle, &ToggleControl::toggleFlipped, [this](bool state) {
    if (state) {
      params.put("ThePondPairingCode", QString::number(QRandomGenerator::system()->bounded(1000000)).rightJustified(6, '0').toStdString());
    } else {
      params.remove("ThePondPairedBrowsers");
      params.remove("ThePondPairingCode");
    }
  });
  settingsList->addItem(pairingToggle);

  publicMapboxKeyControl = new FrogPilotButtonsControl(tr("Public Mapbox Key"), tr("<b>Lets you search for a destination and preview the route without paying for comma's subscription.</b><br><br>It also lets openpilot get speed limits from Mapbox. You make this key yourself on Mapbox's website. Navigation stays locked until both this and the \"Secret Mapbox Key\" are set, so add both. \"Mapbox Setup Instructions\" walks you through it."), "", {tr("ADD"), tr("TEST")});
  QObject::connect(publicMapboxKeyControl, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 0) {
      if (mapboxPublicKeySet) {
        if (FrogPilotConfirmationDialog::yesorno(tr("Remove your Public Mapbox Key?"), this)) {
          params.remove("MapboxPublicKey");

          setupCompleted = false;

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

  secretMapboxKeyControl = new FrogPilotButtonsControl(tr("Secret Mapbox Key"), tr("<b>Draws the map itself on your driving screen and gets your navigation route from Mapbox, alongside what the \"Public Mapbox Key\" does for searching.</b><br><br>Keep this one to yourself, since it can be used to run up charges on your Mapbox account. After you add it you are asked whether to reboot, and the map only appears once the device has actually rebooted."), "", {tr("ADD"), tr("TEST")});
  QObject::connect(secretMapboxKeyControl, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 0) {
      if (mapboxSecretKeySet) {
        if (FrogPilotConfirmationDialog::yesorno(tr("Remove your Secret Mapbox Key?"), this)) {
          params.remove("MapboxSecretKey");

          setupCompleted = false;

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

  speedLimitFillerToggle = new ParamControl("SpeedLimitFiller", tr("Speed Limit Filler"),
                                            tr("<b>Learns speed limits from the signs the camera reads on roads where your downloaded maps have no limit or the wrong one.</b><br><br>"
                                               "Each learned limit is saved for that road and direction and used in place of the map's limit the next time you drive there, "
                                               "and it is forgotten once the map matches the signs. "
                                               "Roads with conditional limits, such as ones that change with the time of day or the weather, are skipped. Needs downloaded maps.<br><br>"
                                               "With \"Share FrogPilot Data\" on, the learned limits, their sign photos, and the roads where the signs matched your map are sent to FrogPilot while parked on unmetered "
                                               "Wi-Fi or Ethernet and used to improve speed limits for everyone using FrogPilot."),
                                            "");
  settingsList->addItem(speedLimitFillerToggle);

  ScrollView *settingsPanel = new ScrollView(settingsList, this);
  primelessLayout->addWidget(settingsPanel);

  imageLabel = new QLabel(this);

  ScrollView *instructionsPanel = new ScrollView(imageLabel, this);
  primelessLayout->addWidget(instructionsPanel);

  toggles["AMapKey1"] = amapKeyControl;
  toggles["ManageSettingsAt"] = ipLabel;
  toggles["MapboxPublicKey"] = publicMapboxKeyControl;
  toggles["MapboxSecretKey"] = secretMapboxKeyControl;
  toggles["MapboxSetup"] = setupButton;
  toggles["SearchInput"] = searchProviderControl;
  toggles["SpeedLimitFiller"] = speedLimitFillerToggle;
  toggles["ThePondPairing"] = pairingToggle;

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

  pairingToggle->setVisible(parent->tuningLevel >= parent->frogpilotToggleLevels.value("ThePondPairing").toDouble());
  speedLimitFillerToggle->setVisible(parent->tuningLevel >= parent->frogpilotToggleLevels.value("SpeedLimitFiller").toDouble());
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

void FrogPilotNavigationPanel::updateButtons() {
  FrogPilotUIState &fs = *frogpilotUIState();

  amapKeyControl->setText(params.get("AMapKey1").empty() ? tr("ADD") : tr("REMOVE"));

  int searchProvider = params.getInt("SearchInput");

  amapKeyControl->setVisible(searchProvider == 1);
  searchProviderControl->setCheckedButton(searchProvider);
  setupButton->setVisible(searchProvider == 0);

  QString ipAddress = fs.wifi->ipv4_address;
  if (ipAddress.isEmpty()) {
    ipLabel->setText(tr("Offline..."));
  } else {
    QString address = QString("%1:8082").arg(ipAddress);
    if (params.getBool("ThePondPairing")) {
      address += tr(" (Code %1)").arg(QString::fromStdString(params.get("ThePondPairingCode")));
    }
    ipLabel->setText(address);
  }

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
