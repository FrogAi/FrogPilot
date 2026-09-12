#include "frogpilot/ui/qt/offroad/utilities.h"

FrogPilotUtilitiesPanel::FrogPilotUtilitiesPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

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
        runOnUIThread(flashPandaButton, [parent, flashPandaButton]() {
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

        runOnUIThread(flashPandaButton, [flashPandaButton, flashed]() {
          flashPandaButton->setValue(flashed ? tr("Flashed!") : tr("Flash failed..."));
        });

        util::sleep_for(2500);

        if (flashed) {
          runOnUIThread(flashPandaButton, [flashPandaButton]() {
            flashPandaButton->setValue(tr("Rebooting..."));
          });

          util::sleep_for(2500);

          Hardware::reboot();
        } else {
          runOnUIThread(flashPandaButton, [parent, flashPandaButton, this]() {
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
      params.putBool("ForceOffroad", true);
      params.putBool("ForceOnroad", false);

      updateFrogPilotToggles();
    } else if (id == 1) {
      if (params.get("CarParamsPersistent").empty()) {
        ConfirmationDialog::alert(tr("openpilot hasn't learned your car yet, so it can't be forced onroad. Complete a drive first."), this);
        forceStartedButton->setCheckedButton(2);
        return;
      }

      params.put("CarParams", params.get("CarParamsPersistent"));
      params.put("FrogPilotCarParams", params.get("FrogPilotCarParamsPersistent"));

      params.putBool("ForceOffroad", false);
      params.putBool("ForceOnroad", true);

      updateFrogPilotToggles();
    } else if (id == 2) {
      params.putBool("ForceOffroad", false);
      params.putBool("ForceOnroad", false);

      updateFrogPilotToggles();
    }
  });
  forceStartedButton->setCheckedButton(2);
  if (forceOpenDescriptions) {
    forceStartedButton->showDescription();
  }
  addItem(forceStartedButton);

  ButtonControl *reportIssueButton = new ButtonControl(tr("Report a Bug or an Issue"), tr("REPORT"), tr("<b>Tell the FrogPilot team what went wrong, straight from the car.</b><br><br>You pick what happened from a list, add a description where it helps, and give your Discord name so they can reach you. Your settings and the most recent error log go along with it so the problem can be traced."));
  QObject::connect(reportIssueButton, &ButtonControl::clicked, [this]() {
    if (!frogpilotUIState()->frogpilot_scene.online) {
      ConfirmationDialog::alert(tr("Please connect to the internet before sending a report!"), this);
      return;
    }

    QStringList report_messages = {
      tr("Acceleration feels harsh or jerky"),
      tr("An alert was unclear and I'm not sure what it meant"),
      tr("Braking is too sudden or uncomfortable"),
      tr("I'm not sure if this is normal or a bug:"),
      tr("My steering wheel buttons aren't working"),
      tr("openpilot disengages when I don't expect it"),
      tr("openpilot feels sluggish or slow to respond"),
      tr("Something else (please describe)")
    };

    if (QFile::exists("/data/error_logs/error.txt")) {
      report_messages.prepend(tr("I saw an alert that said \"openpilot crashed\""));
    }

    QString selected_issue = MultiOptionDialog::getSelection(tr("What's going on?"), report_messages, "", this);
    if (selected_issue.isEmpty()) {
      return;
    }

    if (selected_issue.contains("crashed") || selected_issue.contains("not sure") || selected_issue.contains("Something else")) {
      QString extra_input = InputDialog::getText(tr("Please describe what's happening"), this, tr("Send Report"), false, 10, "", 300).trimmed();
      if (extra_input.isEmpty()) {
        return;
      }
      selected_issue += " — " + extra_input;
    }

    QString discord_user = InputDialog::getText(tr("What's your Discord username?"), this, tr("Send Report"), false, -1, QString::fromStdString(params.get("DiscordUsername"))).trimmed();

    QJsonObject reportData;
    reportData["DiscordUser"] = discord_user;
    reportData["Issue"] = selected_issue;

    params.putNonBlocking("DiscordUsername", discord_user.toStdString());
    params_memory.put("IssueReported", QJsonDocument(reportData).toJson(QJsonDocument::Compact).toStdString());

    ConfirmationDialog::alert(tr("Report Sent! Thanks for letting us know!"), this);
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

    if (ConfirmationDialog::confirm(tr("Reset every FrogPilot setting to its default? You will have to accept the terms, redo the training and set your language again."), tr("Reset"), this)) {
      actionRunning = true;

      std::thread([parent, resetTogglesButton, this]() {
        runOnUIThread(resetTogglesButton, [parent, resetTogglesButton]() {
          parent->keepScreenOn = true;

          resetTogglesButton->setEnabled(false);
          resetTogglesButton->setValue(tr("Resetting..."));
        });

        std::vector<std::string> all_keys = params.allKeys();
        for (const std::string &key : all_keys) {
          if (excluded_keys.count(key)) {
            continue;
          }
          std::optional<std::string> default_value = params.getKeyDefaultValue(key);
          if (default_value.has_value()) {
            params.put(key, default_value.value());
          }
        }

        updateFrogPilotToggles();

        runOnUIThread(resetTogglesButton, [resetTogglesButton]() {
          resetTogglesButton->setValue(tr("Reset!"));
        });

        util::sleep_for(2500);

        runOnUIThread(resetTogglesButton, [parent, resetTogglesButton, this]() {
          parent->updateMetric(params.getBool("IsMetric"), true);

          resetTogglesButton->setEnabled(true);
          resetTogglesButton->setValue("");

          parent->keepScreenOn = false;
          actionRunning = false;
        });
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

    if (ConfirmationDialog::confirm(tr("Reset every setting to match stock openpilot? You will have to accept the terms, redo the training and set your language again."), tr("Reset"), this)) {
      actionRunning = true;

      std::thread([parent, resetTogglesButtonStock, this]() {
        runOnUIThread(resetTogglesButtonStock, [parent, resetTogglesButtonStock]() {
          parent->keepScreenOn = true;

          resetTogglesButtonStock->setEnabled(false);
          resetTogglesButtonStock->setValue(tr("Resetting..."));
        });

        std::vector<std::string> all_keys = params.allKeys();
        for (const std::string &key : all_keys) {
          if (excluded_keys.count(key)) {
            continue;
          }
          std::optional<std::string> stock_value = params.getStockValue(key);
          if (stock_value.has_value()) {
            params.put(key, stock_value.value());
          }
        }

        updateFrogPilotToggles();

        runOnUIThread(resetTogglesButtonStock, [resetTogglesButtonStock]() {
          resetTogglesButtonStock->setValue(tr("Reset!"));
        });

        util::sleep_for(2500);

        runOnUIThread(resetTogglesButtonStock, [parent, resetTogglesButtonStock, this]() {
          parent->updateMetric(params.getBool("IsMetric"), true);

          resetTogglesButtonStock->setEnabled(true);
          resetTogglesButtonStock->setValue("");

          parent->keepScreenOn = false;
          actionRunning = false;
        });
      }).detach();
    }
  });
  if (forceOpenDescriptions) {
    resetTogglesButtonStock->showDescription();
  }
  addItem(resetTogglesButtonStock);
}
