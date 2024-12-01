#include <filesystem>

#include "selfdrive/frogpilot/ui/qt/offroad/utilities.h"

UtilitiesPanel::UtilitiesPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent) {
  ButtonControl *flashPandaBtn = new ButtonControl(tr("Flash Panda"), tr("FLASH"), tr("Flashes the Panda device's firmware if you're running into issues."));
  QObject::connect(flashPandaBtn, &ButtonControl::clicked, [=]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to flash the Panda?"), tr("Flash"), this)) {
      std::thread([=]() {
        flashPandaBtn->setEnabled(false);
        flashPandaBtn->setValue(tr("Flashing..."));

        system("python3 /data/openpilot/panda/board/flash.py");
        system("python3 /data/openpilot/panda/board/recover.py");

        flashPandaBtn->setValue(tr("Flashed!"));
        util::sleep_for(2500);
        flashPandaBtn->setValue(tr("Rebooting..."));
        util::sleep_for(2500);
        Hardware::reboot();
      }).detach();
    }
  });
  addItem(flashPandaBtn);

  forceStartedBtn = new FrogPilotButtonsControl(tr("Force Started State"), tr("Forces openpilot either offroad or onroad."), {tr("OFFROAD"), tr("ONROAD"), tr("OFF")}, true);
  QObject::connect(forceStartedBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    if (id == 0) {
      params_memory.putBool("ForceOffroad", true);
      params_memory.putBool("ForceOnroad", false);
    } else if (id == 1) {
      params_memory.putBool("ForceOffroad", false);
      params_memory.putBool("ForceOnroad", true);
    } else if (id == 2) {
      params_memory.putBool("ForceOffroad", false);
      params_memory.putBool("ForceOnroad", false);
    }
    forceStartedBtn->setCheckedButton(id);
  });
  forceStartedBtn->setCheckedButton(2);
  addItem(forceStartedBtn);

  ButtonControl *resetTogglesBtn = new ButtonControl(tr("Reset Toggles to Default"), tr("RESET"), tr("Reset your toggle settings back to their default settings."));
  QObject::connect(resetTogglesBtn, &ButtonControl::clicked, [=]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to completely reset all of your toggle settings?"), tr("Reset"), this)) {
      std::thread([=]() mutable {
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
