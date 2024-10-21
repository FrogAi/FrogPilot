#include "selfdrive/frogpilot/ui/qt/offroad/sounds_settings.h"

FrogPilotSoundsPanel::FrogPilotSoundsPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> soundsToggles {
    {"AlertVolumeControl", tr("Alert Volume Controller"), tr("Control the volume level for each individual sound in openpilot."), "../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"DisengageVolume", tr("Disengage Volume"), tr("Related alerts:\n\nAdaptive Cruise Disabled\nParking Brake Engaged\nBrake Pedal Pressed\nSpeed too Low"), ""},
    {"EngageVolume", tr("Engage Volume"), tr("Related alerts:\n\nNNFF Torque Controller loaded\nopenpilot engaged"), ""},
    {"PromptVolume", tr("Prompt Volume"), tr("Related alerts:\n\nCar Detected in Blindspot\nSpeed too Low\nSteer Unavailable Below 'X'\nTake Control, Turn Exceeds Steering Limit"), ""},
    {"PromptDistractedVolume", tr("Prompt Distracted Volume"), tr("Related alerts:\n\nPay Attention, Driver Distracted\nTouch Steering Wheel, Driver Unresponsive"), ""},
    {"RefuseVolume", tr("Refuse Volume"), tr("Related alerts:\n\nopenpilot Unavailable"), ""},
    {"WarningSoftVolume", tr("Warning Soft Volume"), tr("Related alerts:\n\nBRAKE!, Risk of Collision\nTAKE CONTROL IMMEDIATELY"), ""},
    {"WarningImmediateVolume", tr("Warning Immediate Volume"), tr("Related alerts:\n\nDISENGAGE IMMEDIATELY, Driver Distracted\nDISENGAGE IMMEDIATELY, Driver Unresponsive"), ""},

    {"CustomAlerts", tr("Custom Alerts"), tr("Custom alerts for openpilot events."), "../frogpilot/assets/toggle_icons/icon_green_light.png"},
    {"GoatScream", tr("Goat Scream Steering Saturated Alert"), tr("Enables the famed 'Goat Scream' that has brought both joy and anger to FrogPilot users all around the world!"), ""},
    {"GreenLightAlert", tr("Green Light Alert"), tr("Plays an alert when a traffic light changes from red to green."), ""},
    {"LeadDepartingAlert", tr("Lead Departing Alert"), tr("Plays an alert when the lead vehicle starts departing when at a standstill."), ""},
    {"LoudBlindspotAlert", tr("Loud Blindspot Alert"), tr("Plays a louder alert for when a vehicle is detected in the blindspot when attempting to change lanes."), ""},
    {"SpeedLimitChangedAlert", tr("Speed Limit Change Alert"), tr("Plays an alert when the speed limit changes."), ""},
  };

  for (const auto &[param, title, desc, icon] : soundsToggles) {
    AbstractControl *soundsToggle;

    if (param == "AlertVolumeControl") {
      FrogPilotParamManageControl *alertVolumeControlToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(alertVolumeControlToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        showToggles(alertVolumeControlKeys);
      });
      soundsToggle = alertVolumeControlToggle;
    } else if (alertVolumeControlKeys.find(param) != alertVolumeControlKeys.end()) {
      std::map<int, QString> volumeLabels;
      for (int i = 0; i <= 101; ++i) {
        volumeLabels[i] = i == 101 ? tr("Auto") : i == 0 ? tr("Muted") : QString::number(i) + "%";
      }
      if (param == "WarningImmediateVolume") {
        soundsToggle = new FrogPilotParamValueControl(param, title, desc, icon, 25, 101, QString(), volumeLabels);
      } else {
        soundsToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 101, QString(), volumeLabels);
      }

    } else if (param == "CustomAlerts") {
      FrogPilotParamManageControl *customAlertsToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(customAlertsToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedCustomAlertsKeys = customAlertsKeys;

        if (!hasBSM) {
          modifiedCustomAlertsKeys.erase("LoudBlindspotAlert");
        }

        if (!(hasOpenpilotLongitudinal && params.getBool("SpeedLimitController"))) {
          modifiedCustomAlertsKeys.erase("SpeedLimitChangedAlert");
        }

        showToggles(modifiedCustomAlertsKeys);
      });
      soundsToggle = customAlertsToggle;

    } else {
      soundsToggle = new ParamControl(param, title, desc, icon);
    }

    addItem(soundsToggle);
    toggles[param] = soundsToggle;

    makeConnections(soundsToggle);

    if (FrogPilotParamManageControl *frogPilotManageToggle = qobject_cast<FrogPilotParamManageControl*>(soundsToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotParamManageControl::manageButtonClicked, this, &FrogPilotSoundsPanel::openParentToggle);
    }

    QObject::connect(soundsToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, this, &FrogPilotSoundsPanel::hideToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::updateCarToggles, this, &FrogPilotSoundsPanel::updateCarToggles);
}

void FrogPilotSoundsPanel::showEvent(QShowEvent *event) {
  customizationLevel = parent->customizationLevel;

  toggles["AlertVolumeControl"]->setVisible(customizationLevel == 2);
}

void FrogPilotSoundsPanel::updateCarToggles() {
  hasBSM = parent->hasBSM;
  hasOpenpilotLongitudinal = parent->hasOpenpilotLongitudinal;

  hideToggles();
}

void FrogPilotSoundsPanel::showToggles(const std::set<QString> &keys) {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(keys.find(key) != keys.end());
  }

  setUpdatesEnabled(true);
  update();
}

void FrogPilotSoundsPanel::hideToggles() {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    bool subToggles = alertVolumeControlKeys.find(key) != alertVolumeControlKeys.end() ||
                      customAlertsKeys.find(key) != customAlertsKeys.end();
    toggle->setVisible(!subToggles);
  }

  toggles["AlertVolumeControl"]->setVisible(customizationLevel == 2);

  setUpdatesEnabled(true);
  update();
}
