#include "frogpilot/ui/qt/offroad/utilities.h"

FrogPilotUtilitiesPanel::FrogPilotUtilitiesPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  ParamControl *debugModeToggle = new ParamControl("DebugMode", tr("Debug Mode"), tr("<b>Show FrogPilot's developer readouts on the driving screen for your next drive, so a bug report can say what openpilot was actually doing.</b><br><br>It switches itself back off once you finish the drive. While it is on, the temperature reads in Celsius and the developer numbers read in scientific units, whatever you picked elsewhere. Your speedometer is not affected."), "");
  if (forceOpenDescriptions) {
    debugModeToggle->showDescription();
  }
  addItem(debugModeToggle);

  ButtonControl *flashPandaButton = new ButtonControl(tr("Flash Panda"), tr("FLASH"), tr("<b>Reinstall the software on the Panda, the small box that lets your device talk to your car.</b><br><br>Try this if openpilot keeps losing contact with the car or the Panda shows up as faulty. Your device reboots once it finishes, and the car has to be off to start."));
  QObject::connect(flashPandaButton, &ButtonControl::clicked, [parent, flashPandaButton, this]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to flash the Panda firmware?"), tr("Flash"), this)) {
      std::thread([parent, flashPandaButton, this]() {
        runOnUIThread(flashPandaButton, [parent, flashPandaButton]() {
          parent->keepScreenOn = true;

          flashPandaButton->setEnabled(false);
          flashPandaButton->setValue(tr("Flashing..."));
        });

        params_memory.putBool("FlashPanda", true);
        while (params_memory.getBool("FlashPanda")) {
          util::sleep_for(UI_FREQ);
        }

        runOnUIThread(flashPandaButton, [flashPandaButton]() {
          flashPandaButton->setValue(tr("Flashed!"));
        });

        util::sleep_for(2500);

        runOnUIThread(flashPandaButton, [flashPandaButton]() {
          flashPandaButton->setValue(tr("Rebooting..."));
        });

        util::sleep_for(2500);

        Hardware::reboot();
      }).detach();
    }
  });
  if (forceOpenDescriptions) {
    flashPandaButton->showDescription();
  }
  addItem(flashPandaButton);

  FrogPilotButtonsControl *forceStartedButton = new FrogPilotButtonsControl(tr("Force Drive State"), tr("<b>Make openpilot behave as though the car is running, or as though it is parked, without the car actually being either.</b><br><br>This is a testing tool. Forcing the running state pins the screen to full brightness and stops openpilot warning you that its controls are unresponsive, so leave it on \"OFF\" unless you know why you need it. It clears itself the next time the device restarts."), "", {tr("OFFROAD"), tr("ONROAD"), tr("OFF")}, true);
  QObject::connect(forceStartedButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 0) {
      params.putBool("ForceOffroad", true);
      params.putBool("ForceOnroad", false);

      updateFrogPilotToggles();
    } else if (id == 1) {
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
  reportIssueButton->setVisible(QString::fromStdString(params.get("GitRemote")).toLower() == "https://github.com/frogai/openpilot.git");

  ButtonControl *resetTogglesButton = new ButtonControl(tr("Reset Toggles to Default"), tr("RESET"), tr("<b>Put every FrogPilot setting back to the value it shipped with.</b><br><br>This also clears your accepted terms, your completed training and your language, so you go through first-time setup again in English. The reset happens while the device reboots, and your drives, backups and downloaded themes are left alone."));
  QObject::connect(resetTogglesButton, &ButtonControl::clicked, [parent, resetTogglesButton, this]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to reset all toggles to their default values?"), tr("Reset"), this)) {
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

        runOnUIThread(resetTogglesButton, [resetTogglesButton]() {
          resetTogglesButton->setValue("");
        });
      }).detach();
    }
  });
  if (forceOpenDescriptions) {
    resetTogglesButton->showDescription();
  }
  addItem(resetTogglesButton);

  ButtonControl *resetTogglesButtonStock = new ButtonControl(tr("Reset Toggles to Stock openpilot"), tr("RESET"), tr("<b>Put every setting back to what plain openpilot uses, turning FrogPilot's own features off rather than back to FrogPilot's defaults.</b><br><br>This also clears your accepted terms, your completed training and your language, so you go through first-time setup again in English. The reset happens while the device reboots, and your drives, backups and downloaded themes are left alone."));
  QObject::connect(resetTogglesButtonStock, &ButtonControl::clicked, [parent, resetTogglesButtonStock, this]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to reset all toggles to match stock openpilot?"), tr("Reset"), this)) {
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

        runOnUIThread(resetTogglesButtonStock, [resetTogglesButtonStock]() {
          resetTogglesButtonStock->setValue("");
        });
      }).detach();
    }
  });
  if (forceOpenDescriptions) {
    resetTogglesButtonStock->showDescription();
  }
  addItem(resetTogglesButtonStock);
}
