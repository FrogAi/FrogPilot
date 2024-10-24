#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "selfdrive/ui/ui.h"

void updateFrogPilotToggles() {
  static Params paramsMemory{"/dev/shm/params"};
  static std::atomic<bool> isUpdating(false);
  static std::thread resetThread;

  bool expected = false;
  if (!isUpdating.compare_exchange_strong(expected, true)) {
    return;
  }

  if (resetThread.joinable()) {
    resetThread.join();
  }

  paramsMemory.putBool("FrogPilotTogglesUpdated", true);

  resetThread = std::thread([&]() {
    util::sleep_for(1000);
    paramsMemory.putBool("FrogPilotTogglesUpdated", false);

    isUpdating.store(false);
  });
}

QColor loadThemeColors(const QString &colorKey, bool clearCache) {
  static QJsonObject cachedColorData;

  if (clearCache) {
    cachedColorData = QJsonObject();
    return QColor();
  }

  if (cachedColorData.isEmpty()) {
    QFile file("../frogpilot/assets/active_theme/colors/colors.json");
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
      QJsonParseError parseError;
      QByteArray fileData = file.readAll();
      QJsonDocument doc = QJsonDocument::fromJson(fileData, &parseError);

      if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        cachedColorData = doc.object();
      }
    }
  }

  if (!cachedColorData.contains(colorKey)) {
    return QColor();
  }

  QJsonObject colorObj = cachedColorData.value(colorKey).toObject();
  return QColor(
    colorObj.value("red").toInt(0),
    colorObj.value("green").toInt(0),
    colorObj.value("blue").toInt(0),
    colorObj.value("alpha").toInt(255)
  );
}

bool FrogPilotConfirmationDialog::toggle(const QString &prompt_text, const QString &confirm_text, QWidget *parent) {
  ConfirmationDialog d(prompt_text, confirm_text, tr("Reboot Later"), false, parent);
  return d.exec();
}

bool FrogPilotConfirmationDialog::toggleAlert(const QString &prompt_text, const QString &button_text, QWidget *parent) {
  ConfirmationDialog d(prompt_text, button_text, "", false, parent);
  return d.exec();
}

bool FrogPilotConfirmationDialog::yesorno(const QString &prompt_text, QWidget *parent) {
  ConfirmationDialog d(prompt_text, tr("Yes"), tr("No"), false, parent);
  return d.exec();
}
