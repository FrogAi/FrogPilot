#include <filesystem>

#include "selfdrive/ui/ui.h"

Params paramsMemory{"/dev/shm/params"};

void updateFrogPilotToggles() {
  std::thread([]() {
    paramsMemory.putBool("FrogPilotTogglesUpdated", true);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    paramsMemory.putBool("FrogPilotTogglesUpdated", false);
  }).detach();
}

bool FrogPilotConfirmationDialog::toggle(const QString &prompt_text, const QString &confirm_text, QWidget *parent) {
  ConfirmationDialog d = ConfirmationDialog(prompt_text, confirm_text, tr("Reboot Later"), false, parent);
  return d.exec();
}

bool FrogPilotConfirmationDialog::toggleAlert(const QString &prompt_text, const QString &button_text, QWidget *parent) {
  ConfirmationDialog d = ConfirmationDialog(prompt_text, button_text, "", false, parent);
  return d.exec();
}

bool FrogPilotConfirmationDialog::yesorno(const QString &prompt_text, QWidget *parent) {
  ConfirmationDialog d = ConfirmationDialog(prompt_text, tr("Yes"), tr("No"), false, parent);
  return d.exec();
}

FrogPilotButtonIconControl::FrogPilotButtonIconControl(const QString &title, const QString &text, const QString &desc, const QString &icon, QWidget *parent) : AbstractControl(title, desc, icon, parent) {
  btn.setText(text);
  btn.setStyleSheet(R"(
    QPushButton {
      padding: 0;
      border-radius: 50px;
      font-size: 35px;
      font-weight: 500;
      color: #E4E4E4;
      background-color: #393939;
    }
    QPushButton:pressed {
      background-color: #4a4a4a;
    }
    QPushButton:disabled {
      color: #33E4E4E4;
    }
  )");
  btn.setFixedSize(250, 100);
  QObject::connect(&btn, &QPushButton::clicked, this, &FrogPilotButtonIconControl::clicked);
  hlayout->addWidget(&btn);
}
