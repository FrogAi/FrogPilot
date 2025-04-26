#include "selfdrive/frogpilot/ui/qt/offroad/sounds_settings.h"

void playSound(const QString &alert, int volume) {
  QString stockPath = "/data/openpilot/selfdrive/assets/sounds/" + alert + ".wav";
  QString themePath = "/data/openpilot/selfdrive/frogpilot/assets/active_theme/sounds/" + alert + ".wav";

  QString filePath = QFile::exists(themePath) ? themePath : stockPath;

  QProcess::execute("pkill", {"-f", "ffplay"});

  int clampedVolume = std::clamp(volume, 0, 100);

  QProcess::startDetached("ffplay", {"-nodisp", "-autoexit", "-volume", QString::number(clampedVolume), filePath});
}

FrogPilotSoundsPanel::FrogPilotSoundsPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QStackedLayout *soundsLayout = new QStackedLayout();
  addItem(soundsLayout);

  FrogPilotListWidget *soundsList = new FrogPilotListWidget(this);

  ScrollView *soundsPanel = new ScrollView(soundsList, this);

  soundsLayout->addWidget(soundsPanel);

  FrogPilotListWidget *alertVolumeControlList = new FrogPilotListWidget(this);
  FrogPilotListWidget *customAlertsList = new FrogPilotListWidget(this);

  ScrollView *alertVolumeControlPanel = new ScrollView(alertVolumeControlList, this);
  ScrollView *customAlertsPanel = new ScrollView(customAlertsList, this);

  soundsLayout->addWidget(alertVolumeControlPanel);
  soundsLayout->addWidget(customAlertsPanel);

  const std::vector<std::tuple<QString, QString, QString, QString>> soundsToggles {
    {"AlertVolumeControl", tr("Alert Volume Control"), tr("Adjust the volume for each alert in openpilot."), "../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"DisengageVolume", tr("Disengage Volume"), tr("Adjust the volume for alerts like:<br><br><b>Adaptive Cruise Disabled</b><br><b>Brake Pedal Pressed</b><br><b>Parking Brake Engaged</b><br><b>Speed too Low</b>"), ""},
    {"EngageVolume", tr("Engage Volume"), tr("Adjust the volume for alerts like:<br><br><b>NNFF Torque Controller loaded</b><br><b>openpilot engaged</b>"), ""},
    {"PromptVolume", tr("Prompt Volume"), tr("Adjust the volume for alerts like:<br><br><b>Car Detected in Blindspot</b><br><b>Steer Unavailable Below \"X\"</b><br><b>Speed too Low</b><br><b>Take Control, Turn Exceeds Steering Limit</b>"), ""},
    {"PromptDistractedVolume", tr("Prompt Distracted Volume"), tr("Adjust the volume for alerts like:<br><br><b>Pay Attention, Driver Distracted</b><br><b>Touch Steering Wheel, Driver Unresponsive</b>"), ""},
    {"RefuseVolume", tr("Refuse Volume"), tr("Adjust the volume for alerts like:<br><br><b>openpilot Unavailable</b>"), ""},
    {"WarningSoftVolume", tr("Warning Soft Volume"), tr("Adjust the volume for alerts like:<br><br><b>BRAKE!, Risk of Collision</b><br><b>TAKE CONTROL IMMEDIATELY</b>"), ""},
    {"WarningImmediateVolume", tr("Warning Immediate Volume"), tr("Adjust the volume for alerts like:<br><br><b>DISENGAGE IMMEDIATELY, Driver Distracted</b><br><b>DISENGAGE IMMEDIATELY, Driver Unresponsive</b>"), ""},

    {"CustomAlerts", tr("FrogPilot Alerts"), tr("FrogPilot alerts for various events in openpilot."), "../frogpilot/assets/toggle_icons/icon_green_light.png"},
    {"GoatScream", tr("Goat Scream Steering Saturated Alert"), tr("The infamous \"Goat Scream\" that has brought both joy and anger to FrogPilot users all around the world!"), ""},
    {"GreenLightAlert", tr("Green Light Alert"), tr("Get an alert when the traffic light changes from red to green."), ""},
    {"LeadDepartingAlert", tr("Lead Departing Alert"), tr("Get an alert when the lead vehicle begins to depart from a standstill."), ""},
    {"LoudBlindspotAlert", tr("Loud \"Car Detected in Blindspot\" Alert"), tr("A louder alert for when a vehicle is detected in the blindspot when attempting to change lanes."), ""},
    {"SpeedLimitChangedAlert", tr("Speed Limit Changed Alert"), tr("Get an alert when the speed limit changes."), ""}
  };

  for (const auto &[param, title, desc, icon] : soundsToggles) {
    AbstractControl *soundsToggle;

    if (param == "AlertVolumeControl") {
      FrogPilotManageControl *alertVolumeControlToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(alertVolumeControlToggle, &FrogPilotManageControl::manageButtonClicked, [soundsLayout, alertVolumeControlPanel]() {
        soundsLayout->setCurrentWidget(alertVolumeControlPanel);
      });
      soundsToggle = alertVolumeControlToggle;
    } else if (alertVolumeControlKeys.find(param) != alertVolumeControlKeys.end()) {
      std::map<float, QString> volumeLabels;
      for (int i = 0; i <= 101; ++i) {
        volumeLabels[i] = i == 0 ? tr("Muted") : i == 101 ? tr("Auto") : QString::number(i) + "%";
      }
      std::vector<QString> alertButton{"Test"};
      if (param == "WarningImmediateVolume" || param == "WarningSoftVolume") {
        soundsToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 25, 101, QString(), volumeLabels, 1, true, {}, alertButton, false, false);
      } else {
        soundsToggle = new FrogPilotParamValueButtonControl(param, title, desc, icon, 0, 101, QString(), volumeLabels, 1, true, {}, alertButton, false, false);
      }

    } else if (param == "CustomAlerts") {
      FrogPilotManageControl *customAlertsToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(customAlertsToggle, &FrogPilotManageControl::manageButtonClicked, [soundsLayout, customAlertsPanel]() {
        soundsLayout->setCurrentWidget(customAlertsPanel);
      });
      soundsToggle = customAlertsToggle;

    } else {
      soundsToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = soundsToggle;

    if (alertVolumeControlKeys.find(param) != alertVolumeControlKeys.end()) {
      alertVolumeControlList->addItem(soundsToggle);
    } else if (customAlertsKeys.find(param) != customAlertsKeys.end()) {
      customAlertsList->addItem(soundsToggle);
    } else {
      soundsList->addItem(soundsToggle);

      parentKeys.insert(param);
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(soundsToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, this, &FrogPilotSoundsPanel::openParentToggle);
    }

    QObject::connect(soundsToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  for (const QString &key : alertVolumeControlKeys) {
    FrogPilotParamValueButtonControl *toggle = static_cast<FrogPilotParamValueButtonControl*>(toggles[key]);
    QObject::connect(toggle, &FrogPilotParamValueButtonControl::buttonClicked, [this, key, toggle]() {
      toggle->updateParam();

      updateFrogPilotToggles();

      util::sleep_for(UI_FREQ);

      QString keyWithoutVolume = key;
      keyWithoutVolume.remove("Volume");

      QString camelCaseAlert = keyWithoutVolume;
      camelCaseAlert[0] = camelCaseAlert[0].toLower();

      QString snakeCaseAlert;
      for (int i = 0; i < keyWithoutVolume.size(); ++i) {
        QChar c = keyWithoutVolume[i];
        if (c.isUpper() && i > 0) {
          snakeCaseAlert += '_';
        }
        snakeCaseAlert += c.toLower();
      }

      if (started) {
        params_memory.put("TestAlert", camelCaseAlert.toStdString());
      } else {
        std::thread([this, key, snakeCaseAlert]() {
          playSound(snakeCaseAlert, params.getInt(key.toStdString()));
        }).detach();
      }
    });
  }

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, [soundsLayout, soundsPanel] {soundsLayout->setCurrentWidget(soundsPanel);});
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotSoundsPanel::updateState);
}

void FrogPilotSoundsPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  hasBSM = parent->hasBSM;
  hasOpenpilotLongitudinal = parent->hasOpenpilotLongitudinal;
  tuningLevel = parent->tuningLevel;

  updateToggles();
}

void FrogPilotSoundsPanel::updateState(const UIState &s) {
  if (!isVisible()) {
    return;
  }

  started = s.scene.started;
}

void FrogPilotSoundsPanel::updateToggles() {
  for (auto &[key, toggle] : toggles) {
    if (parentKeys.find(key) != parentKeys.end()) {
      toggle->setVisible(false);
    }
  }

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.find(key) != parentKeys.end()) {
      continue;
    }

    bool setVisible = tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (key == "LoudBlindspotAlert") {
      setVisible &= hasBSM;
    }

    if (key == "SpeedLimitChangedAlert") {
      setVisible &= params.getBool("ShowSpeedLimits") || (hasOpenpilotLongitudinal && params.getBool("SpeedLimitController"));
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (alertVolumeControlKeys.find(key) != alertVolumeControlKeys.end()) {
        toggles["AlertVolumeControl"]->setVisible(true);
      } else if (customAlertsKeys.find(key) != customAlertsKeys.end()) {
        toggles["CustomAlerts"]->setVisible(true);
      }
    }
  }

  update();
}
