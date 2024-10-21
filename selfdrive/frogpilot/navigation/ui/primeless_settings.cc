#include "selfdrive/frogpilot/navigation/ui/primeless_settings.h"

FrogPilotPrimelessPanel::FrogPilotPrimelessPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent) {
  addItem(ipLabel = new LabelControl(tr("Manage Your Settings At"), tr("Device Offline")));

  std::vector<QString> searchOptions{tr("MapBox"), tr("Amap"), tr("Google")};
  searchInput = new ButtonParamControl("SearchInput", tr("Destination Search Provider"),
                                    tr("The search provider used for destination queries in 'Navigate on Openpilot'. Options include 'MapBox' (recommended), 'Amap', and 'Google Maps'."),
                                       "", searchOptions);
  addItem(searchInput);

  createMapboxKeyControl(publicMapboxKeyControl, tr("Public Mapbox Key"), "MapboxPublicKey", "pk.");
  createMapboxKeyControl(secretMapboxKeyControl, tr("Secret Mapbox Key"), "MapboxSecretKey", "sk.");

  setupButton = new ButtonControl(tr("MapBox Setup Instructions"), tr("VIEW"), tr("View the instructions to set up 'MapBox' for 'Primeless Navigation'."), this);
  QObject::connect(setupButton, &ButtonControl::clicked, [this]() {
    displayMapboxInstructions(true);
    openMapBoxInstructions();
    updateStep();
  });
  addItem(setupButton);

  imageLabel = new QLabel(this);
  addItem(imageLabel);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeMapBoxInstructions, [this]() {displayMapboxInstructions(false);});
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotPrimelessPanel::updateState);

  displayMapboxInstructions(false);
}

void FrogPilotPrimelessPanel::showEvent(QShowEvent *event) {
  QString ipAddress = uiState()->wifi->getIp4Address();
  ipLabel->setText(ipAddress.isEmpty() ? tr("Device Offline") : QString("%1:8082").arg(ipAddress));

  mapboxPublicKeySet = !params.get("MapboxPublicKey").empty();
  mapboxSecretKeySet = !params.get("MapboxSecretKey").empty();
  setupCompleted = mapboxPublicKeySet && mapboxSecretKeySet;

  publicMapboxKeyControl->setText(mapboxPublicKeySet ? tr("REMOVE") : tr("ADD"));
  secretMapboxKeyControl->setText(mapboxSecretKeySet ? tr("REMOVE") : tr("ADD"));
}

void FrogPilotPrimelessPanel::updateState() {
  if (!isVisible()) {
    return;
  }

  if (imageLabel->isVisible()) {
    mapboxPublicKeySet = !params.get("MapboxPublicKey").empty();
    mapboxSecretKeySet = !params.get("MapboxSecretKey").empty();

    updateStep();
  }
}

void FrogPilotPrimelessPanel::createMapboxKeyControl(ButtonControl *&control, const QString &label, const std::string &paramKey, const QString &prefix) {
  control = new ButtonControl(label, "", tr("Manage your %1.").arg(label));

  QObject::connect(control, &ButtonControl::clicked, [=] {
    if (control->text() == tr("ADD")) {
      QString key = InputDialog::getText(tr("Enter your %1").arg(label), this);

      if (!key.startsWith(prefix)) {
        key = prefix + key;
      }
      if (key.length() >= 80) {
        params.putNonBlocking(paramKey, key.toStdString());
      } else {
        FrogPilotConfirmationDialog::toggleAlert(tr("Inputted key is invalid or too short!"), tr("Okay"), this);
      }
    } else {
      if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to remove your %1?").arg(label), this)) {
        control->setText(tr("ADD"));

        params.remove(paramKey);
        paramsStorage.remove(paramKey);

        setupCompleted = false;
      }
    }
  });

  control->setText(params.get(paramKey).empty() ? tr("ADD") : tr("REMOVE"));
  addItem(control);
}

void FrogPilotPrimelessPanel::hideEvent(QHideEvent *event) {
  displayMapboxInstructions(false);
}

void FrogPilotPrimelessPanel::mousePressEvent(QMouseEvent *event) {
  closeMapBoxInstructions();
  displayMapboxInstructions(false);
}

void FrogPilotPrimelessPanel::displayMapboxInstructions(bool visible) {
  setUpdatesEnabled(false);

  imageLabel->setVisible(visible);
  ipLabel->setVisible(!visible);
  publicMapboxKeyControl->setVisible(!visible);
  searchInput->setVisible(!visible);
  secretMapboxKeyControl->setVisible(!visible);
  setupButton->setVisible(!visible);

  setUpdatesEnabled(true);
  update();
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
