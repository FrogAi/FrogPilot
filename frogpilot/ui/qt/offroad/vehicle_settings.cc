#include <QRegularExpression>
#include <QTextStream>

#include "frogpilot/ui/qt/offroad/vehicle_settings.h"
#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

QStringList getCarNames(const QString &carMake, QMap<QString, QString> &carModels) {
  static QMap<QString, QString> makeMap = {
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
    {"skoda", "volkswagen"},
    {"subaru", "subaru"},
    {"tesla", "tesla"},
    {"toyota", "toyota"},
    {"volkswagen", "volkswagen"}
  };

  QStringList carNameList;

  QString makeKey = carMake;
  makeKey.replace("š", "s");

  QFile valuesFile(QString("../car/%1/values.py").arg(makeMap.value(makeKey, makeKey)));
  if (!valuesFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return carNameList;
  }

  QString fileContent = QTextStream(&valuesFile).readAll();
  valuesFile.close();

  fileContent.remove(QRegularExpression("#[^\n]*"));
  fileContent.remove(QRegularExpression("footnotes=\\[[^\\]]*\\],\\s*"));

  static QRegularExpression carNameRegex("CarDocs\\(\\s*\"([^\"]+)\"[^)]*\\)");
  static QRegularExpression platformRegex("((\\w+)\\s*=\\s*\\w+\\s*\\(\\s*\\[([\\s\\S]*?)\\]\\s*,)");
  static QRegularExpression validNameRegex("^[A-Za-z0-9 \u0160.,()-]+$");

  QRegularExpressionMatchIterator platformMatches = platformRegex.globalMatch(fileContent);
  while (platformMatches.hasNext()) {
    QRegularExpressionMatch platformMatch = platformMatches.next();
    QString platformName = platformMatch.captured(2);
    QString platformSection = platformMatch.captured(3);

    QRegularExpressionMatchIterator carNameMatches = carNameRegex.globalMatch(platformSection);
    while (carNameMatches.hasNext()) {
      QString carName = carNameMatches.next().captured(1);

      if (carName.contains(validNameRegex) && carName.count(" ") >= 1) {
        QString firstWord = carName.section(" ", 0, 0);

        if (firstWord.compare(carMake, Qt::CaseInsensitive) == 0) {
          carModels[carName] = platformName;
          carNameList.append(carName);
        }
      }
    }
  }

  carNameList.sort();
  return carNameList;
}

FrogPilotVehiclesPanel::FrogPilotVehiclesPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

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

  selectModelButton = new ButtonControl(tr("Car Model"), tr("SELECT"));

  selectMakeButton = new ButtonControl(tr("Car Make"), tr("SELECT"));
  QObject::connect(selectMakeButton, &ButtonControl::clicked, [makes, this]() {
    QString currentMake = QString::fromStdString(params.get("CarMake"));

    QString makeSelection = MultiOptionDialog::getSelection(tr("Choose your car make"), makes, currentMake, this);
    if (!makeSelection.isEmpty() && makeSelection != currentMake) {
      params.put("CarMake", makeSelection.toStdString());
      params.remove("CarModel");
      params.remove("CarModelName");

      selectMakeButton->setValue(makeSelection);
      selectModelButton->setValue("");
    }
  });
  settingsList->addItem(selectMakeButton);

  QObject::connect(selectModelButton, &ButtonControl::clicked, [this]() {
    QString modelSelection = MultiOptionDialog::getSelection(tr("Choose your car model"), getCarNames(QString::fromStdString(params.get("CarMake")).toLower(), carModels), QString::fromStdString(params.get("CarModelName")), this);
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
  FrogPilotListWidget *hondaList = new FrogPilotListWidget(this);
  FrogPilotListWidget *subaruList = new FrogPilotListWidget(this);
  FrogPilotListWidget *toyotaList = new FrogPilotListWidget(this);
  FrogPilotListWidget *vehicleInfoList = new FrogPilotListWidget(this);

  ScrollView *gmPanel = new ScrollView(gmList, this);
  ScrollView *hkgPanel = new ScrollView(hkgList, this);
  ScrollView *hondaPanel = new ScrollView(hondaList, this);
  ScrollView *subaruPanel = new ScrollView(subaruList, this);
  ScrollView *toyotaPanel = new ScrollView(toyotaList, this);
  ScrollView *vehicleInfoPanel = new ScrollView(vehicleInfoList, this);

  vehiclesLayout->addWidget(gmPanel);
  vehiclesLayout->addWidget(hkgPanel);
  vehiclesLayout->addWidget(hondaPanel);
  vehiclesLayout->addWidget(subaruPanel);
  vehiclesLayout->addWidget(toyotaPanel);
  vehiclesLayout->addWidget(vehicleInfoPanel);

  std::vector<std::tuple<QString, QString, QString, QString>> vehicleToggles {
    {"HondaToggles", tr("Acura/Honda Settings"), tr("<b>Settings that only work on Acura and Honda cars, covering how smoothly openpilot follows, how hard it can brake, and how it pulls away from a stop.</b><br><br>These only show up on Hondas using the older Nidec system, and \"Responsive Pedal at Low Speeds\" needs a comma pedal fitted on top of that."), ""},
    {"HondaAltTune", tr("Gentle Following"), tr("<b>Smooth out how openpilot speeds up and slows down while following the car ahead.</b><br><br>Best in stop-and-go traffic, where the standard tuning can feel like it surges and grabs. The trade is that openpilot reacts a little more slowly when the car ahead changes speed."), ""},
    {"HondaMaxBrake", tr("Increased Braking Force"), tr("<b>Let openpilot brake noticeably harder than Honda normally allows, so it can stop in a shorter distance.</b>"), ""},
    {"HondaLowSpeedPedal", tr("Responsive Pedal at Low Speeds"), tr("<b>Give openpilot a stronger push on the gas when pulling away from a stop, so takeoff feels less sluggish around town.</b><br><br>The softening it removes is openpilot's own, not Honda's: openpilot normally cuts its pedal command to 40% at a standstill and ramps it back to full by about 22 mph. It needs a comma pedal fitted to do anything."), ""},

    {"GMToggles", tr("General Motors Settings"), tr("<b>Settings that only work on Buick, Cadillac, Chevrolet, GMC and Holden cars, covering how openpilot stops, starts and handles hills.</b><br><br>Which of these you see depends on your exact model."), ""},
    {"ExperimentalGMTune", tr("FrogsGoMoo's Experimental Tune"), tr("<b>Swap in FrogsGoMoo's own GM tuning for coming to a stop and pulling away again.</b><br><br>It is still being worked on, so expect it to feel different rather than strictly better. It takes over your stopping and starting values from \"Driving Controls\" and hides those rows while it is on."), ""},
    {"LongPitch", tr("Smooth Pedal Response on Hills"), tr("<b>Even out the gas and brake when the road tilts, so openpilot does not surge downhill or fall behind going up.</b><br><br>This is already on."), ""},
    {"VoltSNG", tr("Stop-and-Go Hack"), tr("<b>Make the car pull away by itself after a full stop on a Chevrolet Volt, which does not do this from the factory.</b><br><br>Without it you have to press the gas or the resume button every time traffic moves off. Keep your foot near the brake the first few times so you can see how it behaves."), ""},

    {"HKGToggles", tr("Hyundai/Kia/Genesis Settings"), tr("<b>Settings that only work on Genesis, Hyundai and Kia cars, covering openpilot's newer gas and brake control and a steering torque hack.</b><br><br>Which of these you see depends on which system your car uses, and the steering hack only appears on cars using CAN-FD."), ""},
    {"NewLongAPI", tr("comma's New Longitudinal API"), tr("<b>Use comma's newer way of sending gas and brake commands to the car, which usually makes speeding up and slowing down smoother.</b><br><br>This is already on. It is not a good fit for every Genesis, Hyundai or Kia, so if your car brakes or accelerates oddly, switch it off and reboot."), ""},
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

    } else if (param == "HondaToggles") {
      ButtonControl *hondaButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(hondaButton, &ButtonControl::clicked, [vehiclesLayout, hondaPanel, this]() {
        openDescriptions(forceOpenDescriptions, toggles);
        vehiclesLayout->setCurrentWidget(hondaPanel);
      });
      vehicleToggle = hondaButton;

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
        params.putFloat("ClusterOffset", params_default.getFloat("ClusterOffset"));
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
    } else if (hondaKeys.contains(param)) {
      hondaList->addItem(vehicleToggle);
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

  static_cast<FrogPilotParamValueControl*>(toggles["LockDoorsTimer"])->setWarning(tr("<b>Warning:</b> openpilot can't tell whether your keys are still in the car, so keep a spare somewhere safe before you rely on this!"));

  QSet<QString> rebootKeys = {"HondaAltTune", "NewLongAPI", "SubaruSNG", "TacoTuneHacks", "ToyotaDSUBypass"};
  for (const QString &key : rebootKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, [this](bool) {
      if (uiState()->scene.started && FrogPilotConfirmationDialog::toggleReboot(this)) {
        Hardware::reboot();
      }
    });
  }

  openDescriptions(forceOpenDescriptions, toggles);

  QObject::connect(uiState(), &UIState::offroadTransition, this, &FrogPilotVehiclesPanel::updateCarLabels);
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
  updateCarLabels();

  if (forceOpenDescriptions) {
    disableOpenpilotLong->showDescription();
    forceFingerprint->showDescription();
  }

  frogpilotToggleLevels = parent->frogpilotToggleLevels;

  QStringList detected;
  if (parent->hasPedal) detected << "comma Pedal";
  if (parent->hasSDSU) detected << "SDSU";
  if (parent->hasZSS) detected << "ZSS";
  QString unknown = tr("Unknown until first drive");

  static_cast<LabelControl*>(toggles["HardwareDetected"])->setText(!parent->carDetected ? unknown : detected.isEmpty() ? tr("None") : detected.join(", "));

  static_cast<LabelControl*>(toggles["BlindSpotSupport"])->setText(!parent->carDetected ? unknown : parent->hasBSM ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["OpenpilotLongitudinal"])->setText(!parent->carDetected ? unknown : parent->hasOpenpilotLongitudinal ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["PedalSupport"])->setText(!parent->carDetected ? unknown : parent->canUsePedal ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["RadarSupport"])->setText(!parent->carDetected ? unknown : parent->hasRadar ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["SDSUSupport"])->setText(!parent->carDetected ? unknown : parent->canUseSDSU ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["SNGSupport"])->setText(!parent->carDetected ? unknown : parent->hasSNG ? tr("Yes") : tr("No"));

  updateToggles();
}

void FrogPilotVehiclesPanel::updateCarLabels() {
  selectMakeButton->setValue(QString::fromStdString(params.get("CarMake")));

  const std::string modelName = params.get("CarModelName");
  if (modelName.empty()) {
    selectModelButton->setValue(QString::fromStdString(params.get("CarModel")));
  } else {
    selectModelButton->setValue(QString::fromStdString(modelName));
  }
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

    bool setVisible = parent->tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (gmKeys.contains(key)) {
      setVisible &= parent->carDetected && parent->isGM;
    } else if (hkgKeys.contains(key)) {
      setVisible &= parent->carDetected && parent->isHKG;
    } else if (hondaKeys.contains(key)) {
      setVisible &= parent->carDetected && parent->isHonda;
    } else if (subaruKeys.contains(key)) {
      setVisible &= parent->carDetected && parent->isSubaru;
    } else if (toyotaKeys.contains(key)) {
      setVisible &= parent->carDetected && parent->isToyota;
    } else if (vehicleInfoKeys.contains(key)) {
      setVisible = true;
    }

    if (longitudinalKeys.contains(key)) {
      setVisible &= parent->hasOpenpilotLongitudinal;
    }

    if (key == "HondaAltTune") {
      setVisible &= parent->isHondaNidec;
    }

    else if (key == "HondaLowSpeedPedal") {
      setVisible &= parent->hasPedal;
    }

    else if (key == "HondaMaxBrake") {
      setVisible &= parent->isHondaNidec;
    }

    else if (key == "SNGHack") {
      setVisible &= !parent->hasPedal && !parent->hasSNG;
    }

    else if (key == "SubaruSNG") {
      setVisible &= parent->hasSNG;
    }

    else if (key == "TacoTuneHacks") {
      setVisible &= parent->isHKGCanFd;
    }

    else if (key == "ToyotaDSUBypass") {
      setVisible &= parent->canUseSDSU && !parent->hasSDSU;
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
      } else if (hondaKeys.contains(key)) {
        toggles["HondaToggles"]->setVisible(true);
      } else if (subaruKeys.contains(key)) {
        toggles["SubaruToggles"]->setVisible(true);
      } else if (toyotaKeys.contains(key)) {
        toggles["ToyotaToggles"]->setVisible(true);
      } else if (vehicleInfoKeys.contains(key)) {
        toggles["VehicleInfo"]->setVisible(true);
      }
    }
  }

  disableOpenpilotLong->setVisible((parent->hasOpenpilotLongitudinal || parent->openpilotLongitudinalControlDisabled) && !parent->hasExperimentalOpenpilotLongitudinal && parent->tuningLevel >= frogpilotToggleLevels["DisableOpenpilotLongitudinal"].toDouble());
  forceFingerprint->setVisible(parent->tuningLevel >= frogpilotToggleLevels["ForceFingerprint"].toDouble());

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
