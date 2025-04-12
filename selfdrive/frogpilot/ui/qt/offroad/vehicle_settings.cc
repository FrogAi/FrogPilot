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

  QFile valuesFile(QString("../car/%1/values.py").arg(makeMap.value(carMake, carMake)));
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
            carNameList.append(carName);
            carModels[carName] = platformName;
          }
          break;
        }
      }
    }
  }

  carNameList.sort();
  return carNameList;
}

FrogPilotVehiclesPanel::FrogPilotVehiclesPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QStackedLayout *vehiclesLayout = new QStackedLayout();
  addItem(vehiclesLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);

  ScrollView *vehiclesPanel = new ScrollView(settingsList, this);

  vehiclesLayout->addWidget(vehiclesPanel);

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

  ScrollView *gmPanel = new ScrollView(gmList, this);
  ScrollView *hkgPanel = new ScrollView(hkgList, this);
  ScrollView *toyotaPanel = new ScrollView(toyotaList, this);

  vehiclesLayout->addWidget(gmPanel);
  vehiclesLayout->addWidget(hkgPanel);
  vehiclesLayout->addWidget(toyotaPanel);

  std::vector<std::tuple<QString, QString, QString, QString>> vehicleToggles {
    {"GMToggles", tr("General Motors Settings"), tr("Settings specific to <b>General Motors</b> vehicles."), ""},
    {"ExperimentalGMTune", tr("FrogsGoMoo's Experimental Tune"), tr("<b>FrogsGoMoo's</b> experimental <b>General Motors</b> tune that aims to smoothen out stopping and takeoff control based on nothing but guesswork. Use at your own risk!"), ""},
    {"LongPitch", tr("Smooth Pedal Response on Hills"), tr("Smoothen the acceleration and braking when driving uphill or downhill."), ""},
    {"VoltSNG", tr("Stop and Go Hack"), tr("Force stop and go on the <b>2017 Chevy Volt</b>."), ""},

    {"HKGToggles", tr("Hyundai/Kia/Genesis Settings"), tr("Settings specific to <b>Hyundai</b>, <b>Kia</b>, and <b>Genesis</b> vehicles."), ""},
    {"NewLongAPI", tr("comma's New Longitudinal API"), tr("comma's new longitudinal control system that has shown great improvement with acceleration and braking, but has issues on some <b>Hyundai</b>/<b>Kia</b>/<b>Genesis</b> vehicles."), ""},

    {"ToyotaToggles", tr("Toyota/Lexus Settings"), tr("Settings specific to <b>Toyota</b> and <b>Lexus</b> vehicles."), ""},
    {"ToyotaDoors", tr("Automatically Lock/Unlock Doors"), tr("Automatically lock the doors when shifting into drive and unlock them when shifting into park."), ""},
    {"ClusterOffset", tr("Cluster Speed Offset"), tr("The cluster speed offset used by openpilot to match the speed displayed on the dash."), ""},
    {"FrogsGoMoosTweak", tr("FrogsGoMoo's Personal Tweaks"), tr("<b>FrogsGoMoo's</b> personal tweaks for quicker acceleration and smoother braking."), ""},
    {"LockDoorsTimer", tr("Lock Doors On Ignition Off After"), tr("Automatically lock the doors after the car's ignition has been turned off and no one is detected in either of the front seats."), ""},
    {"SNGHack", tr("Stop and Go Hack"), tr("Force stop and go on <b>Toyota</b>/<b>Lexus</b> vehicles without stock stop and go functionality."), ""}
  };

  for (const auto &[param, title, desc, icon] : vehicleToggles) {
    AbstractControl *vehicleToggle;

    if (param == "GMToggles") {
      ButtonControl *gmToggle = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(gmToggle, &ButtonControl::clicked, [vehiclesLayout, gmPanel]() {
        vehiclesLayout->setCurrentWidget(gmPanel);
      });
      vehicleToggle = gmToggle;

    } else if (param == "HKGToggles") {
      ButtonControl *hkgToggle = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(hkgToggle, &ButtonControl::clicked, [vehiclesLayout, hkgPanel]() {
        vehiclesLayout->setCurrentWidget(hkgPanel);
      });
      vehicleToggle = hkgToggle;

    } else if (param == "ToyotaToggles") {
      ButtonControl *toyotaToggle = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(toyotaToggle, &ButtonControl::clicked, [vehiclesLayout, toyotaPanel]() {
        vehiclesLayout->setCurrentWidget(toyotaPanel);
      });
      vehicleToggle = toyotaToggle;
    } else if (param == "ToyotaDoors") {
      std::vector<QString> lockToggles{"LockDoors", "UnlockDoors"};
      std::vector<QString> lockToggleNames{tr("Lock"), tr("Unlock")};
      vehicleToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, lockToggles, lockToggleNames);
    } else if (param == "LockDoorsTimer") {
      std::map<float, QString> autoLockLabels;
      for (int i = 0; i <= 300; ++i) {
        autoLockLabels[i] = i == 0 ? tr("Never") : QString::number(i) + tr(" seconds");
      }
      vehicleToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 300, QString(), autoLockLabels, 5);
    } else if (param == "ClusterOffset") {
      std::vector<QString> clusterOffsetButton{"Reset"};
      FrogPilotParamValueButtonControl *clusterOffsetToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 1.000, 1.050, "x", std::map<float, QString>(), 0.001, false, {}, clusterOffsetButton, false, false);
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

    if (ButtonControl *buttonControl = qobject_cast<ButtonControl*>(vehicleToggle)) {
      QObject::connect(buttonControl, &ButtonControl::clicked, this, &FrogPilotVehiclesPanel::openParentToggle);
    }

    QObject::connect(vehicleToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  static_cast<FrogPilotParamValueControl*>(toggles["LockDoorsTimer"])->setWarning("<b>Warning:</b> openpilot can't detect if keys are still inside the car, so ensure you have a spare key to prevent accidental lockouts!");

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

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, [vehiclesLayout, vehiclesPanel] {vehiclesLayout->setCurrentWidget(vehiclesPanel);});
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotVehiclesPanel::updateState);
}

void FrogPilotVehiclesPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  hasExperimentalOpenpilotLongitudinal = parent->hasExperimentalOpenpilotLongitudinal;
  hasOpenpilotLongitudinal = parent->hasOpenpilotLongitudinal;
  hasSNG = parent->hasSNG;
  isC3 = parent->isC3;
  isGM = parent->isGM;
  isHKG = parent->isHKG;
  isHKGCanFd = parent->isHKGCanFd;
  isToyota = parent->isToyota;
  isVolt = parent->isVolt;
  tuningLevel = parent->tuningLevel;

  updateToggles();
}

void FrogPilotVehiclesPanel::updateState(const UIState &s) {
  if (!isVisible()) {
    return;
  }

  started = s.scene.started;
}

void FrogPilotVehiclesPanel::updateToggles() {
  std::set<QString> parentKeys = {
    "GMToggles",
    "HKGToggles",
    "ToyotaToggles"
  };

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.find(key) != parentKeys.end()) {
      toggle->setVisible(false);
    }
  }

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.find(key) != parentKeys.end()) {
      continue;
    }

    bool setVisible = tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (gmKeys.find(key) != gmKeys.end()) {
      setVisible &= isGM;
    } else if (hkgKeys.find(key) != hkgKeys.end()) {
      setVisible &= isHKG;
    } else if (toyotaKeys.find(key) != toyotaKeys.end()) {
      setVisible &= isToyota;
    }

    if (longitudinalKeys.find(key) != longitudinalKeys.end()) {
      setVisible &= hasOpenpilotLongitudinal;
    }

    if (key == "LockDoorsTimer") {
      setVisible &= !isC3;
    }

    if (key == "SNGHack") {
      setVisible &= !hasSNG;
    }

    if (key == "VoltSNG") {
      setVisible &= isVolt && !hasSNG;
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (gmKeys.find(key) != gmKeys.end()) {
        toggles["GMToggles"]->setVisible(true);
      } else if (hkgKeys.find(key) != hkgKeys.end()) {
        toggles["HKGToggles"]->setVisible(true);
      } else if (toyotaKeys.find(key) != toyotaKeys.end()) {
        toggles["ToyotaToggles"]->setVisible(true);
      }
    }
  }

  disableOpenpilotLong->setVisible((hasOpenpilotLongitudinal || params.getBool("DisableOpenpilotLongitudinal")) && !hasExperimentalOpenpilotLongitudinal);

  update();
}
