#include "frogpilot/ui/qt/offroad/utilities.h"

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

  reportIssueButton = new ButtonControl(tr("Report a Bug or an Issue"), tr("REPORT"), tr("<b>Tell the FrogPilot team what went wrong, straight from the car.</b><br><br>You pick what happened from a list, add a description where it helps, and give your Discord name so they can reach you. Your settings and the most recent error log go along with it so the problem can be traced."));
  QObject::connect(reportIssueButton, &ButtonControl::clicked, [this]() {
    if (!frogpilotUIState()->frogpilot_scene.online) {
      ConfirmationDialog::alert(tr("Connect to Wi-Fi or a hotspot first, then send your report."), this);
      return;
    }

    QJsonObject previousReport = QJsonDocument::fromJson((*frogpilotUIState()->sm)["frogpilotProcessState"].getFrogpilotProcessState().getIssueReport().cStr()).object();
    if (previousReport["status"].toString() == "failed") {
      QString retryOption = tr("Retry previous report");
      QString selected = MultiOptionDialog::getSelection(tr("Delivery wasn't confirmed.\nRetry to avoid duplicates."), {retryOption, tr("Start a new report")}, "", this);

      if (selected.isEmpty()) {
        return;
      }

      if (selected == retryOption) {
        frogpilotUIState()->retryIssueReport();

        reportIssueButton->setEnabled(false);
        reportIssueButton->setValue(tr("Sending..."));
        return;
      }
    }

    QString crashOption = tr("I saw an alert that said \"openpilot crashed\"");
    QString unsureOption = tr("I'm not sure if this is normal or a bug:");
    QString otherOption = tr("Something else (please describe)");

    QStringList reportMessages = {
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

    if (QFile::exists("/data/error_logs/error.txt")) {
      reportMessages.prepend(crashOption);
    }

    QStringList needsExtraInput = {crashOption, unsureOption, otherOption};

    QString selectedIssue = MultiOptionDialog::getSelection(tr("What went wrong?"), reportMessages, "", this);
    if (selectedIssue.isEmpty()) {
      return;
    }

    if (needsExtraInput.contains(selectedIssue)) {
      QString extraInput = InputDialog::getText(tr("Please describe what's happening"), this, "", false, 10, "", 300).trimmed();
      if (extraInput.isEmpty()) {
        return;
      }
      if (selectedIssue.endsWith(":")) {
        selectedIssue += " " + extraInput;
      } else {
        selectedIssue += ": " + extraInput;
      }
    }

    QString discordUser = InputDialog::getText(tr("What's your Discord username?"), this, "",
                                              false, -1, QString::fromStdString(params.get("DiscordUsername")), 64).trimmed();
    if (discordUser.isEmpty()) {
      return;
    }

    QJsonObject reportData;
    reportData["DiscordUser"] = discordUser;
    reportData["Issue"] = selectedIssue;

    params.putNonBlocking("DiscordUsername", discordUser.toStdString());
    submitReport(reportData);
  });
  addItem(reportIssueButton);
  std::function<void()> updateReportIssueButton = [this]() {
    reportIssueButton->setVisible(frogpilotUIState()->frogpilot_scene.frogpilot_toggles.value("issue_report_available").toBool());
  };
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::togglesUpdated, reportIssueButton, updateReportIssueButton);
  updateReportIssueButton();

  if (forceOpen) {
    for (AbstractControl *control : std::initializer_list<AbstractControl*>{debugModeToggle, forceStartedButton, flashPandaButton, reportIssueButton}) {
      control->showDescription();
    }
  }

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
  reportIssueButton->setText(status == "failed" ? tr("RETRY") : tr("REPORT"));
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
