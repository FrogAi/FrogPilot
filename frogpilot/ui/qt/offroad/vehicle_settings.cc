#include <QRegularExpression>

#include "frogpilot/ui/qt/offroad/vehicle_settings.h"

QStringList getCarNames(const QString &carMake, QMap<QString, QString> &carModels) {
  static const QHash<QString, QString> makeToFolder = {
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
    {"peugeot", "psa"},
    {"ram", "chrysler"},
    {"rivian", "rivian"},
    {"seat", "volkswagen"},
    {"škoda", "volkswagen"},
    {"subaru", "subaru"},
    {"tesla", "tesla"},
    {"toyota", "toyota"},
    {"volkswagen", "volkswagen"}
  };

  QStringList carNames;

  const QString folder = makeToFolder.value(carMake.toLower());
  if (folder.isEmpty()) {
    return carNames;
  }

  QFile file(QString("../../opendbc/car/%1/values.py").arg(folder));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return carNames;
  }

  QString content = file.readAll();
  file.close();

  static const QRegularExpression commentRe("#[^\\n]*");
  static const QRegularExpression footnoteRe("footnotes=\\[[^\\]]*\\],\\s*");
  content.remove(commentRe).remove(footnoteRe);

  static const QRegularExpression platformRe("(\\w+)\\s*=\\s*\\w+\\s*\\(");
  QRegularExpressionMatchIterator platformIt = platformRe.globalMatch(content);

  QVector<QPair<int, QString>> platforms;
  while (platformIt.hasNext()) {
    QRegularExpressionMatch match = platformIt.next();
    platforms.append({match.capturedStart(), match.captured(1)});
  }
  platforms.append({content.length(), QString()});

  static const QRegularExpression carNameRe("CarDocs\\w*\\s*\\(\\s*\"([^\"]+)\"");
  const QString lowerMake = carMake.toLower();

  for (int i = 0; i < platforms.size() - 1; ++i) {
    int start = platforms[i].first;
    int end = platforms[i + 1].first;
    const QString &platformName = platforms[i].second;

    QRegularExpressionMatchIterator carIt = carNameRe.globalMatch(
      content.mid(start, end - start)
    );

    while (carIt.hasNext()) {
      QString carName = carIt.next().captured(1);
      if (carName.startsWith(carMake, Qt::CaseInsensitive)) {
        carModels[carName] = platformName;
        carNames.append(carName);
      }
    }
  }

  carNames.sort(Qt::CaseInsensitive);
  return carNames;
}

FrogPilotVehiclesPanel::FrogPilotVehiclesPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  QStackedLayout *vehiclesLayout = new QStackedLayout();
  addItem(vehiclesLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);

  ScrollView *vehiclesPanel = new ScrollView(settingsList, this);

  vehiclesLayout->addWidget(vehiclesPanel);

  QStringList makes = {
    "Acura", "Audi", "Buick", "Cadillac", "Chevrolet", "Chrysler", "CUPRA",
    "Dodge", "Ford", "Genesis", "GMC", "Holden", "Honda", "Hyundai", "Jeep",
    "Kia", "Lexus", "Lincoln", "MAN", "Mazda", "Nissan", "Peugeot", "Ram",
    "Rivian", "SEAT", "Škoda", "Subaru", "Tesla", "Toyota", "Volkswagen"
  };

  ButtonControl *selectMakeButton = new ButtonControl(tr("Car Make"), tr("SELECT"));
  QObject::connect(selectMakeButton, &ButtonControl::clicked, [makes, selectMakeButton, this]() {
    QString makeSelection = MultiOptionDialog::getSelection(tr("Choose your car make"), makes, "", this);
    if (!makeSelection.isEmpty()) {
      params.put("CarMake", makeSelection.toStdString());
      selectMakeButton->setValue(makeSelection);
    }
  });
  settingsList->addItem(selectMakeButton);

  ButtonControl *selectModelButton = new ButtonControl(tr("Car Model"), tr("SELECT"));
  QObject::connect(selectModelButton, &ButtonControl::clicked, [selectModelButton, this]() {
    QString modelSelection = MultiOptionDialog::getSelection(tr("Choose your car model"), getCarNames(QString::fromStdString(params.get("CarMake")).toLower(), carModels), "", this);
    if (!modelSelection.isEmpty()) {
      params.put("CarModel", carModels.value(modelSelection).toStdString());
      params.put("CarModelName", modelSelection.toStdString());
      selectModelButton->setValue(modelSelection);
    }
  });
  settingsList->addItem(selectModelButton);

  forceFingerprint = new ParamControl("ForceFingerprint", tr("Disable Automatic Fingerprint Detection"), tr("<b>Lock openpilot to the car you picked and stop it changing on its own.</b>"), "");
  settingsList->addItem(forceFingerprint);

  disableOpenpilotLong = new ParamControl("DisableOpenpilotLongitudinal", tr("Disable openpilot Longitudinal Control"), tr("<b>Let your car's own cruise control handle the gas and brake instead of openpilot.</b>"), "");
  QObject::connect(disableOpenpilotLong, &ToggleControl::toggleFlipped, [parent, this](bool state) {
    if (state) {
      if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely disable openpilot longitudinal control?"), this)) {
        if (uiState()->scene.started) {
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
  FrogPilotListWidget *subaruList = new FrogPilotListWidget(this);
  FrogPilotListWidget *toyotaList = new FrogPilotListWidget(this);
  FrogPilotListWidget *vehicleInfoList = new FrogPilotListWidget(this);

  ScrollView *gmPanel = new ScrollView(gmList, this);
  ScrollView *hkgPanel = new ScrollView(hkgList, this);
  ScrollView *subaruPanel = new ScrollView(subaruList, this);
  ScrollView *toyotaPanel = new ScrollView(toyotaList, this);
  ScrollView *vehicleInfoPanel = new ScrollView(vehicleInfoList, this);

  vehiclesLayout->addWidget(gmPanel);
  vehiclesLayout->addWidget(hkgPanel);
  vehiclesLayout->addWidget(subaruPanel);
  vehiclesLayout->addWidget(toyotaPanel);
  vehiclesLayout->addWidget(vehicleInfoPanel);

  std::vector<std::tuple<QString, QString, QString, QString>> vehicleToggles {
    {"GMToggles", tr("General Motors Settings"), tr("<b>Settings that only work on Buick, Cadillac, Chevrolet, GMC and Holden cars, covering how openpilot stops, starts and handles hills.</b><br><br>Which of these you see depends on your exact model."), ""},
    {"VoltSNG", tr("Stop-and-Go Hack"), tr("<b>Make the car pull away by itself after a full stop on a Chevrolet Volt, which does not do this from the factory.</b><br><br>Without it you have to press the gas or the resume button every time traffic moves off. Keep your foot near the brake the first few times so you can see how it behaves."), ""},

    {"HKGToggles", tr("Hyundai/Kia/Genesis Settings"), tr("<b>Settings that only work on Genesis, Hyundai and Kia cars, covering openpilot's newer gas and brake control and a steering torque hack.</b><br><br>Which of these you see depends on which system your car uses, and the steering hack only appears on cars using CAN-FD."), ""},
    {"TacoTuneHacks", tr("\"Taco Bell Run\" Torque Hack"), tr("<b>Let openpilot pull the wheel harder through turns, using the trick comma demonstrated on their 2022 \"Taco Bell Run\" drive.</b><br><br>It raises the steering limit everywhere, not just at low speed, and it relaxes one of the safety checks that normally caps steering effort. You will also have to grip the wheel more firmly to take over."), ""},

    {"SubaruToggles", tr("Subaru Settings"), tr("<b>Settings that only work on Subaru cars.</b><br><br>There is one, and it decides whether your car pulls away by itself after a stop."), ""},
    {"SubaruSNG", tr("Stop and Go"), tr("<b>Get your car moving again by itself once the car ahead pulls away from a full stop.</b><br><br>Subaru's own cruise holds the brakes and waits for you to press resume after a few seconds stopped. FrogPilot watches the car ahead and sends that resume for you. Keep your foot ready near the brake the first few times so you can see how it behaves."), ""},

    {"ToyotaToggles", tr("Toyota/Lexus Settings"), tr("<b>Settings that only work on Lexus and Toyota cars, covering door locking, dashboard speed, stop-and-go and openpilot's own tuning.</b><br><br>Which of these you see depends on your exact model and on what hardware is fitted."), ""},
    {"ToyotaDoors", tr("Automatically Lock/Unlock Doors"), tr("<b>Lock the doors when you shift out of park and unlock them again when you shift back into it.</b><br><br>This runs whenever the car is on, whether or not openpilot is engaged."), ""},
    {"ClusterOffset", tr("Dashboard Speed Offset"), tr("<b>Line up the speed openpilot shows on screen with the number on your dashboard, which most cars deliberately read a little high.</b><br><br>Raise it until openpilot's number matches your dashboard. This does not change how fast openpilot actually drives, with one exception: while it is following posted speed limits, a higher number here makes it drive slightly slower."), ""},
    {"ToyotaDSUBypass", tr("DSU Re-Route Harness"), tr("<b>Let openpilot control the gas and brake on an older Toyota by rerouting the cruise control computer's messages through a wiring harness you fit yourself.</b><br><br>The DSU is the box that normally runs your car's radar cruise. Only turn this on after the harness is physically installed, because openpilot cannot check for it."), ""},
    {"FrogsGoMoosTweak", tr("FrogsGoMoo's Personal Tweaks"), tr("<b>Swap in FrogsGoMoo's own settings for how openpilot comes to a stop.</b><br><br>These are personal preferences rather than a fix for anything, and they are already on. They take over your stopping and starting values from \"Driving Controls\" and hide those rows while this is on, though on a Toyota the starting value has no effect."), ""},
    {"LockDoorsTimer", tr("Lock Doors On Ignition Off After"), tr("<b>Lock the doors on their own once you have switched the car off and left it, after the number of seconds you pick.</b><br><br>The countdown only starts once the screen has gone dark, and it starts over if the driver camera still sees a face in the driver's seat or if any door is open. Somebody sitting in the front passenger seat will not hold it off. Set it to \"Never\" to switch it off."), ""},
    {"SNGHack", tr("Stop-and-Go Hack"), tr("<b>Make the car pull away by itself after a full stop on a Lexus or Toyota that does not do this from the factory.</b><br><br>Without it you have to press the gas or the resume button every time traffic moves off. It works by telling the car openpilot is never fully stopped, so keep your foot near the brake the first few times."), ""},

    {"VehicleInfo", tr("Vehicle Info"), tr("<b>What openpilot has worked out about your car and what it can do with it.</b><br><br>These rows are read-only. They stay on \"Unknown until first drive\" until openpilot has recognised your car."), ""},
    {"HardwareDetected", tr("3rd Party Hardware Detected"), tr("<b>Extra hardware openpilot has found fitted to your car, such as a comma pedal, an SDSU or a ZSS.</b><br><br>openpilot works these out from your car's wiring on its own. \"None\" is not proof nothing is fitted: on a Toyota a comma pedal is only reported while openpilot is handling the gas and brake, and on a Bosch Honda it is never reported at all."), ""},
    {"BlindSpotSupport", tr("Blind Spot Support"), tr("<b>Whether openpilot can read your car's blind spot sensors, which it uses to hold off a lane change when someone is beside you.</b><br><br>If this says No, check your mirrors yourself before every lane change, because openpilot has nothing to warn it."), ""},
    {"PedalSupport", tr("comma Pedal Support"), tr("<b>Whether a comma pedal would work on your car, which is an add-on that lets openpilot pull away from a stop on cars that cannot do it themselves.</b><br><br>This tells you whether one is worth fitting, not whether you already have one. \"3rd Party Hardware Detected\" above answers that."), ""},
    {"OpenpilotLongitudinal", tr("openpilot Longitudinal Support"), tr("<b>Whether openpilot handles the gas and brake itself, rather than leaving that to your car's own cruise control.</b><br><br>If this says No, openpilot only steers and your car decides the speed, so the settings under \"Driving Controls\" that shape acceleration and braking will not do anything."), ""},
    {"RadarSupport", tr("Radar Support"), tr("<b>Whether openpilot can use your car's radar alongside its camera, which helps it track the car ahead in rain, fog and darkness.</b><br><br>If this says No, openpilot is working from the camera alone and may pick up the car ahead later in poor visibility."), ""},
    {"SDSUSupport", tr("SDSU Support"), tr("<b>Whether an SDSU would work on your car, which is a small board that lets openpilot control the gas and brake on older Toyotas.</b><br><br>This tells you whether one is worth fitting, not whether you already have one."), ""},
    {"SNGSupport", tr("Stop-and-Go Support"), tr("<b>Whether openpilot pulls away by itself after a full stop, instead of waiting for you to press the gas or the resume button.</b><br><br>If this says No, your car's brand group above may still offer a \"Stop-and-Go Hack\" that adds it."), ""}
  };

  for (const auto &[param, title, desc, icon] : vehicleToggles) {
    AbstractControl *vehicleToggle;

    if (param == "GMToggles") {
      ButtonControl *gmButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(gmButton, &ButtonControl::clicked, [vehiclesLayout, gmPanel, this]() {
        openDescriptions(forceOpenDescriptions, toggles);
        vehiclesLayout->setCurrentWidget(gmPanel);
      });
      vehicleToggle = gmButton;

    } else if (param == "HKGToggles") {
      ButtonControl *hkgButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(hkgButton, &ButtonControl::clicked, [vehiclesLayout, hkgPanel, this]() {
        openDescriptions(forceOpenDescriptions, toggles);
        vehiclesLayout->setCurrentWidget(hkgPanel);
      });
      vehicleToggle = hkgButton;

    } else if (param == "SubaruToggles") {
      ButtonControl *subaruButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(subaruButton, &ButtonControl::clicked, [vehiclesLayout, subaruPanel, this]() {
        openDescriptions(forceOpenDescriptions, toggles);
        vehiclesLayout->setCurrentWidget(subaruPanel);
      });
      vehicleToggle = subaruButton;

    } else if (param == "ToyotaToggles") {
      ButtonControl *toyotaButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(toyotaButton, &ButtonControl::clicked, [vehiclesLayout, toyotaPanel, this]() {
        openDescriptions(forceOpenDescriptions, toggles);
        vehiclesLayout->setCurrentWidget(toyotaPanel);
      });
      vehicleToggle = toyotaButton;
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
      QObject::connect(clusterOffsetToggle, &FrogPilotParamValueButtonControl::buttonClicked, [clusterOffsetToggle, this]() {
        params.putFloat("ClusterOffset", std::stof(params.getKeyDefaultValue("ClusterOffset").value()));
        clusterOffsetToggle->refresh();
      });
      vehicleToggle = clusterOffsetToggle;

    } else if (param == "VehicleInfo") {
      ButtonControl *VehicleInfoButton = new ButtonControl(title, tr("VIEW"), desc);
      QObject::connect(VehicleInfoButton, &ButtonControl::clicked, [vehiclesLayout, vehicleInfoPanel, this]() {
        openDescriptions(forceOpenDescriptions, toggles);
        vehiclesLayout->setCurrentWidget(vehicleInfoPanel);
      });
      vehicleToggle = VehicleInfoButton;
    } else if (vehicleInfoKeys.contains(param)) {
      vehicleToggle = new LabelControl(title, "", desc);

    } else {
      vehicleToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = vehicleToggle;

    if (gmKeys.contains(param)) {
      gmList->addItem(vehicleToggle);
    } else if (hkgKeys.contains(param)) {
      hkgList->addItem(vehicleToggle);
    } else if (subaruKeys.contains(param)) {
      subaruList->addItem(vehicleToggle);
    } else if (toyotaKeys.contains(param)) {
      toyotaList->addItem(vehicleToggle);
    } else if (vehicleInfoKeys.contains(param)) {
      vehicleInfoList->addItem(vehicleToggle);
    } else {
      settingsList->addItem(vehicleToggle);

      parentKeys.insert(param);
    }

    if (ButtonControl *buttonControl = qobject_cast<ButtonControl*>(vehicleToggle)) {
      QObject::connect(buttonControl, &ButtonControl::clicked, this, &FrogPilotVehiclesPanel::openSubPanel);
    }

    QObject::connect(vehicleToggle, &AbstractControl::hideDescriptionEvent, [this]() {
      update();
    });
    QObject::connect(vehicleToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  static_cast<FrogPilotParamValueControl*>(toggles["LockDoorsTimer"])->setWarning("<b>Warning:</b> openpilot can't detect if keys are still inside the car, so ensure you have a spare key to prevent accidental lockouts!");

  QSet<QString> rebootKeys = {"TacoTuneHacks"};
  for (const QString &key : rebootKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, [key, this](bool state) {
      if (uiState()->scene.started) {
        if (key == "TacoTuneHacks" && state) {
          if (FrogPilotConfirmationDialog::toggleReboot(this)) {
            Hardware::reboot();
          }
        } else if (key != "TacoTuneHacks") {
          if (FrogPilotConfirmationDialog::toggleReboot(this)) {
            Hardware::reboot();
          }
        }
      }
    });
  }

  openDescriptions(forceOpenDescriptions, toggles);

  std::function<void()> updateCarLabels = [selectMakeButton, selectModelButton, this]() {
    std::thread([selectMakeButton, selectModelButton, this]() {
      QString carMake = QString::fromStdString(params.get("CarMake", true));
      QString carModel = QString::fromStdString(params.get(params.get("CarModelName").empty() ? "CarModel" : "CarModelName", true));

      runOnUIThread(selectMakeButton, [carMake, carModel, selectMakeButton, selectModelButton]() {
        selectMakeButton->setValue(carMake);
        selectModelButton->setValue(carModel);
      });
    }).detach();
  };
  QObject::connect(uiState(), &UIState::offroadTransition, updateCarLabels);
  updateCarLabels();

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [vehiclesLayout, vehiclesPanel, this] {
    if (forceOpenDescriptions) {
      openDescriptions(forceOpenDescriptions, toggles);
      disableOpenpilotLong->showDescription();
      forceFingerprint->showDescription();
    }
    vehiclesLayout->setCurrentWidget(vehiclesPanel);
  });
}

void FrogPilotVehiclesPanel::showEvent(QShowEvent *event) {
  if (forceOpenDescriptions) {
    disableOpenpilotLong->showDescription();
    forceFingerprint->showDescription();
  }

  QStringList detected;
  if (parent->hasPedal) detected << "comma Pedal";
  if (parent->hasSDSU) detected << "SDSU";
  if (parent->hasZSS) detected << "ZSS";
  static_cast<LabelControl*>(toggles["HardwareDetected"])->setText(detected.isEmpty() ? tr("None") : detected.join(", "));

  QString unknown = tr("Unknown until first drive");

  static_cast<LabelControl*>(toggles["BlindSpotSupport"])->setText(!parent->carDetected ? unknown : parent->hasBSM ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["OpenpilotLongitudinal"])->setText(!parent->carDetected ? unknown : parent->hasOpenpilotLongitudinal ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["PedalSupport"])->setText(!parent->carDetected ? unknown : parent->canUsePedal ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["RadarSupport"])->setText(!parent->carDetected ? unknown : parent->hasRadar ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["SDSUSupport"])->setText(!parent->carDetected ? unknown : parent->canUseSDSU ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["SNGSupport"])->setText(!parent->carDetected ? unknown : parent->hasSNG ? tr("Yes") : tr("No"));

  updateToggles();
}

void FrogPilotVehiclesPanel::updateToggles() {
  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      toggle->setVisible(false);
    }
  }

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      continue;
    }

    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels[key].toDouble();

    if (gmKeys.contains(key)) {
      setVisible &= parent->isGM;
    } else if (hkgKeys.contains(key)) {
      setVisible &= parent->isHKG;
    } else if (subaruKeys.contains(key)) {
      setVisible &= parent->isSubaru;
    } else if (toyotaKeys.contains(key)) {
      setVisible &= parent->isToyota;
    } else if (vehicleInfoKeys.contains(key)) {
      setVisible = true;
    }

    if (longitudinalKeys.contains(key)) {
      setVisible &= parent->hasOpenpilotLongitudinal;
    }

    if (key == "SNGHack") {
      setVisible &= !parent->hasSNG;
    }

    else if (key == "SubaruSNG") {
      setVisible &= parent->hasSNG;
    }

    else if (key == "TacoTuneHacks") {
      setVisible &= parent->isHKGCanFd;
    }

    else if (key == "VoltSNG") {
      setVisible &= parent->isVolt && !parent->hasSNG;
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (gmKeys.contains(key)) {
        toggles["GMToggles"]->setVisible(true);
      } else if (hkgKeys.contains(key)) {
        toggles["HKGToggles"]->setVisible(true);
      } else if (subaruKeys.contains(key)) {
        toggles["SubaruToggles"]->setVisible(true);
      } else if (toyotaKeys.contains(key)) {
        toggles["ToyotaToggles"]->setVisible(true);
      } else if (vehicleInfoKeys.contains(key)) {
        toggles["VehicleInfo"]->setVisible(true);
      }
    }
  }

  disableOpenpilotLong->setVisible((parent->hasOpenpilotLongitudinal || parent->openpilotLongitudinalControlDisabled) && !parent->hasAlphaLongitudinal && parent->tuningLevel >= parent->frogpilotToggleLevels["DisableOpenpilotLongitudinal"].toBool());
  forceFingerprint->setVisible(parent->tuningLevel >= parent->frogpilotToggleLevels["ForceFingerprint"].toBool());

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
