#include "selfdrive/frogpilot/navigation/ui/primeless_settings.h"

void FrogPilotPrimelessPanel::createMapboxKeyControl(ButtonControl *&control, const QString &label, const std::string &paramKey, const QString &prefix, FrogPilotListWidget *list) {
  control = new ButtonControl(label, "", tr("Manage your %1.").arg(label));
  QObject::connect(control, &ButtonControl::clicked, [=] {
    if (control->text() == tr("ADD")) {
      QString key = InputDialog::getText(tr("Enter your %1").arg(label), this).trimmed();

      if (!key.startsWith(prefix)) {
        key = prefix + key;
      }
      if (key.length() >= 80) {
        params.put(paramKey, key.toStdString());
      } else {
        ConfirmationDialog::alert(tr("Inputted key is invalid or too short!"), this);
      }
    } else {
      if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to remove your %1?").arg(label), this)) {
        control->setText(tr("ADD"));

        params.put(paramKey, "0");
        params_cache.put(paramKey, "0");

        setupCompleted = false;
      }
    }
  });
  control->setText(QString::fromStdString(params.get(paramKey)).startsWith(prefix) ? tr("REMOVE") : tr("ADD"));
  list->addItem(control);
}

FrogPilotPrimelessPanel::FrogPilotPrimelessPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QVBoxLayout *mainLayout = new QVBoxLayout();
  addItem(mainLayout);

  primelessLayout = new QStackedLayout();
  mainLayout->addLayout(primelessLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);
  ipLabel = new LabelControl(tr("Manage Your Settings At"), tr("Device Offline"));
  settingsList->addItem(ipLabel);

  std::vector<QString> searchOptions{tr("MapBox"), tr("Amap"), tr("Google")};
  FrogPilotButtonsControl *searchInput = new FrogPilotButtonsControl(tr("Destination Search Provider"),
                                         tr("The search provider used for destination queries in 'Navigate on Openpilot'. "
                                         "Options include 'MapBox' (recommended), 'Amap', and 'Google Maps'."),
                                         "", searchOptions, true);

  QObject::connect(searchInput, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    amapKeyControl1->setVisible(id == 1);
    amapKeyControl2->setVisible(id == 1);

    googleKeyControl->setVisible(id == 2);

    publicMapboxKeyControl->setVisible(id == 0);
    secretMapboxKeyControl->setVisible(id == 0);

    params.putInt("SearchInput", id);
    update();
  });
  searchInput->setCheckedButton(params.getInt("SearchInput"));
  settingsList->addItem(searchInput);

  amapKeyControl1 = new ButtonControl(tr("Amap Key #1"), "", tr("Manage your Amap key."));
  QObject::connect(amapKeyControl1, &ButtonControl::clicked, [=] {
    if (amapKeyControl1->text() == tr("ADD")) {
      QString key = InputDialog::getText(tr("Enter your Amap key"), this).trimmed();

      if (key.length() >= 39) {
        params.put("AMapKey1", key.toStdString());
      } else {
        ConfirmationDialog::alert(tr("Inputted key is invalid or too short!"), this);
      }
    } else {
      if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to remove your Amap key?"), this)) {
        amapKeyControl1->setText(tr("ADD"));

        params.put("AMapKey1", "0");
        params_cache.put("AMapKey1", "0");
      }
    }
  });
  amapKeyControl1->setText(QString::fromStdString(params.get("AMapKey1")) != "0" ? tr("REMOVE") : tr("ADD"));
  settingsList->addItem(amapKeyControl1);

  amapKeyControl2 = new ButtonControl(tr("Amap Key #2"), "", tr("Manage your Amap key."));
  QObject::connect(amapKeyControl2, &ButtonControl::clicked, [=] {
    if (amapKeyControl2->text() == tr("ADD")) {
      QString key = InputDialog::getText(tr("Enter your Amap key"), this).trimmed();

      if (key.length() >= 39) {
        params.put("AMapKey2", key.toStdString());
      } else {
        ConfirmationDialog::alert(tr("Inputted key is invalid or too short!"), this);
      }
    } else {
      if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to remove your Amap key?"), this)) {
        amapKeyControl2->setText(tr("ADD"));

        params.put("AMapKey2", "0");
        params_cache.put("AMapKey2", "0");
      }
    }
  });
  amapKeyControl2->setText(QString::fromStdString(params.get("AMapKey2")) != "0" ? tr("REMOVE") : tr("ADD"));
  settingsList->addItem(amapKeyControl2);

  googleKeyControl = new ButtonControl(tr("Google Maps Key"), "", tr("Manage your Google Maps key."));
  QObject::connect(googleKeyControl, &ButtonControl::clicked, [=] {
    if (googleKeyControl->text() == tr("ADD")) {
      QString key = InputDialog::getText(tr("Enter your Google Maps key"), this).trimmed();

      if (key.length() >= 25) {
        params.put("GMapKey", key.toStdString());
      } else {
        ConfirmationDialog::alert(tr("Inputted key is invalid or too short!"), this);
      }
    } else {
      if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to remove your Google Maps key?"), this)) {
        googleKeyControl->setText(tr("ADD"));

        params.put("GMapKey", "0");
        params_cache.put("GMapKey", "0");
      }
    }
  });
  googleKeyControl->setText(QString::fromStdString(params.get("GMapKey")) != "0" ? tr("REMOVE") : tr("ADD"));
  settingsList->addItem(googleKeyControl);

  createMapboxKeyControl(publicMapboxKeyControl, tr("Public Mapbox Key"), "MapboxPublicKey", "pk.", settingsList);
  createMapboxKeyControl(secretMapboxKeyControl, tr("Secret Mapbox Key"), "MapboxSecretKey", "sk.", settingsList);

  ButtonControl *setupButton = new ButtonControl(tr("MapBox Setup Instructions"), tr("VIEW"), tr("View the instructions to set up 'MapBox' for 'Primeless Navigation'."), this);
  QObject::connect(setupButton, &ButtonControl::clicked, [this]() {
    openMapBoxInstructions();
    updateStep();
    primelessLayout->setCurrentIndex(1);
  });
  settingsList->addItem(setupButton);

  ScrollView *settingsPanel = new ScrollView(settingsList, this);
  primelessLayout->addWidget(settingsPanel);

  imageLabel = new QLabel(this);

  ScrollView *instructionsPanel = new ScrollView(imageLabel, this);
  primelessLayout->addWidget(instructionsPanel);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeMapBoxInstructions, [this] {primelessLayout->setCurrentIndex(0);});
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotPrimelessPanel::updateState);
}

void FrogPilotPrimelessPanel::showEvent(QShowEvent *event) {
  QString ipAddress = uiState()->wifi->getIp4Address();
  ipLabel->setText(ipAddress.isEmpty() ? tr("Device Offline") : QString("%1:8082").arg(ipAddress));

  updateButtons();

  setupCompleted = mapboxPublicKeySet && mapboxSecretKeySet;

  int searchInput = params.getInt("SearchInput");

  amapKeyControl1->setVisible(searchInput == 1);
  amapKeyControl2->setVisible(searchInput == 1);

  googleKeyControl->setVisible(searchInput == 2);

  publicMapboxKeyControl->setVisible(searchInput == 0);
  secretMapboxKeyControl->setVisible(searchInput == 0);
}

void FrogPilotPrimelessPanel::hideEvent(QHideEvent *event) {
  primelessLayout->setCurrentIndex(0);
}

void FrogPilotPrimelessPanel::mousePressEvent(QMouseEvent *event) {
  if (primelessLayout->currentIndex() == 1) {
    closeMapBoxInstructions();
    primelessLayout->setCurrentIndex(0);
  }
}

void FrogPilotPrimelessPanel::updateButtons() {
  amapKeyControl1->setText(QString::fromStdString(params.get("AMapKey1")) != "0" ? tr("REMOVE") : tr("ADD"));
  amapKeyControl2->setText(QString::fromStdString(params.get("AMapKey2")) != "0" ? tr("REMOVE") : tr("ADD"));

  googleKeyControl->setText(QString::fromStdString(params.get("GMapKey")) != "0" ? tr("REMOVE") : tr("ADD"));

  mapboxPublicKeySet = QString::fromStdString(params.get("MapboxPublicKey")).startsWith("pk");
  mapboxSecretKeySet = QString::fromStdString(params.get("MapboxSecretKey")).startsWith("sk");

  publicMapboxKeyControl->setText(mapboxPublicKeySet ? tr("REMOVE") : tr("ADD"));
  secretMapboxKeyControl->setText(mapboxSecretKeySet ? tr("REMOVE") : tr("ADD"));
}

void FrogPilotPrimelessPanel::updateState(const UIState &s) {
  if (!isVisible() || s.sm->frame % (UI_FREQ / 2) != 0) {
    return;
  }

  updateButtons();
  updateStep();

  parent->keepScreenOn = primelessLayout->currentIndex() == 1;
}

void FrogPilotPrimelessPanel::updateStep() {
  QString currentStep;
  if (setupCompleted) {
    currentStep = "../frogpilot/navigation/navigation_training/setup_completed.png";
  } else if (mapboxPublicKeySet && mapboxSecretKeySet) {
    currentStep = "../frogpilot/navigation/navigation_training/both_keys_set.png";
  } else if (mapboxPublicKeySet) {
    currentStep = "../frogpilot/navigation/navigation_training/public_key_set.png";
  } else {
    currentStep = "../frogpilot/navigation/navigation_training/no_keys_set.png";
  }

  QPixmap pixmap;
  pixmap.load(currentStep);
  imageLabel->setPixmap(pixmap.scaledToWidth(1500, Qt::SmoothTransformation));

  update();
}
