#include "frogpilot/ui/qt/offroad/utilities.h"
#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

FrogPilotUtilitiesPanel::FrogPilotUtilitiesPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  bool forceOpenDescriptions = false;
  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

  ParamControl *debugModeToggle = new ParamControl("DebugMode", tr("Debug Mode"), tr("<b>Show FrogPilot's developer readouts on the driving screen for your next drive, so a bug report can say what openpilot was actually doing.</b><br><br>It switches itself back off once you finish the drive. While it is on, the temperature reads in Celsius and the developer numbers read in scientific units, whatever you picked elsewhere. Your speedometer is not affected."), "");
  if (forceOpenDescriptions) {
    debugModeToggle->showDescription();
  }
  addItem(debugModeToggle);

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

      std::thread([parent, flashPandaButton, this]() {
        runOnUIThread(flashPandaButton, [=]() {
          parent->keepScreenOn = true;

          flashPandaButton->setEnabled(false);
          flashPandaButton->setValue(tr("Flashing..."));
        });

        params_memory.putBool("FlashPanda", true);

        int elapsed = 0;
        while (params_memory.getBool("FlashPanda") && elapsed < 120000) {
          util::sleep_for(100);
          elapsed += 100;
        }

        bool flashed = !params_memory.getBool("FlashPanda");
        if (!flashed) {
          params_memory.remove("FlashPanda");
        }

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
  if (forceOpenDescriptions) {
    flashPandaButton->showDescription();
  }
  addItem(flashPandaButton);

  FrogPilotButtonsControl *forceStartedButton = new FrogPilotButtonsControl(tr("Force Drive State"), tr("<b>Make openpilot behave as though the car is running, or as though it is parked, without the car actually being either.</b><br><br>This is a testing tool. Forcing the running state pins the screen to full brightness and stops openpilot warning you that its controls are unresponsive, so leave it on \"OFF\" unless you know why you need it. It clears itself the next time the device restarts."), "", {tr("OFFROAD"), tr("ONROAD"), tr("OFF")}, true);
  QObject::connect(forceStartedButton, &FrogPilotButtonsControl::buttonClicked, [forceStartedButton, this](int id) {
    if (id == 0) {
      params_memory.putBool("ForceOffroad", true);
      params_memory.putBool("ForceOnroad", false);

      updateFrogPilotToggles();
    } else if (id == 1) {
      if (params.get("CarParamsPersistent").empty()) {
        ConfirmationDialog::alert(tr("openpilot hasn't learned your car yet, so it can't be forced onroad. Complete a drive first."), this);
        forceStartedButton->setCheckedButton(2);
        return;
      }

      params.put("CarParams", params.get("CarParamsPersistent"));
      params.put("FrogPilotCarParams", params.get("FrogPilotCarParamsPersistent"));

      params_memory.putBool("ForceOffroad", false);
      params_memory.putBool("ForceOnroad", true);

      updateFrogPilotToggles();
    } else if (id == 2) {
      params_memory.putBool("ForceOffroad", false);
      params_memory.putBool("ForceOnroad", false);

      updateFrogPilotToggles();
    }
  });
  forceStartedButton->setCheckedButton(params_memory.getBool("ForceOffroad") ? 0 : params_memory.getBool("ForceOnroad") ? 1 : 2);
  if (forceOpenDescriptions) {
    forceStartedButton->showDescription();
  }
  addItem(forceStartedButton);

  ButtonControl *reportIssueButton = new ButtonControl(tr("Report a Bug or an Issue"), tr("REPORT"), tr("<b>Tell the FrogPilot team what went wrong, straight from the car.</b><br><br>You pick what happened from a list, add a description where it helps, and give your Discord name so they can reach you. Your settings and the most recent error log go along with it so the problem can be traced."));
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
      QString extra_input = InputDialog::getText(tr("Please describe what's happening"), this, tr("The more detail the better"), false, 10, "", 300).trimmed();
      if (extra_input.isEmpty()) {
        return;
      }
      selected_issue += (selected_issue.endsWith(":") ? " " : ": ") + extra_input;
    }

    QString discordUser = InputDialog::getText(tr("What's your Discord username?"), this, tr("So we can reach you about the fix"), false, -1, QString::fromStdString(params.get("DiscordUsername")), 64).trimmed();
    if (discordUser.isEmpty()) {
      return;
    }

    QJsonObject reportData;
    reportData["Issue"] = selected_issue;
    reportData["DiscordUser"] = discordUser;

    params.putNonBlocking("DiscordUsername", discordUser.toStdString());
    params_memory.put("IssueReported", QJsonDocument(reportData).toJson(QJsonDocument::Compact).toStdString());

    ConfirmationDialog::alert(tr("Report sent, thanks for letting us know!"), this);
  });
  if (forceOpenDescriptions) {
    reportIssueButton->showDescription();
  }
  addItem(reportIssueButton);
  reportIssueButton->setVisible(QString::fromStdString(params.get("GitRemote")).toLower().contains("frogai/frogpilot"));

  ButtonControl *resetTogglesButton = new ButtonControl(tr("Reset Settings to Default"), tr("RESET"), tr("<b>Put every FrogPilot setting back to the value it shipped with.</b><br><br>This also clears your accepted terms, your completed training and your language, so you go through first-time setup again in English. The reset happens while the device reboots, and your drives, backups and downloaded themes are left alone."));
  QObject::connect(resetTogglesButton, &ButtonControl::clicked, [parent, resetTogglesButton, this]() {
    if (uiState()->scene.started) {
      ConfirmationDialog::alert(tr("Settings can't be reset while the car is on. Turn the car off and try again."), this);
      return;
    }

    if (actionRunning) {
      ConfirmationDialog::alert(tr("Something else is already running. Wait for it to finish and try again."), this);
      return;
    }

    if (ConfirmationDialog::confirm(tr("Reset every FrogPilot setting to its default? You will have to accept the terms, redo the training and set your language again, and the device reboots."), tr("Reset"), this)) {
      actionRunning = true;

      std::thread([parent, resetTogglesButton, this]() mutable {
        runOnUIThread(resetTogglesButton, [=]() {
          parent->keepScreenOn = true;

          resetTogglesButton->setEnabled(false);
          resetTogglesButton->setValue(tr("Resetting..."));
        });

        params.putBool("DoToggleReset", true);

        runOnUIThread(resetTogglesButton, [=]() {
          resetTogglesButton->setValue(tr("Rebooting to reset..."));
        });

        util::sleep_for(2500);

        Hardware::reboot();
      }).detach();
    }
  });
  if (forceOpenDescriptions) {
    resetTogglesButton->showDescription();
  }
  addItem(resetTogglesButton);

  ButtonControl *resetTogglesButtonStock = new ButtonControl(tr("Reset Settings to Stock openpilot"), tr("RESET"), tr("<b>Put every setting back to what plain openpilot uses, turning FrogPilot's own features off rather than back to FrogPilot's defaults.</b><br><br>This also clears your accepted terms, your completed training and your language, so you go through first-time setup again in English. The reset happens while the device reboots, and your drives, backups and downloaded themes are left alone."));
  QObject::connect(resetTogglesButtonStock, &ButtonControl::clicked, [parent, resetTogglesButtonStock, this]() {
    if (uiState()->scene.started) {
      ConfirmationDialog::alert(tr("Settings can't be reset while the car is on. Turn the car off and try again."), this);
      return;
    }

    if (actionRunning) {
      ConfirmationDialog::alert(tr("Something else is already running. Wait for it to finish and try again."), this);
      return;
    }

    if (ConfirmationDialog::confirm(tr("Reset every setting to match stock openpilot? You will have to accept the terms, redo the training and set your language again, and the device reboots."), tr("Reset"), this)) {
      actionRunning = true;

      std::thread([parent, resetTogglesButtonStock, this]() mutable {
        runOnUIThread(resetTogglesButtonStock, [=]() {
          parent->keepScreenOn = true;

          resetTogglesButtonStock->setEnabled(false);
          resetTogglesButtonStock->setValue(tr("Resetting..."));
        });

        params.putBool("DoToggleResetStock", true);

        runOnUIThread(resetTogglesButtonStock, [=]() {
          resetTogglesButtonStock->setValue(tr("Rebooting to reset..."));
        });

        util::sleep_for(2500);

        Hardware::reboot();
      }).detach();
    }
  });
  if (forceOpenDescriptions) {
    resetTogglesButtonStock->showDescription();
  }
  addItem(resetTogglesButtonStock);
}
