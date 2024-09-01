#include <QDir>
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

  QString targetFolder = makeMap.value(carMake.toLower(), carMake);
  QFile file(QString("../car/%1/values.py").arg(targetFolder));
  QStringList names;
  QSet<QString> uniqueNames;

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return names;
  }

  QString fileContent = QTextStream(&file).readAll();
  file.close();

  fileContent.remove(QRegularExpression("#[^\n]*"));
  fileContent.remove(QRegularExpression("footnotes=\\[[^\\]]*\\],\\s*"));

  QRegularExpression carModelRegex(R"((\w+)\s*=\s*\w+\s*\(\s*\[([\s\S]*?)\]\s*,)");
  QRegularExpression carDocsRegex("CarDocs\\(\\s*\"([^\"]+)\"[^)]*\\)");

  QRegularExpressionMatchIterator carModelIt = carModelRegex.globalMatch(fileContent);
  while (carModelIt.hasNext()) {
    QRegularExpressionMatch carModelMatch = carModelIt.next();
    QString platform = carModelMatch.captured(1);
    QString platformSection = carModelMatch.captured(2);

    QRegularExpressionMatchIterator carDocsIt = carDocsRegex.globalMatch(platformSection);
    while (carDocsIt.hasNext()) {
      QString carName = carDocsIt.next().captured(1);

      if (carName.contains(QRegularExpression("^[A-Za-z0-9 Š.()-]+$")) && carName.count(" ") >= 1) {
        QStringList nameParts = carName.split(" ");
        if (nameParts.contains(carMake, Qt::CaseInsensitive) && !uniqueNames.contains(carName)) {
          uniqueNames.insert(carName);
          names << carName;
          carModels[carName] = platform;
        }
      }
    }
  }

  std::sort(names.begin(), names.end());
  return names;
}

FrogPilotVehiclesPanel::FrogPilotVehiclesPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QStringList makes = {
    "Acura", "Audi", "Buick", "Cadillac", "Chevrolet", "Chrysler", "Dodge", "Ford", "Genesis",
    "GMC", "Holden", "Honda", "Hyundai", "Jeep", "Kia", "Lexus", "Lincoln", "MAN", "Mazda",
    "Nissan", "Ram", "SEAT", "Škoda", "Subaru", "Tesla", "Toyota", "Volkswagen"
  };
  selectMakeButton = new ButtonControl(tr("Select Make"), tr("SELECT"));
  QObject::connect(selectMakeButton, &ButtonControl::clicked, [this, makes]() {
    QString newMakeSelection = MultiOptionDialog::getSelection(tr("Select a Make"), makes, "", this);
    if (!newMakeSelection.isEmpty()) {
      carMake = newMakeSelection;
      params.putNonBlocking("CarMake", carMake.toStdString());
      selectMakeButton->setValue(newMakeSelection);
      setModels();
    }
  });
  addItem(selectMakeButton);

  selectModelButton = new ButtonControl(tr("Select Model"), tr("SELECT"));
  QObject::connect(selectModelButton, &ButtonControl::clicked, [this]() {
    QString newModelSelection = MultiOptionDialog::getSelection(tr("Select a Model"), models, "", this);
    if (!newModelSelection.isEmpty()) {
      carModel = newModelSelection;
      QString modelIdentifier = carModels.value(newModelSelection);
      params.putNonBlocking("CarModel", modelIdentifier.toStdString());
      params.putNonBlocking("CarModelName", newModelSelection.toStdString());
      selectModelButton->setValue(newModelSelection);
    }
  });
  addItem(selectModelButton);
  selectModelButton->setVisible(false);

  ParamControl *forceFingerprint = new ParamControl("ForceFingerprint", tr("Disable Automatic Fingerprint Detection"), tr("Forces the selected fingerprint and prevents it from ever changing."), "");
  addItem(forceFingerprint);

  bool disableOpenpilotLongState = params.getBool("DisableOpenpilotLongitudinal");
  disableOpenpilotLong = new ToggleControl(tr("Disable openpilot Longitudinal Control"), tr("Disable openpilot longitudinal control and use stock ACC instead."), "", disableOpenpilotLongState);
  QObject::connect(disableOpenpilotLong, &ToggleControl::toggleFlipped, [this](bool state) {
    if (state) {
      if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely disable openpilot longitudinal control?"), this)) {
        params.putBool("DisableOpenpilotLongitudinal", state);
        if (started) {
          if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
            Hardware::reboot();
          }
        }
      } else {
        disableOpenpilotLong->refresh();
      }
    } else {
      params.putBool("DisableOpenpilotLongitudinal", state);
    }
  });
  addItem(disableOpenpilotLong);

  std::vector<std::tuple<QString, QString, QString, QString>> vehicleToggles {
    {"VoltSNG", tr("2017 Volt Stop and Go Hack"), tr("Force stop and go for the 2017 Chevy Volt."), ""},
    {"ExperimentalGMTune", tr("Experimental GM Tune"), tr("FrogsGoMoo's experimental GM tune that is based on nothing but guesswork. Use at your own risk!"), ""},
    {"LongPitch", tr("Uphill/Downhill Smoothing"), tr("Smoothen the car’s gas and brake response when driving on slopes."), ""},
    {"NewLongAPIGM", tr("Use comma's New Longitudinal API"), tr("Comma's new longitudinal control system that has shown great improvement with acceleration and braking, but has a few issues on some GM vehicles."), ""},

    {"NewLongAPI", tr("Use comma's New Longitudinal API"), tr("Use comma's new longitudinal control system that has shown great improvement with acceleration and braking, but has a few issues on Hyundai/Kia/Genesis."), ""},

    {"CrosstrekTorque", tr("Subaru Crosstrek Torque Increase"), tr("Increases the maximum allowed torque for the Subaru Crosstrek."), ""},

    {"ToyotaDoors", tr("Automatically Lock/Unlock Doors"), tr("Automatically lock the doors when in drive and unlock when in park."), ""},
    {"ClusterOffset", tr("Cluster Speed Offset"), tr("Set the cluster offset openpilot uses to try and match the speed displayed on the dash."), ""},
    {"NewToyotaTune", tr("comma's New Toyota/Lexus Tune"), tr("Activate comma's latest Toyota tuning, expertly crafted by Shane for enhanced vehicle performance."), ""},
    {"SNGHack", tr("Stop and Go Hack"), tr("Force stop and go for vehicles without stock stop and go functionality."), ""},
  };

  for (const auto &[param, title, desc, icon] : vehicleToggles) {
    AbstractControl *vehicleToggle;

    if (param == "ToyotaDoors") {
      std::vector<QString> lockToggles{"LockDoors", "UnlockDoors"};
      std::vector<QString> lockToggleNames{tr("Lock"), tr("Unlock")};
      vehicleToggle = new FrogPilotButtonToggleControl(param, title, desc, lockToggles, lockToggleNames);
    } else if (param == "ClusterOffset") {
      std::vector<QString> clusterOffsetToggleNames{"Reset"};
      vehicleToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 1.000, 1.050, "x", std::map<int, QString>(), 0.001, {}, clusterOffsetToggleNames, false);
    } else if (param == "NewToyotaTune") {
      std::vector<QString> toyotaTuneToggles{"FrogsGoMoosTweak"};
      std::vector<QString> toyotaTuneToggleNames{tr("FrogsGoMoo's Personal Tweaks")};
      vehicleToggle = new FrogPilotButtonToggleControl(param, title, desc, toyotaTuneToggles, toyotaTuneToggleNames);

    } else {
      vehicleToggle = new ParamControl(param, title, desc, icon);
    }

    vehicleToggle->setVisible(false);
    addItem(vehicleToggle);
    toggles[param] = vehicleToggle;

    makeConnections(vehicleToggle);

    QObject::connect(vehicleToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  FrogPilotParamValueButtonControl *clusterOffsetToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["ClusterOffset"]);
  QObject::connect(clusterOffsetToggle, &FrogPilotParamValueButtonControl::buttonClicked, [=]() {
    params.putFloat("ClusterOffset", 1.015);
    clusterOffsetToggle->refresh();
    updateFrogPilotToggles();
  });

  std::set<QString> rebootKeys = {"CrosstrekTorque", "ExperimentalGMTune", "NewLongAPI", "NewLongAPIGM", "NewToyotaTune"};
  for (const QString &key : rebootKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, [this]() {
      if (started) {
        if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
          Hardware::reboot();
        }
      }
    });
  }

  QObject::connect(uiState(), &UIState::offroadTransition, [this]() {
    std::thread([this]() {
      while (carMake.isEmpty()) {
        util::sleep_for(1000);
        carMake = QString::fromStdString(params.get("CarMake"));
        carModel = QString::fromStdString(params.get(params.get("CarModelName").empty() ? "CarModel" : "CarModelName"));
      }
      setModels();
    }).detach();
  });

  carMake = QString::fromStdString(params.get("CarMake"));
  carModel = QString::fromStdString(params.get(params.get("CarModelName").empty() ? "CarModel" : "CarModelName"));

  if (!carMake.isEmpty()) {
    setModels();
  }

  QObject::connect(parent, &FrogPilotSettingsWindow::updateCarToggles, this, &FrogPilotVehiclesPanel::updateCarToggles);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotVehiclesPanel::updateState);
}

void FrogPilotVehiclesPanel::updateCarToggles() {
  disableOpenpilotLongitudinal = parent->disableOpenpilotLongitudinal;
  hasExperimentalOpenpilotLongitudinal = parent->hasExperimentalOpenpilotLongitudinal;
  hasOpenpilotLongitudinal = parent->hasOpenpilotLongitudinal;
  hasSNG = parent->hasSNG;
  isGMPCMCruise = parent->isGMPCMCruise;
  isImpreza = parent->isImpreza;
  isVolt = parent->isVolt;

  updateToggles();
}

void FrogPilotVehiclesPanel::updateState(const UIState &s) {
  if (!isVisible()) return;

  started = s.scene.started;
}

void FrogPilotVehiclesPanel::setModels() {
  models = getCarNames(carMake.toLower(), carModels);
  updateToggles();
}

void FrogPilotVehiclesPanel::updateToggles() {
  setUpdatesEnabled(false);

  disableOpenpilotLong->setVisible((hasOpenpilotLongitudinal && !hasExperimentalOpenpilotLongitudinal && !isGMPCMCruise) || disableOpenpilotLongitudinal);

  selectMakeButton->setValue(carMake);
  selectModelButton->setValue(carModel);
  selectModelButton->setVisible(!carMake.isEmpty());

  bool gm = carMake == "Buick" || carMake == "Cadillac" || carMake == "Chevrolet" || carMake == "GM" || carMake == "GMC";
  bool hyundai = carMake == "Genesis" || carMake == "Hyundai" || carMake == "Kia";
  bool subaru = carMake == "Subaru";
  bool toyota = carMake == "Lexus" || carMake == "Toyota";

  for (auto &[key, toggle] : toggles) {
    bool setVisible = false;

    if (gm && gmKeys.find(key) != gmKeys.end()) {
      if (voltKeys.find(key) != voltKeys.end()) {
        setVisible = isVolt && hasOpenpilotLongitudinal && !disableOpenpilotLongitudinal;
      } else if (longitudinalKeys.find(key) != longitudinalKeys.end()) {
        setVisible = hasOpenpilotLongitudinal && !disableOpenpilotLongitudinal;
      } else {
        setVisible = true;
      }
    } else if (hyundai && hyundaiKeys.find(key) != hyundaiKeys.end()) {
      if (longitudinalKeys.find(key) != longitudinalKeys.end()) {
        setVisible = hasOpenpilotLongitudinal && !disableOpenpilotLongitudinal;
      } else {
        setVisible = true;
      }
    } else if (subaru && subaruKeys.find(key) != subaruKeys.end()) {
      if (imprezaKeys.find(key) != imprezaKeys.end()) {
        setVisible = isImpreza;
      } else if (longitudinalKeys.find(key) != longitudinalKeys.end()) {
        setVisible = hasOpenpilotLongitudinal && !disableOpenpilotLongitudinal;
      } else {
        setVisible = true;
      }
    } else if (toyota && toyotaKeys.find(key) != toyotaKeys.end()) {
      if (sngKeys.find(key) != sngKeys.end()) {
        setVisible = !hasSNG && hasOpenpilotLongitudinal && !disableOpenpilotLongitudinal;
      } else if (longitudinalKeys.find(key) != longitudinalKeys.end()) {
        setVisible = hasOpenpilotLongitudinal && !disableOpenpilotLongitudinal;
      } else {
        setVisible = true;
      }
    }

    toggle->setVisible(setVisible);
  }

  setUpdatesEnabled(true);
  update();
}
