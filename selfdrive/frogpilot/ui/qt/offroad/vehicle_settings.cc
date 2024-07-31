#include <QDir>
#include <QRegularExpression>
#include <QTextStream>

#include "selfdrive/frogpilot/ui/qt/offroad/vehicle_settings.h"

QStringList getCarNames(const QString &carMake, QMap<QString, QString> &carModels) {
  QMap<QString, QString> makeMap;
  makeMap["acura"] = "honda";
  makeMap["audi"] = "volkswagen";
  makeMap["buick"] = "gm";
  makeMap["cadillac"] = "gm";
  makeMap["chevrolet"] = "gm";
  makeMap["chrysler"] = "chrysler";
  makeMap["dodge"] = "chrysler";
  makeMap["ford"] = "ford";
  makeMap["genesis"] = "hyundai";
  makeMap["gmc"] = "gm";
  makeMap["holden"] = "gm";
  makeMap["honda"] = "honda";
  makeMap["hyundai"] = "hyundai";
  makeMap["jeep"] = "chrysler";
  makeMap["kia"] = "hyundai";
  makeMap["lexus"] = "toyota";
  makeMap["lincoln"] = "ford";
  makeMap["man"] = "volkswagen";
  makeMap["mazda"] = "mazda";
  makeMap["nissan"] = "nissan";
  makeMap["ram"] = "chrysler";
  makeMap["seat"] = "volkswagen";
  makeMap["škoda"] = "volkswagen";
  makeMap["subaru"] = "subaru";
  makeMap["tesla"] = "tesla";
  makeMap["toyota"] = "toyota";
  makeMap["volkswagen"] = "volkswagen";

  QString targetFolder = makeMap.value(carMake, carMake);
  QFile file(QString("../car/%1/values.py").arg(targetFolder));
  QStringList names;
  QSet<QString> uniqueNames;

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return names;

  QTextStream in(&file);
  QString fileContent = in.readAll();
  file.close();

  fileContent.remove(QRegularExpression("#[^\n]*"));
  fileContent.remove(QRegularExpression("footnotes=\\[[^\\]]*\\],\\s*"));

  QRegularExpression carModelRegex(R"delimiter((\w+)\s*=\s*\w+\s*\(\s*\[([\s\S]*?)\]\s*,)delimiter");
  QRegularExpression carDocsRegex(R"delimiter(CarDocs\(\s*"([^"]+)"[^)]*\))delimiter");

  QRegularExpressionMatchIterator carModelIt = carModelRegex.globalMatch(fileContent);
  while (carModelIt.hasNext()) {
    QRegularExpressionMatch carModelMatch = carModelIt.next();
    QString platform = carModelMatch.captured(1);
    QString platformSection = carModelMatch.captured(2);

    QRegularExpressionMatchIterator carDocsIt = carDocsRegex.globalMatch(platformSection);
    while (carDocsIt.hasNext()) {
      QRegularExpressionMatch match = carDocsIt.next();
      QString carName = match.captured(1);

      if (carName.contains(QRegularExpression("^[A-Za-z0-9 Š.()-]+$")) && carName.count(" ") >= 1) {
        QStringList nameParts = carName.split(" ");
        if (nameParts.contains(carMake, Qt::CaseInsensitive)) {
          if (!uniqueNames.contains(carName)) {
            names << carName;
            carModels[carName] = platform;
            uniqueNames.insert(carName);
          }
        }
      }
    }
  }

  std::sort(names.begin(), names.end());
  return names;
}

FrogPilotVehiclesPanel::FrogPilotVehiclesPanel(SettingsWindow *parent) : FrogPilotListWidget(parent) {
  selectMakeButton = new ButtonControl(tr("Select Make"), tr("SELECT"));
  QObject::connect(selectMakeButton, &ButtonControl::clicked, [this]() {
    QStringList makes = {
      "Acura", "Audi", "Buick", "Cadillac", "Chevrolet", "Chrysler", "Dodge", "Ford", "Genesis",
      "GMC", "Holden", "Honda", "Hyundai", "Jeep", "Kia", "Lexus", "Lincoln", "MAN", "Mazda",
      "Nissan", "Ram", "SEAT", "Škoda", "Subaru", "Tesla", "Toyota", "Volkswagen",
    };

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

  ParamControl *forceFingerprint = new ParamControl("ForceFingerprint", tr("Disable Automatic Fingerprint Detection"), tr("Forces the selected fingerprint and prevents it from ever changing."), "", this);
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
    updateCarToggles();
  });
  addItem(disableOpenpilotLong);

  std::vector<std::tuple<QString, QString, QString, QString>> vehicleToggles {
    {"LongPitch", tr("Long Pitch Compensation"), tr("Smoothen out the gas and pedal controls."), ""},
    {"VoltSNG", tr("2017 Volt SNG"), tr("Enable the 'Stop and Go' hack for 2017 Chevy Volts."), ""},
    {"NewLongAPIGM", tr("Use comma's New Longitudinal API"), tr("Use comma's new longitudinal controls that have shown great improvement with acceleration and braking, but has a few issues on some GM vehicles."), ""},

    {"NewLongAPI", tr("Use comma's New Longitudinal API"), tr("Use comma's new longitudinal controls that have shown great improvement with acceleration and braking, but has a few issues on Hyundai/Kia/Genesis."), ""},

    {"CrosstrekTorque", tr("Subaru Crosstrek Torque Increase"), tr("Increases the maximum allowed torque for the Subaru Crosstrek."), ""},

    {"ToyotaDoors", tr("Automatically Lock/Unlock Doors"), tr("Automatically lock the doors when in drive and unlock when in park."), ""},
    {"ClusterOffset", tr("Cluster Offset"), tr("Set the cluster offset openpilot uses to try and match the speed displayed on the dash."), ""},
    {"SNGHack", tr("Stop and Go Hack"), tr("Enable the 'Stop and Go' hack for vehicles without stock stop and go functionality."), ""},
    {"ToyotaTune", tr("Toyota Tune"), tr("Use a custom Toyota longitudinal tune.\n\nCydia = More focused on TSS-P vehicles but works for all Toyotas\n\nDragonPilot = Focused on TSS2 vehicles\n\nFrogPilot = Takes the best of both worlds with some personal tweaks focused around FrogsGoMoo's 2019 Lexus ES 350"), ""},
  };

  for (const auto &[param, title, desc, icon] : vehicleToggles) {
    AbstractControl *vehicleToggle;

    if (param == "ToyotaDoors") {
      std::vector<QString> lockToggles{"LockDoors", "UnlockDoors"};
      std::vector<QString> lockToggleNames{tr("Lock"), tr("Unlock")};
      vehicleToggle = new FrogPilotParamToggleControl(param, title, desc, icon, lockToggles, lockToggleNames);

    } else if (param == "ClusterOffset") {
      vehicleToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1.000, 1.050, std::map<int, QString>(), this, false, "x", 1, 0.001);

    } else if (param == "ToyotaTune") {
      std::vector<std::pair<QString, QString>> tuneOptions{
        {"StockTune", tr("Stock")},
        {"CydiaTune", tr("Cydia")},
        {"FrogsGoMooTune", tr("FrogPilot")},
      };

      FrogPilotButtonsParamControl *toyotaTuneToggle = new FrogPilotButtonsParamControl(param, title, desc, icon, tuneOptions);
      vehicleToggle = toyotaTuneToggle;

      QObject::connect(toyotaTuneToggle, &FrogPilotButtonsParamControl::buttonClicked, [this]() {
        if (started) {
          if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
            Hardware::reboot();
          }
        }
      });

    } else {
      vehicleToggle = new ParamControl(param, title, desc, icon, this);
    }

    vehicleToggle->setVisible(false);
    addItem(vehicleToggle);
    toggles[param.toStdString()] = vehicleToggle;

    QObject::connect(static_cast<ToggleControl*>(vehicleToggle), &ToggleControl::toggleFlipped, &updateFrogPilotToggles);
    QObject::connect(static_cast<FrogPilotParamToggleControl*>(vehicleToggle), &FrogPilotParamToggleControl::buttonTypeClicked, &updateFrogPilotToggles);

    QObject::connect(vehicleToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  std::set<QString> rebootKeys = {"CrosstrekTorque"};
  for (const QString &key : rebootKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key.toStdString().c_str()]), &ToggleControl::toggleFlipped, [this]() {
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
        std::this_thread::sleep_for(std::chrono::seconds(1));
        carMake = QString::fromStdString(params.get("CarMake"));
        carModel = QString::fromStdString(params.get(params.get("CarModelName").empty() ? "CarModel" : "CarModelName"));
      }
      setModels();
      updateCarToggles();
    }).detach();
  });

  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotVehiclesPanel::updateState);

  carMake = QString::fromStdString(params.get("CarMake"));
  carModel = QString::fromStdString(params.get(params.get("CarModelName").empty() ? "CarModel" : "CarModelName"));

  if (!carMake.isEmpty()) {
    setModels();
  }
}

void FrogPilotVehiclesPanel::updateState(const UIState &s) {
  if (!isVisible()) return;

  started = s.scene.started;
}

void FrogPilotVehiclesPanel::updateCarToggles() {
  auto carParams = params.get("CarParamsPersistent");
  if (!carParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(carParams.data(), carParams.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();

    auto carFingerprint = CP.getCarFingerprint();

    hasExperimentalOpenpilotLongitudinal = CP.getExperimentalLongitudinalAvailable();
    hasOpenpilotLongitudinal = hasLongitudinalControl(CP);
    hasSNG = CP.getMinEnableSpeed() <= 0;
    isGMPCMCruise = CP.getCarName() == "gm" && CP.getPcmCruise();
    isImpreza = carFingerprint == "SUBARU_IMPREZA";
    isVolt = carFingerprint == "CHEVROLET_VOLT";
  } else {
    hasExperimentalOpenpilotLongitudinal = false;
    hasOpenpilotLongitudinal = true;
    hasSNG = false;
    isImpreza = true;
    isVolt = true;
  }

  hideToggles();
}

void FrogPilotVehiclesPanel::setModels() {
  models = getCarNames(carMake.toLower(), carModels);
  hideToggles();
}

void FrogPilotVehiclesPanel::hideToggles() {
  disableOpenpilotLong->setVisible(hasOpenpilotLongitudinal && !hasExperimentalOpenpilotLongitudinal && !isGMPCMCruise || params.getBool("DisableOpenpilotLongitudinal"));

  selectMakeButton->setValue(carMake);
  selectModelButton->setValue(carModel);
  selectModelButton->setVisible(!carMake.isEmpty());

  bool gm = carMake == "Buick" || carMake == "Cadillac" || carMake == "Chevrolet" || carMake == "GM" || carMake == "GMC";
  bool hyundai = carMake == "Genesis" || carMake == "Hyundai" || carMake == "Kia";
  bool subaru = carMake == "Subaru";
  bool toyota = carMake == "Lexus" || carMake == "Toyota";

  std::set<QString> imprezaKeys = {"CrosstrekTorque"};
  std::set<QString> longitudinalKeys = {"ToyotaTune", "LongPitch", "SNGHack"};
  std::set<QString> sngKeys = {"SNGHack"};
  std::set<QString> voltKeys = {"VoltSNG"};

  for (auto &[key, toggle] : toggles) {
    if (toggle) {
      toggle->setVisible(false);

      if ((!hasOpenpilotLongitudinal || params.getBool("DisableOpenpilotLongitudinal")) && longitudinalKeys.find(key.c_str()) != longitudinalKeys.end()) {
        continue;
      }

      if (hasSNG && sngKeys.find(key.c_str()) != sngKeys.end()) {
        continue;
      }

      if (!isImpreza && imprezaKeys.find(key.c_str()) != imprezaKeys.end()) {
        continue;
      }

      if (!isVolt && voltKeys.find(key.c_str()) != voltKeys.end()) {
        continue;
      }

      if (hyundai) {
        toggle->setVisible(hyundaiKeys.find(key.c_str()) != hyundaiKeys.end());
      } else if (gm) {
        toggle->setVisible(gmKeys.find(key.c_str()) != gmKeys.end());
      } else if (subaru) {
        toggle->setVisible(subaruKeys.find(key.c_str()) != subaruKeys.end());
      } else if (toyota) {
        toggle->setVisible(toyotaKeys.find(key.c_str()) != toyotaKeys.end());
      }
    }
  }

  update();
}
