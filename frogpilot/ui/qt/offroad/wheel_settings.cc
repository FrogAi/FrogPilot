#include "frogpilot/ui/qt/offroad/wheel_settings.h"

FrogPilotWheelPanel::FrogPilotWheelPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

  const std::vector<std::tuple<QString, QString, QString>> wheelToggles {
    {"DistanceButtonControl", tr("Distance Button"), tr("<b>Action performed when the \"Distance\" button is pressed.</b>")},
    {"LongDistanceButtonControl", tr("Distance Button (Long Press)"), tr("<b>Action performed when the \"Distance\" button is pressed for more than 0.5 seconds.</b><br><br>On GM cars the hold is 0.75 seconds instead.")},
    {"VeryLongDistanceButtonControl", tr("Distance Button (Very Long Press)"), tr("<b>Action performed when the \"Distance\" button is pressed for more than 2.5 seconds.</b>")},
    {"LKASButtonControl", tr("LKAS Button"), tr("<b>Action performed when the \"LKAS\" button is pressed.</b>")}
  };

  for (const auto &[param, title, desc] : wheelToggles) {
    ButtonControl *wheelToggle = new ButtonControl(title, tr("SELECT"), desc);
    QObject::connect(wheelToggle, &ButtonControl::clicked, [key = param, wheelToggle, this]() {
      QMap<int, QString> functionsMap = buttonFunctions();
      QString currentFunction = functionsMap.value(params.getInt(key.toStdString()), functionsMap.value(0));

      QString selection = MultiOptionDialog::getSelection(tr("Select a function to assign to this button"), functionsMap.values(), currentFunction, this);
      if (!selection.isEmpty()) {
        params.putInt(key.toStdString(), functionsMap.key(selection));

        wheelToggle->setValue(selection);
      }
    });

    toggles[param] = wheelToggle;

    addItem(wheelToggle);

    QObject::connect(wheelToggle, &AbstractControl::hideDescriptionEvent, [this]() {
      update();
    });
    QObject::connect(wheelToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  updateButtonValues();

  openDescriptions(forceOpenDescriptions, toggles);
}

QMap<int, QString> FrogPilotWheelPanel::buttonFunctions() {
  QMap<int, QString> functionsMap {
    {0, tr("No Action")},
    {3, tr("Pause Steering")}
  };

  if (parent->hasOpenpilotLongitudinal) {
    functionsMap.insert(1, tr("Change \"Personality Profile\""));
    functionsMap.insert(2, tr("Force openpilot to Coast"));
    functionsMap.insert(4, tr("Pause Acceleration/Braking"));
    functionsMap.insert(5, tr("Toggle \"Experimental Mode\" On/Off"));
    functionsMap.insert(6, tr("Toggle \"Traffic Mode\" On/Off"));
  }

  return functionsMap;
}

void FrogPilotWheelPanel::updateButtonValues() {
  QMap<int, QString> functionsMap = buttonFunctions();

  for (auto &[key, toggle] : toggles) {
    static_cast<ButtonControl*>(toggle)->setValue(functionsMap.value(params.getInt(key.toStdString()), functionsMap.value(0)));
  }
}

void FrogPilotWheelPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;

  updateToggles();
}

void FrogPilotWheelPanel::updateToggles() {
  for (auto &[key, toggle] : toggles) {
    bool setVisible = parent->tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (key == "LKASButtonControl") {
      setVisible &= !parent->isSubaru;
      setVisible &= !(parent->useLkasForAOL && params.getBool("AlwaysOnLateral") && params.getBool("AlwaysOnLateralLKAS"));
    }

    toggle->setVisible(setVisible);
  }

  updateButtonValues();

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
