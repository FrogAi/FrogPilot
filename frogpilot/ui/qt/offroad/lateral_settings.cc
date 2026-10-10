#include "frogpilot/ui/qt/offroad/lateral_settings.h"

FrogPilotLateralPanel::FrogPilotLateralPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  QStackedLayout *lateralLayout = new QStackedLayout();
  addItem(lateralLayout);

  FrogPilotListWidget *lateralList = new FrogPilotListWidget(this);

  ScrollView *lateralPanel = new ScrollView(lateralList, this);

  lateralLayout->addWidget(lateralPanel);

  FrogPilotListWidget *advancedLateralTuneList = new FrogPilotListWidget(this);
  FrogPilotListWidget *aolList = new FrogPilotListWidget(this);
  FrogPilotListWidget *laneChangeList = new FrogPilotListWidget(this);
  FrogPilotListWidget *lateralTuneList = new FrogPilotListWidget(this);
  FrogPilotListWidget *qolList = new FrogPilotListWidget(this);

  ScrollView *advancedLateralTunePanel = new ScrollView(advancedLateralTuneList, this);
  ScrollView *aolPanel = new ScrollView(aolList, this);
  ScrollView *laneChangePanel = new ScrollView(laneChangeList, this);
  ScrollView *lateralTunePanel = new ScrollView(lateralTuneList, this);
  ScrollView *qolPanel = new ScrollView(qolList, this);

  lateralLayout->addWidget(advancedLateralTunePanel);
  lateralLayout->addWidget(aolPanel);
  lateralLayout->addWidget(laneChangePanel);
  lateralLayout->addWidget(lateralTunePanel);
  lateralLayout->addWidget(qolPanel);

  const std::vector<std::tuple<QString, QString, QString, QString>> lateralToggles {
    {"AdvancedLateralTune", tr("Advanced Lateral Tuning"), tr("<b>Hand-set the steering numbers openpilot normally works out for itself, and switch that learning on or off.</b><br><br>Wrong values show up as a wheel that feels twitchy or lazy. Every number has a \"Reset\" button that puts your car's original value back."), "../../frogpilot/assets/toggle_icons/icon_advanced_lateral_tune.svg"},
    {"SteerDelay", tr("Actuator Delay"), tr("<b>How long your car takes to respond after openpilot turns the wheel.</b><br><br>Raise it if your car reacts late. Lower it if the steering feels jumpy. openpilot learns this on its own by default."), ""},
    {"SteerFriction", tr("Friction"), tr("<b>How much extra effort openpilot uses to get the wheel moving off center.</b><br><br>Raise it if the wheel sticks near center and openpilot is slow to start correcting. Lower it if the wheel jitters on a straight road."), ""},
    {"SteerKP", tr("Kp Factor"), tr("<b>How hard openpilot pushes the wheel to pull your car back to the middle of the lane.</b><br><br>Raise it if your car sits off to one side or is slow to come back. Lower it if the wheel feels twitchy or keeps overshooting. openpilot never changes this one on its own."), ""},
    {"SteerLatAccel", tr("Lateral Acceleration"), tr("<b>How much steering effort openpilot uses to turn your car, where lower values make it steer harder and higher values make it steer more gently.</b><br><br>Lower it if your car drifts wide in curves. Raise it if the car turns in more sharply than you want."), ""},
    {"SteerRatio", tr("Steer Ratio"), tr("<b>How far your steering wheel turns to swing the front wheels a set amount, where raising it makes openpilot turn the wheel further for the same corner.</b><br><br>Lower it if openpilot feels twitchy or keeps overshooting the middle of the lane. Raise it if openpilot reacts too slowly and lets the car drift wide. openpilot learns this on its own by default."), ""},
    {"ForceAutoTune", tr("Force Auto-Tune On"), tr("<b>Let openpilot work out its own steering values on a car that doesn't do this automatically.</b><br><br>What openpilot learns replaces the numbers you set, so those rows disappear while this is on."), ""},
    {"ForceAutoTuneOff", tr("Force Auto-Tune Off"), tr("<b>Stop openpilot from working out its own steering values, and use the numbers you set instead.</b><br><br>Only offered on cars that normally tune themselves."), ""},
    {"ForceTorqueController", tr("Force Torque Controller"), tr("<b>Switch openpilot to steering by effort instead of by wheel angle, which usually holds the lane more smoothly through curves.</b><br><br>Only offered on cars that don't already steer this way. Changing this while driving asks you to reboot."), ""},

    {"AlwaysOnLateral", tr("Always On Lateral"), tr("<b>openpilot keeps steering for you even when it isn't controlling the gas and brake, so it holds your lane when you press a pedal, cancel, or haven't engaged openpilot at all.</b><br><br>It steers from the moment your car's cruise control is switched on until you switch that back off or shift out of drive, and it pauses while you brake below the speed set in \"Pause on Brake Press Below\", except at a full stop. On the newer Hyundai, Kia and Genesis cars, the LKAS button takes the place of cruise control while \"Enable With LKAS\" is on."), "../../frogpilot/assets/toggle_icons/icon_always_on_lateral.svg"},
    {"AlwaysOnLateralLKAS", tr("Enable With LKAS"), tr("<b>Use the LKAS button to arm steering, so openpilot keeps steering even when it is not engaged.</b><br><br>openpilot does not read your car's LKAS status for this. It starts every drive disarmed and each press of the LKAS button flips it, so expect one press after starting the car. With this off, the LKAS button is free to reassign under \"LKAS Button\", and steering stops as soon as openpilot is no longer engaged, unless openpilot handles the gas and brake, in which case the cruise control button arms it instead."), ""},
    {"PauseAOLOnBrake", tr("Pause on Brake Press Below"), tr("<b>Pause \"Always On Lateral\" below the set speed while the brake pedal is pressed, except at a full stop.</b>"), ""},

    {"LaneChanges", tr("Lane Changes"), tr("<b>Allow openpilot to change lanes.</b>"), "../../frogpilot/assets/toggle_icons/icon_lane.svg"},
    {"NudgelessLaneChange", tr("Automatic Lane Changes"), tr("<b>With your turn signal on, openpilot starts the lane change on its own instead of waiting for a small push on the wheel from you.</b><br><br>It waits out \"Lane Change Delay\" before moving over, skips the move below the minimum lane change speed, and stays out of lanes narrower than any \"Minimum Lane Width\" you set. It only holds off for a car beside you if your car came with factory blind spot monitoring, so without that hardware there is no blind spot check at all. Check that the lane is clear yourself before you signal."), ""},
    {"LaneChangeTime", tr("Lane Change Delay"), tr("<b>Delay between turn signal activation and the start of an automatic lane change.</b>"), ""},
    {"MinimumLaneChangeSpeed", tr("Minimum Lane Change Speed"), tr("<b>The slowest speed at which openpilot will change lanes for you.</b><br><br>Below this speed you steer into the lane change yourself. Set it to \"Any speed\" to let openpilot change lanes at any speed, but that also switches off \"Steer Into Turns Below Lane Change Speed\", which only ever runs below this number."), ""},
    {"LaneDetectionWidth", tr("Minimum Lane Width"), tr("<b>Prevent automatic lane changes into lanes narrower than the set width.</b>"), ""},
    {"OneLaneChange", tr("One Lane Change Per Signal"), tr("<b>Only one lane change per turn signal.</b><br><br>Switch the signal off and back on to change lanes again. Lane changes you start yourself by pushing the wheel count toward this too."), ""},

    {"LateralTune", tr("Lateral Tuning"), tr("<b>Switch openpilot's steering over to a neural network for a smoother wheel on cars that support it, and have it steer into turns when you signal below your minimum lane change speed.</b>"), "../../frogpilot/assets/toggle_icons/icon_lateral_tune.svg"},
    {"NNFF", tr("Neural Network Feedforward (NNFF)"), tr("<b>openpilot steers using a model trained on real driving data from cars like yours, which usually holds the lane more steadily and tracks curves more closely.</b><br><br>It only appears when a trained model matches your car. While it's on it takes over the \"Friction\" and \"Lateral Acceleration\" tuning, so those two rows disappear. Built by Twilsonco."), ""},
    {"NNFFLite", tr("Neural Network Feedforward (NNFF) Lite"), tr("<b>openpilot looks ahead at how sharply the road is about to bend and starts adjusting its steering early, which smooths how it enters and leaves curves.</b><br><br>Use this when the full \"Neural Network Feedforward (NNFF)\" setting isn't offered for your car. It borrows the look-ahead without the trained model, so the improvement is smaller."), ""},
    {"TurnDesires", tr("Steer Into Turns Below Lane Change Speed"), tr("<b>With your turn signal on below your minimum lane change speed, openpilot steers with you into the turn instead of pulling back toward the lane you're leaving.</b><br><br>Use it for intersections and tight exit ramps, where openpilot normally works against your steering. You still choose where and when to turn."), ""},

    {"QOLLateral", tr("Quality of Life"), tr("<b>Hand the wheel back to yourself below a speed you set.</b>"), "../../frogpilot/assets/toggle_icons/icon_quality_of_life.svg"},
    {"PauseLateralSpeed", tr("Pause Steering Below"), tr("<b>openpilot stops steering below the speed you set and hands the wheel back to you, and \"Turn Signal Only\" narrows that to just the moments a turn signal is flashing.</b><br><br>The gas and brake carry on as normal and nothing warns you when the steering stops, so be ready to take the wheel at low speed. This pauses \"Always On Lateral\" too."), ""}
  };

  for (const auto &[param, title, desc, icon] : lateralToggles) {
    AbstractControl *lateralToggle;

    if (param == "AdvancedLateralTune") {
      FrogPilotManageControl *advancedLateralTuneToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(advancedLateralTuneToggle, &FrogPilotManageControl::manageButtonClicked, [lateralLayout, advancedLateralTunePanel]() {
        lateralLayout->setCurrentWidget(advancedLateralTunePanel);
      });
      lateralToggle = advancedLateralTuneToggle;
    } else if (param == "SteerDelay" || param == "SteerFriction") {
      std::vector<QString> resetButton{tr("Reset")};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0.01, 1, QString(), std::map<float, QString>(), 0.01, false, {}, resetButton);
    } else if (param == "SteerKP" || param == "SteerLatAccel" || param == "SteerRatio") {
      std::vector<QString> resetButton{tr("Reset")};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0, 0, QString(), std::map<float, QString>(), 0.01, false, {}, resetButton);

    } else if (param == "AlwaysOnLateral") {
      FrogPilotManageControl *aolToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(aolToggle, &FrogPilotManageControl::manageButtonClicked, [lateralLayout, aolPanel]() {
        lateralLayout->setCurrentWidget(aolPanel);
      });
      lateralToggle = aolToggle;
    } else if (param == "PauseAOLOnBrake") {
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, QString(), std::map<float, QString>(), 1, true);

    } else if (param == "LaneChanges") {
      FrogPilotManageControl *laneChangeToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(laneChangeToggle, &FrogPilotManageControl::manageButtonClicked, [lateralLayout, laneChangePanel]() {
        lateralLayout->setCurrentWidget(laneChangePanel);
      });
      lateralToggle = laneChangeToggle;
    } else if (param == "LaneChangeTime") {
      const QString second = tr(" second");
      const QString seconds = tr(" seconds");

      std::map<float, QString> laneChangeTimeLabels;
      for (float i = 0; i <= 5; i += 0.1) {
        laneChangeTimeLabels[i] = i == 0 ? tr("Instant") : std::lround(i / 0.1) == 1 / 0.1 ? QString::number(i, 'f', 1) + second : QString::number(i, 'f', 1) + seconds;
      }
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 5, QString(), laneChangeTimeLabels, 0.1);
    } else if (param == "LaneDetectionWidth") {
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 15, QString(), std::map<float, QString>(), 0.1, true);
    } else if (param == "MinimumLaneChangeSpeed") {
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, QString(), std::map<float, QString>(), 1, true);

    } else if (param == "LateralTune") {
      FrogPilotManageControl *lateralTuneToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(lateralTuneToggle, &FrogPilotManageControl::manageButtonClicked, [lateralLayout, lateralTunePanel]() {
        lateralLayout->setCurrentWidget(lateralTunePanel);
      });
      lateralToggle = lateralTuneToggle;

    } else if (param == "QOLLateral") {
      FrogPilotManageControl *qolLateralToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(qolLateralToggle, &FrogPilotManageControl::manageButtonClicked, [lateralLayout, qolPanel]() {
        lateralLayout->setCurrentWidget(qolPanel);
      });
      lateralToggle = qolLateralToggle;
    } else if (param == "PauseLateralSpeed") {
      std::vector<QString> pauseLateralToggles{"PauseLateralOnSignal"};
      std::vector<QString> pauseLateralToggleNames{tr("Turn Signal Only")};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0, 99, QString(), std::map<float, QString>(), 1, true, pauseLateralToggles, pauseLateralToggleNames, true);

    } else {
      lateralToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = lateralToggle;

    if (advancedLateralTuneKeys.contains(param)) {
      advancedLateralTuneList->addItem(lateralToggle);
    } else if (aolKeys.contains(param)) {
      aolList->addItem(lateralToggle);
    } else if (laneChangeKeys.contains(param)) {
      laneChangeList->addItem(lateralToggle);
    } else if (lateralTuneKeys.contains(param)) {
      lateralTuneList->addItem(lateralToggle);
    } else if (qolKeys.contains(param)) {
      qolList->addItem(lateralToggle);
    } else {
      lateralList->addItem(lateralToggle);

      parentKeys.insert(param);
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(lateralToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        emit openSubPanel();
        openDescriptions(forceOpenDescriptions, toggles);
      });
    }
  }

  QSet<QString> forceUpdateKeys = {"ForceAutoTune", "ForceAutoTuneOff", "LateralTune", "NNFF", "NudgelessLaneChange"};
  for (const QString &key : forceUpdateKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, this, &FrogPilotLateralPanel::updateToggles);
  }

  QSet<QString> rebootKeys = {"AdvancedLateralTune", "AlwaysOnLateral", "ForceTorqueController", "LateralTune", "NNFF", "NNFFLite"};
  for (const QString &key : rebootKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, [key, parent, this](bool state) {
      bool rebootRequired = true;
      if (key == "AdvancedLateralTune") {
        rebootRequired = !parent->isAngleCar && params.getBool("ForceTorqueController");
      } else if (key == "AlwaysOnLateral") {
        rebootRequired = state;
      } else if (key == "LateralTune") {
        bool usingNNFF = parent->hasNNFFLog && (parent->tuningLevel < parent->frogpilotToggleLevels.value("NNFF").toDouble() || params.getBool("NNFF"));
        bool usingNNFFLite = parent->tuningLevel < parent->frogpilotToggleLevels.value("NNFFLite").toDouble() || params.getBool("NNFFLite");
        rebootRequired = !parent->isAngleCar && (usingNNFF || usingNNFFLite);
      }

      if (uiState()->scene.started && rebootRequired && FrogPilotConfirmationDialog::toggleReboot(this)) {
        FrogPilotConfirmationDialog::softReboot(this);
      }
    });
  }

  steerDelayToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerDelay"]);
  QObject::connect(steerDelayToggle, &FrogPilotParamValueButtonControl::buttonClicked, [parent, this]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Reset <b>Actuator Delay</b> to its default value?"), this)) {
      params.putFloat("SteerDelay", parent->steerActuatorDelay);
      steerDelayToggle->refresh();
    }
  });

  steerFrictionToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerFriction"]);
  QObject::connect(steerFrictionToggle, &FrogPilotParamValueButtonControl::buttonClicked, [parent, this]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Reset <b>Friction</b> to its default value?"), this)) {
      params.putFloat("SteerFriction", parent->friction);
      steerFrictionToggle->refresh();
    }
  });

  steerKPToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerKP"]);
  QObject::connect(steerKPToggle, &FrogPilotParamValueButtonControl::buttonClicked, [parent, this]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Reset <b>Kp Factor</b> to its default value?"), this)) {
      params.putFloat("SteerKP", parent->steerKp);
      steerKPToggle->refresh();
    }
  });

  steerLatAccelToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerLatAccel"]);
  QObject::connect(steerLatAccelToggle, &FrogPilotParamValueButtonControl::buttonClicked, [parent, this]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Reset <b>Lateral Acceleration</b> to its default value?"), this)) {
      params.putFloat("SteerLatAccel", parent->latAccelFactor);
      steerLatAccelToggle->refresh();
    }
  });

  steerRatioToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerRatio"]);
  QObject::connect(steerRatioToggle, &FrogPilotParamValueButtonControl::buttonClicked, [parent, this]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Reset <b>Steer Ratio</b> to its default value?"), this)) {
      params.putFloat("SteerRatio", parent->steerRatio);
      steerRatioToggle->refresh();
    }
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [lateralLayout, lateralPanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    lateralLayout->setCurrentWidget(lateralPanel);
  });
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, this, &FrogPilotLateralPanel::updateMetric);
}

void FrogPilotLateralPanel::showEvent(QShowEvent *event) {
  if (parent->carDetected) {
    steerDelayToggle->setTitle(QString(tr("Actuator Delay (Default: %1)")).arg(QString::number(parent->steerActuatorDelay, 'f', 2)));
    steerFrictionToggle->setTitle(QString(tr("Friction (Default: %1)")).arg(QString::number(parent->friction, 'f', 2)));
    steerKPToggle->setTitle(QString(tr("Kp Factor (Default: %1)")).arg(QString::number(parent->steerKp, 'f', 2)));
    steerKPToggle->updateControl(parent->steerKp * 0.5, parent->steerKp * 1.5);
    steerLatAccelToggle->setTitle(QString(tr("Lateral Acceleration (Default: %1)")).arg(QString::number(parent->latAccelFactor, 'f', 2)));
    steerLatAccelToggle->updateControl(parent->latAccelFactor * 0.5, parent->latAccelFactor * 1.5);
    steerRatioToggle->setTitle(QString(tr("Steer Ratio (Default: %1)")).arg(QString::number(parent->steerRatio, 'f', 2)));
    steerRatioToggle->updateControl(parent->steerRatio * 0.5, parent->steerRatio * 1.5);
  }

  updateToggles();
}

void FrogPilotLateralPanel::updateMetric(bool metric, bool bootRun) {
  static bool previousMetric;
  if (metric != previousMetric && !bootRun) {
    double distanceConversion = metric ? FOOT_TO_METER : METER_TO_FOOT;
    double speedConversion = metric ? MILE_TO_KM : KM_TO_MILE;

    params.putFloat("LaneDetectionWidth", params.getFloat("LaneDetectionWidth") * distanceConversion);

    params.putFloat("MinimumLaneChangeSpeed", std::lround(params.getFloat("MinimumLaneChangeSpeed") * speedConversion));
    params.putInt("PauseAOLOnBrake", std::lround(params.getInt("PauseAOLOnBrake") * speedConversion));
    params.putFloat("PauseLateralSpeed", std::lround(params.getFloat("PauseLateralSpeed") * speedConversion));
  }
  previousMetric = metric;

  static std::map<float, QString> imperialDistanceLabels;
  static std::map<float, QString> metricDistanceLabels;
  static std::map<float, QString> minimumSpeedLabels;
  static std::map<float, QString> speedLabels;

  static bool labelsInitialized = false;
  if (!labelsInitialized) {
    const QString foot = tr(" foot");
    const QString feet = tr(" feet");
    const QString meter = tr(" meter");
    const QString meters = tr(" meters");

    for (int i = 0; i <= 150; ++i) {
      float key = i / 10.0f;
      imperialDistanceLabels[key] = key == 0 ? tr("Off") : QString::number(key, 'f', 1) + (key == 1 ? foot : feet);
    }

    for (int i = 0; i <= 50; ++i) {
      float key = i / 10.0f;
      metricDistanceLabels[key] = key == 0 ? tr("Off") : QString::number(key, 'f', 1) + (key == 1 ? meter : meters);
    }

    minimumSpeedLabels[0] = tr("Any speed");
    speedLabels[0] = tr("Off");

    labelsInitialized = true;
  }

  FrogPilotParamValueControl *laneWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["LaneDetectionWidth"]);
  FrogPilotParamValueControl *minimumLaneChangeSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["MinimumLaneChangeSpeed"]);
  FrogPilotParamValueControl *pauseAOLOnBrakeToggle = static_cast<FrogPilotParamValueControl*>(toggles["PauseAOLOnBrake"]);
  FrogPilotParamValueControl *pauseLateralToggle = static_cast<FrogPilotParamValueControl*>(toggles["PauseLateralSpeed"]);

  if (metric) {
    laneWidthToggle->updateControl(0, 5, metricDistanceLabels);

    minimumLaneChangeSpeedToggle->updateControl(0, 150, minimumSpeedLabels, tr(" km/h"));
    pauseAOLOnBrakeToggle->updateControl(0, 150, speedLabels, tr(" km/h"));
    pauseLateralToggle->updateControl(0, 150, speedLabels, tr(" km/h"));
  } else {
    laneWidthToggle->updateControl(0, 15, imperialDistanceLabels);

    minimumLaneChangeSpeedToggle->updateControl(0, 99, minimumSpeedLabels, tr(" mph"));
    pauseAOLOnBrakeToggle->updateControl(0, 99, speedLabels, tr(" mph"));
    pauseLateralToggle->updateControl(0, 99, speedLabels, tr(" mph"));
  }
}

void FrogPilotLateralPanel::updateToggles() {
  bool forcingAutoTuneOff = parent->hasAutoTune && params.getBool("ForceAutoTuneOff");
  bool forcingTorqueController = !parent->isAngleCar && params.getBool("ForceTorqueController");
  bool nudgelessLaneChange = params.getBool("NudgelessLaneChange");
  bool usingNNFF = parent->hasNNFFLog && params.getBool("LateralTune") && params.getBool("NNFF");
  bool forcingAutoTune = !parent->hasAutoTune && !parent->isAngleCar && (parent->isTorqueCar || forcingTorqueController || usingNNFF) && params.getBool("ForceAutoTune");

  QSet<QString> visibleParents;

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      continue;
    }

    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels.value(key).toDouble();

    if (key == "AlwaysOnLateralLKAS") {
      setVisible &= parent->lkasAllowedForAOL;
    }

    else if (key == "ForceAutoTune") {
      setVisible &= !parent->hasAutoTune;
      setVisible &= !parent->isAngleCar;
      setVisible &= parent->isTorqueCar || forcingTorqueController || usingNNFF;
    }

    else if (key == "ForceAutoTuneOff") {
      setVisible &= parent->hasAutoTune;
    }

    else if (key == "ForceTorqueController") {
      setVisible &= !parent->isAngleCar;
      setVisible &= !parent->isTorqueCar || forcingTorqueController;
    }

    else if (key == "LaneChangeTime") {
      setVisible &= nudgelessLaneChange;
    }

    else if (key == "LaneDetectionWidth") {
      setVisible &= nudgelessLaneChange || frogpilotUIState()->frogpilot_scene.frogpilot_toggles.value("conditional_signal_lane_detection").toBool();
    }

    else if (key == "NNFF") {
      setVisible &= parent->hasNNFFLog;
      setVisible &= !parent->isAngleCar;
    }

    else if (key == "NNFFLite") {
      setVisible &= !usingNNFF;
      setVisible &= !parent->isAngleCar;
    }

    else if (key == "SteerDelay") {
      setVisible &= parent->steerActuatorDelay != 0;
    }

    else if (key == "SteerFriction") {
      setVisible &= parent->friction != 0;
      setVisible &= parent->hasAutoTune ? forcingAutoTuneOff : !forcingAutoTune;
      setVisible &= parent->isTorqueCar || forcingTorqueController;
      setVisible &= !usingNNFF;
    }

    else if (key == "SteerKP") {
      setVisible &= parent->steerKp != 0;
      setVisible &= parent->isTorqueCar || forcingTorqueController || usingNNFF;
      setVisible &= !parent->isAngleCar;
    }

    else if (key == "SteerLatAccel") {
      setVisible &= parent->latAccelFactor != 0;
      setVisible &= parent->hasAutoTune ? forcingAutoTuneOff : !forcingAutoTune;
      setVisible &= parent->isTorqueCar || forcingTorqueController;
      setVisible &= !usingNNFF;
    }

    else if (key == "SteerRatio") {
      setVisible &= parent->steerRatio != 0;
      setVisible &= !forcingAutoTune;
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (advancedLateralTuneKeys.contains(key)) {
        visibleParents.insert("AdvancedLateralTune");
      } else if (aolKeys.contains(key)) {
        visibleParents.insert("AlwaysOnLateral");
      } else if (laneChangeKeys.contains(key)) {
        visibleParents.insert("LaneChanges");
      } else if (lateralTuneKeys.contains(key)) {
        visibleParents.insert("LateralTune");
      } else if (qolKeys.contains(key)) {
        visibleParents.insert("QOLLateral");
      }
    }
  }

  for (const QString &key : parentKeys) {
    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels.value(key).toDouble();

    if (key == "LateralTune") {
      setVisible &= !parent->isAngleCar || visibleParents.contains(key);
    }

    FrogPilotManageControl *parentToggle = static_cast<FrogPilotManageControl*>(toggles[key]);
    parentToggle->setVisible(setVisible);
    parentToggle->setManageVisible(visibleParents.contains(key));
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
