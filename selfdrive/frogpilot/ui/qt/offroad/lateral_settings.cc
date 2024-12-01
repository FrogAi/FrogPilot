#include "selfdrive/frogpilot/ui/qt/offroad/lateral_settings.h"

FrogPilotLateralPanel::FrogPilotLateralPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> lateralToggles {
    {"AdvancedLateralTune", tr("Advanced Lateral Tuning"), tr("Advanced settings for fine tuning openpilot's lateral controls."), "../frogpilot/assets/toggle_icons/icon_advanced_lateral_tune.png"},
    {"SteerFriction", steerFrictionStock != 0 ? QString(tr("Friction (Default: %1)")).arg(QString::number(steerFrictionStock, 'f', 2)) : tr("Friction"), tr("Adjusts the resistance in steering. Higher values provide more stable steering but can make it feel heavy, while lower values allow lighter steering but may feel too sensitive."), ""},
    {"SteerKP", steerKPStock != 0 ? QString(tr("Kp Factor (Default: %1)")).arg(QString::number(steerKPStock, 'f', 2)) : tr("Kp Factor"), tr("Adjusts how aggressively the car corrects its steering. Higher values offer quicker corrections but may feel jerky, while lower values make steering smoother but slower to respond."), ""},
    {"SteerLatAccel", steerLatAccelStock != 0 ? QString(tr("Lateral Accel (Default: %1)")).arg(QString::number(steerLatAccelStock, 'f', 2)) : tr("Lateral Accel"), tr("Adjusts how fast the car can steer from side to side. Higher values allow quicker lane changes but can feel unstable, while lower values provide smoother steering but may feel sluggish."), ""},
    {"SteerRatio", steerRatioStock != 0 ? QString(tr("Steer Ratio (Default: %1)")).arg(QString::number(steerRatioStock, 'f', 2)) : tr("Steer Ratio"), tr("Adjusts how much openpilot needs to turn the wheel to steer. Higher values feel like driving a truck, more stable at high speeds, but harder to steer quickly at low speeds, while lower values feel like a go-kart, easier to steer in tight spots but more sensitive and less stable at high speeds."), ""},
    {"ForceAutoTune", tr("Force Auto Tune On"), tr("Forces comma's auto lateral tuning for unsupported vehicles."), ""},
    {"ForceAutoTuneOff", tr("Force Auto Tune Off"), tr("Forces comma's auto lateral tuning off for supported vehicles."), ""},

    {"AlwaysOnLateral", tr("Always on Lateral"), tr("openpilot's steering control stays active even when the brake or gas pedals are pressed.\n\nDeactivate only occurs with the 'Cruise Control' button."), "../frogpilot/assets/toggle_icons/icon_always_on_lateral.png"},
    {"AlwaysOnLateralLKAS", tr("Control with LKAS Button"), tr("Controls the current state of 'Always on Lateral' with the 'LKAS' button."), ""},
    {"AlwaysOnLateralMain", tr("Enable with Cruise Control"), tr("Activates 'Always on Lateral' whenever 'Cruise Control' is active bypassing the requirement to enable openpilot first."), ""},
    {"PauseAOLOnBrake", tr("Pause on Brake Below"), tr("Pauses 'Always on Lateral' when the brake pedal is pressed below the set speed."), ""},
    {"HideAOLStatusBar", tr("Hide the Status Bar"), tr("Hides status bar for 'Always On Lateral'."), ""},

    {"LaneChangeCustomizations", tr("Lane Change Settings"), tr("How openpilot handles lane changes."), "../frogpilot/assets/toggle_icons/icon_lane.png"},
    {"NudgelessLaneChange", tr("Automatic Lane Changes"), tr("Conducts lane changes without needing to touch the steering wheel upon turn signal activation."), ""},
    {"LaneChangeTime", tr("Lane Change Delay"), tr("Delays lane changes by the set time to prevent sudden changes."), ""},
    {"LaneDetectionWidth", tr("Lane Width Requirement"), tr("Sets the minimum lane width for openpilot to detect a lane as a lane."), ""},
    {"MinimumLaneChangeSpeed", tr("Minimum Speed for Lane Change"), tr("Sets the minimum speed required for openpilot to perform a lane change."), ""},
    {"OneLaneChange", tr("Only One Lane Change Per Signal"), tr("Limits lane changes to one per turn signal activation."), ""},

    {"LateralTune", tr("Lateral Tuning"), tr("Settings for fine tuning openpilot's lateral controls."), "../frogpilot/assets/toggle_icons/icon_lateral_tune.png"},
    {"TurnDesires", tr("Force Turn Desires Below Lane Change Speed"), tr("Forces the model to use turn desires when driving below the minimum lane change speed to help make left and right turns more precisely."), ""},
    {"NNFF", tr("Neural Network Feedforward (NNFF)"), tr("Uses Twilsonco's 'Neural Network FeedForward' for more precise steering control."), ""},
    {"NNFFLite", tr("Smooth Curve Handling"), tr("Smoothens the steering control when entering and exiting curves by using Twilsonco's torque adjustments."), ""},

    {"QOLLateral", tr("Quality of Life Improvements"), tr("Miscellaneous lateral focused features to improve your overall openpilot experience."), "../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"PauseLateralSpeed", tr("Pause Steering Below"), tr("Pauses steering control when driving below the set speed."), ""}
  };

  for (const auto &[param, title, desc, icon] : lateralToggles) {
    AbstractControl *lateralToggle;

    if (param == "AdvancedLateralTune") {
      FrogPilotParamManageControl *advancedLateralTuneToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(advancedLateralTuneToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedAdvancedLateralTuneKeys = advancedLateralTuneKeys;

        bool forcingAutoTune = params.getBool("ForceAutoTune");
        bool forcingAutoTuneOff = params.getBool("ForceAutoTuneOff");
        if (!hasAutoTune && forcingAutoTune || hasAutoTune && !forcingAutoTuneOff) {
          modifiedAdvancedLateralTuneKeys.erase("SteerFriction");
          modifiedAdvancedLateralTuneKeys.erase("SteerKP");
          modifiedAdvancedLateralTuneKeys.erase("SteerLatAccel");
          modifiedAdvancedLateralTuneKeys.erase("SteerRatio");
        }

        if (hasAutoTune) {
          modifiedAdvancedLateralTuneKeys.erase("ForceAutoTune");
        } else if (isPIDCar) {
          modifiedAdvancedLateralTuneKeys.erase("ForceAutoTuneOff");
          modifiedAdvancedLateralTuneKeys.erase("SteerFriction");
          modifiedAdvancedLateralTuneKeys.erase("SteerKP");
          modifiedAdvancedLateralTuneKeys.erase("SteerLatAccel");
        } else {
          modifiedAdvancedLateralTuneKeys.erase("ForceAutoTuneOff");
        }

        bool usingNNFF = hasNNFFLog && params.getBool("LateralTune") && params.getBool("NNFF");
        if (!liveValid || usingNNFF) {
          modifiedAdvancedLateralTuneKeys.erase("SteerFriction");
          modifiedAdvancedLateralTuneKeys.erase("SteerLatAccel");
        }

        showToggles(modifiedAdvancedLateralTuneKeys);
      });
      lateralToggle = advancedLateralTuneToggle;
    } else if (param == "SteerFriction") {
      std::vector<QString> steerFrictionButton{"Reset"};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0.01, 0.25, QString(), std::map<int, QString>(), 0.01, {}, steerFrictionButton, false, false);
    } else if (param == "SteerKP") {
      std::vector<QString> steerKPButton{"Reset"};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, steerKPStock * 0.50, steerKPStock * 1.50, QString(), std::map<int, QString>(), 0.01, {}, steerKPButton, false, false);
    } else if (param == "SteerLatAccel") {
      std::vector<QString> steerLatAccelButton{"Reset"};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, steerLatAccelStock * 0.25, steerLatAccelStock * 1.25, QString(), std::map<int, QString>(), 0.01, {}, steerLatAccelButton, false, false);
    } else if (param == "SteerRatio") {
      std::vector<QString> steerRatioButton{"Reset"};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, steerRatioStock * 0.75, steerRatioStock * 1.25, QString(), std::map<int, QString>(), 0.01, {}, steerRatioButton, false, false);

    } else if (param == "AlwaysOnLateral") {
      FrogPilotParamManageControl *aolToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(aolToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedAOLKeys = aolKeys;

        if (isSubaru || (params.getBool("ExperimentalModeActivation") && params.getBool("ExperimentalModeViaLKAS"))) {
          modifiedAOLKeys.erase("AlwaysOnLateralLKAS");
        }

        showToggles(modifiedAOLKeys);
      });
      lateralToggle = aolToggle;
    } else if (param == "PauseAOLOnBrake") {
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr("mph"));

    } else if (param == "LaneChangeCustomizations") {
      FrogPilotParamManageControl *laneChangeToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(laneChangeToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        showToggles(laneChangeKeys);
      });
      lateralToggle = laneChangeToggle;
    } else if (param == "LaneChangeTime") {
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 5, tr(" seconds"), {{0, "Instant"}, {10, "1.0 second"}}, 0.1);
    } else if (param == "LaneDetectionWidth") {
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 15, tr(" feet"), std::map<int, QString>(), 0.1);
    } else if (param == "MinimumLaneChangeSpeed") {
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr("mph"));

    } else if (param == "LateralTune") {
      FrogPilotParamManageControl *lateralTuneToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(lateralTuneToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedLateralTuneKeys = lateralTuneKeys;

        bool usingNNFF = hasNNFFLog && params.getBool("LateralTune") && params.getBool("NNFF");
        if (!hasNNFFLog) {
          modifiedLateralTuneKeys.erase("NNFF");
        } else if (usingNNFF) {
          modifiedLateralTuneKeys.erase("NNFFLite");
        }

        showToggles(modifiedLateralTuneKeys);
      });
      lateralToggle = lateralTuneToggle;

    } else if (param == "QOLLateral") {
      FrogPilotParamManageControl *qolLateralToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(qolLateralToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        showToggles(qolKeys);
      });
      lateralToggle = qolLateralToggle;
    } else if (param == "PauseLateralSpeed") {
      std::vector<QString> pauseLateralToggles{"PauseLateralOnSignal"};
      std::vector<QString> pauseLateralToggleNames{"Turn Signal Only"};
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0, 99, tr("mph"), std::map<int, QString>(), 1, pauseLateralToggles, pauseLateralToggleNames, true);
    } else if (param == "PauseLateralOnSignal") {
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr("mph"));

    } else {
      lateralToggle = new ParamControl(param, title, desc, icon);
    }

    addItem(lateralToggle);
    toggles[param] = lateralToggle;

    if (FrogPilotParamManageControl *frogPilotManageToggle = qobject_cast<FrogPilotParamManageControl*>(lateralToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotParamManageControl::manageButtonClicked, this, &FrogPilotLateralPanel::openParentToggle);
    }

    QObject::connect(lateralToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  QObject::connect(static_cast<ToggleControl*>(toggles["AlwaysOnLateralLKAS"]), &ToggleControl::toggleFlipped, [this](bool state) {
    if (state && params.getBool("ExperimentalModeViaLKAS")) {
      params.putBoolNonBlocking("ExperimentalModeViaLKAS", false);
    }
  });

  QObject::connect(static_cast<ToggleControl*>(toggles["ForceAutoTune"]), &ToggleControl::toggleFlipped, [this](bool state) {
    std::set<QString> modifiedAdvancedLateralTuneKeys = advancedLateralTuneKeys;

    modifiedAdvancedLateralTuneKeys.erase("ForceAutoTuneOff");

    if (!hasAutoTune && state) {
      modifiedAdvancedLateralTuneKeys.erase("SteerFriction");
      modifiedAdvancedLateralTuneKeys.erase("SteerKP");
      modifiedAdvancedLateralTuneKeys.erase("SteerLatAccel");
      modifiedAdvancedLateralTuneKeys.erase("SteerRatio");
    }

    showToggles(modifiedAdvancedLateralTuneKeys);
  });

  QObject::connect(static_cast<ToggleControl*>(toggles["ForceAutoTuneOff"]), &ToggleControl::toggleFlipped, [this](bool state) {
    std::set<QString> modifiedAdvancedLateralTuneKeys = advancedLateralTuneKeys;

    modifiedAdvancedLateralTuneKeys.erase("ForceAutoTune");

    if (hasAutoTune && !state) {
      modifiedAdvancedLateralTuneKeys.erase("SteerFriction");
      modifiedAdvancedLateralTuneKeys.erase("SteerKP");
      modifiedAdvancedLateralTuneKeys.erase("SteerLatAccel");
      modifiedAdvancedLateralTuneKeys.erase("SteerRatio");
    }

    showToggles(modifiedAdvancedLateralTuneKeys);
  });

  QObject::connect(static_cast<ToggleControl*>(toggles["NNFF"]), &ToggleControl::toggleFlipped, [this](bool state) {
    std::set<QString> modifiedLateralTuneKeys = lateralTuneKeys;

    bool usingNNFF = hasNNFFLog && state;
    if (!hasNNFFLog) {
      modifiedLateralTuneKeys.erase("NNFF");
    } else if (usingNNFF) {
      modifiedLateralTuneKeys.erase("NNFFLite");
    }

    showToggles(modifiedLateralTuneKeys);
  });

  std::set<QString> rebootKeys = {"AlwaysOnLateral", "NNFF", "NNFFLite"};
  for (const QString &key : rebootKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key.toStdString().c_str()]), &ToggleControl::toggleFlipped, [this, key](bool state) {
      if (started) {
        if (key == "AlwaysOnLateral" && state) {
          if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
            Hardware::reboot();
          }
        } else if (key != "AlwaysOnLateral") {
          if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
            Hardware::reboot();
          }
        }
      }
    });
  }

  steerFrictionToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerFriction"]);
  QObject::connect(steerFrictionToggle, &FrogPilotParamValueButtonControl::buttonClicked, [this]() {
    params.putFloat("SteerFriction", steerFrictionStock);
    steerFrictionToggle->refresh();
  });

  steerKPToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerKP"]);
  QObject::connect(steerKPToggle, &FrogPilotParamValueButtonControl::buttonClicked, [this]() {
    params.putFloat("SteerKP", steerKPStock);
    steerKPToggle->refresh();
  });

  steerLatAccelToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerLatAccel"]);
  QObject::connect(steerLatAccelToggle, &FrogPilotParamValueButtonControl::buttonClicked, [this]() {
    params.putFloat("SteerLatAccel", steerLatAccelStock);
    steerLatAccelToggle->refresh();
  });

  steerRatioToggle = static_cast<FrogPilotParamValueButtonControl*>(toggles["SteerRatio"]);
  QObject::connect(steerRatioToggle, &FrogPilotParamValueButtonControl::buttonClicked, [this]() {
    params.putFloat("SteerRatio", steerRatioStock);
    steerRatioToggle->refresh();
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, this, &FrogPilotLateralPanel::hideToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, this, &FrogPilotLateralPanel::updateMetric);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotLateralPanel::updateState);

  updateMetric();
}

void FrogPilotLateralPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  hasAutoTune = parent->hasAutoTune;
  hasNNFFLog = parent->hasNNFFLog;
  isPIDCar = parent->isPIDCar;
  isSubaru = parent->isSubaru;
  liveValid = parent->liveValid;
  steerFrictionStock = parent->steerFrictionStock;
  steerKPStock = parent->steerKPStock;
  steerLatAccelStock = parent->steerLatAccelStock;
  steerRatioStock = parent->steerRatioStock;
  tuningLevel = parent->tuningLevel;

  steerFrictionToggle->setTitle(QString(tr("Friction (Default: %1)")).arg(QString::number(steerFrictionStock, 'f', 2)));
  steerKPToggle->setTitle(QString(tr("Kp Factor (Default: %1)")).arg(QString::number(steerKPStock, 'f', 2)));
  steerKPToggle->updateControl(steerKPStock * 0.50, steerKPStock * 1.50);
  steerLatAccelToggle->setTitle(QString(tr("Lateral Accel (Default: %1)")).arg(QString::number(steerLatAccelStock, 'f', 2)));
  steerLatAccelToggle->updateControl(steerLatAccelStock * 0.75, steerLatAccelStock * 1.25);
  steerRatioToggle->setTitle(QString(tr("Steer Ratio (Default: %1)")).arg(QString::number(steerRatioStock, 'f', 2)));
  steerRatioToggle->updateControl(steerRatioStock * 0.75, steerRatioStock * 1.25);

  hideToggles();
}

void FrogPilotLateralPanel::updateState(const UIState &s) {
  if (!isVisible()) return;

  started = s.scene.started;
}

void FrogPilotLateralPanel::updateMetric() {
  bool previousIsMetric = isMetric;
  isMetric = params.getBool("IsMetric");

  if (isMetric != previousIsMetric) {
    double distanceConversion = isMetric ? FOOT_TO_METER : METER_TO_FOOT;
    double speedConversion = isMetric ? MILE_TO_KM : KM_TO_MILE;

    params.putFloatNonBlocking("LaneDetectionWidth", params.getFloat("LaneDetectionWidth") * distanceConversion);

    params.putFloatNonBlocking("MinimumLaneChangeSpeed", params.getFloat("MinimumLaneChangeSpeed") * speedConversion);
    params.putFloatNonBlocking("PauseAOLOnBrake", params.getFloat("PauseAOLOnBrake") * speedConversion);
    params.putFloatNonBlocking("PauseLateralOnSignal", params.getFloat("PauseLateralOnSignal") * speedConversion);
    params.putFloatNonBlocking("PauseLateralSpeed", params.getFloat("PauseLateralSpeed") * speedConversion);
  }

  FrogPilotParamValueControl *laneWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["LaneDetectionWidth"]);
  FrogPilotParamValueControl *minimumLaneChangeSpeedToggle = static_cast<FrogPilotParamValueControl*>(toggles["MinimumLaneChangeSpeed"]);
  FrogPilotParamValueControl *pauseAOLOnBrakeToggle = static_cast<FrogPilotParamValueControl*>(toggles["PauseAOLOnBrake"]);
  FrogPilotParamValueControl *pauseLateralToggle = static_cast<FrogPilotParamValueControl*>(toggles["PauseLateralSpeed"]);

  if (isMetric) {
    minimumLaneChangeSpeedToggle->updateControl(0, 150, tr("kph"));
    pauseAOLOnBrakeToggle->updateControl(0, 99, tr("kph"));
    pauseLateralToggle->updateControl(0, 99, tr("kph"));

    laneWidthToggle->updateControl(0, 5, tr(" meters"));
  } else {
    minimumLaneChangeSpeedToggle->updateControl(0, 99, tr("mph"));
    pauseAOLOnBrakeToggle->updateControl(0, 99, tr("mph"));
    pauseLateralToggle->updateControl(0, 99, tr("mph"));

    laneWidthToggle->updateControl(0, 15, tr(" feet"));
  }
}

void FrogPilotLateralPanel::showToggles(const std::set<QString> &keys) {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(keys.find(key) != keys.end() && tuningLevel >= frogpilotToggleLevels[key].toDouble());
  }

  static_cast<FrogPilotParamManageControl*>(toggles["AlwaysOnLateral"])->setVisibleButton(tuningLevel >= 1);

  setUpdatesEnabled(true);
  update();
}

void FrogPilotLateralPanel::hideToggles() {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    bool subToggles = advancedLateralTuneKeys.find(key) != advancedLateralTuneKeys.end() ||
                      aolKeys.find(key) != aolKeys.end() ||
                      laneChangeKeys.find(key) != laneChangeKeys.end() ||
                      lateralTuneKeys.find(key) != lateralTuneKeys.end() ||
                      qolKeys.find(key) != qolKeys.end();

    toggle->setVisible(!subToggles && tuningLevel >= frogpilotToggleLevels[key].toDouble());
  }

  static_cast<FrogPilotParamManageControl*>(toggles["AlwaysOnLateral"])->setVisibleButton(tuningLevel >= 1);

  setUpdatesEnabled(true);
  update();
}
