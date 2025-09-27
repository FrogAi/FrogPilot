#include "frogpilot/ui/qt/offroad/wheel_settings.h"

FrogPilotWheelPanel::FrogPilotWheelPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

  const std::vector<std::tuple<QString, QString, QString, QString>> wheelToggles {
    {"DistanceButtonControl", tr("Distance Button"), tr("<b>Action performed when the \"Distance\" button is pressed.</b>"), "../../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"LongDistanceButtonControl", tr("Distance Button (Long Press)"), tr("<b>Action performed when the \"Distance\" button is pressed for more than 0.5 seconds.</b>"), "../../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"VeryLongDistanceButtonControl", tr("Distance Button (Very Long Press)"), tr("<b>Action performed when the \"Distance\" button is pressed for more than 2.5 seconds.</b>"), "../../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"LKASButtonControl", tr("LKAS Button"), tr("<b>Action performed when the \"LKAS\" button is pressed.</b>"), "../../frogpilot/assets/toggle_icons/icon_mute.png"}
  };

  for (const auto &[param, title, desc, icon] : wheelToggles) {
    QMap<int, QString> functionsMap {
      {0, tr("No Action")},
      {3, tr("Pause Steering")}
    };

    QMap<int, QString> longitudinalFunctionsMap {
      {1, tr("Change \"Personality Profile\"")},
      {2, tr("Force openpilot to Coast")},
      {4, tr("Pause Acceleration/Braking")},
      {5, tr("Toggle \"Experimental Mode\" On/Off")},
      {6, tr("Toggle \"Traffic Mode\" On/Off")}
    };

    ButtonControl *wheelToggle = new ButtonControl(title, tr("SELECT"), desc);
    QObject::connect(wheelToggle, &ButtonControl::clicked, [functionsMap, longitudinalFunctionsMap, key = param, wheelToggle, this]() mutable {
      if (hasOpenpilotLongitudinal) {
        QMap<int, QString>::const_iterator it;
        for (it = longitudinalFunctionsMap.constBegin(); it != longitudinalFunctionsMap.constEnd(); ++it) {
          functionsMap[it.key()] = it.value();
        }
      }

      QString selection = MultiOptionDialog::getSelection(tr("Select a function to assign to this button"), functionsMap.values(), functionsMap[params.getInt(key.toStdString())], this);
      if (!selection.isEmpty()) {
        params.putInt(key.toStdString(), functionsMap.key(selection));

        wheelToggle->setValue(selection);
      }
    });
    QMap<int, QString> mergedFunctionsMap = functionsMap;
    QMap<int, QString>::const_iterator it;
    for (it = longitudinalFunctionsMap.constBegin(); it != longitudinalFunctionsMap.constEnd(); ++it) {
      mergedFunctionsMap[it.key()] = it.value();
    }
    wheelToggle->setValue(mergedFunctionsMap[params.getInt(param.toStdString())]);

    toggles[param] = wheelToggle;

    addItem(wheelToggle);

    QObject::connect(wheelToggle, &AbstractControl::hideDescriptionEvent, [this]() {
      update();
    });
    QObject::connect(wheelToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  openDescriptions(forceOpenDescriptions, toggles);
}

void FrogPilotWheelPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  hasOpenpilotLongitudinal = parent->hasOpenpilotLongitudinal;
  isSubaru = parent->isSubaru;
  tuningLevel = parent->tuningLevel;

  updateToggles();
}

void FrogPilotWheelPanel::updateToggles() {
  for (auto &[key, toggle] : toggles) {
    bool setVisible = tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (key == "LKASButtonControl") {
      setVisible &= !isSubaru;
      setVisible &= !(params.getBool("AlwaysOnLateral") && params.getBool("AlwaysOnLateralLKAS"));
    }

    toggle->setVisible(setVisible);
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
