#include "frogpilot/ui/qt/offroad/device_settings.h"
#include "frogpilot/ui/qt/onroad/screen_recorder.h"

FrogPilotDevicePanel::FrogPilotDevicePanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  const QString gitBranch = QString::fromStdString(params.get("GitBranch"));
  developmentBranch = gitBranch == "FrogPilot-Development";
  vettingBranch = gitBranch == "FrogPilot-Vetting";

  QStackedLayout *deviceLayout = new QStackedLayout();
  addItem(deviceLayout);

  FrogPilotListWidget *deviceList = new FrogPilotListWidget(this);

  ScrollView *devicePanel = new ScrollView(deviceList, this);

  deviceLayout->addWidget(devicePanel);

  FrogPilotListWidget *deviceManagementList = new FrogPilotListWidget(this);
  FrogPilotListWidget *screenList = new FrogPilotListWidget(this);

  ScrollView *deviceManagementPanel = new ScrollView(deviceManagementList, this);
  ScrollView *screenPanel = new ScrollView(screenList, this);

  deviceLayout->addWidget(deviceManagementPanel);
  deviceLayout->addWidget(screenPanel);

  const std::vector<std::tuple<QString, QString, QString, QString>> deviceToggles {
    {"DeviceManagement", tr("Device Settings"), tr("<b>Change how the device powers off, handles heat, and records your drives.</b>"), "../../frogpilot/assets/toggle_icons/icon_device.svg"},
    {"DeviceShutdown", tr("Device Shutdown Timer"), tr("<b>How long the device stays on after you finish driving before it shuts itself off.</b><br><br>Shorter times use less of your car's battery. The lowest setting is 5 minutes."), ""},
    {"NoLogging", tr("Disable Logging"), tr("<b>Stop the device from recording your drives.</b><br><br>No driving logs or camera footage are saved, so you won't be able to review your drives later or send a useful bug report. Screen recordings and the device's own system logs are still saved."), ""},
    {"NoUploads", tr("Disable Uploads"), tr("<b>Stop the device from uploading your drives to \"comma connect\".</b><br><br>Your drives are still saved on the device. comma uses uploads for debugging and official support, so turning this on limits the help they can give. \"Disable Onroad Only\" pauses uploads while you drive and lets them finish once you park, but only while the device is on Wi-Fi or Ethernet."), ""},
    {"HigherBitrate", tr("High-Quality Recording"), tr("<b>Record your drives in higher video quality.</b><br><br>This row only appears once \"Disable Uploads\" is on and \"Disable Onroad Only\" is off, since the larger files are not meant to be uploaded. The device needs to reboot for it to take effect."), ""},
    {"LowVoltageShutdown", tr("Low-Voltage Cutoff"), tr("<b>Shut the device down when your car's battery drops below the voltage you pick.</b><br><br>This only happens while parked, and keeps the device from draining the battery too far to start the car."), ""},
    {"IncreaseThermalLimits", tr("Raise Temperature Limits"), tr("<b>Let the device run about 6 degrees Celsius hotter than normal before openpilot disengages because of the heat.</b><br><br>Normally openpilot disengages and will not re-engage once the device gets hot, and drops back to the offroad screen if it keeps climbing. This only makes the first happen later: the device still drops back to the offroad screen at the normal temperature. Running the device that hot can shorten its life or damage it, so only use this if you understand the risk."), ""},
    {"UseKonikServer", tr("Use Konik Server"), tr("<b>Upload your drives to \"stable.konik.ai\" instead of \"connect.comma.ai\".</b><br><br>The device needs to reboot for this to take effect."), ""},

    {"ScreenManagement", tr("Screen Settings"), tr("<b>Change how bright the screen is, how long it stays on, and whether you can record it.</b>"), "../../frogpilot/assets/toggle_icons/icon_light.svg"},
    {"InstantReplay", tr("Capture Recent Footage"), tr("<b>Save what just happened on your driving screen.</b><br><br>Choose how far back to keep, then tap \"CAPTURE\" to save a moment you want to review or share. There's no need to remember to start recording beforehand."), ""},
    {"ScreenBrightness", tr("Screen Brightness (Offroad)"), tr("<b>How bright the screen is while you're not driving.</b><br><br>\"Auto\" only follows the light around you while you are driving. While you are parked it is a fixed 50%, whatever the light is like."), ""},
    {"ScreenBrightnessOnroad", tr("Screen Brightness (Onroad)"), tr("<b>How bright the screen is while you're driving.</b><br><br>\"Auto\" matches the light around you, and \"Screen Off\" keeps the display dark until you tap it. A tap brightens anything below 5% to 5% until the screen times out."), ""},
    {"ScreenRecorder", tr("Screen Recorder"), tr("<b>Add a button to the driving screen that records what's on it.</b><br><br>Your recordings are saved on the device and can be renamed or deleted under \"Screen Recordings\" in the \"DATA\" panel."), ""},
    {"ScreenTimeout", tr("Screen Timeout (Offroad)"), tr("<b>How long the screen stays on after you tap it while not driving.</b>"), ""},
    {"ScreenTimeoutOnroad", tr("Screen Timeout (Onroad)"), tr("<b>How long the screen stays on after you tap it while driving.</b>"), ""},
    {"StandbyMode", tr("Standby Mode"), tr("<b>Turn the screen off while driving, and wake it up automatically for alerts or when openpilot engages or disengages.</b><br><br>Tapping the screen wakes it up too."), ""}
  };

  for (const auto &[param, title, desc, icon] : deviceToggles) {
    AbstractControl *deviceToggle;

    if (param == "DeviceManagement") {
      FrogPilotManageControl *deviceManagementToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(deviceManagementToggle, &FrogPilotManageControl::manageButtonClicked, [deviceLayout, deviceManagementPanel]() {
        deviceLayout->setCurrentWidget(deviceManagementPanel);
      });
      deviceToggle = deviceManagementToggle;
    } else if (param == "DeviceShutdown") {
      std::map<float, QString> shutdownLabels;
      for (int i = 0; i <= 33; ++i) {
        shutdownLabels[i] = i == 0 ? tr("5 mins") : i <= 3 ? QString::number(i * 15) + tr(" mins") : QString::number(i - 3) + (i == 4 ? tr(" hour") : tr(" hours"));
      }
      deviceToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 33, QString(), shutdownLabels);
    } else if (param == "NoUploads") {
      std::vector<QString> uploadsToggles{"DisableOnroadUploads"};
      std::vector<QString> uploadsToggleNames{tr("Disable Onroad Only")};
      deviceToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, uploadsToggles, uploadsToggleNames);
    } else if (param == "LowVoltageShutdown") {
      deviceToggle = new FrogPilotParamValueControl(param, title, desc, icon, 11.8, 12.5, tr(" volts"), std::map<float, QString>(), 0.1);

    } else if (param == "ScreenManagement") {
      FrogPilotManageControl *screenToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(screenToggle, &FrogPilotManageControl::manageButtonClicked, [deviceLayout, screenPanel]() {
        deviceLayout->setCurrentWidget(screenPanel);
      });
      deviceToggle = screenToggle;
    } else if (param == "InstantReplay") {
      std::map<float, QString> replayLabels{{0, tr("Off")}, {30, tr("30 seconds")}, {60, tr("1 minute")}};
      for (int seconds = 90; seconds <= 300; seconds += 30) {
        replayLabels[seconds] = QString::number(seconds / 60.0) + tr(" minutes");
      }
      deviceToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 300, QString(), replayLabels, 30);
    } else if (param == "ScreenBrightness" || param == "ScreenBrightnessOnroad") {
      std::map<float, QString> brightnessLabels{{0, tr("Screen Off")}, {101, tr("Auto")}};
      int minBrightness = (param == "ScreenBrightnessOnroad") ? 0 : 1;
      deviceToggle = new FrogPilotParamValueControl(param, title, desc, icon, minBrightness, 101, "%", brightnessLabels, 1, true);
    } else if (param == "ScreenRecorder") {
      FrogPilotButtonToggleControl *recorderToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, {}, {tr("Start Recording"), tr("Stop Recording")});
      std::function<void()> updateRecorderToggle = [recorderToggle]() {
        bool recording = ScreenRecorder::active();
        if (recording) {
          recorderToggle->setCheckedButton(1);
        } else {
          recorderToggle->clearCheckedButtons();
        }
        recorderToggle->setVisibleButton(0, !recording);
        recorderToggle->setVisibleButton(1, recording);
      };
      QObject::connect(recorderToggle, &FrogPilotButtonToggleControl::buttonClicked, [updateRecorderToggle](int id) {
        if (id == 0) {
          ScreenRecorder::start();
        } else {
          ScreenRecorder::stop();
        }
        updateRecorderToggle();
      });
      QObject::connect(recorderToggle, &ToggleControl::toggleFlipped, recorderToggle, [](bool state) {
        if (!state) {
          ScreenRecorder::stop();
        }
      });
      QObject::connect(uiState(), &UIState::offroadTransition, recorderToggle, [updateRecorderToggle](bool) {
        ScreenRecorder::stop();
        updateRecorderToggle();
      });
      QObject::connect(frogpilotUIState(), &FrogPilotUIState::togglesUpdated, recorderToggle, [] {
        if (!frogpilotUIState()->frogpilot_scene.frogpilot_toggles.value("screen_recorder").toBool()) {
          ScreenRecorder::stop();
        }
      });
      QObject::connect(uiState(), &UIState::uiUpdate, recorderToggle, [recorderToggle, updateRecorderToggle]() {
        if (recorderToggle->isVisible()) {
          updateRecorderToggle();
        }
      });
      updateRecorderToggle();
      deviceToggle = recorderToggle;
    } else if (param == "ScreenTimeout" || param == "ScreenTimeoutOnroad") {
      deviceToggle = new FrogPilotParamValueControl(param, title, desc, icon, 5, 60, tr(" seconds"), {}, 5);

    } else {
      deviceToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = deviceToggle;

    if (deviceManagementKeys.contains(param)) {
      deviceManagementList->addItem(deviceToggle);
    } else if (screenKeys.contains(param)) {
      screenList->addItem(deviceToggle);
    } else {
      deviceList->addItem(deviceToggle);

      parentKeys.insert(param);
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(deviceToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        emit openSubPanel();
        openDescriptions(forceOpenDescriptions, toggles);
      });
    }
  }

  static_cast<ParamControl*>(toggles["IncreaseThermalLimits"])->setConfirmation(true, false);
  static_cast<ParamControl*>(toggles["NoLogging"])->setConfirmation(true, false);
  static_cast<ParamControl*>(toggles["NoUploads"])->setConfirmation(true, false);

  if (QFile::exists("/data/openpilot/not_vetted")) {
    static_cast<ParamControl*>(toggles["UseKonikServer"])->forceOn();
  }

  QSet<QString> brightnessKeys = {"ScreenBrightness", "ScreenBrightnessOnroad"};
  for (const QString &key : brightnessKeys) {
    FrogPilotParamValueControl *paramControl = static_cast<FrogPilotParamValueControl*>(toggles[key]);
    QObject::connect(paramControl, &FrogPilotParamValueControl::valueChanged, [key](float value) {
      QJsonObject &frogpilot_toggles = frogpilotUIState()->frogpilot_scene.frogpilot_toggles;
      if (key == "ScreenBrightnessOnroad" && frogpilot_toggles.value("force_onroad").toBool()) {
        return;
      }
      frogpilot_toggles.insert(key == "ScreenBrightness" ? "screen_brightness" : "screen_brightness_onroad", value);
    });
  }

  QObject::connect(static_cast<FrogPilotButtonToggleControl*>(toggles["NoUploads"]), &FrogPilotButtonToggleControl::buttonClicked, this, &FrogPilotDevicePanel::updateToggles);
  QObject::connect(static_cast<ToggleControl*>(toggles["NoUploads"]), &ToggleControl::toggleFlipped, this, &FrogPilotDevicePanel::updateToggles);

  std::function<void()> updateHigherBitrate = [this]() {
    bool useHigherBitrate = params.getBool("HigherBitrate") && params.getBool("NoUploads") && !params.getBool("DisableOnroadUploads");

    QFile toggleFile("/cache/use_HD");
    if (toggleFile.exists() == useHigherBitrate) {
      return;
    }

    if (useHigherBitrate) {
      toggleFile.open(QIODevice::WriteOnly);
      toggleFile.close();
    } else {
      toggleFile.remove();
    }

    if (FrogPilotConfirmationDialog::toggleReboot(this)) {
      FrogPilotConfirmationDialog::softReboot(this);
    }
  };
  QObject::connect(static_cast<ToggleControl*>(toggles["HigherBitrate"]), &ToggleControl::toggleFlipped, updateHigherBitrate);
  QObject::connect(static_cast<FrogPilotButtonToggleControl*>(toggles["NoUploads"]), &FrogPilotButtonToggleControl::buttonClicked, updateHigherBitrate);
  QObject::connect(static_cast<ToggleControl*>(toggles["NoUploads"]), &ToggleControl::toggleFlipped, updateHigherBitrate);

  QObject::connect(static_cast<ToggleControl*>(toggles["UseKonikServer"]), &ToggleControl::toggleFlipped, [this](bool state) {
    if (!FrogPilotConfirmationDialog::toggleReboot(this)) {
      return;
    }

    if (!isOpenpilotSteering()) {
      QFile toggleFile("/cache/use_konik");
      if (state) {
        toggleFile.open(QIODevice::WriteOnly);
        toggleFile.close();
      } else {
        toggleFile.remove();
      }
    }

    FrogPilotConfirmationDialog::softReboot(this);
  });

  openDescriptions(forceOpenDescriptions, toggles);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [deviceLayout, devicePanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    deviceLayout->setCurrentWidget(devicePanel);
  });
}

void FrogPilotDevicePanel::showEvent(QShowEvent *event) {
  updateToggles();
}

void FrogPilotDevicePanel::updateToggles() {
  QSet<QString> visibleParents;

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      continue;
    }

    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels.value(key).toDouble();

    if (key == "HigherBitrate") {
      setVisible &= !developmentBranch && !vettingBranch && params.getBool("NoUploads") && !params.getBool("DisableOnroadUploads");
    }

    else if (key == "NoLogging") {
      setVisible &= !vettingBranch;
    }

    else if (key == "NoUploads") {
      setVisible &= !developmentBranch && !vettingBranch;
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (deviceManagementKeys.contains(key)) {
        visibleParents.insert("DeviceManagement");
      } else if (screenKeys.contains(key)) {
        visibleParents.insert("ScreenManagement");
      }
    }
  }

  for (const QString &key : parentKeys) {
    toggles[key]->setVisible(visibleParents.contains(key));
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
