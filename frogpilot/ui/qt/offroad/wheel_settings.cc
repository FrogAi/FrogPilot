#include "frogpilot/ui/qt/offroad/wheel_settings.h"

FrogPilotWheelPanel::FrogPilotWheelPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  const std::vector<std::tuple<QString, QString, QString>> wheelToggles {
    {"DistanceButtonControl", tr("Distance Button"), tr("<b>Action performed when the \"Distance\" button is pressed.</b><br><br>On GM cars with adaptive cruise, changing the \"Personality Profile\" takes a second press within 3.5 seconds of the first.")},
    {"LongDistanceButtonControl", tr("Distance Button (Long Press)"), tr("<b>Action performed when the \"Distance\" button is pressed for more than 0.5 seconds.</b><br><br>On GM cars the hold is 0.75 seconds instead.")},
    {"VeryLongDistanceButtonControl", tr("Distance Button (Very Long Press)"), tr("<b>Action performed when the \"Distance\" button is pressed for more than 2.5 seconds.</b>")},
    {"LKASButtonControl", tr("LKAS Button"), tr("<b>Action performed when the \"LKAS\" button is pressed.</b>")}
  };

  for (const auto &[param, title, desc] : wheelToggles) {
    ButtonControl *wheelToggle = new ButtonControl(title, tr("SELECT"), desc);

    toggles[param] = wheelToggle;

    addItem(wheelToggle);
  }
}

void FrogPilotWheelPanel::showEvent(QShowEvent *event) {
  updateToggles();
}

void FrogPilotWheelPanel::updateToggles() {
  for (auto &[key, toggle] : toggles) {
    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels.value(key).toDouble();

    if (key == "LKASButtonControl") {
      setVisible &= parent->hasLKASButton;
      bool lkasRequiredForLateral = parent->isHKGCanFd && !parent->hasOpenpilotLongitudinal;
      setVisible &= !parent->lkasAllowedForAOL || !(params.getBool("AlwaysOnLateral") && (lkasRequiredForLateral || params.getBool("AlwaysOnLateralLKASButton")));
    } else {
      setVisible &= parent->hasDistanceButton;
    }

    toggle->setVisible(setVisible);
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
