#include "frogpilot/ui/qt/offroad/sounds_settings.h"

void playSound(const QString &alert, int volume) {
  QString stockPath = "../../selfdrive/assets/sounds/" + alert + ".wav";
  QString themePath = "../../frogpilot/assets/active_theme/sounds/" + alert + ".wav";

  QString filePath = QFile::exists(themePath) ? themePath : stockPath;

  QProcess::execute("pkill", {"-f", "ffplay"});

  int clampedVolume = std::clamp(volume, 0, 100);

  QProcess::startDetached("ffplay", {"-nodisp", "-autoexit", "-volume", QString::number(clampedVolume), filePath});
}

FrogPilotSoundsPanel::FrogPilotSoundsPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

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
    {"AlertVolumeControl", tr("Alert Volume Controller"), tr("<b>Set how loud each type of openpilot alert is</b> to keep routine prompts from becoming distracting."), "../../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"DisengageVolume", tr("Disengage Volume"), tr("<b>Set the volume for alerts when openpilot disengages.</b><br><br>Examples include: \"Cruise Fault: Restart the Car\", \"Parking Brake Engaged\", \"Pedal Pressed\"."), ""},
    {"EngageVolume", tr("Engage Volume"), tr("<b>Set the volume for the chime when openpilot engages</b>, such as after pressing the \"RESUME\" or \"SET\" steering wheel buttons."), ""},
    {"PromptVolume", tr("Prompt Volume"), tr("<b>Set the volume for prompts that need attention.</b><br><br>Examples include: \"Car Detected in Blindspot\", \"Steering Temporarily Unavailable\", \"Turn Exceeds Steering Limit\"."), ""},
    {"PromptDistractedVolume", tr("Prompt Distracted Volume"), tr("<b>Set the volume for prompts when openpilot detects driver distraction or unresponsiveness.</b><br><br>Examples include: \"Pay Attention\", \"Touch Steering Wheel\"."), ""},
    {"RefuseVolume", tr("Refuse Volume"), tr("<b>Set the volume for alerts when openpilot refuses to engage.</b><br><br>Examples include: \"Brake Hold Active\", \"Door Open\", \"Seatbelt Unlatched\"."), ""},
    {"WarningSoftVolume", tr("Warning Soft Volume"), tr("<b>Set the volume for softer warnings about potential risks.</b><br><br>Examples include: \"BRAKE! Risk of Collision\", \"Steering Temporarily Unavailable\"."), ""},
    {"WarningImmediateVolume", tr("Warning Immediate Volume"), tr("<b>Set the volume for the loudest warnings that require urgent attention.</b><br><br>Examples include: \"DISENGAGE IMMEDIATELY — Driver Distracted\", \"DISENGAGE IMMEDIATELY — Driver Unresponsive\"."), ""},

    {"CustomAlerts", tr("FrogPilot Alerts"), tr("<b>Optional FrogPilot alerts</b> that highlight driving events in a more noticeable way."), "../../frogpilot/assets/toggle_icons/icon_green_light.png"},
    {"GoatScream", tr("Goat Scream"), tr("<b>Play the infamous \"Goat Scream\" when the steering controller reaches its limit.</b> Based on the \"Turn Exceeds Steering Limit\" event."), ""},
    {"GreenLightAlert", tr("Green Light Alert"), tr("<b>Play an alert when the model predicts a red light has turned green.</b><br><br><i><b>Disclaimer</b>: openpilot does not explicitly detect traffic lights. This alert is based on end-to-end model predictions from camera input and may trigger even when the light has not changed.</i>"), ""},
    {"LeadDepartingAlert", tr("Lead Departing Alert"), tr("<b>Play an alert when the lead vehicle departs from a stop.</b>"), ""},
    {"LoudBlindspotAlert", tr("Loud \"Car Detected in Blindspot\" Alert"), tr("<b>Play a louder alert if a vehicle is in the blind spot when attempting to change lanes.</b> Based on the \"Car Detected in Blindspot\" event."), ""},
    {"SpeedLimitChangedAlert", tr("Speed Limit Changed Alert"), tr("<b>Play an alert when the posted speed limit changes.</b>"), ""}
  };

  for (const auto &[param, title, desc, icon] : soundsToggles) {
    AbstractControl *soundsToggle;

    if (param == "AlertVolumeControl") {
      FrogPilotManageControl *alertVolumeControlToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(alertVolumeControlToggle, &FrogPilotManageControl::manageButtonClicked, [soundsLayout, alertVolumeControlPanel]() {
        soundsLayout->setCurrentWidget(alertVolumeControlPanel);
      });
      soundsToggle = alertVolumeControlToggle;
    } else if (alertVolumeControlKeys.contains(param)) {
      std::map<float, QString> volumeLabels;
      for (int i = 0; i <= 101; ++i) {
        volumeLabels[i] = i == 0 ? tr("Muted") : i == 101 ? tr("Auto") : QString::number(i) + "%";
      }
      std::vector<QString> alertButton{tr("Test")};
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

    if (alertVolumeControlKeys.contains(param)) {
      alertVolumeControlList->addItem(soundsToggle);
    } else if (customAlertsKeys.contains(param)) {
      customAlertsList->addItem(soundsToggle);
    } else {
      soundsList->addItem(soundsToggle);

      parentKeys.insert(param);
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(soundsToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        emit openSubPanel();
        openDescriptions(forceOpenDescriptions, toggles);
      });
    }

    QObject::connect(soundsToggle, &AbstractControl::hideDescriptionEvent, [this]() {
      update();
    });
    QObject::connect(soundsToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  for (const QString &key : alertVolumeControlKeys) {
    FrogPilotParamValueButtonControl *toggle = static_cast<FrogPilotParamValueButtonControl*>(toggles[key]);
    QObject::connect(toggle, &FrogPilotParamValueButtonControl::buttonClicked, [key, toggle, this]() {
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
        std::thread([key, snakeCaseAlert, this]() {
          playSound(snakeCaseAlert, params.getInt(key.toStdString()));
        }).detach();
      }
    });
  }

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [soundsLayout, soundsPanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    soundsLayout->setCurrentWidget(soundsPanel);
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotSoundsPanel::updateState);

  for (auto &[key, toggle] : toggles) {
    if (alertVolumeControlKeys.contains(key)) {
      toggle->setVisible(true);
    }
  }

  updateToggles();
}

void FrogPilotSoundsPanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;

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
    if (parentKeys.contains(key)) {
      toggle->setVisible(false);
    }
  }

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      continue;
    }

    bool setVisible = parent->tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (key == "LoudBlindspotAlert") {
      setVisible &= parent->hasBSM;
    }

    else if (key == "SpeedLimitChangedAlert") {
      setVisible &= params.getBool("ShowSpeedLimits") || (parent->hasOpenpilotLongitudinal && params.getBool("SpeedLimitController"));
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (alertVolumeControlKeys.contains(key)) {
        toggles["AlertVolumeControl"]->setVisible(true);
      } else if (customAlertsKeys.contains(key)) {
        toggles["CustomAlerts"]->setVisible(true);
      }
    }
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
