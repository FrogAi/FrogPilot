#include "selfdrive/frogpilot/ui/qt/offroad/wheel_settings.h"

FrogPilotWheelPanel::FrogPilotWheelPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> wheelToggles {
    {"DistanceButtonControl", tr("Distance Button"), tr("The action for a short press of the <b>Distance</b> button."), "../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"LongDistanceButtonControl", tr("Distance Button (Long Press)"), tr("The action for a 0.5+ second press of the <b>Distance</b> button."), "../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"VeryLongDistanceButtonControl", tr("Distance Button (Very Long Press)"), tr("The action for a 2.5+ second press of the <b>Distance</b> button."), "../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"LKASButtonControl", tr("LKAS Button"), tr("The action for pressing the <b>LKAS</b> button."), "../frogpilot/assets/toggle_icons/icon_mute.png"}
  };

  for (const auto &[param, title, desc, icon] : wheelToggles) {
    QMap<int, QString> functionsMap {
      {0, tr("Nothing")},
      {3, tr("Pause Lateral")}
    };

    QMap<int, QString> longitudinalFunctionsMap {
      {1, tr("Change \"Personality Profile\"")},
      {2, tr("Force openpilot to Coast")},
      {4, tr("Pause Longitudinal")},
      {5, tr("Toggle \"Experimental Mode\" On/Off")},
      {6, tr("Toggle \"Traffic Mode\" On/Off")}
    };

    ButtonControl *wheelToggle = new ButtonControl(title, tr("SELECT"), desc);
    QObject::connect(wheelToggle, &ButtonControl::clicked, [this, functionsMap, longitudinalFunctionsMap, key = param, wheelToggle]() mutable {
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

    QObject::connect(wheelToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }
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

  update();
}
