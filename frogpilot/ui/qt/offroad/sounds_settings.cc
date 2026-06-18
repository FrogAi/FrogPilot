#include "frogpilot/ui/qt/offroad/sounds_settings.h"

FrogPilotSoundsPanel::FrogPilotSoundsPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

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
    {"AlertVolumeControl", tr("Alert Volumes"), tr("<b>Set your own volume for each type of openpilot alert instead of letting openpilot pick.</b> Every alert starts on \"Auto\", which raises the volume as your car gets noisier and lowers it when things are quiet. Turning this back off puts all of them back on \"Auto\"."), "../../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"DisengageVolume", tr("Disengage Volume"), tr("<b>Set the volume for the sound openpilot makes when it stops driving and hands the car back to you.</b><br><br>You hear this when you tap the brake, press \"Cancel\", or when your car's cruise control drops out. Most faults that force openpilot to give up use a warning chime instead, which comes from \"Soft Warning Volume\" or \"Urgent Warning Volume\"."), ""},
    {"PromptDistractedVolume", tr("Distracted Driver Volume"), tr("<b>Set the volume for the sound openpilot makes when the driver camera thinks you've stopped watching the road.</b><br><br>You hear this if you look down at your phone, or if the camera cannot see your face and you go too long without touching the wheel. Ignore it long enough and it escalates into the \"DISENGAGE IMMEDIATELY\" warning, which plays at \"Urgent Warning Volume\" instead. openpilot will not hand the car back on its own, so taking over is on you."), ""},
    {"EngageVolume", tr("Engage Volume"), tr("<b>Set the volume for the sound openpilot makes when it starts driving.</b><br><br>You hear this right after you press \"SET\" or \"RESUME\" on your steering wheel."), ""},
    {"PromptVolume", tr("Prompt Volume"), tr("<b>Set the volume for the quick chimes openpilot uses when it needs you to notice something.</b><br><br>You hear these for things like a car sitting in your blind spot when you signal, or a turn too sharp for openpilot to steer through on its own. The \"Goat Scream\" alert plays at this volume as well."), ""},
    {"WarningSoftVolume", tr("Soft Warning Volume"), tr("<b>Set the volume for openpilot's serious warnings, such as \"BRAKE! Risk of Collision\" when it thinks you're about to hit something.</b><br><br>The \"Louder Blind Spot Alert\" plays at this volume too. This one stops at 25% so a warning can always reach you."), ""},
    {"WarningImmediateVolume", tr("Urgent Warning Volume"), tr("<b>Set the volume for openpilot's most urgent warnings, the ones telling you to take the wheel right now.</b><br><br>\"DISENGAGE IMMEDIATELY\" plays at this volume, which is what you get once you've ignored the driver camera long enough that openpilot no longer trusts you're paying attention. This one stops at 25% so a warning can always reach you."), ""},
    {"RefuseVolume", tr("Won't Engage Volume"), tr("<b>Set the volume for the sound openpilot makes when you try to turn it on and it refuses.</b><br><br>You hear this when something is in the way, like an open door, an unbuckled seatbelt, or the parking brake still being on."), ""},

    {"CustomAlerts", tr("FrogPilot Alerts"), tr("<b>Turn on extra alerts stock openpilot doesn't have, for things you would otherwise have to catch yourself.</b><br><br>These cover the light turning green, the car ahead pulling away, and the speed limit changing. The blind spot one in here is not a new alert: openpilot already chimes when you signal for a lane change and there's a car beside you, and this only swaps that chime for a louder one."), "../../frogpilot/assets/toggle_icons/icon_green_light.png"},
    {"GoatScream", tr("Goat Scream"), tr("<b>Swap the chime for \"Turn Exceeds Steering Limit\" out for a screaming goat.</b><br><br>That alert means the turn is sharper than openpilot can steer through on its own, so it's asking you to help turn the wheel. It plays at whatever you set \"Prompt Volume\" to."), ""},
    {"GreenLightAlert", tr("Green Light Alert"), tr("<b>Play a chime when you're stopped at a light and openpilot sees the road ahead open up.</b><br><br>This only fires when nobody is stopped in front of you. For a line of cars at a light you need \"Lead Departing Alert\" switched on as well, and that one starts off.<br><br><i><b>Disclaimer</b>: openpilot does not actually read traffic lights. It is going off what the camera sees, so it can chime when the light has not changed.</i>"), ""},
    {"LeadDepartingAlert", tr("Lead Departing Alert"), tr("<b>Play a chime when you're stopped in traffic and the car in front of you starts moving again.</b><br><br>It waits until that car has genuinely pulled away rather than just crept forward, so it won't chime the moment they roll an inch."), ""},
    {"LoudBlindspotAlert", tr("Louder Blind Spot Alert"), tr("<b>Upgrade \"Car Detected in Blindspot\" from openpilot's quiet prompt chime to its warning chime.</b><br><br>You get this alert when you signal for a lane change while openpilot is steering and there is a car beside you where you cannot see it. It needs at least 20 mph, so signalling slower than that gets you nothing. Because it becomes a warning, its volume comes from \"Soft Warning Volume\" instead of \"Prompt Volume\"."), ""},
    {"SpeedLimitChangedAlert", tr("Speed Limit Changed Alert"), tr("<b>Play a chime whenever the speed limit openpilot is reading changes.</b><br><br>That limit comes from your car's dashboard, your downloaded map data, or your navigation route, depending on which of those you have set up. You hear it right as the limit changes, such as entering a school zone or coming off the highway."), ""}
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
      testSound(key);
    });
  }

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [soundsLayout, soundsPanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    soundsLayout->setCurrentWidget(soundsPanel);
  });
  soundPlayerProcess = new QProcess(this);

  updateToggles();
}

void FrogPilotSoundsPanel::showEvent(QShowEvent *event) {
  updateToggles();
}

void FrogPilotSoundsPanel::playSound(const QString &path, float volume) {
  static const QString program = R"(
import numpy as np
import sounddevice as sd
import sys
import wave

try:
  with wave.open(sys.argv[1], 'rb') as sound_file:
    audio = np.frombuffer(sound_file.readframes(sound_file.getnframes()), dtype=np.int16).astype(np.float32) / 32768.0
    sample_rate = sound_file.getframerate()

  sd.play(audio * float(sys.argv[2]), sample_rate)
  sd.wait()
except Exception:
  pass
)";

  if (soundPlayerProcess->state() != QProcess::NotRunning) {
    soundPlayerProcess->kill();
    soundPlayerProcess->waitForFinished(1000);
    if (soundPlayerProcess->state() != QProcess::NotRunning) {
      return;
    }
  }
  soundPlayerProcess->start("python3", QStringList{"-u", "-c", program, path, QString::number(volume)});
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

    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels[key].toDouble();

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

void FrogPilotSoundsPanel::testSound(const QString &key) {
  QString baseName = QString(key).remove("Volume");

  if (uiState()->scene.started) {
    updateFrogPilotToggles();

    QString camelCaseAlert = QString(baseName).replace(0, 1, baseName[0].toLower());
    params_memory.put("TestAlert", camelCaseAlert.toStdString());
  } else {
    QString snakeCaseAlert = QString(baseName).replace(QRegularExpression("([A-Z])"), "_\\1").toLower().mid(1);
    QString stockPath = "../../selfdrive/assets/sounds/" + snakeCaseAlert + ".wav";
    QString themePath = "../../frogpilot/assets/active_theme/sounds/" + snakeCaseAlert + ".wav";

    float volume = params.getFloat(key.toStdString()) / 100.0f;

    playSound(QFile::exists(themePath) ? themePath : stockPath, volume);
  }
}
