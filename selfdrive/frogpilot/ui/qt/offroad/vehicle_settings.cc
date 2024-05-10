#include <QDir>
#include <QRegularExpression>
#include <QTextStream>

#include "selfdrive/frogpilot/ui/qt/offroad/vehicle_settings.h"

QStringList getCarNames(const QString &carMake) {
  QMap<QString, QString> makeMap;
  makeMap["acura"] = "honda";
  makeMap["audi"] = "volkswagen";
  makeMap["buick"] = "gm";
  makeMap["cadillac"] = "gm";
  makeMap["chevrolet"] = "gm";
  makeMap["chrysler"] = "chrysler";
  makeMap["dodge"] = "chrysler";
  makeMap["ford"] = "ford";
  makeMap["gm"] = "gm";
  makeMap["gmc"] = "gm";
  makeMap["genesis"] = "hyundai";
  makeMap["honda"] = "honda";
  makeMap["hyundai"] = "hyundai";
  makeMap["infiniti"] = "nissan";
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

  QString dirPath = "../car";
  QDir dir(dirPath);
  QString targetFolder = makeMap.value(carMake, carMake);
  QStringList names;

  QString filePath = dir.absoluteFilePath(targetFolder + "/values.py");
  QFile file(filePath);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&file);
    QRegularExpression regex(R"delimiter(\w+\s*=\s*\w+PlatformConfig\(\s*"([^"]+)",)delimiter");
    QRegularExpressionMatchIterator it = regex.globalMatch(in.readAll());
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      names << match.captured(1);
    }
    file.close();
  }

  std::sort(names.begin(), names.end());
  return names;
}

FrogPilotVehiclesPanel::FrogPilotVehiclesPanel(SettingsWindow *parent) : FrogPilotListWidget(parent) {
  selectMakeButton = new ButtonControl(tr("Select Make"), tr("SELECT"));
  QObject::connect(selectMakeButton, &ButtonControl::clicked, [this]() {
    QStringList makes = {
      "Acura", "Audi", "BMW", "Buick", "Cadillac", "Chevrolet", "Chrysler", "Dodge", "Ford", "GM", "GMC",
      "Genesis", "Honda", "Hyundai", "Infiniti", "Jeep", "Kia", "Lexus", "Lincoln", "MAN", "Mazda",
      "Mercedes", "Nissan", "Ram", "SEAT", "Škoda", "Subaru", "Tesla", "Toyota", "Volkswagen", "Volvo",
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
      params.putNonBlocking("CarModel", newModelSelection.toStdString());
      selectModelButton->setValue(newModelSelection);
    }
  });
  addItem(selectModelButton);
  selectModelButton->setVisible(false);

  ParamControl *forceFingerprint = new ParamControl("ForceFingerprint", tr("Disable Automatic Fingerprint Detection"), tr("Forces the selected fingerprint and prevents it from ever changing."), "", this);
  addItem(forceFingerprint);

  bool disableOpenpilotLongState = params.getBool("DisableOpenpilotLongitudinal");
  disableOpenpilotLong = new ToggleControl(tr("Disable openpilot Longitudinal Control"), tr("Disable openpilot longitudinal control and use stock ACC instead."), "", disableOpenpilotLongState);
  QObject::connect(disableOpenpilotLong, &ToggleControl::toggleFlipped, [=](bool state) {
    if (state) {
      if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely disable openpilot longitudinal control?"), this)) {
        params.putBoolNonBlocking("DisableOpenpilotLongitudinal", state);
        if (started) {
          if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
            Hardware::reboot();
          }
        }
      }
    } else {
      params.putBoolNonBlocking("DisableOpenpilotLongitudinal", state);
    }
    updateCarToggles();
  });
  addItem(disableOpenpilotLong);

  std::vector<std::tuple<QString, QString, QString, QString>> vehicleToggles {
    {"LongPitch", tr("Long Pitch Compensation"), tr("Smoothen out the gas and pedal controls."), ""},
    {"GasRegenCmd", tr("Truck Tune"), tr("Increase the acceleration and smoothen out the brake control when coming to a stop. For use on Silverado/Sierra only."), ""},

    {"CrosstrekTorque", tr("Subaru Crosstrek Torque Increase"), tr("Increases the maximum allowed torque for the Subaru Crosstrek."), ""},

    {"ToyotaDoors", tr("Automatically Lock/Unlock Doors"), tr("Automatically lock the doors when in drive and unlock when in park."), ""},
    {"ClusterOffset", tr("Cluster Offset"), tr("Set the cluster offset openpilot uses to try and match the speed displayed on the dash."), ""},
    {"SNGHack", tr("Stop and Go Hack"), tr("Enable the 'Stop and Go' hack for vehicles without stock stop and go functionality."), ""},
    {"ToyotaTune", tr("Toyota Tune"), tr("Use a custom Toyota longitudinal tune.\n\nCydia = More focused on TSS-P vehicles but works for all Toyotas\n\nDragonPilot = Focused on TSS2 vehicles\n\nFrogPilot = Takes the best of both worlds with some personal tweaks focused around FrogsGoMoo's 2019 Lexus ES 350"), ""},
  };

  for (const auto &[param, title, desc, icon] : vehicleToggles) {
    AbstractControl *toggle;

    if (param == "ToyotaDoors") {
      std::vector<QString> lockToggles{"LockDoors", "UnlockDoors"};
      std::vector<QString> lockToggleNames{tr("Lock"), tr("Unlock")};
      toggle = new FrogPilotParamToggleControl(param, title, desc, icon, lockToggles, lockToggleNames);

    } else if (param == "ClusterOffset") {
      toggle = new FrogPilotParamValueControl(param, title, desc, icon, 1.000, 1.050, std::map<int, QString>(), this, false, "x", 1, 0.001);

    } else if (param == "ToyotaTune") {
      std::vector<std::pair<QString, QString>> tuneOptions{
        {"StockTune", tr("Stock")},
        {"CydiaTune", tr("Cydia")},
        {"DragonPilotTune", tr("DragonPilot")},
        {"FrogsGoMooTune", tr("FrogPilot")},
      };
      toggle = new FrogPilotButtonsParamControl(param, title, desc, icon, tuneOptions);

      QObject::connect(static_cast<FrogPilotButtonsParamControl*>(toggle), &FrogPilotButtonsParamControl::buttonClicked, [this]() {
        if (started) {
          if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
            Hardware::reboot();
          }
        }
      });

    } else {
      toggle = new ParamControl(param, title, desc, icon, this);
    }

    toggle->setVisible(false);
    addItem(toggle);
    toggles[param.toStdString()] = toggle;

    QObject::connect(static_cast<ToggleControl*>(toggle), &ToggleControl::toggleFlipped, &updateFrogPilotToggles);

    QObject::connect(toggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  std::set<QString> rebootKeys = {"CrosstrekTorque", "GasRegenCmd"};
  for (const QString &key : rebootKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key.toStdString().c_str()]), &ToggleControl::toggleFlipped, [this]() {
      if (started) {
        if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
          Hardware::reboot();
        }
      }
    });
  }

  QObject::connect(uiState(), &UIState::offroadTransition, [this](bool offroad) {
    std::thread([this]() {
      while (carMake.isEmpty()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        carMake = QString::fromStdString(params.get("CarMake"));
        carModel = QString::fromStdString(params.get("CarModel"));
      }
      setModels();
      updateCarToggles();
    }).detach();
  });

  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotVehiclesPanel::updateState);

  carMake = QString::fromStdString(params.get("CarMake"));
  carModel = QString::fromStdString(params.get("CarModel"));

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
    hasOpenpilotLongitudinal = CP.getOpenpilotLongitudinalControl();
    hasSNG = CP.getMinEnableSpeed() <= 0;
    isGMTruck = carFingerprint == "CHEVROLET SILVERADO 1500 2020";
    isImpreza = carFingerprint == "SUBARU IMPREZA LIMITED 2019";
  } else {
    hasExperimentalOpenpilotLongitudinal = false;
    hasOpenpilotLongitudinal = true;
    hasSNG = false;
    isGMTruck = true;
    isImpreza = true;
  }

  hideToggles();
}

void FrogPilotVehiclesPanel::setModels() {
  models = getCarNames(carMake.toLower());
  hideToggles();
}

void FrogPilotVehiclesPanel::hideToggles() {
  disableOpenpilotLong->setVisible(hasOpenpilotLongitudinal && !hasExperimentalOpenpilotLongitudinal && !params.getBool("HideDisableOpenpilotLongitudinal"));

  selectMakeButton->setValue(carMake);
  selectModelButton->setValue(carModel);
  selectModelButton->setVisible(!carMake.isEmpty());

  bool gm = carMake == "Buick" || carMake == "Cadillac" || carMake == "Chevrolet" || carMake == "GM" || carMake == "GMC";
  bool subaru = carMake == "Subaru";
  bool toyota = carMake == "Lexus" || carMake == "Toyota";

  std::set<QString> gmTruckKeys = {"GasRegenCmd"};
  std::set<QString> imprezaKeys = {"CrosstrekTorque"};
  std::set<QString> longitudinalKeys = {"GasRegenCmd", "ToyotaTune", "LongPitch", "SNGHack"};
  std::set<QString> sngKeys = {"SNGHack"};

  for (auto &[key, toggle] : toggles) {
    if (toggle) {
      toggle->setVisible(false);

      if ((!hasOpenpilotLongitudinal || params.getBool("DisableOpenpilotLongitudinal")) && longitudinalKeys.find(key.c_str()) != longitudinalKeys.end()) {
        continue;
      }

      if (hasSNG && sngKeys.find(key.c_str()) != sngKeys.end()) {
        continue;
      }

      if (!isGMTruck && gmTruckKeys.find(key.c_str()) != gmTruckKeys.end()) {
        continue;
      }

      if (!isImpreza && imprezaKeys.find(key.c_str()) != imprezaKeys.end()) {
        continue;
      }

      if (gm) {
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
