#include "selfdrive/frogpilot/ui/qt/offroad/utilities.h"

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
    } else if (id == 1) {
      params_memory.putBool("ForceOffroad", false);
      params_memory.putBool("ForceOnroad", true);

      util::sleep_for(1000);

      params.put("CarParams", params.get("CarParamsPersistent"));
    } else if (id == 2) {
      params_memory.remove("ForceOffroad");
      params_memory.remove("ForceOnroad");
    }
  });
  forceStartedBtn->setCheckedButton(2);
  addItem(forceStartedBtn);

  ButtonControl *reportIssueBtn = new ButtonControl(tr("Report a Bug or an Issue"), tr("REPORT"), tr("Let FrogsGoMoo know about an issue you're facing!"));
  QObject::connect(reportIssueBtn, &ButtonControl::clicked, [this]() {
    QStringList report_messages = {
      tr("I saw an alert that said <b>openpilot crashed</b>"),
      tr("I'm noticing harsh acceleration"),
      tr("I'm noticing harsh braking"),
      tr("I'm noticing unusual steering"),
      tr("My car isn't staying in its lane"),
      tr("Something else")
    };

    QString selected_issue = MultiOptionDialog::getSelection(tr("What's going on?"), report_messages, "", this);
    if (selected_issue.isEmpty()) {
      return;
    } else if (selected_issue == "Something else") {
      selected_issue = InputDialog::getText(tr("Please describe what's happening"), this, tr("Send Report"), false, 10, "", 100).trimmed();
      if (selected_issue.isEmpty()) {
        return;
      }
    }

    QJsonObject reportData;
    reportData["Issue"] = selected_issue;
    reportData["DiscordUser"] = InputDialog::getText(tr("What's your Discord username?"), this, tr("Send Report"), false, -1, QString::fromStdString(params.get("DiscordUsername"))).trimmed();

    params.putNonBlocking("DiscordUsername", reportData["DiscordUser"].toString().toStdString());
    params_memory.put("IssueReported", QJsonDocument(reportData).toJson(QJsonDocument::Compact).toStdString());

    ConfirmationDialog::alert(tr("Your report has been submitted. Thanks for letting us know!"), this);
  });
  addItem(reportIssueBtn);

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
}
