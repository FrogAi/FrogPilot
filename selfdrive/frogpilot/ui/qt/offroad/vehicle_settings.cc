#include <QRegularExpression>
#include <QTextStream>

#include "selfdrive/frogpilot/ui/qt/offroad/vehicle_settings.h"

QStringList getCarNames(const QString &carMake, QMap<QString, QString> &carModels) {
  static const QMap<QString, QString> makeMap = {
    {"acura", "honda"},
    {"audi", "volkswagen"},
    {"buick", "gm"},
    {"cadillac", "gm"},
    {"chevrolet", "gm"},
    {"chrysler", "chrysler"},
    {"cupra", "volkswagen"},
    {"dodge", "chrysler"},
    {"ford", "ford"},
    {"genesis", "hyundai"},
    {"gmc", "gm"},
    {"holden", "gm"},
    {"honda", "honda"},
    {"hyundai", "hyundai"},
    {"jeep", "chrysler"},
    {"kia", "hyundai"},
    {"lexus", "toyota"},
    {"lincoln", "ford"},
    {"man", "volkswagen"},
    {"mazda", "mazda"},
    {"nissan", "nissan"},
    {"ram", "chrysler"},
    {"seat", "volkswagen"},
    {"škoda", "volkswagen"},
    {"subaru", "subaru"},
    {"tesla", "tesla"},
    {"toyota", "toyota"},
    {"volkswagen", "volkswagen"}
  };

  QStringList carNameList;
  QSet<QString> uniqueCarNames;

  QString filePath = QString("../car/%1/values.py").arg(makeMap.value(carMake, carMake));
  QFile valuesFile(filePath);
  if (!valuesFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return carNameList;
  }

  QTextStream in(&valuesFile);
  QString fileContent = in.readAll();
  valuesFile.close();

  fileContent.remove(QRegularExpression("#[^\n]*"));
  fileContent.remove(QRegularExpression("footnotes=\\[[^\\]]*\\],\\s*"));

  static const QRegularExpression carNameRegex("CarDocs\\(\\s*\"([^\"]+)\"[^)]*\\)");
  static const QRegularExpression platformRegex("((\\w+)\\s*=\\s*\\w+\\s*\\(\\s*\\[([\\s\\S]*?)\\]\\s*,)");
  static const QRegularExpression validNameRegex("^[A-Za-z0-9 \u0160.()-]+$");

  QRegularExpressionMatchIterator platformMatches = platformRegex.globalMatch(fileContent);
  while (platformMatches.hasNext()) {
    QRegularExpressionMatch platformMatch = platformMatches.next();
    QString platformName = platformMatch.captured(2);
    QString platformSection = platformMatch.captured(3);

    QRegularExpressionMatchIterator carNameMatches = carNameRegex.globalMatch(platformSection);
    while (carNameMatches.hasNext()) {
      QString carName = carNameMatches.next().captured(1);
      if (carName.contains(validNameRegex) && carName.count(" ") >= 1) {
        QStringList carNameParts = carName.split(" ");
        for (const QString &part : carNameParts) {
          if (part.compare(carMake, Qt::CaseInsensitive) == 0) {
            if (!uniqueCarNames.contains(carName)) {
              uniqueCarNames.insert(carName);
              carNameList.append(carName);
              carModels[carName] = platformName;
            }
            break;
          }
        }
      }
    }
  }

  carNameList.sort();
  return carNameList;
}

FrogPilotVehiclesPanel::FrogPilotVehiclesPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QVBoxLayout *mainLayout = new QVBoxLayout();
  addItem(mainLayout);

  vehiclesLayout = new QStackedLayout();
  mainLayout->addLayout(vehiclesLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);

  QStringList makes = {
    "Acura", "Audi", "Buick", "Cadillac", "Chevrolet", "Chrysler",
    "CUPRA", "Dodge", "Ford", "Genesis", "GMC", "Holden", "Honda",
    "Hyundai", "Jeep", "Kia", "Lexus", "Lincoln", "MAN", "Mazda",
    "Nissan", "Ram", "SEAT", "Škoda", "Subaru", "Tesla", "Toyota",
    "Volkswagen"
  };

  ButtonControl *selectMakeButton = new ButtonControl(tr("Select Make"), tr("SELECT"));
  QObject::connect(selectMakeButton, &ButtonControl::clicked, [this, makes, selectMakeButton]() {
    QString makeSelection = MultiOptionDialog::getSelection(tr("Select a Make"), makes, "", this);
    if (!makeSelection.isEmpty()) {
      params.put("CarMake", makeSelection.toStdString());
      selectMakeButton->setValue(makeSelection);
    }
  });
  settingsList->addItem(selectMakeButton);

  ButtonControl *selectModelButton = new ButtonControl(tr("Select Model"), tr("SELECT"));
  QObject::connect(selectModelButton, &ButtonControl::clicked, [this, selectModelButton]() {
    QString modelSelection = MultiOptionDialog::getSelection(tr("Select a Model"), getCarNames(QString::fromStdString(params.get("CarMake")).toLower(), carModels), "", this);
    if (!modelSelection.isEmpty()) {
      params.put("CarModel", carModels.value(modelSelection).toStdString());
      params.put("CarModelName", modelSelection.toStdString());
      selectModelButton->setValue(modelSelection);
    }
  });
  settingsList->addItem(selectModelButton);

  ParamControl *forceFingerprint = new ParamControl("ForceFingerprint", tr("Disable Automatic Fingerprint Detection"), tr("Forces the selected fingerprint and prevents it from ever changing."), "");
  settingsList->addItem(forceFingerprint);

  disableOpenpilotLong = new ParamControl("DisableOpenpilotLongitudinal", tr("Disable openpilot Longitudinal Control"), tr("Disables openpilot longitudinal control and uses the car's stock ACC instead."), "");
  QObject::connect(disableOpenpilotLong, &ToggleControl::toggleFlipped, [this, parent](bool state) {
    if (state) {
      if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely disable openpilot longitudinal control?"), this)) {
        if (started) {
          if (FrogPilotConfirmationDialog::toggleReboot(this)) {
            Hardware::reboot();
          }
        }
      } else {
        params.putBool("DisableOpenpilotLongitudinal", false);
        disableOpenpilotLong->refresh();
      }
    }

    parent->updateVariables();
    updateToggles();
  });
  settingsList->addItem(disableOpenpilotLong);

  FrogPilotListWidget *gmList = new FrogPilotListWidget(this);
  FrogPilotListWidget *hkgList = new FrogPilotListWidget(this);
  FrogPilotListWidget *toyotaList = new FrogPilotListWidget(this);

  std::vector<std::tuple<QString, QString, QString, QString>> vehicleToggles {
    {"GMToggles", tr("General Motors Toggles"), tr("Toggles catered towards 'General Motors' vehicles."), ""},
    {"ExperimentalGMTune", tr("Enable FrogsGoMoo's Experimental Longitudinal Tune"), tr("Enable FrogsGoMoo's experimental GM longitudinal tune that is based on nothing but guesswork. Use at your own risk!"), ""},
    {"VoltSNG", tr("Enable Stop and Go Hack"), tr("Force stop and go for the 2017 Chevy Volt."), ""},
    {"LongPitch", tr("Smoothen Pedal Response While Going Downhill/Uphill"), tr("Smoothen the gas and brake response when driving downhill or uphill."), ""},

    {"HKGToggles", tr("Hyundai/Kia/Genesis Toggles"), tr("Toggles catered towards 'Hyundai/Kia/Genesis' vehicles."), ""},
    {"NewLongAPI", tr("Enable comma's New Longitudinal API"), tr("Enable comma's new longitudinal control system that has shown great improvement with acceleration and braking, but has issues on some Hyundai/Kia/Genesis vehicles."), ""},

    {"ToyotaToggles", tr("Toyota/Lexus Toggles"), tr("Toggles catered towards 'Toyota/Lexus' vehicles."), ""},
    {"ToyotaDoors", tr("Automatically Lock/Unlock Doors"), tr("Automatically lock the doors when in drive and unlock when in park."), ""},
    {"ClusterOffset", tr("Cluster Speed Offset"), tr("Set the cluster offset openpilot uses to try and match the speed displayed on the dash."), ""},
    {"FrogsGoMoosTweak", tr("Enable FrogsGoMoo's Personal Tweaks"), tr("FrogsGoMoo's personal tweaks that aim to take off faster and stop smoother."), ""},
    {"SNGHack", tr("Enable Stop and Go Hack"), tr("Force stop and go for vehicles without stock stop and go functionality."), ""},
  };

  for (const auto &[param, title, desc, icon] : vehicleToggles) {
    AbstractControl *vehicleToggle;

    if (param == "GMToggles") {
      ButtonControl *gmToggle = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(gmToggle, &ButtonControl::clicked, [this]() {
        vehiclesLayout->setCurrentIndex(1);
        openParentToggle();
      });
      vehicleToggle = gmToggle;

    } else if (param == "HKGToggles") {
      ButtonControl *hkgToggle = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(hkgToggle, &ButtonControl::clicked, [this]() {
        vehiclesLayout->setCurrentIndex(2);
        openParentToggle();
      });
      vehicleToggle = hkgToggle;

    } else if (param == "ToyotaToggles") {
      ButtonControl *toyotaToggle = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(toyotaToggle, &ButtonControl::clicked, [this]() {
        vehiclesLayout->setCurrentIndex(3);
        openParentToggle();
      });
      vehicleToggle = toyotaToggle;
    } else if (param == "ToyotaDoors") {
      std::vector<QString> lockToggles{"LockDoors", "UnlockDoors"};
      std::vector<QString> lockToggleNames{tr("Lock"), tr("Unlock")};
      vehicleToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, lockToggles, lockToggleNames);
    } else if (param == "ClusterOffset") {
      std::vector<QString> clusterOffsetButton{"Reset"};
      FrogPilotParamValueButtonControl *clusterOffsetToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 1.000, 1.050, "x", std::map<float, QString>(), 0.001, {}, clusterOffsetButton, false, false);
      QObject::connect(clusterOffsetToggle, &FrogPilotParamValueButtonControl::buttonClicked, [this, clusterOffsetToggle]() {
        params.putFloat("ClusterOffset", params_default.getFloat("ClusterOffset"));
        clusterOffsetToggle->refresh();
      });
      vehicleToggle = clusterOffsetToggle;

    } else {
      vehicleToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = vehicleToggle;

    if (gmKeys.find(param) != gmKeys.end()) {
      gmList->addItem(vehicleToggle);
    } else if (hkgKeys.find(param) != hkgKeys.end()) {
      hkgList->addItem(vehicleToggle);
    } else if (toyotaKeys.find(param) != toyotaKeys.end()) {
      toyotaList->addItem(vehicleToggle);
    } else {
      settingsList->addItem(vehicleToggle);
    }
  }

  ScrollView *settingsPanel = new ScrollView(settingsList, this);
  vehiclesLayout->addWidget(settingsPanel);

  ScrollView *gmPanel = new ScrollView(gmList, this);
  vehiclesLayout->addWidget(gmPanel);
  ScrollView *hkgPanel = new ScrollView(hkgList, this);
  vehiclesLayout->addWidget(hkgPanel);
  ScrollView *toyotaPanel = new ScrollView(toyotaList, this);
  vehiclesLayout->addWidget(toyotaPanel);

  std::set<QString> rebootKeys = {"NewLongAPI"};
  for (const QString &key : rebootKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, [this]() {
      if (started) {
        if (FrogPilotConfirmationDialog::toggleReboot(this)) {
          Hardware::reboot();
        }
      }
    });
  }

  QObject::connect(uiState(), &UIState::offroadTransition, [this, selectMakeButton, selectModelButton]() {
    std::thread([this, selectMakeButton, selectModelButton]() {
      selectMakeButton->setValue(QString::fromStdString(params.get("CarMake", true)));
      selectModelButton->setValue(QString::fromStdString(params.get(params.get("CarModelName").empty() ? "CarModel" : "CarModelName")));
    }).detach();
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, [this] {vehiclesLayout->setCurrentIndex(0);});
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotVehiclesPanel::updateState);
}

void FrogPilotVehiclesPanel::showEvent(QShowEvent *event) {
  updateToggles();
}

void FrogPilotVehiclesPanel::updateState(const UIState &s) {
  if (!isVisible()) {
    return;
  }

  started = s.scene.started;
}

void FrogPilotVehiclesPanel::updateToggles() {
  toggles["GMToggles"]->setVisible(false);
  toggles["HKGToggles"]->setVisible(false);
  toggles["ToyotaToggles"]->setVisible(false);

  for (auto &[key, toggle] : toggles) {
    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels[key].toDouble();

    if (key == "GMToggles" || gmKeys.find(key) != gmKeys.end()) {
      setVisible = parent->isGM;
    } else if (key == "HKGToggles" || hkgKeys.find(key) != hkgKeys.end()) {
      setVisible = parent->isHKG;
    } else if (key == "ToyotaToggles" || toyotaKeys.find(key) != toyotaKeys.end()) {
      setVisible = parent->isToyota;
    }

    if (longitudinalKeys.find(key) != longitudinalKeys.end()) {
      setVisible &= parent->hasOpenpilotLongitudinal;
    }

    if (key == "SNGHack") {
      setVisible &= !parent->hasSNG;
    }

    if (key == "VoltSNG") {
      setVisible &= parent->isVolt && !parent->hasSNG;
    }

    toggle->setVisible(setVisible);

    if (gmKeys.find(key) != gmKeys.end() && setVisible) {
      toggles["GMToggles"]->setVisible(true);
    }

    if (hkgKeys.find(key) != hkgKeys.end() && setVisible) {
      toggles["HKGToggles"]->setVisible(true);
    }

    if (toyotaKeys.find(key) != toyotaKeys.end() && setVisible) {
      toggles["ToyotaToggles"]->setVisible(true);
    }
  }

  disableOpenpilotLong->setVisible((parent->hasOpenpilotLongitudinal || params.getBool("DisableOpenpilotLongitudinal")) && !parent->hasExperimentalOpenpilotLongitudinal);
}
