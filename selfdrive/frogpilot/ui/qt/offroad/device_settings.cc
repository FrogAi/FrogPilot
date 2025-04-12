#include "selfdrive/frogpilot/screenrecorder/screenrecorder.h"
#include "selfdrive/frogpilot/ui/qt/offroad/device_settings.h"

FrogPilotDevicePanel::FrogPilotDevicePanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
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
    {"DeviceManagement", tr("Device Settings"), tr("Settings that control device behavior."), "../frogpilot/assets/toggle_icons/icon_device.png"},
    {"LowVoltageShutdown", tr("Battery Shutdown Threshold"), tr("Automatically shut down the device when the vehicle's battery voltage reaches the set threshold preventing excessive battery drain to protect the battery."), ""},
    {"DeviceShutdown", tr("Device Shutdown Timer"), tr("How long the device stays on for after you go offroad."), ""},
    {"NoLogging", tr("Disable Data Logging"), QString("<b>%1</b><br><br>%2").arg(tr("WARNING: This will prevent your drives from being recorded and all data will be unobtainable!")).arg(tr("Disable all data logging to improve privacy.")), ""},
    {"NoUploads", tr("Disable Data Uploads"), QString("<b>%1</b><br><br>%2").arg(tr("WARNING: This will prevent your drives from appearing on <b>comma connect</b> which may impact debugging and support!")).arg(tr("Prevent the device from sending any data to <b>comma</b>'s servers.")), ""},
    {"OfflineMode", tr("Disable Internet Requirement"), tr("Allow the device to work indefinitely without an internet connection."), ""},
    {"IncreaseThermalLimits", tr("Increase Thermal Safety Limit"), QString("<b>%1</b><br><br>%2").arg(tr("WARNING: This can damage your device by exceeding safe temperature limits!")).arg(tr("Allow the device to run hotter than comma recommended limit.")), ""},

    {"ScreenManagement", tr("Screen Settings"), tr("Settings that control screen behavior."), "../frogpilot/assets/toggle_icons/icon_light.png"},
    {"ScreenBrightness", tr("Screen Brightness (Offroad)"), tr("The screen brightness when not driving."), ""},
    {"ScreenBrightnessOnroad", tr("Screen Brightness (Onroad)"), tr("The screen brightness while driving."), ""},
    {"ScreenRecorder", tr("Screen Recorder"), tr("Enable a button in the driving screen to record the screen."), ""},
    {"ScreenTimeout", tr("Screen Timeout (Offroad)"), tr("How long it takes for the screen to turn off when not driving."), ""},
    {"ScreenTimeoutOnroad", tr("Screen Timeout (Onroad)"), tr("How long it takes for the screen to turn off while driving."), ""}
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
      std::vector<QString> uploadsToggleNames{tr("Only Disable While Onroad")};
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
      FrogPilotButtonControl *recorderToggle = new FrogPilotButtonControl(param, title, desc, icon, recorderButtonNames);
      QObject::connect(recorderToggle, &FrogPilotButtonControl::buttonClicked, [recorderToggle, screenRecorder](int id) {
        if (id == 0) {
          recorderToggle->setCheckedButton(1);

          recorderToggle->setVisibleButton(0, false);
          recorderToggle->setVisibleButton(1, true);

          screenRecorder->startRecording();
        } else {
          recorderToggle->clearCheckedButtons();

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

    if (deviceManagementKeys.find(param) != deviceManagementKeys.end()) {
      deviceManagementList->addItem(deviceToggle);
    } else if (screenKeys.find(param) != screenKeys.end()) {
      screenList->addItem(deviceToggle);
    } else {
      deviceList->addItem(deviceToggle);
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(deviceToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, this, &FrogPilotDevicePanel::openParentToggle);
    }

    QObject::connect(deviceToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  static_cast<ParamControl*>(toggles["IncreaseThermalLimits"])->setConfirmation(true, false);
  static_cast<ParamControl*>(toggles["NoLogging"])->setConfirmation(true, false);
  static_cast<ParamControl*>(toggles["NoUploads"])->setConfirmation(true, false);

  std::set<QString> brightnessKeys = {"ScreenBrightness", "ScreenBrightnessOnroad"};
  for (const QString &key : brightnessKeys) {
    FrogPilotParamValueControl *paramControl = static_cast<FrogPilotParamValueControl*>(toggles[key]);
    QObject::connect(paramControl, &FrogPilotParamValueControl::valueChanged, [this, key](int value) {
      if (!started && key == "ScreenBrightness") {
        Hardware::set_brightness(value);
      } else if (started && key == "ScreenBrightnessOnroad") {
        Hardware::set_brightness(value);
      }
    });
  }

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, [deviceLayout, devicePanel] {deviceLayout->setCurrentWidget(devicePanel);});
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotDevicePanel::updateState);
}

void FrogPilotDevicePanel::showEvent(QShowEvent *event) {
  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  tuningLevel = parent->tuningLevel;

  updateToggles();
}

void FrogPilotDevicePanel::updateState(const UIState &s) {
  if (!isVisible()) {
    return;
  }

  started = s.scene.started;
}

void FrogPilotDevicePanel::updateToggles() {
  std::set<QString> parentKeys = {
    "DeviceManagement",
    "ScreenManagement"
  };

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

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (deviceManagementKeys.find(key) != deviceManagementKeys.end()) {
        toggles["DeviceManagement"]->setVisible(true);
      } else if (screenKeys.find(key) != screenKeys.end()) {
        toggles["ScreenManagement"]->setVisible(true);
      }
    }
  }

  update();
}
