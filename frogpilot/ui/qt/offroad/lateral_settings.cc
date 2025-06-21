#include "frogpilot/ui/qt/offroad/lateral_settings.h"

FrogPilotLateralPanel::FrogPilotLateralPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
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
    {"AdvancedLateralTune", tr("Advanced Lateral Tuning"), tr("Advanced settings for fine-tuning openpilot's lateral controls."), "../../frogpilot/assets/toggle_icons/icon_advanced_lateral_tune.png"},
    {"SteerDelay", delayStock != 0 ? QString(tr("Actuator Delay (Default: %1)")).arg(QString::number(delayStock, 'f', 2)) : tr("Actuator Delay"), tr("How long the steering wheel takes to respond to commands. Higher values account for slower steering; lower values for quicker response."), ""},
    {"SteerFriction", frictionStock != 0 ? QString(tr("Friction (Default: %1)")).arg(QString::number(frictionStock, 'f', 2)) : tr("Friction"), tr("Adjust steering resistance. Higher values feel more stable but heavier; lower values feel lighter but more sensitive."), ""},
    {"SteerKP", kpStock != 0 ? QString(tr("Kp Factor (Default: %1)")).arg(QString::number(kpStock, 'f', 2)) : tr("Kp Factor"), tr("How aggressively openpilot corrects steering. Higher values respond faster but may feel jerky; lower values are smoother but slower."), ""},
    {"SteerLatAccel", latAccelStock != 0 ? QString(tr("Lateral Accel (Default: %1)")).arg(QString::number(latAccelStock, 'f', 2)) : tr("Lateral Accel"), tr("How quickly openpilot makes lateral adjustments. Higher values allow sharper turns; lower values provide smoother steering."), ""},
    {"SteerRatio", steerRatioStock != 0 ? QString(tr("Steer Ratio (Default: %1)")).arg(QString::number(steerRatioStock, 'f', 2)) : tr("Steer Ratio"), tr("How much the steering wheel turns in response to commands. Higher values feel more stable; lower values feel quicker."), ""},
    {"ForceAutoTune", tr("Force Auto Tune On"), tr("Force-enable comma’s auto lateral tuning."), ""},
    {"ForceAutoTuneOff", tr("Force Auto Tune Off"), tr("Force-disable comma’s auto lateral tuning."), ""},

    {"AlwaysOnLateral", tr("Always on Lateral"), tr("openpilot's steering control stays active even when the brake or gas pedals are pressed.<br><br>Deactivation only occurs with the <b>Cruise Control</b> button."), "../../frogpilot/assets/toggle_icons/icon_always_on_lateral.png"},
    {"AlwaysOnLateralMain", tr("Enable With Cruise Control"), tr("Allow <b>Always on Lateral</b> to be active whenever <b>Cruise Control</b> is active, bypassing the need to enable openpilot first."), ""},
    {"AlwaysOnLateralLKAS", tr("Enable With LKAS Button"), tr("Allow <b>Always on Lateral</b> to be active whenever <b>LKAS</b> is active, bypassing the need to enable openpilot first."), ""},
    {"PauseAOLOnBrake", tr("Pause on Brake Below"), tr("Temporarily pause <b>Always on Lateral</b> below the set speed when braking."), ""},

    {"LaneChangeCustomizations", tr("Lane Changes"), tr("Customize how openpilot performs lane changes."), "../../frogpilot/assets/toggle_icons/icon_lane.png"},
    {"NudgelessLaneChange", tr("Automatic Lane Changes"), tr("Change lanes automatically when the turn signal is on. No steering input needed!"), ""},
    {"LaneChangeTime", tr("Lane Change Delay"), tr("Delay automatic lane changes by the set amount of time."), ""},
    {"MinimumLaneChangeSpeed", tr("Minimum Lane Change Speed"), tr("Minimum speed required for openpilot to perform a lane change."), ""},
    {"LaneDetectionWidth", tr("Minimum Lane Width"), tr("openpilot won't initiate a lane change into a lane narrower than this width."), ""},
    {"OneLaneChange", tr("One Lane Change Per Signal"), tr("Limit lane changes to one per turn signal activation."), ""},

    {"LateralTune", tr("Lateral Tuning"), tr("Settings for fine-tuning openpilot's lateral controls."), "../../frogpilot/assets/toggle_icons/icon_lateral_tune.png"},
    {"TurnDesires", tr("Force Turn Desires Below Lane Change Speed"), tr("Force turn desires below the minimum lane change speed to improve turning accuracy."), ""},
    {"NNFF", tr("Neural Network Feedforward (NNFF)"), tr("Use <b>Twilsonco’s</b> <b>Neural Network FeedForward</b> model for smoother, model-based steering trained on your vehicle's data."), ""},
    {"NNFFLite", tr("Smooth Curve Handling"), tr("Use <b>Twilsonco’s</b> torque-based adjustments to smooth out steering during curves."), ""},

    {"QOLLateral", tr("Quality of Life"), tr("Miscellaneous features to improve the steering experience."), "../../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"PauseLateralSpeed", tr("Pause Steering Below"), tr("Temporarily pause steering control below the set speed."), ""},

    {"IgnoreMe", "Ignore Me", "This is simply used to fix the layout when the user opens the descriptions and the menu gets wonky. No idea why it happens, but I can't be asked to properly fix it so whatever. Sue me.", ""},
    {"IgnoreMe2", "Ignore Me", "This is simply used to fix the layout when the user opens the descriptions and the menu gets wonky. No idea why it happens, but I can't be asked to properly fix it so whatever. Sue me.", ""}
  };

  for (const auto &[param, title, desc, icon] : lateralToggles) {
    AbstractControl *lateralToggle;

    if (param == "AdvancedLateralTune") {
      FrogPilotManageControl *advancedLateralTuneToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(advancedLateralTuneToggle, &FrogPilotManageControl::manageButtonClicked, [lateralLayout, advancedLateralTunePanel]() {
        lateralLayout->setCurrentWidget(advancedLateralTunePanel);
      });
      lateralToggle = advancedLateralTuneToggle;
    } else if (param == "SteerDelay") {
      std::vector<QString> steerDelayButton{"Reset"};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, delayStock * 0.5, delayStock * 1.5, QString(), std::map<float, QString>(), 0.001, false, {}, steerDelayButton, false, false);
    } else if (param == "SteerFriction") {
      std::vector<QString> steerFrictionButton{"Reset"};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0, 0.5, QString(), std::map<float, QString>(), 0.001, false, {}, steerFrictionButton, false, false);
    } else if (param == "SteerKP") {
      std::vector<QString> steerKPButton{"Reset"};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, kpStock * 0.5, kpStock * 1.5, QString(), std::map<float, QString>(), 0.001, false, {}, steerKPButton, false, false);
    } else if (param == "SteerLatAccel") {
      std::vector<QString> steerLatAccelButton{"Reset"};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, latAccelStock * 0.5, latAccelStock * 1.5, QString(), std::map<float, QString>(), 0.001, false, {}, steerLatAccelButton, false, false);
    } else if (param == "SteerRatio") {
      std::vector<QString> steerRatioButton{"Reset"};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, steerRatioStock * 0.5, steerRatioStock * 1.5, QString(), std::map<float, QString>(), 0.001, false, {}, steerRatioButton, false, false);

    } else if (param == "AlwaysOnLateral") {
      FrogPilotManageControl *aolToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(aolToggle, &FrogPilotManageControl::manageButtonClicked, [lateralLayout, aolPanel]() {
        lateralLayout->setCurrentWidget(aolPanel);
      });
      lateralToggle = aolToggle;
    } else if (param == "PauseAOLOnBrake") {
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, QString(), std::map<float, QString>(), 1, true);

    } else if (param == "LaneChangeCustomizations") {
      FrogPilotManageControl *laneChangeToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(laneChangeToggle, &FrogPilotManageControl::manageButtonClicked, [lateralLayout, laneChangePanel]() {
        lateralLayout->setCurrentWidget(laneChangePanel);
      });
      lateralToggle = laneChangeToggle;
    } else if (param == "LaneChangeTime") {
      std::map<float, QString> laneChangeTimeLabels;
      for (float i = 0; i <= 5; i += 0.1) {
        laneChangeTimeLabels[i] = i == 0 ? tr("Instant") : std::lround(i / 0.1) == 1 / 0.1 ? QString::number(i, 'f', 1) + tr(" second") : QString::number(i, 'f', 1) + tr(" seconds");
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
      std::vector<QString> pauseLateralToggleNames{"Turn Signal Only"};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0, 99, QString(), std::map<float, QString>(), 1, true, pauseLateralToggles, pauseLateralToggleNames, true);

    } else {
      lateralToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = lateralToggle;

    if (advancedLateralTuneKeys.find(param) != advancedLateralTuneKeys.end()) {
      advancedLateralTuneList->addItem(lateralToggle);
    } else if (aolKeys.find(param) != aolKeys.end()) {
      aolList->addItem(lateralToggle);
    } else if (laneChangeKeys.find(param) != laneChangeKeys.end()) {
      laneChangeList->addItem(lateralToggle);
    } else if (lateralTuneKeys.find(param) != lateralTuneKeys.end()) {
      lateralTuneList->addItem(lateralToggle);
    } else if (qolKeys.find(param) != qolKeys.end()) {
      qolList ->addItem(lateralToggle);
    } else {
      lateralList->addItem(lateralToggle);

      parentKeys.insert(param);
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(lateralToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, this, &FrogPilotLateralPanel::openSubPanel);
    }

    QObject::connect(lateralToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  std::set<QString> forceUpdateKeys = {"ForceAutoTune", "ForceAutoTuneOff", "LateralTune", "NNFF", "NudgelessLaneChange"};
  for (const QString &key : forceUpdateKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, this, &FrogPilotLateralPanel::updateToggles);
  }

  std::set<QString> rebootKeys = {"AlwaysOnLateral", "NNFF", "NNFFLite"};
  for (const QString &key : rebootKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, [this, key](bool state) {
      if (started) {
        if (key == "AlwaysOnLateral" && state) {
          if (FrogPilotConfirmationDialog::toggleReboot(this)) {
            Hardware::reboot();
          }
        } else if (key != "AlwaysOnLateral") {
          if (FrogPilotConfirmationDialog::toggleReboot(this)) {
            Hardware::reboot();
          }
        }
      }
    });
  }

  steerDelayToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerDelay"]);
  QObject::connect(steerDelayToggle, &FrogPilotParamValueButtonControl::buttonClicked, [this]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your <b>Actuator Delay</b>?"), this)) {
      params.putFloat("SteerDelay", delayStock);
      steerDelayToggle->refresh();
    }
  });

  steerFrictionToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerFriction"]);
  QObject::connect(steerFrictionToggle, &FrogPilotParamValueButtonControl::buttonClicked, [this]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your <b>Friction</b>?"), this)) {
      params.putFloat("SteerFriction", frictionStock);
      steerFrictionToggle->refresh();
    }
  });

  steerKPToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerKP"]);
  QObject::connect(steerKPToggle, &FrogPilotParamValueButtonControl::buttonClicked, [this]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your <b>Kp Factor</b>?"), this)) {
      params.putFloat("SteerKP", kpStock);
      steerKPToggle->refresh();
    }
  });

  steerLatAccelToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerLatAccel"]);
  QObject::connect(steerLatAccelToggle, &FrogPilotParamValueButtonControl::buttonClicked, [this]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your <b>Lateral Accel</b>?"), this)) {
      params.putFloat("SteerLatAccel", latAccelStock);
      steerLatAccelToggle->refresh();
    }
  });

  steerRatioToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerRatio"]);
  QObject::connect(steerRatioToggle, &FrogPilotParamValueButtonControl::buttonClicked, [this]() {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your <b>Steer Ratio</b>?"), this)) {
      params.putFloat("SteerRatio", steerRatioStock);
      steerRatioToggle->refresh();
    }
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [lateralLayout, lateralPanel] {lateralLayout->setCurrentWidget(lateralPanel);});
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, this, &FrogPilotLateralPanel::updateMetric);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotLateralPanel::updateState);
}

void FrogPilotLateralPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  delayStock = parent->delayStock;
  hasAutoTune = parent->hasAutoTune;
  hasNNFFLog = parent->hasNNFFLog;
  hasOpenpilotLongitudinal = parent->hasOpenpilotLongitudinal;
  isHKGCanFd = parent->isHKGCanFd;
  isHonda = parent->isHonda;
  isTorqueCar = parent->isTorqueCar;
  frictionStock = parent->frictionStock;
  kpStock = parent->kpStock;
  latAccelStock = parent->latAccelStock;
  steerRatioStock = parent->steerRatioStock;
  tuningLevel = parent->tuningLevel;

  steerDelayToggle->setTitle(QString(tr("Actuator Delay (Default: %1)")).arg(QString::number(delayStock, 'f', 2)));
  steerDelayToggle->updateControl(delayStock * 0.5, delayStock * 1.5);
  steerFrictionToggle->setTitle(QString(tr("Friction (Default: %1)")).arg(QString::number(frictionStock, 'f', 2)));
  steerKPToggle->setTitle(QString(tr("Kp Factor (Default: %1)")).arg(QString::number(kpStock, 'f', 2)));
  steerKPToggle->updateControl(kpStock * 0.5, kpStock * 1.5);
  steerLatAccelToggle->setTitle(QString(tr("Lateral Accel (Default: %1)")).arg(QString::number(latAccelStock, 'f', 2)));
  steerLatAccelToggle->updateControl(latAccelStock * 0.5, latAccelStock * 1.5);
  steerRatioToggle->setTitle(QString(tr("Steer Ratio (Default: %1)")).arg(QString::number(steerRatioStock, 'f', 2)));
  steerRatioToggle->updateControl(steerRatioStock * 0.5, steerRatioStock * 1.5);

  updateToggles();
}

void FrogPilotLateralPanel::updateState(const UIState &s) {
  if (!isVisible()) return;

  started = s.scene.started;
}

void FrogPilotLateralPanel::updateMetric(bool metric, bool bootRun) {
  static bool previousMetric;
  if (metric != previousMetric && !bootRun) {
    double distanceConversion = metric ? FOOT_TO_METER : METER_TO_FOOT;
    double speedConversion = metric ? MILE_TO_KM : KM_TO_MILE;

    params.putFloatNonBlocking("LaneDetectionWidth", params.getFloat("LaneDetectionWidth") * distanceConversion);

    params.putIntNonBlocking("MinimumLaneChangeSpeed", params.getInt("MinimumLaneChangeSpeed") * speedConversion);
    params.putIntNonBlocking("PauseAOLOnBrake", params.getInt("PauseAOLOnBrake") * speedConversion);
    params.putIntNonBlocking("PauseLateralSpeed", params.getInt("PauseLateralSpeed") * speedConversion);
  }
  previousMetric = metric;

  static std::map<float, QString> imperialDistanceLabels;
  static std::map<float, QString> imperialSpeedLabels;
  static std::map<float, QString> metricDistanceLabels;
  static std::map<float, QString> metricSpeedLabels;

  static bool labelsInitialized = false;
  if (!labelsInitialized) {
    for (int i = 0; i <= 150; ++i) {
      float key = i / 10.0f;
      imperialDistanceLabels[key] = key == 0 ? tr("Off") : i == 1 ? QString::number(i) + tr(" foot") : QString::number(key, 'f', 1) + tr(" feet");
    }

    for (int i = 0; i <= 99; ++i) {
      imperialSpeedLabels[i] = i == 0 ? tr("Off") : QString::number(i) + tr(" mph");
    }

    for (int i = 0; i <= 50; ++i) {
      float key = i / 10.0f;
      metricDistanceLabels[key] = key == 0 ? tr("Off") : i == 1 ? QString::number(i) + tr(" meter") : QString::number(key, 'f', 1) + tr(" meters");
    }

    for (int i = 0; i <= 150; ++i) {
      metricSpeedLabels[i] = i == 0 ? tr("Off") : QString::number(i) + tr(" km/h");
    }

    labelsInitialized = true;
  }

  FrogPilotParamValueControl *laneWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["LaneDetectionWidth"]);
  FrogPilotParamValueControl *minimumLaneChangeSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["MinimumLaneChangeSpeed"]);
  FrogPilotParamValueControl *pauseAOLOnBrakeToggle = static_cast<FrogPilotParamValueControl*>(toggles["PauseAOLOnBrake"]);
  FrogPilotParamValueControl *pauseLateralToggle = static_cast<FrogPilotParamValueControl*>(toggles["PauseLateralSpeed"]);

  if (metric) {
    laneWidthToggle->updateControl(0, 5, metricDistanceLabels);

    minimumLaneChangeSpeedToggle->updateControl(0, 150, metricSpeedLabels);
    pauseAOLOnBrakeToggle->updateControl(0, 150, metricSpeedLabels);
    pauseLateralToggle->updateControl(0, 150, metricSpeedLabels);
  } else {
    laneWidthToggle->updateControl(0, 15, imperialDistanceLabels);

    minimumLaneChangeSpeedToggle->updateControl(0, 99, imperialSpeedLabels);
    pauseAOLOnBrakeToggle->updateControl(0, 99, imperialSpeedLabels);
    pauseLateralToggle->updateControl(0, 99, imperialSpeedLabels);
  }
}

void FrogPilotLateralPanel::updateToggles() {
  for (auto &[key, toggle] : toggles) {
    if (parentKeys.find(key) != parentKeys.end()) {
      toggle->setVisible(false);
    }
  }

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.find(key) != parentKeys.end()) {
      continue;
    }

    bool forcingAutoTune = !hasAutoTune && params.getBool("ForceAutoTune");
    bool forcingAutoTuneOff = hasAutoTune && params.getBool("ForceAutoTuneOff");
    bool usingNNFF = hasNNFFLog && params.getBool("LateralTune") && params.getBool("NNFF");

    bool setVisible = tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (key == "AlwaysOnLateralLKAS") {
      setVisible &= isHKGCanFd;
      setVisible &= !hasOpenpilotLongitudinal;
    }

    if (key == "AlwaysOnLateralMain") {
      setVisible &= !isHKGCanFd;
      setVisible |= hasOpenpilotLongitudinal;
    }

    if (key == "ForceAutoTune") {
      setVisible &= !hasAutoTune;
      setVisible &= isTorqueCar;
    }

    if (key == "ForceAutoTuneOff") {
      setVisible &= hasAutoTune;
    }

    if (key == "LaneChangeTime") {
      setVisible &= params.getBool("LaneChangeCustomizations") && params.getBool("NudgelessLaneChange");
    }

    if (key == "NNFF") {
      setVisible &= hasNNFFLog;
      setVisible &= !isHonda;
    }

    if (key == "NNFFLite") {
      setVisible &= !usingNNFF;
      setVisible &= !isHonda;
    }

    if (key == "SteerDelay") {
      setVisible &= params.getFloat("SteerDelayStock") != 0;
    }

    if (key == "SteerFriction") {
      setVisible &= params.getFloat("SteerFrictionStock") != 0;
      setVisible &= hasAutoTune ? forcingAutoTuneOff : !forcingAutoTune;
      setVisible &= isTorqueCar;
      setVisible &= !usingNNFF;
    }

    if (key == "SteerKP") {
      setVisible &= params.getFloat("SteerKPStock") != 0;
      setVisible &= hasAutoTune ? forcingAutoTuneOff : !forcingAutoTune;
      setVisible &= isTorqueCar;
    }

    if (key == "SteerLatAccel") {
      setVisible &= params.getFloat("SteerLatAccelStock") != 0;
      setVisible &= hasAutoTune ? forcingAutoTuneOff : !forcingAutoTune;
      setVisible &= isTorqueCar;
      setVisible &= !usingNNFF;
    }

    if (key == "SteerRatio") {
      setVisible &= params.getFloat("SteerRatioStock") != 0;
      setVisible &= hasAutoTune ? forcingAutoTuneOff : !forcingAutoTune;
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (advancedLateralTuneKeys.find(key) != advancedLateralTuneKeys.end()) {
        toggles["AdvancedLateralTune"]->setVisible(true);
      } else if (aolKeys.find(key) != aolKeys.end()) {
        toggles["AlwaysOnLateral"]->setVisible(true);
      } else if (laneChangeKeys.find(key) != laneChangeKeys.end()) {
        toggles["LaneChangeCustomizations"]->setVisible(true);
      } else if (lateralTuneKeys.find(key) != lateralTuneKeys.end()) {
        toggles["LateralTune"]->setVisible(true);
      } else if (qolKeys.find(key) != qolKeys.end()) {
        toggles["QOLLateral"]->setVisible(true);
      }
    }
  }

  update();
}
