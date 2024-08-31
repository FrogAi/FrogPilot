#include "selfdrive/ui/ui.h"

Params paramsMemory{"/dev/shm/params"};

std::atomic<int> callCounter(0);

void updateFrogPilotToggles() {
  int currentCall = ++callCounter;
  std::thread([currentCall]() {
    paramsMemory.putBool("FrogPilotTogglesUpdated", true);
    util::sleep_for(1000);
    if (currentCall == callCounter) {
      paramsMemory.putBool("FrogPilotTogglesUpdated", false);
    }
  }).detach();
}

QColor loadThemeColors(const QString &colorKey) {
  QFile file("../frogpilot/assets/active_theme/colors/colors.json");
  if (!file.open(QIODevice::ReadOnly)) return QColor();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
  file.close();

  if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return QColor();

  QJsonObject colorObj = doc.object().value(colorKey).toObject();
  int red = colorObj["red"].toInt();
  int green = colorObj["green"].toInt();
  int blue = colorObj["blue"].toInt();
  int alpha = colorObj["alpha"].toInt();

  return QColor(red, green, blue, alpha);
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
