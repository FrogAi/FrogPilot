#include <QMouseEvent>

#include "selfdrive/frogpilot/navigation/ui/navigation_settings.h"

FrogPilotNavigationPanel::FrogPilotNavigationPanel(QWidget *parent) : QFrame(parent), scene(uiState()->scene) {
  mainLayout = new QStackedLayout(this);

  navigationWidget = new QWidget();
  QVBoxLayout *navigationLayout = new QVBoxLayout(navigationWidget);
  navigationLayout->setMargin(40);

  FrogPilotListWidget *list = new FrogPilotListWidget(navigationWidget);

  Primeless *primelessPanel = new Primeless(this);
  mainLayout->addWidget(primelessPanel);

  ButtonControl *manageNOOButton = new ButtonControl(tr("Manage Primeless Navigation Settings"), tr("MANAGE"), tr("Manage primeless navigate on openpilot settings."));
  QObject::connect(manageNOOButton, &ButtonControl::clicked, [=]() { mainLayout->setCurrentWidget(primelessPanel); });
  QObject::connect(primelessPanel, &Primeless::backPress, [=]() { mainLayout->setCurrentWidget(navigationWidget); });
  list->addItem(manageNOOButton);
  manageNOOButton->setVisible(!uiState()->hasPrime());
}

Primeless::Primeless(QWidget *parent) : QWidget(parent) {
  QStackedLayout *primelessLayout = new QStackedLayout(this);

  QWidget *mainWidget = new QWidget();
  mainLayout = new QVBoxLayout(mainWidget);
  mainLayout->setMargin(40);

  backButton = new QPushButton(tr("Back"), this);
  backButton->setObjectName("backButton");
  backButton->setFixedSize(400, 100);
  QObject::connect(backButton, &QPushButton::clicked, this, [this]() { emit backPress(); });
  mainLayout->addWidget(backButton, 0, Qt::AlignLeft);

  list = new FrogPilotListWidget(mainWidget);

  wifi = new WifiManager(this);
  ipLabel = new LabelControl(tr("Manage Your Settings At"), QString("%1:8082").arg(wifi->getIp4Address()));
  list->addItem(ipLabel);

  std::vector<QString> searchOptions{tr("MapBox"), tr("Amap"), tr("Google")};
  ButtonParamControl *searchInput = new ButtonParamControl("SearchInput", tr("Destination Search Provider"),
                                       tr("Select a search provider for destination queries in Navigate on Openpilot. Options include MapBox (recommended), Amap, and Google Maps."),
                                       "", searchOptions);
  list->addItem(searchInput);

  createMapboxKeyControl(publicMapboxKeyControl, tr("Public Mapbox Key"), "MapboxPublicKey", "pk.");
  createMapboxKeyControl(secretMapboxKeyControl, tr("Secret Mapbox Key"), "MapboxSecretKey", "sk.");

  mapboxPublicKeySet = !params.get("MapboxPublicKey").empty();
  mapboxSecretKeySet = !params.get("MapboxSecretKey").empty();
  setupCompleted = mapboxPublicKeySet && mapboxSecretKeySet;

  QHBoxLayout *setupLayout = new QHBoxLayout();
  setupLayout->setMargin(0);

  imageLabel = new QLabel(this);
  pixmap.load(currentStep);
  imageLabel->setPixmap(pixmap.scaledToWidth(1500, Qt::SmoothTransformation));
  setupLayout->addWidget(imageLabel, 0, Qt::AlignCenter);
  imageLabel->hide();

  ButtonControl *setupButton = new ButtonControl(tr("Setup Instructions"), tr("VIEW"), tr("View the instructions to set up MapBox for Primeless Navigation."), this);
  QObject::connect(setupButton, &ButtonControl::clicked, this, [this]() {
    updateStep();
    backButton->hide();
    list->setVisible(false);
    imageLabel->show();
  });
  list->addItem(setupButton);

  QObject::connect(uiState(), &UIState::uiUpdate, this, &Primeless::updateState);

  mainLayout->addLayout(setupLayout);
  mainLayout->addWidget(new ScrollView(list, mainWidget));
  mainWidget->setLayout(mainLayout);
  primelessLayout->addWidget(mainWidget);

  setLayout(primelessLayout);

  setStyleSheet(R"(
    QPushButton {
      font-size: 50px;
      margin: 0px;
      padding: 15px;
      border-width: 0;
      border-radius: 30px;
      color: #dddddd;
      background-color: #393939;
    }
    QPushButton:pressed {
      background-color: #4a4a4a;
    }
  )");
}

void Primeless::hideEvent(QHideEvent *event) {
  QWidget::hideEvent(event);
  backButton->show();
  list->setVisible(true);
  imageLabel->hide();
}

void Primeless::mousePressEvent(QMouseEvent *event) {
  backButton->show();
  list->setVisible(true);
  imageLabel->hide();
}

void Primeless::updateState() {
  if (!isVisible()) return;

  QString ipAddress = wifi->getIp4Address();
  ipLabel->setText(ipAddress.isEmpty() ? tr("Device Offline") : QString("%1:8082").arg(ipAddress));

  mapboxPublicKeySet = !params.get("MapboxPublicKey").empty();
  mapboxSecretKeySet = !params.get("MapboxSecretKey").empty();
  setupCompleted = mapboxPublicKeySet && mapboxSecretKeySet && setupCompleted;

  publicMapboxKeyControl->setText(mapboxPublicKeySet ? tr("REMOVE") : tr("ADD"));
  secretMapboxKeyControl->setText(mapboxSecretKeySet ? tr("REMOVE") : tr("ADD"));

  if (imageLabel->isVisible()) {
    updateStep();
  }
}

void Primeless::createMapboxKeyControl(ButtonControl *&control, const QString &label, const std::string &paramKey, const QString &prefix) {
  control = new ButtonControl(label, "", tr("Manage your %1."), this);
  QObject::connect(control, &ButtonControl::clicked, this, [this, control, label, paramKey, prefix] {
    if (control->text() == tr("ADD")) {
      QString key = InputDialog::getText(tr("Enter your %1").arg(label), this);
      if (!key.startsWith(prefix)) {
        key = prefix + key;
      }
      if (key.length() >= 80) {
        params.put(paramKey, key.toStdString());
      }
    } else {
      params.remove(paramKey);
    }
  });
  list->addItem(control);
  control->setText(params.get(paramKey).empty() ? tr("ADD") : tr("REMOVE"));
}

void Primeless::updateStep() {
  currentStep = setupCompleted ? "../frogpilot/navigation/navigation_training/setup_completed.png" :
                (mapboxPublicKeySet && mapboxSecretKeySet) ? "../frogpilot/navigation/navigation_training/both_keys_set.png" :
                mapboxPublicKeySet ? "../frogpilot/navigation/navigation_training/public_key_set.png" : "../frogpilot/navigation/navigation_training/no_keys_set.png";

  pixmap.load(currentStep);
  imageLabel->setPixmap(pixmap.scaledToWidth(1500, Qt::SmoothTransformation));
}
