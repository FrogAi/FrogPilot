#include "frogpilot/ui/qt/offroad/utilities.h"

const std::set<std::string> excluded_keys = {
  "ApiCache_DriveStats", "CalibratedLateralAcceleration", "CalibrationParams",
  "CalibrationProgress", "CarParams", "CarParamsPersistent",
  "CurvatureData", "DiscordUsername", "DongleId",
  "DoReboot", "DoShutdown", "DoSoftReboot",
  "DownloadableColors", "DownloadableDistanceIcons", "DownloadableIcons",
  "DownloadableSignals", "DownloadableSounds", "DownloadableWheels",
  "ForceOffroad", "ForceOnroad", "FrogPilotApiToken",
  "FrogPilotCarParams", "FrogPilotCarParamsPersistent", "FrogPilotRegistration",
  "FrogPilotStats", "GithubSshKeys", "GithubUsername",
  "HardwareSerial", "IMEI", "IsOffroad",
  "IsOnroad", "KonikDongleId", "KonikMinutes",
  "LastMapsUpdate", "LastUpdateRouteCount", "LastUpdateUptimeOnroad",
  "LiveDelay", "LiveParameters", "LiveParametersV2",
  "LiveTorqueParameters", "MapboxPublicKey", "MapBoxRequests",
  "MapboxSecretKey", "MapdSettings", "MaxLateralAcceleration",
  "MinimumBackupSize", "ModelDrivesAndScores", "openpilotMinutes",
  "PreviousSpeedLimit", "RandomizedModel", "RouteCount",
  "SecOCKeys", "SpeedLimits", "SpeedLimitsUploadedHash",
  "StockDongleId", "ThemesDownloaded", "Timezone",
  "UpdateAvailable", "Updated", "UptimeOffroad",
  "UptimeOnroad", "WeatherToken",
};

FrogPilotUtilitiesPanel::FrogPilotUtilitiesPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent) {
  ParamControl *debugModeToggle = new ParamControl("DebugMode", tr("Debug Mode"), tr("<b>Show FrogPilot's developer readouts on the driving screen for your next drive, so a bug report can say what openpilot was actually doing.</b><br><br>It switches itself back off once you finish the drive. While it is on, the temperature reads in Celsius and the developer numbers read in scientific units, whatever you picked elsewhere. It also brings back anything you hid from the driving screen and uses the default \"Model UI\" sizes and \"Camera View\", with \"Rainbow Path\" off, until the drive ends. Your speedometer keeps its own units."), "");
  addItem(debugModeToggle);

  FrogPilotButtonsControl *forceStartedButton = new FrogPilotButtonsControl(tr("Force Drive State"), tr("<b>Make openpilot behave as though the car is running, or as though it is parked, without the car actually being either.</b><br><br>This is a testing tool. Forcing the running state ignores your \"Screen Brightness (Onroad)\" setting and stops openpilot warning you that its controls are unresponsive, so leave it on \"OFF\" unless you know why you need it. Forcing the running state ends when you turn the car on, and either state clears itself the next time the device restarts. While it is forced, the device still drops back to offroad if it overheats and still shuts down on its usual timer or a low car battery. With the car off, a forced running state runs on the car's battery until the device powers itself off."), "", {tr("OFFROAD"), tr("ONROAD"), tr("OFF")}, true);
  std::function<void()> updateForceStartedButton = [forceStartedButton, this]() {
    if (params.getBool("ForceOffroad")) {
      forceStartedButton->setCheckedButton(0);
    } else if (params.getBool("ForceOnroad")) {
      forceStartedButton->setCheckedButton(1);
    } else {
      forceStartedButton->setCheckedButton(2);
    }
  };
  QObject::connect(forceStartedButton, &FrogPilotButtonsControl::buttonClicked, [updateForceStartedButton, this](int id) {
    if (id == 0 && isOpenpilotSteering()) {
      ConfirmationDialog::alert(tr("The parked state can't be forced while openpilot is steering. Disengage and try again."), this);
      updateForceStartedButton();
      return;
    }

    if (id == 1 && uiState()->scene.ignition) {
      ConfirmationDialog::alert(tr("The running state can't be forced while the car is on. Turn the car off and try again."), this);
      updateForceStartedButton();
      return;
    }

    if (id == 0) {
      params.putBool("ForceOffroad", true);
      params.putBool("ForceOnroad", false);
    } else if (id == 1) {
      params.put("CarParams", params.get("CarParamsPersistent"));
      params.put("FrogPilotCarParams", params.get("FrogPilotCarParamsPersistent"));

      params.putBool("ForceOffroad", false);
      params.putBool("ForceOnroad", true);
    } else if (id == 2) {
      params.putBool("ForceOffroad", false);
      params.putBool("ForceOnroad", false);
    }

    frogpilotUIState()->updateToggles();
  });
  QObject::connect(uiState(), &UIState::offroadTransition, forceStartedButton, updateForceStartedButton);
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::togglesUpdated, forceStartedButton, updateForceStartedButton);
  updateForceStartedButton();
  QObject::connect(parent, &FrogPilotSettingsWindow::tuningLevelChanged, [forceStartedButton, parent, this](int tuningLevel) {
    bool visible = tuningLevel >= parent->frogpilotToggleLevels.value("ForceOnroad").toDouble();
    forceStartedButton->setVisible(visible);

    if (!visible && (params.getBool("ForceOffroad") || params.getBool("ForceOnroad"))) {
      params.putBool("ForceOffroad", false);
      params.putBool("ForceOnroad", false);

      frogpilotUIState()->updateToggles();
    }
  });
  addItem(forceStartedButton);

  ButtonControl *flashPandaButton = new ButtonControl(tr("Reflash the Panda"), tr("FLASH"), tr("<b>Reinstall the software on the Panda, the small box that lets your device talk to your car.</b><br><br>Try this if openpilot keeps losing contact with the car or the Panda shows up as faulty. Your device reboots once it finishes, and the car has to be off to start."));
  QObject::connect(flashPandaButton, &ButtonControl::clicked, [parent, flashPandaButton, this]() {
    if (uiState()->scene.started) {
      ConfirmationDialog::alert(tr("The Panda can't be reflashed while the car is on. Turn the car off and try again."), this);
      return;
    }

    if (actionRunning) {
      ConfirmationDialog::alert(tr("Something else is already running. Wait for it to finish and try again."), this);
      return;
    }

    if (ConfirmationDialog::confirm(tr("Reflash the Panda? Your device reboots once it finishes."), tr("Flash"), this)) {
      actionRunning = true;

      frogpilotUIState()->flashPanda();
      uint64_t flashPandaRequestTime = frogpilotUIState()->flash_panda_request_time;

      std::thread([parent, flashPandaButton, flashPandaRequestTime, this]() {
        runOnUIThread(flashPandaButton, [parent, flashPandaButton]() {
          parent->activeOperations++;

          flashPandaButton->setEnabled(false);
          flashPandaButton->setValue(tr("Flashing..."));
        });

        SubMaster sm({"frogpilotProcessState"});
        while (sm["frogpilotProcessState"].getFrogpilotProcessState().getFlashPandaRequestTime() < flashPandaRequestTime || sm["frogpilotProcessState"].getFrogpilotProcessState().getFlashingPanda()) {
          sm.update(1000);
        }

        if (sm["frogpilotProcessState"].getFrogpilotProcessState().getFlashedPanda()) {
          runOnUIThread(flashPandaButton, [flashPandaButton]() {
            flashPandaButton->setValue(tr("Flashed!"));
          });

          util::sleep_for(2500);

          runOnUIThread(flashPandaButton, [flashPandaButton]() {
            flashPandaButton->setValue(tr("Rebooting..."));
          });

          util::sleep_for(2500);

          Hardware::reboot();
        } else {
          runOnUIThread(flashPandaButton, [flashPandaButton]() {
            flashPandaButton->setValue(tr("Flash failed..."));
          });

          util::sleep_for(2500);

          runOnUIThread(flashPandaButton, [parent, flashPandaButton, this]() {
            flashPandaButton->setEnabled(true);
            flashPandaButton->setValue("");

            parent->activeOperations--;
            actionRunning = false;
          });
        }
      }).detach();
    }
  });
  addItem(flashPandaButton);

  std::function<void(ButtonControl*, bool, const QString&)> resetSettings = [parent, this](ButtonControl *button, bool stock, const QString &confirmText) {
    if (uiState()->scene.started) {
      ConfirmationDialog::alert(tr("Settings can't be reset while the car is on. Turn the car off and try again."), this);
      return;
    }

    if (actionRunning) {
      ConfirmationDialog::alert(tr("Something else is already running. Wait for it to finish and try again."), this);
      return;
    }

    if (ConfirmationDialog::confirm(confirmText, tr("Reset"), this)) {
      actionRunning = true;

      std::thread([parent, button, stock, this]() {
        runOnUIThread(button, [parent, button]() {
          parent->activeOperations++;

          button->setEnabled(false);
          button->setValue(tr("Resetting..."));
        });

        std::vector<std::string> all_keys = params.allKeys();
        for (const std::string &key : all_keys) {
          if (excluded_keys.count(key)) {
            continue;
          }
          std::optional<std::string> reset_value = stock ? params.getStockValue(key) : params.getKeyDefaultValue(key);
          if (reset_value.has_value()) {
            if (reset_value->empty()) {
              params.remove(key);
            } else if (params.get(key) != reset_value.value()) {
              params.put(key, reset_value.value());
            }
          }
        }

        if (stock) {
          params.putInt("TuningLevel", 3);
          params.putBool("TuningLevelConfirmed", true);
        }

        runOnUIThread(button, [parent, button, this]() {
          parent->updateMetric(false, true);
          parent->updateMetric(params.getBool("IsMetric"));
          frogpilotUIState()->updateToggles();
          parent->updateTuningLevel();

          button->setValue(tr("Reset!"));
        });

        util::sleep_for(2500);

        runOnUIThread(button, [parent, button, this]() {
          button->setEnabled(true);
          button->setValue("");

          parent->activeOperations--;
          actionRunning = false;
        });
      }).detach();
    }
  };

  ButtonControl *resetTogglesButton = new ButtonControl(tr("Reset Settings to Default"), tr("RESET"), tr("<b>Put every FrogPilot setting back to the value it shipped with, along with some of openpilot's own settings.</b><br><br>Those openpilot settings are \"Enable openpilot\", \"Disengage on Accelerator Pedal\", \"Driving Personality\" and \"Cellular Metered\". This also clears your accepted terms, your completed training and your language, so you go through first-time setup again in English the next time the device starts. Your drives, backups, downloaded themes, learned driving data and saved keys are left alone."));
  QObject::connect(resetTogglesButton, &ButtonControl::clicked, [resetSettings, resetTogglesButton]() {
    resetSettings(resetTogglesButton, false, tr("Reset all FrogPilot and some openpilot settings? You will have to accept the terms, redo the training and set your language again."));
  });
  addItem(resetTogglesButton);

  ButtonControl *resetTogglesButtonStock = new ButtonControl(tr("Reset Settings to Stock openpilot"), tr("RESET"), tr("<b>Put every setting back to what plain openpilot uses, turning FrogPilot's own features off rather than back to FrogPilot's defaults.</b><br><br>This also clears your accepted terms, your completed training and your language, so you go through first-time setup again in English the next time the device starts. Your drives, backups, downloaded themes, learned driving data and saved keys are left alone."));
  QObject::connect(resetTogglesButtonStock, &ButtonControl::clicked, [resetSettings, resetTogglesButtonStock]() {
    resetSettings(resetTogglesButtonStock, true, tr("Reset every setting to match stock openpilot? You will have to accept the terms, redo the training and set your language again."));
  });
  addItem(resetTogglesButtonStock);

  if (forceOpen) {
    for (AbstractControl *control : std::initializer_list<AbstractControl*>{debugModeToggle, forceStartedButton, flashPandaButton, reportIssueButton, resetTogglesButton, resetTogglesButtonStock}) {
      control->showDescription();
    }
  }
}
