#include "frogpilot/ui/qt/offroad/utilities.h"

FrogPilotUtilitiesPanel::FrogPilotUtilitiesPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  ParamControl *debugModeToggle = new ParamControl("DebugMode", tr("Debug Mode"), tr("<b>Show FrogPilot's developer readouts on the driving screen for your next drive, so a bug report can say what openpilot was actually doing.</b><br><br>It switches itself back off once you finish the drive. While it is on, the temperature reads in Celsius and the developer numbers read in scientific units, whatever you picked elsewhere. It also brings back anything you hid from the driving screen and uses the default \"Model UI\" sizes and \"Camera View\", with \"Rainbow Path\" off, until the drive ends. Your speedometer keeps its own units."), "");
  if (forceOpen) {
    debugModeToggle->showDescription();
  }
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
      if (params.get("CarParamsPersistent").empty()) {
        ConfirmationDialog::alert(tr("openpilot hasn't learned your car yet, so it can't be forced onroad. Complete a drive first."), this);
        updateForceStartedButton();
        return;
      }

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
  if (forceOpen) {
    forceStartedButton->showDescription();
  }
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
        runOnUIThread(flashPandaButton, [=]() {
          parent->keepScreenOn = true;

          flashPandaButton->setEnabled(false);
          flashPandaButton->setValue(tr("Flashing..."));
        });

        SubMaster sm({"frogpilotProcessState"});
        while (sm["frogpilotProcessState"].getFrogpilotProcessState().getFlashPandaRequestTime() < flashPandaRequestTime || sm["frogpilotProcessState"].getFrogpilotProcessState().getFlashingPanda()) {
          sm.update(1000);
        }

        bool flashed = sm["frogpilotProcessState"].getFrogpilotProcessState().getFlashedPanda();

        runOnUIThread(flashPandaButton, [=]() {
          flashPandaButton->setValue(flashed ? tr("Flashed!") : tr("Flash failed..."));
        });

        util::sleep_for(2500);

        if (flashed) {
          runOnUIThread(flashPandaButton, [=]() {
            flashPandaButton->setValue(tr("Rebooting..."));
          });

          util::sleep_for(2500);

          Hardware::reboot();
        } else {
          runOnUIThread(flashPandaButton, [=]() {
            flashPandaButton->setEnabled(true);
            flashPandaButton->setValue("");

            parent->keepScreenOn = false;
            actionRunning = false;
          });
        }
      }).detach();
    }
  });
  if (forceOpen) {
    flashPandaButton->showDescription();
  }
  addItem(flashPandaButton);

  reportIssueButton = new ButtonControl(tr("Report a Bug or an Issue"), tr("REPORT"), tr("<b>Tell the FrogPilot team what went wrong, straight from the car.</b><br><br>You pick what happened from a list, add a description where it helps, and give your Discord name so they can reach you. Your settings and the most recent error log go along with it so the problem can be traced."));
  QObject::connect(reportIssueButton, &ButtonControl::clicked, [this]() {
    if (!frogpilotUIState()->frogpilot_scene.online) {
      ConfirmationDialog::alert(tr("Connect to Wi-Fi or a hotspot first, then send your report."), this);
      return;
    }

    QStringList report_messages;
    QString crash_report = tr("I saw an alert that said \"openpilot crashed\"");
    if (QFile::exists("/data/error_logs/error.txt")) {
      report_messages << crash_report;
    }
    QString crashOption = crash_report;
    QString unsureOption = tr("I'm not sure if this is normal or a bug:");
    QString otherOption = tr("Something else (please describe)");

    QStringList additional_issues = {
      tr("Acceleration feels harsh or jerky"),
      tr("An alert was unclear and I didn't know what it meant"),
      tr("Braking is too sudden or uncomfortable"),
      unsureOption,
      tr("My screen froze or is stuck loading something"),
      tr("My steering wheel buttons aren't working"),
      tr("openpilot disengages when I don't expect it"),
      tr("openpilot doesn't react to stopped vehicles ahead"),
      tr("openpilot doesn't resume from a stop"),
      tr("openpilot feels sluggish or slow to respond"),
      tr("Steering feels twitchy or unnatural"),
      tr("The car doesn't follow curves well"),
      tr("The car isn't staying centered in its lane"),
      otherOption
    };
    report_messages.append(additional_issues);

    QStringList needs_extra_input = {crashOption, unsureOption, otherOption};

    QString selected_issue = MultiOptionDialog::getSelection(tr("What went wrong?"), report_messages, "", this);
    if (selected_issue.isEmpty()) {
      return;
    }

    if (needs_extra_input.contains(selected_issue)) {
      QString extra_input = InputDialog::getText(tr("Please describe what's happening"), this, "", false, 10, "", 300).trimmed();
      if (extra_input.isEmpty()) {
        return;
      }
      selected_issue += (selected_issue.endsWith(":") ? " " : ": ") + extra_input;
    }

    QString discordUser = InputDialog::getText(tr("What's your Discord username?"), this, "", false, -1, QString::fromStdString(params.get("DiscordUsername")), 64).trimmed();
    if (discordUser.isEmpty()) {
      return;
    }

    QJsonObject reportData;
    reportData["DiscordUser"] = discordUser;
    reportData["Issue"] = selected_issue;

    params.putNonBlocking("DiscordUsername", discordUser.toStdString());

    submitReport(reportData);
  });
  if (forceOpen) {
    reportIssueButton->showDescription();
  }
  addItem(reportIssueButton);
  std::function<void()> updateReportIssueButton = [this]() {
    reportIssueButton->setVisible(frogpilotUIState()->frogpilot_scene.frogpilot_toggles.value("issue_report_available").toBool());
  };
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::togglesUpdated, reportIssueButton, updateReportIssueButton);
  updateReportIssueButton();

  ButtonControl *resetTogglesButton = new ButtonControl(tr("Reset Settings to Default"), tr("RESET"), tr("<b>Put every FrogPilot setting back to the value it shipped with, along with some of openpilot's own settings.</b><br><br>Those openpilot settings are \"Enable openpilot\", \"Disengage on Accelerator Pedal\", \"Driving Personality\", \"Cellular Metered\", \"Enable Lane Departure Warnings\", \"Always-On Driver Monitoring\", \"Record and Upload Driver Camera\", \"Use Metric System\", and \"Enable Roaming\". This also clears your accepted terms, your completed training, and your language, so you go through first-time setup again in English. It also erases your saved keys (Amap, Mapbox, and weather), favorite places, Discord name, chosen car make and model, SSH keys, cellular APN, map settings, and speed limit sign clips, turns SSH and tethering off, asks again whether to share FrogPilot data, and unlinks this device from your FrogPilot.com account. The reset happens while the device reboots. Your drives, backups, downloaded themes, learned driving data, Speed Limit Filler data, FrogPilot Stats, Toyota security keys, \"The Pond\" pairing, Tailscale sign-in, \"Experimental Mode\", and \"openpilot Longitudinal Control (Alpha)\" are left alone, and settings backups still hold your keys and favorite places until you delete them."));
  QObject::connect(resetTogglesButton, &ButtonControl::clicked, [parent, resetTogglesButton, this]() {
    if (uiState()->scene.started) {
      ConfirmationDialog::alert(tr("Settings can't be reset while the car is on. Turn the car off and try again."), this);
      return;
    }

    if (actionRunning) {
      ConfirmationDialog::alert(tr("Something else is already running. Wait for it to finish and try again."), this);
      return;
    }

    ConfirmationDialog confirmationDialog(tr("Reset all FrogPilot and some openpilot settings? This erases your saved keys, favorite places, Discord name, chosen car make and model, SSH keys, cellular APN, map settings, and speed limit sign clips, turns SSH and tethering off, asks again whether to share FrogPilot data, and unlinks this device from your FrogPilot.com account. The device reboots into first-time setup. Learned driving data, Speed Limit Filler data, FrogPilot Stats, Toyota security keys, \"The Pond\" pairing, and Tailscale sign-in stay."), tr("Reset"), ConfirmationDialog::tr("Cancel"), true, this);
    if (confirmationDialog.exec()) {
      actionRunning = true;

      frogpilotUIState()->unlinkAccount();
      uint64_t unlinkRequestTime = frogpilotUIState()->account_link_request_time;

      std::thread([parent, resetTogglesButton, unlinkRequestTime, this]() mutable {
        runOnUIThread(resetTogglesButton, [=]() {
          parent->keepScreenOn = true;

          resetTogglesButton->setEnabled(false);
          resetTogglesButton->setValue(tr("Resetting..."));
        });

        SubMaster sm({"frogpilotAccountState"});
        for (int i = 0; i < 10; i++) {
          sm.update(1000);

          if (sm["frogpilotAccountState"].getFrogpilotAccountState().getLinkRequestTime() >= unlinkRequestTime) {
            break;
          }
        }

        params.putBool("DoToggleReset", true);

        runOnUIThread(resetTogglesButton, [=]() {
          resetTogglesButton->setValue(tr("Rebooting to reset..."));
        });

        util::sleep_for(2500);

        Hardware::reboot();
      }).detach();
    }
  });
  if (forceOpen) {
    resetTogglesButton->showDescription();
  }
  addItem(resetTogglesButton);

  ButtonControl *resetTogglesButtonStock = new ButtonControl(tr("Reset Settings to Stock openpilot"), tr("RESET"), tr("<b>Put every setting back to what plain openpilot uses, turning FrogPilot's own features off rather than back to FrogPilot's defaults.</b><br><br>This also clears your accepted terms, your completed training, and your language, so you go through first-time setup again in English. It also erases your saved keys (Amap, Mapbox, and weather), favorite places, Discord name, chosen car make and model, SSH keys, cellular APN, map settings, and speed limit sign clips, turns SSH and tethering off, asks again whether to share FrogPilot data, and unlinks this device from your FrogPilot.com account. The reset happens while the device reboots. Your drives, backups, downloaded themes, learned driving data, Speed Limit Filler data, FrogPilot Stats, Toyota security keys, \"The Pond\" pairing, Tailscale sign-in, \"Experimental Mode\", and \"openpilot Longitudinal Control (Alpha)\" are left alone, and settings backups still hold your keys and favorite places until you delete them."));
  QObject::connect(resetTogglesButtonStock, &ButtonControl::clicked, [parent, resetTogglesButtonStock, this]() {
    if (uiState()->scene.started) {
      ConfirmationDialog::alert(tr("Settings can't be reset while the car is on. Turn the car off and try again."), this);
      return;
    }

    if (actionRunning) {
      ConfirmationDialog::alert(tr("Something else is already running. Wait for it to finish and try again."), this);
      return;
    }

    ConfirmationDialog confirmationDialog(tr("Reset every setting to match stock openpilot? This erases your saved keys, favorite places, Discord name, chosen car make and model, SSH keys, cellular APN, map settings, and speed limit sign clips, turns SSH and tethering off, asks again whether to share FrogPilot data, and unlinks this device from your FrogPilot.com account. The device reboots into first-time setup. Learned driving data, Speed Limit Filler data, FrogPilot Stats, Toyota security keys, \"The Pond\" pairing, and Tailscale sign-in stay."), tr("Reset"), ConfirmationDialog::tr("Cancel"), true, this);
    if (confirmationDialog.exec()) {
      actionRunning = true;

      frogpilotUIState()->unlinkAccount();
      uint64_t unlinkRequestTime = frogpilotUIState()->account_link_request_time;

      std::thread([parent, resetTogglesButtonStock, unlinkRequestTime, this]() mutable {
        runOnUIThread(resetTogglesButtonStock, [=]() {
          parent->keepScreenOn = true;

          resetTogglesButtonStock->setEnabled(false);
          resetTogglesButtonStock->setValue(tr("Resetting..."));
        });

        SubMaster sm({"frogpilotAccountState"});
        for (int i = 0; i < 10; i++) {
          sm.update(1000);

          if (sm["frogpilotAccountState"].getFrogpilotAccountState().getLinkRequestTime() >= unlinkRequestTime) {
            break;
          }
        }

        params.putBool("DoToggleResetStock", true);

        runOnUIThread(resetTogglesButtonStock, [=]() {
          resetTogglesButtonStock->setValue(tr("Rebooting to reset..."));
        });

        util::sleep_for(2500);

        Hardware::reboot();
      }).detach();
    }
  });
  if (forceOpen) {
    resetTogglesButtonStock->showDescription();
  }
  addItem(resetTogglesButtonStock);

  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotUtilitiesPanel::updateState);
}

void FrogPilotUtilitiesPanel::submitReport(QJsonObject report) {
  frogpilotUIState()->reportIssue(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Compact)));

  reportIssueButton->setEnabled(false);
  reportIssueButton->setValue(tr("Sending..."));
}

void FrogPilotUtilitiesPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible() || s.sm->frame % UI_FREQ != 0) {
    return;
  }

  const cereal::FrogPilotProcessState::Reader &frogpilotProcessState = (*fs.sm)["frogpilotProcessState"].getFrogpilotProcessState();

  QString status = QJsonDocument::fromJson(frogpilotProcessState.getIssueReport().cStr()).object()["status"].toString();
  if (fs.issue_report_request_time > frogpilotProcessState.getIssueReportRequestTime() || status == "uploading") {
    status = "pending";
  }

  reportIssueButton->setEnabled(status != "pending");
  if (status == "pending") {
    reportIssueButton->setValue(tr("Sending..."));
  } else if (status == "sent") {
    reportIssueButton->setValue(tr("Sent"));
  } else if (status == "failed") {
    reportIssueButton->setValue(tr("Failed"));
  } else {
    reportIssueButton->setValue("");
  }
}
