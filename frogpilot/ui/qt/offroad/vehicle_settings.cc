#include "frogpilot/ui/qt/offroad/vehicle_settings.h"

FrogPilotVehiclesPanel::FrogPilotVehiclesPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  QStackedLayout *vehiclesLayout = new QStackedLayout();
  addItem(vehiclesLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);

  ScrollView *vehiclesPanel = new ScrollView(settingsList, this);

  vehiclesLayout->addWidget(vehiclesPanel);

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

  const std::vector<std::tuple<QString, QString, QString, QString>> vehicleToggles {
    {"HondaToggles", tr("Acura/Honda Settings"), tr("<b>Settings that only work on Acura and Honda cars with the older Nidec cruise control, covering how openpilot follows and brakes.</b>"), ""},
    {"HondaAltTune", tr("Gentle Following"), tr("<b>Soften how hard openpilot corrects its speed behind the car ahead, for smoother following in stop-and-go traffic.</b><br><br>openpilot settles small speed differences more slowly, so it can take a moment longer to close a gap after the car ahead pulls away."), ""},
    {"HondaMaxBrake", tr("Increased Braking Force"), tr("<b>Let openpilot use your car's full braking force for its hardest stops.</b><br><br>Without it, openpilot's hardest stop only uses about four fifths of the brakes on these cars. With it on, every stop brakes a little firmer for the same request."), ""},

    {"GMToggles", tr("General Motors Settings"), tr("<b>Settings that only work on Buick, Cadillac, Chevrolet, GMC and Holden cars, covering pedal response on hills and how openpilot stops and starts.</b>"), ""},
    {"LongPitch", tr("Smooth Pedal Response on Hills"), tr("<b>Add gas going uphill and ease off going downhill, so openpilot holds a steady pace on hills instead of correcting after the car has already slowed down or sped up.</b><br><br>Slopes gentler than about 1% are ignored. The brakes get the same adjustment above about 22 mph, and it fades out as you slow to about 11 mph so stops feel the same as on flat ground."), ""},
    {"VoltSNG", tr("Stop-and-Go Hack"), tr("<b>Make the car pull away by itself after a full stop on a Chevrolet Volt, which does not do this from the factory.</b><br><br>Without it you have to press the gas or the resume button every time traffic moves off. Keep your foot near the brake the first few times so you can see how it behaves."), ""},

    {"HKGToggles", tr("Hyundai/Kia/Genesis Settings"), tr("<b>Settings that only work on Genesis, Hyundai and Kia cars, covering a steering torque hack.</b><br><br>The steering hack only appears on cars using CAN-FD."), ""},
    {"TacoTuneHacks", tr("\"Taco Bell Run\" Torque Hack"), tr("<b>Let openpilot pull the wheel harder through turns, using the trick comma demonstrated on their 2022 \"Taco Bell Run\" drive.</b><br><br>It raises the steering limit everywhere, not just at low speed, and it relaxes one of the safety checks that normally caps steering effort. You will also have to grip the wheel more firmly to take over."), ""},

    {"SubaruToggles", tr("Subaru Settings"), tr("<b>Settings that only work on Subaru cars.</b><br><br>There is one, and it decides whether your car pulls away by itself after a stop."), ""},
    {"SubaruSNG", tr("Stop and Go"), tr("<b>Get your car moving again by itself once the car ahead pulls away from a full stop.</b><br><br>Subaru's own cruise holds the brakes and waits for you to press resume after a few seconds stopped. FrogPilot watches the car ahead and sends that resume for you. Keep your foot ready near the brake the first few times so you can see how it behaves."), ""},

    {"ToyotaToggles", tr("Toyota/Lexus Settings"), tr("<b>Settings that only work on Lexus and Toyota cars, covering door locking, dashboard speed, stop-and-go and openpilot's own tuning.</b><br><br>Which of these you see depends on your exact model and on what hardware is fitted."), ""},
    {"ToyotaDoors", tr("Automatically Lock/Unlock Doors"), tr("<b>Lock the doors when you shift out of park and unlock them again when you shift back into it.</b><br><br>This runs whenever the car is on, whether or not openpilot is engaged."), ""},
    {"ClusterOffset", tr("Dashboard Speed Offset"), tr("<b>Line up the speed openpilot shows on screen with the number on your dashboard, which most cars deliberately read a little high.</b><br><br>With \"Use Wheel Speed\" off, raise it until openpilot's number matches your dashboard. This does not change how fast openpilot actually drives, with one exception: while it is following posted speed limits, a higher number here makes it drive slightly slower."), ""},
    {"ToyotaDSUBypass", tr("DSU Re-Route Harness"), tr("<b>Let openpilot control the gas and brake on an older Toyota by rerouting the cruise control computer's messages through a wiring harness you fit yourself.</b><br><br>The DSU is the box that normally runs your car's radar cruise. Only turn this on after the harness is physically installed, because openpilot cannot check for it."), ""},
    {"FrogsGoMoosTweak", tr("FrogsGoMoo's Personal Tweaks"), tr("<b>Swap in FrogsGoMoo's own settings for how openpilot comes to a stop.</b><br><br>These are personal preferences rather than a fix for anything, and they are already on. They take over your stopping and starting values from \"Driving Controls\" and hide those rows while this is on, though on a Toyota the starting value has no effect."), ""},
    {"LockDoorsTimer", tr("Lock Doors On Ignition Off After"), tr("<b>Lock the doors on their own once you have switched the car off and left it, after the number of seconds you pick.</b><br><br>The countdown only starts once the screen has gone dark, and it starts over if the driver camera still sees a face in the driver's seat or if any door is open. Somebody sitting in the front passenger seat will not hold it off. Set it to \"Never\" to switch it off."), ""},
    {"SNGHack", tr("Stop-and-Go Hack"), tr("<b>Make the car pull away by itself after a full stop on a Lexus or Toyota that does not do this from the factory.</b><br><br>Without it you have to press the gas or the resume button every time traffic moves off. It works by telling the car openpilot is never fully stopped, so keep your foot near the brake the first few times."), ""},

    {"VehicleInfo", tr("Vehicle Info"), tr("<b>What openpilot has worked out about your car and what it can do with it.</b><br><br>These rows are read-only. They stay on \"Unknown until first drive\" until openpilot has recognised your car."), ""},
    {"HardwareDetected", tr("3rd Party Hardware Detected"), tr("<b>Extra hardware openpilot has found fitted to your car, such as a comma pedal, an SDSU or a ZSS.</b><br><br>openpilot works these out from your car's wiring on its own. \"None\" is not proof nothing is fitted: a comma pedal is only reported on cars that support one."), ""},
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
      QObject::connect(gmButton, &ButtonControl::clicked, [vehiclesLayout, gmPanel]() {
        vehiclesLayout->setCurrentWidget(gmPanel);
      });
      vehicleToggle = gmButton;

    } else if (param == "HondaToggles") {
      ButtonControl *hondaButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(hondaButton, &ButtonControl::clicked, [vehiclesLayout, hondaPanel]() {
        vehiclesLayout->setCurrentWidget(hondaPanel);
      });
      vehicleToggle = hondaButton;

    } else if (param == "HKGToggles") {
      ButtonControl *hkgButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(hkgButton, &ButtonControl::clicked, [vehiclesLayout, hkgPanel]() {
        vehiclesLayout->setCurrentWidget(hkgPanel);
      });
      vehicleToggle = hkgButton;

    } else if (param == "SubaruToggles") {
      ButtonControl *subaruButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(subaruButton, &ButtonControl::clicked, [vehiclesLayout, subaruPanel]() {
        vehiclesLayout->setCurrentWidget(subaruPanel);
      });
      vehicleToggle = subaruButton;

    } else if (param == "ToyotaToggles") {
      ButtonControl *toyotaButton = new ButtonControl(title, tr("MANAGE"), desc);
      QObject::connect(toyotaButton, &ButtonControl::clicked, [vehiclesLayout, toyotaPanel]() {
        vehiclesLayout->setCurrentWidget(toyotaPanel);
      });
      vehicleToggle = toyotaButton;
    } else if (param == "ToyotaDoors") {
      std::vector<QString> lockToggles{"LockDoors", "UnlockDoors"};
      std::vector<QString> lockToggleNames{tr("Lock"), tr("Unlock")};
      vehicleToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, lockToggles, lockToggleNames);
    } else if (param == "LockDoorsTimer") {
      std::map<float, QString> autoLockLabels{{0, tr("Never")}};
      vehicleToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 300, tr(" seconds"), autoLockLabels, 5);
    } else if (param == "ClusterOffset") {
      std::vector<QString> clusterOffsetButton{tr("Reset")};
      FrogPilotParamValueButtonControl *clusterOffsetToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 1.000, 1.050, "x", std::map<float, QString>(), 0.001, false, {}, clusterOffsetButton);
      QObject::connect(clusterOffsetToggle, &FrogPilotParamValueButtonControl::buttonClicked, [clusterOffsetToggle, this]() {
        params.putFloat("ClusterOffset", std::stof(params.getKeyDefaultValue("ClusterOffset").value()));
        clusterOffsetToggle->refresh();
      });
      vehicleToggle = clusterOffsetToggle;

    } else if (param == "VehicleInfo") {
      ButtonControl *vehicleInfoButton = new ButtonControl(title, tr("VIEW"), desc);
      QObject::connect(vehicleInfoButton, &ButtonControl::clicked, [vehiclesLayout, vehicleInfoPanel]() {
        vehiclesLayout->setCurrentWidget(vehicleInfoPanel);
      });
      vehicleToggle = vehicleInfoButton;
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
      QObject::connect(buttonControl, &ButtonControl::clicked, [this]() {
        emit openSubPanel();
        openDescriptions(forceOpenDescriptions, toggles);
      });
    }
  }

  static_cast<FrogPilotParamValueControl*>(toggles["LockDoorsTimer"])->setWarning(tr(
    "<b>Warning:</b> openpilot can't tell whether your keys are still in the car, so keep a spare somewhere safe before you rely on this!"));

  QSet<QString> rebootKeys = {"HondaAltTune", "SubaruSNG", "TacoTuneHacks", "ToyotaDSUBypass"};
  for (const QString &key : rebootKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, [key, this](bool state) {
      if (uiState()->scene.started) {
        if (key == "TacoTuneHacks" && state) {
          if (FrogPilotConfirmationDialog::toggleReboot(this)) {
            FrogPilotConfirmationDialog::softReboot(this);
          }
        } else if (key != "TacoTuneHacks") {
          if (FrogPilotConfirmationDialog::toggleReboot(this)) {
            FrogPilotConfirmationDialog::softReboot(this);
          }
        }
      }
    });
  }

  QObject::connect(static_cast<ToggleControl*>(toggles["SubaruSNG"]), &ToggleControl::toggleFlipped, [this](bool state) {
    static_cast<LabelControl*>(toggles["SNGSupport"])->setText(state ? tr("Yes") : tr("No"));
  });

  openDescriptions(forceOpenDescriptions, toggles);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [vehiclesLayout, vehiclesPanel, this] {
    if (forceOpenDescriptions) {
      openDescriptions(forceOpenDescriptions, toggles);
    }
    vehiclesLayout->setCurrentWidget(vehiclesPanel);
  });
}

void FrogPilotVehiclesPanel::showEvent(QShowEvent *event) {
  if (forceOpenDescriptions) {
  }

  QStringList detected;
  if (parent->hasPedal) {
    detected << "comma Pedal";
  }
  if (parent->hasSDSU) {
    detected << "SDSU";
  }
  if (parent->hasZSS) {
    detected << "ZSS";
  }
  bool subaruSNG = parent->tuningLevel < parent->frogpilotToggleLevels.value("SubaruSNG").toDouble() || params.getBool("SubaruSNG");
  bool supportsSNG = parent->hasSNG && (!parent->isSubaru || subaruSNG);
  QString unknown = tr("Unknown until first drive");

  static_cast<LabelControl*>(toggles["HardwareDetected"])->setText(!parent->carDetected ? unknown : detected.isEmpty() ? tr("None") : detected.join(", "));

  static_cast<LabelControl*>(toggles["BlindSpotSupport"])->setText(!parent->carDetected ? unknown : parent->hasBSM ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["OpenpilotLongitudinal"])->setText(!parent->carDetected ? unknown : parent->hasOpenpilotLongitudinal ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["PedalSupport"])->setText(!parent->carDetected ? unknown : parent->canUsePedal ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["RadarSupport"])->setText(!parent->carDetected ? unknown : parent->hasRadar ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["SDSUSupport"])->setText(!parent->carDetected ? unknown : parent->canUseSDSU ? tr("Yes") : tr("No"));
  static_cast<LabelControl*>(toggles["SNGSupport"])->setText(!parent->carDetected ? unknown : supportsSNG ? tr("Yes") : tr("No"));

  updateToggles();
}

void FrogPilotVehiclesPanel::updateToggles() {
  QSet<QString> visibleParents;

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      continue;
    }

    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels.value(key).toDouble();

    if (gmKeys.contains(key)) {
      setVisible &= parent->carDetected && parent->isGM;
    } else if (hkgKeys.contains(key)) {
      setVisible &= parent->carDetected && parent->isHKG;
    } else if (hondaKeys.contains(key)) {
      setVisible &= parent->carDetected && parent->isHondaNidec;
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

    if (key == "LongPitch") {
      setVisible &= !parent->isGMCCOnly;
    }

    else if (key == "SNGHack") {
      setVisible &= !parent->hasSNG;
    }

    else if (key == "SubaruSNG") {
      setVisible &= parent->hasSNG;
    }

    else if (key == "TacoTuneHacks") {
      setVisible &= parent->isHKGCanFd;
    }

    else if (key == "ToyotaDSUBypass") {
      setVisible &= parent->canUseDSUBypass;
    }

    else if (key == "VoltSNG") {
      setVisible &= parent->isVolt && !parent->hasSNG;
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (gmKeys.contains(key)) {
        visibleParents.insert("GMToggles");
      } else if (hkgKeys.contains(key)) {
        visibleParents.insert("HKGToggles");
      } else if (hondaKeys.contains(key)) {
        visibleParents.insert("HondaToggles");
      } else if (subaruKeys.contains(key)) {
        visibleParents.insert("SubaruToggles");
      } else if (toyotaKeys.contains(key)) {
        visibleParents.insert("ToyotaToggles");
      } else if (vehicleInfoKeys.contains(key)) {
        visibleParents.insert("VehicleInfo");
      }
    }
  }

  for (const QString &key : parentKeys) {
    toggles[key]->setVisible(visibleParents.contains(key));
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
