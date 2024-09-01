#include "selfdrive/frogpilot/ui/qt/offroad/lateral_settings.h"

FrogPilotLateralPanel::FrogPilotLateralPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> lateralToggles {
    {"AlwaysOnLateral", tr("Always on Lateral"), tr("openpilot's steering control stays active even when the brake or gas pedals are pressed.\n\nDeactivate only occurs with the 'Cruise Control' button."), "../frogpilot/assets/toggle_icons/icon_always_on_lateral.png"},
    {"AlwaysOnLateralLKAS", tr("Control with LKAS Button"), tr("'Always on Lateral' gets turned on or off using the 'LKAS' button."), ""},
    {"AlwaysOnLateralMain", tr("Enable with Cruise Control"), tr("'Always on Lateral' gets turned on by pressing the 'Cruise Control' button bypassing the requirement to enable openpilot first."), ""},
    {"PauseAOLOnBrake", tr("Pause on Brake Below"), tr("'Always on Lateral' pauses when the brake pedal is pressed below the set speed."), ""},
    {"HideAOLStatusBar", tr("Hide the Status Bar"), tr("The status bar for 'Always on Lateral' is hidden."), ""},

    {"LaneChangeCustomizations", tr("Lane Change Settings"), tr("How openpilot handles lane changes."), "../frogpilot/assets/toggle_icons/icon_lane.png"},
    {"NudgelessLaneChange", tr("Hands-Free Lane Change"), tr("Lane changes are conducted without needing to touch the steering wheel upon turn signal activation."), ""},
    {"LaneChangeTime", tr("Lane Change Delay"), tr("How long openpilot waits before changing lanes."), ""},
    {"LaneDetectionWidth", tr("Lane Width Requirement"), tr("The minimum lane width for openpilot to detect a lane as a lane."), ""},
    {"MinimumLaneChangeSpeed", tr("Minimum Speed for Lane Change"), tr("The minimum speed required for openpilot to perform a lane change."), ""},
    {"OneLaneChange", tr("Single Lane Change Per Signal"), tr("Lane changes are limited to one per turn signal activation."), ""},

    {"LateralTune", tr("Lateral Tuning"), tr("Settings that control how openpilot manages steering."), "../frogpilot/assets/toggle_icons/icon_lateral_tune.png"},
    {"NNFF", tr("Neural Network Feedforward (NNFF)"), tr("Twilsonco's 'Neural Network FeedForward' for more precise steering control."), ""},
    {"NNFFLite", tr("Smooth Curve Handling"), tr("Smoother steering when entering and exiting curves with Twilsonco's torque adjustments."), ""},

    {"QOLLateral", tr("Quality of Life Improvements"), tr("Miscellaneous lateral focused features to improve your overall openpilot experience."), "../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"PauseLateralSpeed", tr("Pause Steering Below"), tr("Pauses steering control when driving below the set speed."), ""}
  };

  for (const auto &[param, title, desc, icon] : lateralToggles) {
    AbstractControl *lateralToggle;

    if (param == "AlwaysOnLateral") {
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
      lateralToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0, 99, tr("mph"), std::map<int, QString>(), 1, pauseLateralToggles, pauseLateralToggleNames);
    } else if (param == "PauseLateralOnSignal") {
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr("mph"));

    } else if (param == "LaneChangeCustomizations") {
      FrogPilotParamManageControl *laneChangeToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(laneChangeToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        showToggles(laneChangeKeys);
      });
      lateralToggle = laneChangeToggle;
    } else if (param == "LaneChangeTime") {
      std::map<int, QString> laneChangeTimeLabels;
      for (int i = 0; i <= 10; ++i) {
        laneChangeTimeLabels[i] = i == 0 ? "Instant" : QString::number(i / 2.0) + " seconds";
      }
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 10, QString(), laneChangeTimeLabels);
    } else if (param == "LaneDetectionWidth") {
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, tr(" feet"), std::map<int, QString>(), 0.1);
    } else if (param == "MinimumLaneChangeSpeed") {
      lateralToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 99, tr("mph"));

    } else {
      lateralToggle = new ParamControl(param, title, desc, icon);
    }

    addItem(lateralToggle);
    toggles[param] = lateralToggle;

    makeConnections(lateralToggle);

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

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, this, &FrogPilotLateralPanel::hideToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::updateCarToggles, this, &FrogPilotLateralPanel::updateCarToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, this, &FrogPilotLateralPanel::updateMetric);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotLateralPanel::updateState);

  updateMetric();
}

void FrogPilotLateralPanel::updateCarToggles() {
  hasAutoTune = parent->hasAutoTune;
  hasNNFFLog = parent->hasNNFFLog;
  isSubaru = parent->isSubaru;

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

    laneWidthToggle->updateControl(0, 30, tr(" meters"));
  } else {
    minimumLaneChangeSpeedToggle->updateControl(0, 99, tr("mph"));
    pauseAOLOnBrakeToggle->updateControl(0, 99, tr("mph"));
    pauseLateralToggle->updateControl(0, 99, tr("mph"));

    laneWidthToggle->updateControl(0, 100, tr(" feet"));
  }
}

void FrogPilotLateralPanel::showToggles(const std::set<QString> &keys) {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(keys.find(key) != keys.end());
  }

  setUpdatesEnabled(true);
  update();
}

void FrogPilotLateralPanel::hideToggles() {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    bool subToggles = aolKeys.find(key) != aolKeys.end() ||
                      laneChangeKeys.find(key) != laneChangeKeys.end() ||
                      lateralTuneKeys.find(key) != lateralTuneKeys.end() ||
                      qolKeys.find(key) != qolKeys.end();

    toggle->setVisible(!subToggles);
  }

  setUpdatesEnabled(true);
  update();
}
