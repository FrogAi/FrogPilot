#include "frogpilot/ui/qt/offroad/device_settings.h"
#include "frogpilot/ui/qt/onroad/screen_recorder.h"

FrogPilotDevicePanel::FrogPilotDevicePanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

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
    {"DeviceManagement", tr("Device Settings"), tr("<b>Change how the device powers off, handles heat, and records your drives.</b>"), "../../frogpilot/assets/toggle_icons/icon_device.png"},
    {"DeviceShutdown", tr("Device Shutdown Timer"), tr("<b>How long the device stays on after you finish driving before it shuts itself off.</b><br><br>Shorter times use less of your car's battery. The lowest setting is 5 minutes."), ""},
    {"NoLogging", tr("Disable Logging"), tr("<b>Stop the device from saving anything from your drives.</b><br><br>Nothing is written to storage, so you won't be able to review your drives later or send a useful bug report."), ""},
    {"NoUploads", tr("Disable Uploads"), tr("<b>Stop the device from uploading your drives to \"comma connect\".</b><br><br>Your drives are still saved on the device. comma uses uploads for debugging and official support, so turning this on limits the help they can give. \"Disable Onroad Only\" pauses uploads while you drive and lets them finish once you park, but only while the device is on Wi-Fi or Ethernet."), ""},
    {"HigherBitrate", tr("High-Quality Recording"), tr("<b>Record your drives in higher video quality.</b><br><br>This row only appears once \"Disable Uploads\" is on and \"Disable Onroad Only\" is off, since the larger files are not meant to be uploaded. The device needs to reboot for it to take effect."), ""},
    {"LowVoltageShutdown", tr("Low-Voltage Cutoff"), tr("<b>Shut the device down when your car's battery drops below the voltage you pick.</b><br><br>This only happens while parked, and keeps the device from draining the battery too far to start the car."), ""},
    {"IncreaseThermalLimits", tr("Raise Temperature Limits"), tr("<b>Let the device run about 6 degrees Celsius hotter than normal before openpilot reacts to the heat.</b><br><br>Normally openpilot disengages and will not re-engage once the device gets hot, and drops back to the offroad screen if it keeps climbing. This makes both happen later. Running the device that hot can shorten its life or damage it, so only use this if you understand the risk."), ""},
    {"UseKonikServer", tr("Use Konik Server"), tr("<b>Upload your drives to \"stable.konik.ai\" instead of \"connect.comma.ai\".</b><br><br>The device needs to reboot for this to take effect."), ""},

    {"ScreenManagement", tr("Screen Settings"), tr("<b>Change how bright the screen is, how long it stays on, and whether you can record it.</b>"), "../../frogpilot/assets/toggle_icons/icon_light.png"},
    {"ScreenBrightness", tr("Screen Brightness (Offroad)"), tr("<b>How bright the screen is while you're not driving.</b><br><br>\"Auto\" only follows the light around you while you are driving. While you are parked it is a fixed 50%, whatever the light is like."), ""},
    {"ScreenBrightnessOnroad", tr("Screen Brightness (Onroad)"), tr("<b>How bright the screen is while you're driving.</b><br><br>\"Auto\" matches the light around you, and \"Screen Off\" keeps the display dark until you tap it."), ""},
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
    } else if (param == "ScreenBrightness" || param == "ScreenBrightnessOnroad") {
      std::map<float, QString> brightnessLabels;
      int minBrightness = (param == "ScreenBrightnessOnroad") ? 0 : 1;
      for (int i = 0; i <= 101; ++i) {
        brightnessLabels[i] = i == 0 ? tr("Screen Off") : i == 101 ? tr("Auto") : QString::number(i) + "%";
      }
      deviceToggle = new FrogPilotParamValueControl(param, title, desc, icon, minBrightness, 101, QString(), brightnessLabels, 1, true);
    } else if (param == "ScreenRecorder") {
      FrogPilotButtonControl *recorderToggle = new FrogPilotButtonControl(param, title, desc, icon, {tr("Start Recording"), tr("Stop Recording")}, true);
      auto updateRecorderToggle = [recorderToggle]() {
        bool recording = ScreenRecorder::active();
        if (recording) {
          recorderToggle->setCheckedButton(1);
        } else {
          recorderToggle->clearCheckedButtons();
        }
        recorderToggle->setVisibleButton(0, !recording);
        recorderToggle->setVisibleButton(1, recording);
      };
      QObject::connect(recorderToggle, &FrogPilotButtonControl::buttonClicked, [recorderToggle, updateRecorderToggle](int id) {
        if (id == 0) {
          ScreenRecorder::start();
        } else {
          ScreenRecorder::stop();
        }

        if (id == 0 && !ScreenRecorder::active()) {
          ConfirmationDialog::alert(
            tr("Couldn't start recording. Check that there's enough free space and that a recording isn't already running."), recorderToggle->window());
        }
        updateRecorderToggle();
      });
      QObject::connect(uiState(), &UIState::offroadTransition, recorderToggle, [updateRecorderToggle](bool) {
        ScreenRecorder::stop();
        updateRecorderToggle();
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

      if (qobject_cast<FrogPilotManageControl*>(deviceToggle)) {
        parentKeys.insert(param);
      }
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(deviceToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        emit openSubPanel();
        openDescriptions(forceOpenDescriptions, toggles);
      });
    }

    QObject::connect(deviceToggle, &AbstractControl::hideDescriptionEvent, [this]() {
      update();
    });
    QObject::connect(deviceToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  static_cast<ParamControl*>(toggles["IncreaseThermalLimits"])->setConfirmation(true, false);
  static_cast<ParamControl*>(toggles["NoLogging"])->setConfirmation(true, false);
  static_cast<ParamControl*>(toggles["NoUploads"])->setConfirmation(true, false);

  QSet<QString> brightnessKeys = {"ScreenBrightness", "ScreenBrightnessOnroad"};
  for (const QString &key : brightnessKeys) {
    FrogPilotParamValueControl *paramControl = static_cast<FrogPilotParamValueControl*>(toggles[key]);
    QObject::connect(paramControl, &FrogPilotParamValueControl::valueChanged, [key](float value) {
      if (!uiState()->scene.started && key == "ScreenBrightness") {
        Hardware::set_brightness(std::lround(value));
      } else if (uiState()->scene.started && key == "ScreenBrightnessOnroad") {
        Hardware::set_brightness(std::lround(value));
      }
    });
  }

  QSet<QString> forceUpdateKeys = {"NoUploads"};
  for (const QString &key : forceUpdateKeys) {
    QObject::connect(static_cast<FrogPilotButtonToggleControl*>(toggles[key]), &FrogPilotButtonToggleControl::buttonClicked, this, &FrogPilotDevicePanel::updateToggles);
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, this, &FrogPilotDevicePanel::updateToggles);
  }

  QSet<QString> rebootKeys = {"HigherBitrate", "UseKonikServer"};
  for (const QString &key : rebootKeys) {
    QObject::connect(static_cast<ToggleControl*>(toggles[key]), &ToggleControl::toggleFlipped, [key, this](bool state) {
      QString filePath;
      if (key == "HigherBitrate") {
        filePath = "/cache/use_HD";
      } else if (key == "UseKonikServer") {
        filePath = "/cache/use_konik";
      }

      if (!filePath.isEmpty()) {
        QFile toggleFile(filePath);
        if (state) {
          if (!toggleFile.exists()) {
            toggleFile.open(QIODevice::WriteOnly);
            toggleFile.close();
          }
        } else {
          if (toggleFile.exists()) {
            toggleFile.remove();
          }
        }
      }

      if (FrogPilotConfirmationDialog::toggleReboot(this)) {
        Hardware::reboot();
      }
    });
  }

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
  const QString gitBranch = QString::fromStdString(params.get("GitBranch"));
  const bool developmentBranch = gitBranch == "FrogPilot-Development";
  const bool vettingBranch = gitBranch == "FrogPilot-Vetting";

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

    if (key == "HigherBitrate" && !developmentBranch && !vettingBranch) {
      setVisible &= params.getBool("DeviceManagement") && params.getBool("NoUploads") && !params.getBool("DisableOnroadUploads");
    }

    else if ((key == "NoLogging" && vettingBranch) ||
             (key == "NoUploads" && (developmentBranch || vettingBranch)) ||
             (key == "HigherBitrate" && (developmentBranch || vettingBranch))) {
      setVisible = false;
    }

    else if (key == "UseKonikServer" && QFile("/data/openpilot/not_vetted").exists()) {
      static_cast<ToggleControl*>(toggle)->forceOn(true);
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (deviceManagementKeys.contains(key)) {
        toggles["DeviceManagement"]->setVisible(true);
      } else if (screenKeys.contains(key)) {
        toggles["ScreenManagement"]->setVisible(true);
      }
    }
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
