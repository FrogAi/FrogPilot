#include <filesystem>

#include "selfdrive/frogpilot/ui/qt/offroad/sounds_settings.h"

void playSound(const std::string &alert, int volume) {
  Params params_memory{"/dev/shm/params"};

  std::string stockPath = "/data/openpilot/selfdrive/assets/sounds/" + alert + ".wav";
  std::string themePath = "/data/openpilot/selfdrive/frogpilot/assets/active_theme/sounds/" + alert + ".wav";

  std::string filePath;
  if (std::filesystem::exists(themePath)) {
    filePath = themePath;
  } else if (std::filesystem::exists(stockPath)) {
    filePath = stockPath;
  } else {
    return;
  }

  params_memory.putBool("TestingSound", true);

  std::system("pkill -f 'ffplay'");

  volume = std::clamp(volume, 0, 100);
  std::string command = "ffplay -nodisp -autoexit -volume " + std::to_string(volume) + " \"" + filePath + "\"";
  std::system(command.c_str());

  params_memory.putBool("TestingSound", false);
}

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
    {"LeadDepartingAlert", tr("Lead Departing Alert"), tr("Plays an alert when the lead vehicle starts starts to depart when at a standstill."), ""},
    {"LoudBlindspotAlert", tr("Loud Blindspot Alert"), tr("Plays a louder alert for when a vehicle is detected in the blindspot when attempting to change lanes."), ""},
    {"SpeedLimitChangedAlert", tr("Speed Limit Changed Alert"), tr("Plays an alert when the speed limit changes."), ""},
  };

  for (const auto &[param, title, desc, icon] : soundsToggles) {
    AbstractControl *soundsToggle;

    if (param == "AlertVolumeControl") {
      FrogPilotManageControl *alertVolumeControlToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(alertVolumeControlToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        showToggles(alertVolumeControlKeys);
      });
      soundsToggle = alertVolumeControlToggle;
    } else if (alertVolumeControlKeys.find(param) != alertVolumeControlKeys.end()) {
      std::map<float, QString> volumeLabels;
      for (int i = 0; i <= 101; ++i) {
        volumeLabels[i] = i == 101 ? tr("Auto") : i == 0 ? tr("Muted") : QString::number(i) + "%";
      }
      std::vector<QString> alertButton{"Test"};
      if (param == "WarningImmediateVolume") {
        soundsToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 25, 101, QString(), volumeLabels, 1, {}, alertButton, false, false);
      } else {
        soundsToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0, 101, QString(), volumeLabels, 1, {}, alertButton, false, false);
      }

    } else if (param == "CustomAlerts") {
      FrogPilotManageControl *customAlertsToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(customAlertsToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        std::set<QString> modifiedCustomAlertsKeys = customAlertsKeys;

        if (!hasBSM) {
          modifiedCustomAlertsKeys.erase("LoudBlindspotAlert");
        }

        if (!(params.getBool("ShowSpeedLimits") || hasOpenpilotLongitudinal && params.getBool("SpeedLimitController"))) {
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

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(soundsToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, this, &FrogPilotSoundsPanel::openParentToggle);
    }
  }

  for (const QString &key : alertVolumeControlKeys) {
    FrogPilotParamValueButtonControl *toggle = static_cast<FrogPilotParamValueButtonControl*>(toggles[key]);
    QObject::connect(toggle, &FrogPilotParamValueButtonControl::buttonClicked, [this, key]() {
      QString alertKey = key;
      alertKey.remove("Volume");
      QString snakeCaseKey;
      for (int i = 0; i < alertKey.size(); ++i) {
        QChar c = alertKey[i];
        if (c.isUpper() && i > 0) {
          snakeCaseKey += '_';
        }
        snakeCaseKey += c.toLower();
      }

      std::thread([this, key, snakeCaseKey]() {
        playSound(snakeCaseKey.toStdString(), params.getInt(key.toStdString()));
      }).detach();
    });
  }

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, this, &FrogPilotSoundsPanel::hideToggles);
}

void FrogPilotSoundsPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  hasBSM = parent->hasBSM;
  hasOpenpilotLongitudinal = parent->hasOpenpilotLongitudinal;
  tuningLevel = parent->tuningLevel;

  hideToggles();
}

void FrogPilotSoundsPanel::showToggles(const std::set<QString> &keys) {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(keys.find(key) != keys.end() && tuningLevel >= frogpilotToggleLevels[key].toDouble());
  }

  setUpdatesEnabled(true);

  update();
}

void FrogPilotSoundsPanel::hideToggles() {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    bool subToggles = alertVolumeControlKeys.find(key) != alertVolumeControlKeys.end() ||
                      customAlertsKeys.find(key) != customAlertsKeys.end();

    toggle->setVisible(!subToggles && tuningLevel >= frogpilotToggleLevels[key].toDouble());
  }

  setUpdatesEnabled(true);

  update();
}
