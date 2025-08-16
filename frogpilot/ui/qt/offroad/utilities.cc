#include "frogpilot/ui/qt/offroad/utilities.h"

FrogPilotUtilitiesPanel::FrogPilotUtilitiesPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  ParamControl *debugModeToggle = new ParamControl("DebugMode", tr("Debug Mode"), tr("Debug FrogPilot during the next drive by utilizing all of FrogPilot's developer metrics for either bug reporting, or self-debugging."), "");
  addItem(debugModeToggle);

  ButtonControl *flashPandaBtn = new ButtonControl(tr("Flash Panda"), tr("FLASH"), tr("Flash the Panda's firmware. Use if you're running into issues with the Panda."));
  QObject::connect(flashPandaBtn, &ButtonControl::clicked, [this, parent, flashPandaBtn]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to flash the Panda?"), tr("Flash"), this)) {
      std::thread([this, parent, flashPandaBtn]() {
        parent->keepScreenOn = true;

        flashPandaBtn->setEnabled(false);
        flashPandaBtn->setValue(tr("Flashing..."));

        params_memory.putBool("FlashPanda", true);
        while (params_memory.getBool("FlashPanda")) {
          util::sleep_for(UI_FREQ);
        }

        flashPandaBtn->setValue(tr("Flashed!"));

        util::sleep_for(2500);

        flashPandaBtn->setValue(tr("Rebooting..."));

        util::sleep_for(2500);

        Hardware::reboot();
      }).detach();
    }
  });
  addItem(flashPandaBtn);

  FrogPilotButtonsControl *forceStartedBtn = new FrogPilotButtonsControl(tr("Force Started State"), tr("Force openpilot either offroad or onroad."), "", {tr("OFFROAD"), tr("ONROAD"), tr("OFF")}, true);
  QObject::connect(forceStartedBtn, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 0) {
      params_memory.putBool("ForceOffroad", true);
      params_memory.putBool("ForceOnroad", false);

      updateFrogPilotToggles();
    } else if (id == 1) {
      params_memory.putBool("ForceOffroad", false);
      params_memory.putBool("ForceOnroad", true);

      params.put("CarParams", params.get("CarParamsPersistent"));
      params.put("FrogPilotCarParams", params.get("FrogPilotCarParamsPersistent"));

      updateFrogPilotToggles();

      while (!params.get("CarParams").empty()) {
        util::sleep_for(UI_FREQ);
      }

      params.put("CarParams", params.get("CarParamsPersistent"));
      params.put("FrogPilotCarParams", params.get("FrogPilotCarParamsPersistent"));
    } else if (id == 2) {
      params_memory.putBool("ForceOffroad", false);
      params_memory.putBool("ForceOnroad", false);

      updateFrogPilotToggles();
    }
  });
  forceStartedBtn->setCheckedButton(2);
  addItem(forceStartedBtn);

  ButtonControl *reportIssueBtn = new ButtonControl(tr("Report a Bug or an Issue"), tr("REPORT"), tr("Let FrogsGoMoo know about an issue you're facing!"));
  QObject::connect(reportIssueBtn, &ButtonControl::clicked, [this]() {
    if (!frogpilotUIState()->frogpilot_scene.online) {
      ConfirmationDialog::alert(tr("Ensure your device has an internet connection before sending a report!"), this);
      return;
    }

    QStringList report_messages;
    QString crash_report = tr("I saw an alert that said \"openpilot crashed\"");
    if (QFile::exists("/data/error_logs/error.txt")) {
      report_messages << crash_report;
    }
    QStringList additional_issues = {
      tr("Acceleration feels too harsh or jerky"),
      tr("An alert confused me and I didn’t know what it meant"),
      tr("Braking is too sudden or uncomfortable"),
      tr("I’m not sure if this is normal or a bug:"),
      tr("Lane changes don’t work properly or feel unsafe"),
      tr("My screen froze or got stuck on loading"),
      tr("My steering wheel buttons aren’t working"),
      tr("openpilot disengages when I don't expect it"),
      tr("openpilot doesn’t resume after I stop"),
      tr("openpilot doesn't react to stopped vehicles ahead"),
      tr("openpilot feels sluggish or slow to respond"),
      tr("Steering feels twitchy or unnatural"),
      tr("The car doesn’t follow curves well"),
      tr("The car isn’t staying centered in its lane"),
      tr("The speed or display info looks wrong"),
      tr("Something else (please describe)")
    };
    report_messages.append(additional_issues);

    QMap<QString, bool> needs_extra_input;
    for (const QString &issue : report_messages) {
      if (issue.contains("confused") ||
          issue.contains("crashed") ||
          issue.contains("not sure") ||
          issue.contains("Something else")) {
        needs_extra_input[issue] = true;
      }
    }

    QString selected_issue = MultiOptionDialog::getSelection(tr("What's going on?"), report_messages, "", this);
    if (selected_issue.isEmpty()) {
      return;
    }

    if (needs_extra_input.value(selected_issue, false)) {
      QString extra_input = InputDialog::getText(tr("Please describe what's happening"), this, tr("Send Report"), false, 10, "", 300).trimmed();
      if (extra_input.isEmpty()) {
        return;
      }
      selected_issue += " — " + extra_input;
    }

    QJsonObject reportData;
    reportData["Issue"] = selected_issue;
    reportData["DiscordUser"] = InputDialog::getText(tr("What's your Discord username?"), this, tr("Send Report"), false, -1, QString::fromStdString(params.get("DiscordUsername"))).trimmed();

    params.putNonBlocking("DiscordUsername", reportData["DiscordUser"].toString().toStdString());
    params_memory.put("IssueReported", QJsonDocument(reportData).toJson(QJsonDocument::Compact).toStdString());

    ConfirmationDialog::alert(tr("Your report has been submitted. Thanks for letting us know!"), this);
  });
  addItem(reportIssueBtn);
  reportIssueBtn->setVisible(QString::fromStdString(params.get("GitRemote")).toLower() == "https://github.com/frogai/openpilot.git");

  ButtonControl *resetTogglesBtn = new ButtonControl(tr("Reset Toggles to Default"), tr("RESET"), tr("Reset all toggles to their default values."));
  QObject::connect(resetTogglesBtn, &ButtonControl::clicked, [this, parent, resetTogglesBtn]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to reset all toggles to their default values?"), tr("Reset"), this)) {
      std::thread([this, parent, resetTogglesBtn]() mutable {
        parent->keepScreenOn = true;

        resetTogglesBtn->setEnabled(false);
        resetTogglesBtn->setValue(tr("Resetting..."));

        params.putBool("DoToggleReset", true);

        resetTogglesBtn->setValue(tr("Reset!"));

        util::sleep_for(2500);

        resetTogglesBtn->setValue(tr("Rebooting..."));

        util::sleep_for(2500);

        Hardware::reboot();
      }).detach();
    }
  });
  addItem(resetTogglesBtn);

  ButtonControl *resetTogglesBtnStock = new ButtonControl(tr("Reset Toggles to Match Stock openpilot"), tr("RESET"), tr("Reset all toggles to match stock openpilot."));
  QObject::connect(resetTogglesBtnStock, &ButtonControl::clicked, [this, parent, resetTogglesBtnStock]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to reset all toggles to match stock openpilot?"), tr("Reset"), this)) {
      std::thread([this, parent, resetTogglesBtnStock]() mutable {
        parent->keepScreenOn = true;

        resetTogglesBtnStock->setEnabled(false);
        resetTogglesBtnStock->setValue(tr("Resetting..."));

        params.putBool("DoToggleResetStock", true);

        resetTogglesBtnStock->setValue(tr("Reset!"));

        util::sleep_for(2500);

        resetTogglesBtnStock->setValue(tr("Rebooting..."));

        util::sleep_for(2500);

        Hardware::reboot();
      }).detach();
    }
  });
  addItem(resetTogglesBtnStock);
}
