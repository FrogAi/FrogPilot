#include "frogpilot/ui/screenrecorder/screenrecorder.h"
#include "frogpilot/ui/qt/offroad/device_settings.h"

FrogPilotDevicePanel::FrogPilotDevicePanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

  ScreenRecorder *screenRecorder = new ScreenRecorder(this);
  screenRecorder->setVisible(false);

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
    {"DeviceManagement", tr("Device Settings"), tr("<b>Settings that control how the device runs, powers off, and manages driving data.</b>"), "../../frogpilot/assets/toggle_icons/icon_device.png"},
    {"DeviceShutdown", tr("Device Shutdown Timer"), tr("<b>Keep the device on for the set amount of time after a drive</b> before it shuts down automatically."), ""},
    {"NoLogging", tr("Disable Logging"), QString("<b>%1</b><br><br>%2").arg(tr("WARNING: This will prevent your drives from being recorded and all data will be unobtainable!")).arg(tr("<b>Prevent the device from saving driving data.</b>")), ""},
    {"NoUploads", tr("Disable Uploads"), QString("<b>%1</b><br><br>%2").arg(tr("WARNING: This will prevent your drives from being uploaded to <b>comma connect</b> which will impact debugging and official support from comma!")).arg(tr("<b>Prevent the device from uploading driving data.</b>")), ""},
    {"HigherBitrate", tr("High-Quality Recording"), tr("<b>Save drive footage in higher video quality.</b>"), ""},
    {"LowVoltageShutdown", tr("Low-Voltage Cutoff"), tr("<b>While parked, if the battery voltage falls below the set level, the device shuts down</b> to prevent excessive battery drain."), ""},
    {"IncreaseThermalLimits", tr("Raise Temperature Limits"), QString("<b>%1</b><br><br>%2").arg(tr("WARNING: Running at higher temperatures may damage your device!")).arg(tr("<b>Allow the device to run at higher temperatures</b> before throttling or shutting down. Use only if you understand the risks!")), ""},
    {"UseKonikServer", tr("Use Konik Server"), tr("<b>Upload driving data to \"connect.konik.ai\" instead of \"connect.comma.ai\".</b>"), ""},

    {"ScreenManagement", tr("Screen Settings"), tr("<b>Settings that control screen brightness, screen recording, and timeout duration.</b>"), "../../frogpilot/assets/toggle_icons/icon_light.png"},
    {"ScreenBrightness", tr("Screen Brightness (Offroad)"), tr("<b>The screen brightness while not driving.</b>"), ""},
    {"ScreenBrightnessOnroad", tr("Screen Brightness (Onroad)"), tr("<b>The screen brightness while driving.</b>"), ""},
    {"ScreenRecorder", tr("Screen Recorder"), tr("<b>Add a button to the driving screen to record the display.</b>"), ""},
    {"ScreenTimeout", tr("Screen Timeout (Offroad)"), tr("<b>How long the screen stays on after being tapped while not driving.</b>"), ""},
    {"ScreenTimeoutOnroad", tr("Screen Timeout (Onroad)"), tr("<b>How long the screen stays on after being tapped while driving.</b>"), ""},
    {"StandbyMode", tr("Standby Mode"), tr("<b>Turn the screen off while driving and automatically wake it up for alerts or engagement state changes.</b>"), ""},

    {"IgnoreMe", "Ignore Me", "This is simply used to fix the layout when the user opens the descriptions and the menu gets wonky. No idea why it happens, but I can't be asked to properly fix it so whatever. Sue me.", ""},
    {"IgnoreMe2", "Ignore Me", "This is simply used to fix the layout when the user opens the descriptions and the menu gets wonky. No idea why it happens, but I can't be asked to properly fix it so whatever. Sue me.", ""},
    {"IgnoreMe3", "Ignore Me", "This is simply used to fix the layout when the user opens the descriptions and the menu gets wonky. No idea why it happens, but I can't be asked to properly fix it so whatever. Sue me.", ""},
    {"IgnoreMe4", "Ignore Me", "This is simply used to fix the layout when the user opens the descriptions and the menu gets wonky. No idea why it happens, but I can't be asked to properly fix it so whatever. Sue me.", ""},
    {"IgnoreMe5", "Ignore Me", "This is simply used to fix the layout when the user opens the descriptions and the menu gets wonky. No idea why it happens, but I can't be asked to properly fix it so whatever. Sue me.", ""}
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
      deviceToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 33, QString(), shutdownLabels, 1, true);
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
      std::vector<QString> recorderButtonNames{tr("Start Recording"), tr("Stop Recording")};
      FrogPilotButtonControl *recorderToggle = new FrogPilotButtonControl(param, title, desc, icon, recorderButtonNames, true);
      QObject::connect(recorderToggle, &FrogPilotButtonControl::buttonClicked, [recorderToggle, screenRecorder](int id) {
        if (id == 0) {
          recorderToggle->setCheckedButton(1);

          recorderToggle->setVisibleButton(0, false);
          recorderToggle->setVisibleButton(1, true);

          screenRecorder->startRecording();
        } else if (id == 1) {
          recorderToggle->clearCheckedButtons(true);

          recorderToggle->setVisibleButton(0, true);
          recorderToggle->setVisibleButton(1, false);

          screenRecorder->stopRecording();
        }
      });
      recorderToggle->setVisibleButton(1, false);
      deviceToggle = recorderToggle;
    } else if (param == "ScreenTimeout" || param == "ScreenTimeoutOnroad") {
      deviceToggle = new FrogPilotParamValueControl(param, title, desc, icon, 5, 60, tr(" seconds"));

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
    QObject::connect(paramControl, &FrogPilotParamValueControl::valueChanged, [key, this](int value) {
      if (!started && key == "ScreenBrightness") {
        Hardware::set_brightness(value);
      } else if (started && key == "ScreenBrightnessOnroad") {
        Hardware::set_brightness(value);
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
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotDevicePanel::updateState);
}

void FrogPilotDevicePanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;

  updateToggles();
}

void FrogPilotDevicePanel::updateState(const UIState &s) {
  if (!isVisible()) {
    return;
  }

  started = s.scene.started;
}

void FrogPilotDevicePanel::updateToggles() {
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

    if (key == "HigherBitrate") {
      setVisible &= params.getBool("DeviceManagement") && params.getBool("NoUploads") && !params.getBool("DisableOnroadUploads");
    }

    else if (key == "UseKonikServer" && QFile("/data/not_vetted").exists()) {
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
