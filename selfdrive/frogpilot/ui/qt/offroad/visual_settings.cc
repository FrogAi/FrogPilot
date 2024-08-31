#include "selfdrive/frogpilot/ui/qt/offroad/visual_settings.h"

FrogPilotVisualsPanel::FrogPilotVisualsPanel(SettingsWindow *parent) : FrogPilotListWidget(parent) {
  std::string branch = params.get("GitBranch");
  isRelease = branch == "FrogPilot";

  const std::vector<std::tuple<QString, QString, QString, QString>> visualToggles {
    {"AlertVolumeControl", tr("Alert Volume Controller"), tr("Control the volume level for each individual sound in openpilot."), "../frogpilot/assets/toggle_icons/icon_mute.png"},
    {"DisengageVolume", tr("Disengage Volume"), tr("Related alerts:\n\nAdaptive Cruise Disabled\nParking Brake Engaged\nBrake Pedal Pressed\nSpeed too Low"), ""},
    {"EngageVolume", tr("Engage Volume"), tr("Related alerts:\n\nNNFF Torque Controller loaded\nopenpilot engaged"), ""},
    {"PromptVolume", tr("Prompt Volume"), tr("Related alerts:\n\nCar Detected in Blindspot\nSpeed too Low\nSteer Unavailable Below 'X'\nTake Control, Turn Exceeds Steering Limit"), ""},
    {"PromptDistractedVolume", tr("Prompt Distracted Volume"), tr("Related alerts:\n\nPay Attention, Driver Distracted\nTouch Steering Wheel, Driver Unresponsive"), ""},
    {"RefuseVolume", tr("Refuse Volume"), tr("Related alerts:\n\nopenpilot Unavailable"), ""},
    {"WarningSoftVolume", tr("Warning Soft Volume"), tr("Related alerts:\n\nBRAKE!, Risk of Collision\nTAKE CONTROL IMMEDIATELY"), ""},
    {"WarningImmediateVolume", tr("Warning Immediate Volume"), tr("Related alerts:\n\nDISENGAGE IMMEDIATELY, Driver Distracted\nDISENGAGE IMMEDIATELY, Driver Unresponsive"), ""},

    {"BonusContent", tr("Bonus Content"), tr("Bonus FrogPilot features to make openpilot a bit more fun!"), "../frogpilot/assets/toggle_icons/frog.png"},
    {"GoatScream", tr("Goat Scream"), tr("Enable the famed 'Goat Scream' that has brought both joy and anger to FrogPilot users all around the world!"), ""},
    {"HolidayThemes", tr("Holiday Themes"), tr("The openpilot theme changes according to the current/upcoming holiday. Minor holidays last a day, while major holidays (Easter, Christmas, Halloween, etc.) last a week."), ""},
    {"PersonalizeOpenpilot", tr("Personalize openpilot"), tr("Customize openpilot to your personal tastes!"), ""},
    {"CustomColors", tr("Color Theme"), tr("Switch out the standard openpilot color scheme with themed colors.\n\nWant to submit your own color scheme? Post it in the 'feature-request' channel in the FrogPilot Discord!"), ""},
    {"CustomIcons", tr("Icon Pack"), tr("Switch out the standard openpilot icons with a set of themed icons.\n\nWant to submit your own icon pack? Post it in the 'feature-request' channel in the FrogPilot Discord!"), ""},
    {"CustomSounds", tr("Sound Pack"), tr("Switch out the standard openpilot sounds with a set of themed sounds.\n\nWant to submit your own sound pack? Post it in the 'feature-request' channel in the FrogPilot Discord!"), ""},
    {"CustomSignals", tr("Turn Signals"), tr("Add themed animation for your turn signals.\n\nWant to submit your own turn signal animation? Post it in the 'feature-request' channel in the FrogPilot Discord!"), ""},
    {"WheelIcon", tr("Steering Wheel"), tr("Replace the default steering wheel icon with a custom icon."), ""},
    {"DownloadStatusLabel", tr("Download Status"), "", ""},
    {"StartupAlert", tr("Startup Alert"), tr("Customize the 'Startup' alert that is shown when you go onroad."), ""},
    {"RandomEvents", tr("Random Events"), tr("Enjoy a bit of unpredictability with random events that can occur during certain driving conditions. This is purely cosmetic and has no impact on driving controls!"), ""},

    {"CustomAlerts", tr("Custom Alerts"), tr("Enable custom alerts for openpilot events."), "../frogpilot/assets/toggle_icons/icon_green_light.png"},
    {"GreenLightAlert", tr("Green Light Alert"), tr("Get an alert when a traffic light changes from red to green."), ""},
    {"LeadDepartingAlert", tr("Lead Departing Alert"), tr("Get an alert when the lead vehicle starts departing when at a standstill."), ""},
    {"LoudBlindspotAlert", tr("Loud Blindspot Alert"), tr("Enable a louder alert for when a vehicle is detected in the blindspot when attempting to change lanes."), ""},

    {"CustomUI", tr("Custom Onroad UI"), tr("Customize the Onroad UI."), "../assets/offroad/icon_road.png"},
    {"Compass", tr("Compass"), tr("Add a compass to the onroad UI."), ""},
    {"CustomPaths", tr("Paths"), tr("Show your projected acceleration on the driving path, detected adjacent lanes, or when a vehicle is detected in your blindspot."), ""},
    {"PedalsOnUI", tr("Pedals Being Pressed"), tr("Display the brake and gas pedals on the onroad UI below the steering wheel icon."), ""},
    {"RoadNameUI", tr("Road Name"), tr("Display the current road's name at the bottom of the screen. Sourced from OpenStreetMap."), ""},
    {"RotatingWheel", tr("Rotating Steering Wheel"), tr("Rotate the steering wheel in the onroad UI alongside your physical steering wheel."), ""},
    {"ShowStoppingPoint", tr("Stopping Points"), tr("Display the point where openpilot wants to stop for red lights/stop signs."), ""},

    {"DeveloperUI", tr("Developer UI"), tr("Get various detailed information of what openpilot is doing behind the scenes."), "../frogpilot/assets/toggle_icons/icon_device.png"},
    {"BorderMetrics", tr("Border Metrics"), tr("Display metrics in onroad UI border."), ""},
    {"FPSCounter", tr("FPS Counter"), tr("Display the 'Frames Per Second' (FPS) of your onroad UI for monitoring system performance."), ""},
    {"LateralMetrics", tr("Lateral Metrics"), tr("Display various metrics related to the lateral performance of openpilot."), ""},
    {"LongitudinalMetrics", tr("Longitudinal Metrics"), tr("Display various metrics related to the longitudinal performance of openpilot."), ""},
    {"NumericalTemp", tr("Numerical Temperature Gauge"), tr("Replace the 'GOOD', 'OK', and 'HIGH' temperature statuses with a numerical temperature gauge based on the highest temperature between the memory, CPU, and GPU."), ""},
    {"SidebarMetrics", tr("Sidebar"), tr("Display various custom metrics on the sidebar for the CPU, GPU, RAM, IP, and storage used/left."), ""},
    {"UseSI", tr("Use International System of Units"), tr("Display relevant metrics in the SI format."), ""},

    {"ModelUI", tr("Model UI"), tr("Customize the model visualizations on the screen."), "../assets/offroad/icon_calibration.png"},
    {"DynamicPathWidth", tr("Dynamic Path Width"), tr("Have the path width dynamically adjust based on the current engagement state of openpilot."), ""},
    {"HideLeadMarker", tr("Hide Lead Marker"), tr("Hide the lead marker from the onroad UI."), ""},
    {"LaneLinesWidth", tr("Lane Lines"), tr("Adjust the visual thickness of lane lines on your display.\n\nDefault matches the MUTCD average of 4 inches."), ""},
    {"PathEdgeWidth", tr("Path Edges"), tr("Adjust the width of the path edges shown on your UI to represent different driving modes and statuses.\n\nDefault is 20% of the total path.\n\nBlue = Navigation\nLight Blue = 'Always On Lateral'\nGreen = Default\nOrange = 'Experimental Mode'\nRed = 'Traffic Mode'\nYellow = 'Conditional Experimental Mode' Overridden"), ""},
    {"PathWidth", tr("Path Width"), tr("Customize the width of the driving path shown on your UI.\n\nDefault matches the width of a 2019 Lexus ES 350."), ""},
    {"RoadEdgesWidth", tr("Road Edges"), tr("Adjust the visual thickness of road edges on your display.\n\nDefault is 1/2 of the MUTCD average lane line width of 4 inches."), ""},
    {"UnlimitedLength", tr("'Unlimited' Road UI Length"), tr("Extend the display of the path, lane lines, and road edges out as far as the model can see."), ""},

    {"QOLVisuals", tr("Quality of Life"), tr("Miscellaneous quality of life changes to improve your overall openpilot experience."), "../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"BigMap", tr("Big Map"), tr("Increase the size of the map in the onroad UI."), ""},
    {"CameraView", tr("Camera View"), tr("Choose your preferred camera view for the onroad UI. This is purely a visual change and doesn't impact how openpilot drives."), ""},
    {"DriverCamera", tr("Driver Camera On Reverse"), tr("Show the driver camera feed when in reverse."), ""},
    {"HideSpeed", tr("Hide Speed"), tr("Hide the speed indicator in the onroad UI. Additional toggle allows it to be hidden/shown via tapping the speed itself."), ""},
    {"MapStyle", tr("Map Style"), tr("Select a map style to use with navigation."), ""},
    {"StoppedTimer", tr("Stopped Timer"), tr("Display a timer in the onroad UI that indicates how long you've been stopped for."), ""},
    {"WheelSpeed", tr("Use Wheel Speed"), tr("Use the wheel speed instead of the cluster speed in the onroad UI."), ""},

    {"ScreenManagement", tr("Screen Management"), tr("Manage your screen's brightness, timeout settings, and hide onroad UI elements."), "../frogpilot/assets/toggle_icons/icon_light.png"},
    {"HideUIElements", tr("Hide UI Elements"), tr("Hide the selected UI elements from the onroad screen."), ""},
    {"ScreenBrightness", tr("Screen Brightness"), tr("Customize your screen brightness when offroad."), ""},
    {"ScreenBrightnessOnroad", tr("Screen Brightness (Onroad)"), tr("Customize your screen brightness when onroad."), ""},
    {"ScreenRecorder", tr("Screen Recorder"), tr("Enable the ability to record the screen while onroad."), ""},
    {"ScreenTimeout", tr("Screen Timeout"), tr("Customize how long it takes for your screen to turn off."), ""},
    {"ScreenTimeoutOnroad", tr("Screen Timeout (Onroad)"), tr("Customize how long it takes for your screen to turn off when onroad."), ""},
    {"StandbyMode", tr("Standby Mode"), tr("Turn the screen off after your screen times out when onroad, but wake it back up when engagement state changes or important alerts are triggered."), ""},
  };

  for (const auto &[param, title, desc, icon] : visualToggles) {
    AbstractControl *visualToggle;

    if (param == "AlertVolumeControl") {
      FrogPilotParamManageControl *alertVolumeControlToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(alertVolumeControlToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(alertVolumeControlKeys.find(key.c_str()) != alertVolumeControlKeys.end());
        }
      });
      visualToggle = alertVolumeControlToggle;
    } else if (alertVolumeControlKeys.find(param) != alertVolumeControlKeys.end()) {
      if (param == "WarningImmediateVolume") {
        visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 25, 100, std::map<int, QString>(), this, false, "%");
      } else {
        visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, std::map<int, QString>(), this, false, "%");
      }

    } else if (param == "BonusContent") {
      FrogPilotParamManageControl *BonusContentToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(BonusContentToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(bonusContentKeys.find(key.c_str()) != bonusContentKeys.end());
        }
      });
      visualToggle = BonusContentToggle;
    } else if (param == "PersonalizeOpenpilot") {
      FrogPilotParamManageControl *personalizeOpenpilotToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(personalizeOpenpilotToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openSubParentToggle();

        personalizeOpenpilotOpen = true;
        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(personalizeOpenpilotKeys.find(key.c_str()) != personalizeOpenpilotKeys.end());
        }
      });
      visualToggle = personalizeOpenpilotToggle;
    } else if (param == "CustomColors") {
      std::vector<QString> customColorsOptions{tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")};
      manageCustomColorsBtn = new FrogPilotButtonsControl(title, desc, icon, customColorsOptions);

      std::function<QString(QString)> formatColorName = [](QString name) -> QString {
        QChar separator = name.contains('_') ? '_' : '-';
        QStringList parts = name.replace(separator, ' ').split(' ');

        for (int i = 0; i < parts.size(); ++i) {
          parts[i][0] = parts[i][0].toUpper();
        }

        if (separator == '-' && parts.size() > 1) {
          return parts.first() + " (" + parts.last() + ")";
        }

        return parts.join(' ');
      };

      std::function<QString(QString)> formatColorNameForStorage = [](QString name) -> QString {
        name = name.toLower();
        name = name.replace(" (", "-");
        name = name.replace(' ', '_');
        name.remove('(').remove(')');
        return name;
      };

      QObject::connect(manageCustomColorsBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        QDir themesDir{"/data/themes/theme_packs"};
        QFileInfoList dirList = themesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

        QString currentColor = QString::fromStdString(params.get("CustomColors")).replace('_', ' ').replace('-', " (").toLower();
        currentColor[0] = currentColor[0].toUpper();
        for (int i = 1; i < currentColor.length(); i++) {
          if (currentColor[i - 1] == ' ' || currentColor[i - 1] == '(') {
            currentColor[i] = currentColor[i].toUpper();
          }
        }
        if (currentColor.contains(" (")) {
          currentColor.append(')');
        }

        QStringList availableColors;
        for (const QFileInfo &dirInfo : dirList) {
          QString colorSchemeDir = dirInfo.absoluteFilePath();

          if (QDir(colorSchemeDir + "/colors").exists()) {
            availableColors << formatColorName(dirInfo.fileName());
          }
        }
        availableColors.append("Stock");
        std::sort(availableColors.begin(), availableColors.end());

        if (id == 0) {
          QStringList colorSchemesList = availableColors;
          colorSchemesList.removeAll("Stock");
          colorSchemesList.removeAll(currentColor);

          QString colorSchemeToDelete = MultiOptionDialog::getSelection(tr("Select a color scheme to delete"), colorSchemesList, "", this);
          if (!colorSchemeToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' color scheme?").arg(colorSchemeToDelete), tr("Delete"), this)) {
            themeDeleting = true;
            colorsDownloaded = false;

            QString selectedColor = formatColorNameForStorage(colorSchemeToDelete);
            for (const QFileInfo &dirInfo : dirList) {
              if (dirInfo.fileName() == selectedColor) {
                QDir colorDir(dirInfo.absoluteFilePath() + "/colors");
                if (colorDir.exists()) {
                  colorDir.removeRecursively();
                }
              }
            }

            QStringList downloadableColors = QString::fromStdString(params.get("DownloadableColors")).split(",");
            downloadableColors << colorSchemeToDelete;
            downloadableColors.removeDuplicates();
            downloadableColors.removeAll("");
            std::sort(downloadableColors.begin(), downloadableColors.end());

            params.put("DownloadableColors", downloadableColors.join(",").toStdString());
            themeDeleting = false;
          }
        } else if (id == 1) {
          if (manageCustomColorsBtn->getButton(id)->text() == tr("CANCEL")) {
            paramsMemory.putBool("CancelThemeDownload", true);
            cancellingDownload = true;

            QTimer::singleShot(2000, [=]() {
              paramsMemory.putBool("CancelThemeDownload", false);
              cancellingDownload = false;
              colorDownloading = false;
              themeDownloading = false;
            });
          } else {
            QStringList downloadableColors = QString::fromStdString(params.get("DownloadableColors")).split(",");
            QString colorSchemeToDownload = MultiOptionDialog::getSelection(tr("Select a color scheme to download"), downloadableColors, "", this);

            if (!colorSchemeToDownload.isEmpty()) {
              QString convertedColorScheme = formatColorNameForStorage(colorSchemeToDownload);
              paramsMemory.put("ColorToDownload", convertedColorScheme.toStdString());
              downloadStatusLabel->setText("Downloading...");
              paramsMemory.put("ThemeDownloadProgress", "Downloading...");
              colorDownloading = true;
              themeDownloading = true;

              downloadableColors.removeAll(colorSchemeToDownload);
              params.put("DownloadableColors", downloadableColors.join(",").toStdString());
            }
          }
        } else if (id == 2) {
          QString colorSchemeToSelect = MultiOptionDialog::getSelection(tr("Select a color scheme"), availableColors, currentColor, this);
          if (!colorSchemeToSelect.isEmpty()) {
            params.put("CustomColors", formatColorNameForStorage(colorSchemeToSelect).toStdString());
            manageCustomColorsBtn->setValue(colorSchemeToSelect);
            paramsMemory.putBool("UpdateTheme", true);
          }
        }
      });

      QString currentColor = QString::fromStdString(params.get("CustomColors")).replace('_', ' ').replace('-', " (").toLower();
      currentColor[0] = currentColor[0].toUpper();
      for (int i = 1; i < currentColor.length(); i++) {
        if (currentColor[i - 1] == ' ' || currentColor[i - 1] == '(') {
          currentColor[i] = currentColor[i].toUpper();
        }
      }
      if (currentColor.contains(" (")) {
        currentColor.append(')');
      }
      manageCustomColorsBtn->setValue(currentColor);
      visualToggle = reinterpret_cast<AbstractControl*>(manageCustomColorsBtn);
    } else if (param == "CustomIcons") {
      std::vector<QString> customIconsOptions{tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")};
      manageCustomIconsBtn = new FrogPilotButtonsControl(title, desc, icon, customIconsOptions);

      std::function<QString(QString)> formatIconName = [](QString name) -> QString {
        QChar separator = name.contains('_') ? '_' : '-';
        QStringList parts = name.replace(separator, ' ').split(' ');

        for (int i = 0; i < parts.size(); ++i) {
          parts[i][0] = parts[i][0].toUpper();
        }

        if (separator == '-' && parts.size() > 1) {
          return parts.first() + " (" + parts.last() + ")";
        }

        return parts.join(' ');
      };

      std::function<QString(QString)> formatIconNameForStorage = [](QString name) -> QString {
        name = name.toLower();
        name = name.replace(" (", "-");
        name = name.replace(' ', '_');
        name.remove('(').remove(')');
        return name;
      };

      QObject::connect(manageCustomIconsBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        QDir themesDir{"/data/themes/theme_packs"};
        QFileInfoList dirList = themesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

        QString currentIcon = QString::fromStdString(params.get("CustomIcons")).replace('_', ' ').replace('-', " (").toLower();
        currentIcon[0] = currentIcon[0].toUpper();
        for (int i = 1; i < currentIcon.length(); i++) {
          if (currentIcon[i - 1] == ' ' || currentIcon[i - 1] == '(') {
            currentIcon[i] = currentIcon[i].toUpper();
          }
        }
        if (currentIcon.contains(" (")) {
          currentIcon.append(')');
        }

        QStringList availableIcons;
        for (const QFileInfo &dirInfo : dirList) {
          QString iconDir = dirInfo.absoluteFilePath();
          if (QDir(iconDir + "/icons").exists()) {
            availableIcons << formatIconName(dirInfo.fileName());
          }
        }
        availableIcons.append("Stock");
        std::sort(availableIcons.begin(), availableIcons.end());

        if (id == 0) {
          QStringList customIconsList = availableIcons;
          customIconsList.removeAll("Stock");
          customIconsList.removeAll(currentIcon);

          QString iconPackToDelete = MultiOptionDialog::getSelection(tr("Select an icon pack to delete"), customIconsList, "", this);
          if (!iconPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' icon pack?").arg(iconPackToDelete), tr("Delete"), this)) {
            themeDeleting = true;
            iconsDownloaded = false;

            QString selectedIcon = formatIconNameForStorage(iconPackToDelete);
            for (const QFileInfo &dirInfo : dirList) {
              if (dirInfo.fileName() == selectedIcon) {
                QDir iconDir(dirInfo.absoluteFilePath() + "/icons");
                if (iconDir.exists()) {
                  iconDir.removeRecursively();
                }
              }
            }

            QStringList downloadableIcons = QString::fromStdString(params.get("DownloadableIcons")).split(",");
            downloadableIcons << iconPackToDelete;
            downloadableIcons.removeDuplicates();
            downloadableIcons.removeAll("");
            std::sort(downloadableIcons.begin(), downloadableIcons.end());

            params.put("DownloadableIcons", downloadableIcons.join(",").toStdString());
            themeDeleting = false;
          }
        } else if (id == 1) {
          if (manageCustomIconsBtn->getButton(id)->text() == tr("CANCEL")) {
            paramsMemory.putBool("CancelThemeDownload", true);
            cancellingDownload = true;

            QTimer::singleShot(2000, [=]() {
              paramsMemory.putBool("CancelThemeDownload", false);
              cancellingDownload = false;
              iconDownloading = false;
              themeDownloading = false;
            });
          } else {
            QStringList downloadableIcons = QString::fromStdString(params.get("DownloadableIcons")).split(",");
            QString iconPackToDownload = MultiOptionDialog::getSelection(tr("Select an icon pack to download"), downloadableIcons, "", this);

            if (!iconPackToDownload.isEmpty()) {
              QString convertedIconPack = formatIconNameForStorage(iconPackToDownload);
              paramsMemory.put("IconToDownload", convertedIconPack.toStdString());
              downloadStatusLabel->setText("Downloading...");
              paramsMemory.put("ThemeDownloadProgress", "Downloading...");
              iconDownloading = true;
              themeDownloading = true;

              downloadableIcons.removeAll(iconPackToDownload);
              params.put("DownloadableIcons", downloadableIcons.join(",").toStdString());
            }
          }
        } else if (id == 2) {
          QString iconPackToSelect = MultiOptionDialog::getSelection(tr("Select an icon pack"), availableIcons, currentIcon, this);
          if (!iconPackToSelect.isEmpty()) {
            params.put("CustomIcons", formatIconNameForStorage(iconPackToSelect).toStdString());
            manageCustomIconsBtn->setValue(iconPackToSelect);
            paramsMemory.putBool("UpdateTheme", true);
          }
        }
      });

      QString currentIcon = QString::fromStdString(params.get("CustomIcons")).replace('_', ' ').replace('-', " (").toLower();
      currentIcon[0] = currentIcon[0].toUpper();
      for (int i = 1; i < currentIcon.length(); i++) {
        if (currentIcon[i - 1] == ' ' || currentIcon[i - 1] == '(') {
          currentIcon[i] = currentIcon[i].toUpper();
        }
      }
      if (currentIcon.contains(" (")) {
        currentIcon.append(')');
      }
      manageCustomIconsBtn->setValue(currentIcon);
      visualToggle = reinterpret_cast<AbstractControl*>(manageCustomIconsBtn);
    } else if (param == "CustomSignals") {
      std::vector<QString> customSignalsOptions{tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")};
      manageCustomSignalsBtn = new FrogPilotButtonsControl(title, desc, icon, customSignalsOptions);

      std::function<QString(QString)> formatSignalName = [](QString name) -> QString {
        QChar separator = name.contains('_') ? '_' : '-';
        QStringList parts = name.replace(separator, ' ').split(' ');

        for (int i = 0; i < parts.size(); ++i) {
          parts[i][0] = parts[i][0].toUpper();
        }

        if (separator == '-' && parts.size() > 1) {
          return parts.first() + " (" + parts.last() + ")";
        }

        return parts.join(' ');
      };

      std::function<QString(QString)> formatSignalNameForStorage = [](QString name) -> QString {
        name = name.toLower();
        name = name.replace(" (", "-");
        name = name.replace(' ', '_');
        name.remove('(').remove(')');
        return name;
      };

      QObject::connect(manageCustomSignalsBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        QDir themesDir{"/data/themes/theme_packs"};
        QFileInfoList dirList = themesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

        QString currentSignal = QString::fromStdString(params.get("CustomSignals")).replace('_', ' ').replace('-', " (").toLower();
        currentSignal[0] = currentSignal[0].toUpper();
        for (int i = 1; i < currentSignal.length(); i++) {
          if (currentSignal[i - 1] == ' ' || currentSignal[i - 1] == '(') {
            currentSignal[i] = currentSignal[i].toUpper();
          }
        }
        if (currentSignal.contains(" (")) {
          currentSignal.append(')');
        }

        QStringList availableSignals;
        for (const QFileInfo &dirInfo : dirList) {
          QString signalDir = dirInfo.absoluteFilePath();
          if (QDir(signalDir + "/signals").exists()) {
            availableSignals << formatSignalName(dirInfo.fileName());
          }
        }
        availableSignals.append("Stock");
        std::sort(availableSignals.begin(), availableSignals.end());

        if (id == 0) {
          QStringList customSignalsList = availableSignals;
          customSignalsList.removeAll("Stock");
          customSignalsList.removeAll(currentSignal);

          QString signalPackToDelete = MultiOptionDialog::getSelection(tr("Select a signal pack to delete"), customSignalsList, "", this);
          if (!signalPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' signal pack?").arg(signalPackToDelete), tr("Delete"), this)) {
            themeDeleting = true;
            signalsDownloaded = false;

            QString selectedSignal = formatSignalNameForStorage(signalPackToDelete);
            for (const QFileInfo &dirInfo : dirList) {
              if (dirInfo.fileName() == selectedSignal) {
                QDir signalDir(dirInfo.absoluteFilePath() + "/signals");
                if (signalDir.exists()) {
                  signalDir.removeRecursively();
                }
              }
            }

            QStringList downloadableSignals = QString::fromStdString(params.get("DownloadableSignals")).split(",");
            downloadableSignals << signalPackToDelete;
            downloadableSignals.removeDuplicates();
            downloadableSignals.removeAll("");
            std::sort(downloadableSignals.begin(), downloadableSignals.end());

            params.put("DownloadableSignals", downloadableSignals.join(",").toStdString());
            themeDeleting = false;
          }
        } else if (id == 1) {
          if (manageCustomSignalsBtn->getButton(id)->text() == tr("CANCEL")) {
            paramsMemory.putBool("CancelThemeDownload", true);
            cancellingDownload = true;

            QTimer::singleShot(2000, [=]() {
              paramsMemory.putBool("CancelThemeDownload", false);
              cancellingDownload = false;
              signalDownloading = false;
              themeDownloading = false;
            });
          } else {
            QStringList downloadableSignals = QString::fromStdString(params.get("DownloadableSignals")).split(",");
            QString signalPackToDownload = MultiOptionDialog::getSelection(tr("Select a signal pack to download"), downloadableSignals, "", this);

            if (!signalPackToDownload.isEmpty()) {
              QString convertedSignalPack = formatSignalNameForStorage(signalPackToDownload);
              paramsMemory.put("SignalToDownload", convertedSignalPack.toStdString());
              downloadStatusLabel->setText("Downloading...");
              paramsMemory.put("ThemeDownloadProgress", "Downloading...");
              signalDownloading = true;
              themeDownloading = true;

              downloadableSignals.removeAll(signalPackToDownload);
              params.put("DownloadableSignals", downloadableSignals.join(",").toStdString());
            }
          }
        } else if (id == 2) {
          QString signalPackToSelect = MultiOptionDialog::getSelection(tr("Select a signal pack"), availableSignals, currentSignal, this);
          if (!signalPackToSelect.isEmpty()) {
            params.put("CustomSignals", formatSignalNameForStorage(signalPackToSelect).toStdString());
            manageCustomSignalsBtn->setValue(signalPackToSelect);
            paramsMemory.putBool("UpdateTheme", true);
          }
        }
      });

      QString currentSignal = QString::fromStdString(params.get("CustomSignals")).replace('_', ' ').replace('-', " (").toLower();
      currentSignal[0] = currentSignal[0].toUpper();
      for (int i = 1; i < currentSignal.length(); i++) {
        if (currentSignal[i - 1] == ' ' || currentSignal[i - 1] == '(') {
          currentSignal[i] = currentSignal[i].toUpper();
        }
      }
      if (currentSignal.contains(" (")) {
        currentSignal.append(')');
      }
      manageCustomSignalsBtn->setValue(currentSignal);
      visualToggle = reinterpret_cast<AbstractControl*>(manageCustomSignalsBtn);
    } else if (param == "CustomSounds") {
      std::vector<QString> customSoundsOptions{tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")};
      manageCustomSoundsBtn = new FrogPilotButtonsControl(title, desc, icon, customSoundsOptions);

      std::function<QString(QString)> formatSoundName = [](QString name) -> QString {
        QChar separator = name.contains('_') ? '_' : '-';
        QStringList parts = name.replace(separator, ' ').split(' ');

        for (int i = 0; i < parts.size(); ++i) {
          parts[i][0] = parts[i][0].toUpper();
        }

        if (separator == '-' && parts.size() > 1) {
          return parts.first() + " (" + parts.last() + ")";
        }

        return parts.join(' ');
      };

      std::function<QString(QString)> formatSoundNameForStorage = [](QString name) -> QString {
        name = name.toLower();
        name = name.replace(" (", "-");
        name = name.replace(' ', '_');
        name.remove('(').remove(')');
        return name;
      };

      QObject::connect(manageCustomSoundsBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        QDir themesDir{"/data/themes/theme_packs"};
        QFileInfoList dirList = themesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

        QString currentSound = QString::fromStdString(params.get("CustomSounds")).replace('_', ' ').replace('-', " (").toLower();
        currentSound[0] = currentSound[0].toUpper();
        for (int i = 1; i < currentSound.length(); i++) {
          if (currentSound[i - 1] == ' ' || currentSound[i - 1] == '(') {
            currentSound[i] = currentSound[i].toUpper();
          }
        }
        if (currentSound.contains(" (")) {
          currentSound.append(')');
        }

        QStringList availableSounds;
        for (const QFileInfo &dirInfo : dirList) {
          QString soundDir = dirInfo.absoluteFilePath();
          if (QDir(soundDir + "/sounds").exists()) {
            availableSounds << formatSoundName(dirInfo.fileName());
          }
        }
        availableSounds.append("Stock");
        std::sort(availableSounds.begin(), availableSounds.end());

        if (id == 0) {
          QStringList customSoundsList = availableSounds;
          customSoundsList.removeAll("Stock");
          customSoundsList.removeAll(currentSound);

          QString soundSchemeToDelete = MultiOptionDialog::getSelection(tr("Select a sound pack to delete"), customSoundsList, "", this);
          if (!soundSchemeToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' sound scheme?").arg(soundSchemeToDelete), tr("Delete"), this)) {
            themeDeleting = true;
            soundsDownloaded = false;

            QString selectedSound = formatSoundNameForStorage(soundSchemeToDelete);
            for (const QFileInfo &dirInfo : dirList) {
              if (dirInfo.fileName() == selectedSound) {
                QDir soundDir(dirInfo.absoluteFilePath() + "/sounds");
                if (soundDir.exists()) {
                  soundDir.removeRecursively();
                }
              }
            }

            QStringList downloadableSounds = QString::fromStdString(params.get("DownloadableSounds")).split(",");
            downloadableSounds << soundSchemeToDelete;
            downloadableSounds.removeDuplicates();
            downloadableSounds.removeAll("");
            std::sort(downloadableSounds.begin(), downloadableSounds.end());

            params.put("DownloadableSounds", downloadableSounds.join(",").toStdString());
            themeDeleting = false;
          }
        } else if (id == 1) {
          if (manageCustomSoundsBtn->getButton(id)->text() == tr("CANCEL")) {
            paramsMemory.putBool("CancelThemeDownload", true);
            cancellingDownload = true;

            QTimer::singleShot(2000, [=]() {
              paramsMemory.putBool("CancelThemeDownload", false);
              cancellingDownload = false;
              soundDownloading = false;
              themeDownloading = false;
            });
          } else {
            QStringList downloadableSounds = QString::fromStdString(params.get("DownloadableSounds")).split(",");
            QString soundSchemeToDownload = MultiOptionDialog::getSelection(tr("Select a sound pack to download"), downloadableSounds, "", this);

            if (!soundSchemeToDownload.isEmpty()) {
              QString convertedSoundScheme = formatSoundNameForStorage(soundSchemeToDownload);
              paramsMemory.put("SoundToDownload", convertedSoundScheme.toStdString());
              downloadStatusLabel->setText("Downloading...");
              paramsMemory.put("ThemeDownloadProgress", "Downloading...");
              soundDownloading = true;
              themeDownloading = true;

              downloadableSounds.removeAll(soundSchemeToDownload);
              params.put("DownloadableSounds", downloadableSounds.join(",").toStdString());
            }
          }
        } else if (id == 2) {
          QString soundSchemeToSelect = MultiOptionDialog::getSelection(tr("Select a sound scheme"), availableSounds, currentSound, this);
          if (!soundSchemeToSelect.isEmpty()) {
            params.put("CustomSounds", formatSoundNameForStorage(soundSchemeToSelect).toStdString());
            manageCustomSoundsBtn->setValue(soundSchemeToSelect);
            paramsMemory.putBool("UpdateTheme", true);
          }
        }
      });

      QString currentSound = QString::fromStdString(params.get("CustomSounds")).replace('_', ' ').replace('-', " (").toLower();
      currentSound[0] = currentSound[0].toUpper();
      for (int i = 1; i < currentSound.length(); i++) {
        if (currentSound[i - 1] == ' ' || currentSound[i - 1] == '(') {
          currentSound[i] = currentSound[i].toUpper();
        }
      }
      if (currentSound.contains(" (")) {
        currentSound.append(')');
      }
      manageCustomSoundsBtn->setValue(currentSound);
      visualToggle = reinterpret_cast<AbstractControl*>(manageCustomSoundsBtn);
    } else if (param == "WheelIcon") {
      std::vector<QString> wheelIconOptions{tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")};
      manageWheelIconsBtn = new FrogPilotButtonsControl(title, desc, icon, wheelIconOptions);

      std::function<QString(QString)> formatWheelName = [](QString name) -> QString {
        QChar separator = name.contains('_') ? '_' : '-';
        QStringList parts = name.replace(separator, ' ').split(' ');

        for (int i = 0; i < parts.size(); ++i) {
          parts[i][0] = parts[i][0].toUpper();
        }

        if (separator == '-' && parts.size() > 1) {
          return parts.first() + " (" + parts.last() + ")";
        }

        return parts.join(' ');
      };

      std::function<QString(QString)> formatWheelNameForStorage = [](QString name) -> QString {
        name = name.toLower();
        name = name.replace(" (", "-");
        name = name.replace(' ', '_');
        name.remove('(').remove(')');
        return name;
      };

      QObject::connect(manageWheelIconsBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        QDir wheelDir{"/data/themes/steering_wheels"};
        QFileInfoList fileList = wheelDir.entryInfoList(QDir::Files);

        QString currentWheel = QString::fromStdString(params.get("WheelIcon")).replace('_', ' ').replace('-', " (").toLower();
        currentWheel[0] = currentWheel[0].toUpper();
        for (int i = 1; i < currentWheel.length(); i++) {
          if (currentWheel[i - 1] == ' ' || currentWheel[i - 1] == '(') {
            currentWheel[i] = currentWheel[i].toUpper();
          }
        }
        if (currentWheel.contains(" (")) {
          currentWheel.append(')');
        }

        QStringList availableWheels;
        for (const QFileInfo &fileInfo : fileList) {
          QString baseName = fileInfo.completeBaseName();
          QString formattedName = formatWheelName(baseName);
          if (formattedName != "Img Chffr Wheel") {
            availableWheels << formattedName;
          }
        }
        availableWheels.append("Stock");
        availableWheels.append("None");
        std::sort(availableWheels.begin(), availableWheels.end());

        if (id == 0) {
          QStringList steeringWheelList = availableWheels;
          steeringWheelList.removeAll("None");
          steeringWheelList.removeAll("Stock");
          steeringWheelList.removeAll(currentWheel);

          QString imageToDelete = MultiOptionDialog::getSelection(tr("Select a steering wheel to delete"), steeringWheelList, "", this);
          if (!imageToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' steering wheel image?").arg(imageToDelete), tr("Delete"), this)) {
            themeDeleting = true;
            wheelsDownloaded = false;

            QString selectedImage = formatWheelNameForStorage(imageToDelete);
            for (const QFileInfo &fileInfo : fileList) {
              if (fileInfo.completeBaseName() == selectedImage) {
                QFile::remove(fileInfo.filePath());
              }
            }

            QStringList downloadableWheels = QString::fromStdString(params.get("DownloadableWheels")).split(",");
            downloadableWheels << imageToDelete;
            downloadableWheels.removeDuplicates();
            downloadableWheels.removeAll("");
            std::sort(downloadableWheels.begin(), downloadableWheels.end());

            params.put("DownloadableWheels", downloadableWheels.join(",").toStdString());
            themeDeleting = false;
          }
        } else if (id == 1) {
          if (manageWheelIconsBtn->getButton(id)->text() == tr("CANCEL")) {
            paramsMemory.putBool("CancelThemeDownload", true);
            cancellingDownload = true;

            QTimer::singleShot(2000, [=]() {
              paramsMemory.putBool("CancelThemeDownload", false);
              cancellingDownload = false;
              themeDownloading = false;
              wheelDownloading = false;
            });
          } else {
            QStringList downloadableWheels = QString::fromStdString(params.get("DownloadableWheels")).split(",");
            QString wheelToDownload = MultiOptionDialog::getSelection(tr("Select a steering wheel to download"), downloadableWheels, "", this);

            if (!wheelToDownload.isEmpty()) {
              QString convertedImage = formatWheelNameForStorage(wheelToDownload);
              paramsMemory.put("WheelToDownload", convertedImage.toStdString());
              downloadStatusLabel->setText("Downloading...");
              paramsMemory.put("ThemeDownloadProgress", "Downloading...");
              themeDownloading = true;
              wheelDownloading = true;

              downloadableWheels.removeAll(wheelToDownload);
              params.put("DownloadableWheels", downloadableWheels.join(",").toStdString());
            }
          }
        } else if (id == 2) {
          QString imageToSelect = MultiOptionDialog::getSelection(tr("Select a steering wheel"), availableWheels, currentWheel, this);
          if (!imageToSelect.isEmpty()) {
            params.put("WheelIcon", formatWheelNameForStorage(imageToSelect).toStdString());
            manageWheelIconsBtn->setValue(imageToSelect);
            paramsMemory.putBool("UpdateTheme", true);
          }
        }
      });

      QString currentWheel = QString::fromStdString(params.get("WheelIcon")).replace('_', ' ').replace('-', " (").toLower();
      currentWheel[0] = currentWheel[0].toUpper();
      for (int i = 1; i < currentWheel.length(); i++) {
        if (currentWheel[i - 1] == ' ' || currentWheel[i - 1] == '(') {
          currentWheel[i] = currentWheel[i].toUpper();
        }
      }
      if (currentWheel.contains(" (")) {
        currentWheel.append(')');
      }
      manageWheelIconsBtn->setValue(currentWheel);
      visualToggle = reinterpret_cast<AbstractControl*>(manageWheelIconsBtn);
    } else if (param == "DownloadStatusLabel") {
      downloadStatusLabel = new LabelControl(title, "Idle");
      visualToggle = reinterpret_cast<AbstractControl*>(downloadStatusLabel);
    } else if (param == "StartupAlert") {
      std::vector<QString> startupAlertOptions{tr("STOCK"), tr("FROGPILOT"), tr("CUSTOM"), tr("CLEAR")};
      FrogPilotButtonsControl *startupAlertButton = new FrogPilotButtonsControl(title, desc, icon, startupAlertOptions);
      QObject::connect(startupAlertButton, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        int maxLengthTop = 35;
        int maxLengthBottom = 45;

        QString stockTop = "Be ready to take over at any time";
        QString stockBottom = "Always keep hands on wheel and eyes on road";

        QString frogpilotTop = "Hippity hoppity this is my property";
        QString frogpilotBottom = "so I do what I want 🐸";

        QString currentTop = QString::fromStdString(params.get("StartupMessageTop"));
        QString currentBottom = QString::fromStdString(params.get("StartupMessageBottom"));

        if (id == 0) {
          params.put("StartupMessageTop", stockTop.toStdString());
          params.put("StartupMessageBottom", stockBottom.toStdString());
        } else if (id == 1) {
          params.put("StartupMessageTop", frogpilotTop.toStdString());
          params.put("StartupMessageBottom", frogpilotBottom.toStdString());
        } else if (id == 2) {
          QString newTop = InputDialog::getText(tr("Enter your text for the top half"), this, tr("Characters: 0/%1").arg(maxLengthTop), false, -1, currentTop, maxLengthTop).trimmed();
          if (newTop.length() > 0) {
            params.putNonBlocking("StartupMessageTop", newTop.toStdString());
            QString newBottom = InputDialog::getText(tr("Enter your text for the bottom half"), this, tr("Characters: 0/%1").arg(maxLengthBottom), false, -1, currentBottom, maxLengthBottom).trimmed();
            if (newBottom.length() > 0) {
              params.putNonBlocking("StartupMessageBottom", newBottom.toStdString());
            }
          }
        } else if (id == 3) {
          params.remove("StartupMessageTop");
          params.remove("StartupMessageBottom");
        }
      });
      visualToggle = reinterpret_cast<AbstractControl*>(startupAlertButton);

    } else if (param == "CustomAlerts") {
      FrogPilotParamManageControl *customAlertsToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(customAlertsToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedCustomAlertsKeys = customAlertsKeys;

          if (!hasBSM) {
            modifiedCustomAlertsKeys.erase("LoudBlindspotAlert");
          }

          toggle->setVisible(modifiedCustomAlertsKeys.find(key.c_str()) != modifiedCustomAlertsKeys.end());
        }
      });
      visualToggle = customAlertsToggle;

    } else if (param == "CustomUI") {
      FrogPilotParamManageControl *customUIToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(customUIToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(customOnroadUIKeys.find(key.c_str()) != customOnroadUIKeys.end());
        }
      });
      visualToggle = customUIToggle;
    } else if (param == "CustomPaths") {
      std::vector<QString> pathToggles{"AccelerationPath", "AdjacentPath", "BlindSpotPath", "AdjacentPathMetrics"};
      std::vector<QString> pathToggleNames{tr("Acceleration"), tr("Adjacent"), tr("Blind Spot"), tr("Metrics")};
      visualToggle = new FrogPilotParamToggleControl(param, title, desc, icon, pathToggles, pathToggleNames);
    } else if (param == "PedalsOnUI") {
      std::vector<QString> pedalsToggles{"DynamicPedalsOnUI", "StaticPedalsOnUI"};
      std::vector<QString> pedalsToggleNames{tr("Dynamic"), tr("Static")};
      FrogPilotParamToggleControl *pedalsToggle = new FrogPilotParamToggleControl(param, title, desc, icon, pedalsToggles, pedalsToggleNames);
      QObject::connect(pedalsToggle, &FrogPilotParamToggleControl::buttonTypeClicked, this, [this, pedalsToggle](int index) {
        if (index == 0) {
          params.putBool("StaticPedalsOnUI", false);
        } else if (index == 1) {
          params.putBool("DynamicPedalsOnUI", false);
        }

        pedalsToggle->updateButtonStates();
      });
      visualToggle = pedalsToggle;
    } else if (param == "ShowStoppingPoint") {
      std::vector<QString> stoppingPointToggles{"ShowStoppingPointMetrics"};
      std::vector<QString> stoppingPointToggleNames{tr("Show Distance")};
      visualToggle = new FrogPilotParamToggleControl(param, title, desc, icon, stoppingPointToggles, stoppingPointToggleNames);

    } else if (param == "DeveloperUI") {
      FrogPilotParamManageControl *developerUIToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(developerUIToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedDeveloperUIKeys  = developerUIKeys ;

          toggle->setVisible(modifiedDeveloperUIKeys.find(key.c_str()) != modifiedDeveloperUIKeys.end());
        }
      });
      visualToggle = developerUIToggle;
    } else if (param == "BorderMetrics") {
      std::vector<QString> borderToggles{"BlindSpotMetrics", "ShowSteering", "SignalMetrics"};
      std::vector<QString> borderToggleNames{tr("Blind Spot"), tr("Steering Torque"), tr("Turn Signal")};
      visualToggle = new FrogPilotParamToggleControl(param, title, desc, icon, borderToggles, borderToggleNames);
    } else if (param == "LateralMetrics") {
      std::vector<QString> lateralToggles{"TuningInfo"};
      std::vector<QString> lateralToggleNames{tr("Auto Tune")};
      visualToggle = new FrogPilotParamToggleControl(param, title, desc, icon, lateralToggles, lateralToggleNames);
    } else if (param == "LongitudinalMetrics") {
      std::vector<QString> longitudinalToggles{"LeadInfo", "JerkInfo"};
      std::vector<QString> longitudinalToggleNames{tr("Lead Info"), tr("Longitudinal Jerk")};
      visualToggle = new FrogPilotParamToggleControl(param, title, desc, icon, longitudinalToggles, longitudinalToggleNames);
    } else if (param == "NumericalTemp") {
      std::vector<QString> temperatureToggles{"Fahrenheit"};
      std::vector<QString> temperatureToggleNames{tr("Fahrenheit")};
      visualToggle = new FrogPilotParamToggleControl(param, title, desc, icon, temperatureToggles, temperatureToggleNames);
    } else if (param == "SidebarMetrics") {
      std::vector<QString> sidebarMetricsToggles{"ShowCPU", "ShowGPU", "ShowIP", "ShowMemoryUsage", "ShowStorageLeft", "ShowStorageUsed"};
      std::vector<QString> sidebarMetricsToggleNames{tr("CPU"), tr("GPU"), tr("IP"), tr("RAM"), tr("SSD Left"), tr("SSD Used")};
      FrogPilotParamToggleControl *sidebarMetricsToggle = new FrogPilotParamToggleControl(param, title, desc, icon, sidebarMetricsToggles, sidebarMetricsToggleNames, this, 125);
      QObject::connect(sidebarMetricsToggle, &FrogPilotParamToggleControl::buttonTypeClicked, this, [this, sidebarMetricsToggle](int index) {
        if (index == 0) {
          params.putBool("ShowGPU", false);
        } else if (index == 1) {
          params.putBool("ShowCPU", false);
        } else if (index == 3) {
          params.putBool("ShowStorageLeft", false);
          params.putBool("ShowStorageUsed", false);
        } else if (index == 4) {
          params.putBool("ShowMemoryUsage", false);
          params.putBool("ShowStorageUsed", false);
        } else if (index == 5) {
          params.putBool("ShowMemoryUsage", false);
          params.putBool("ShowStorageLeft", false);
        }

        sidebarMetricsToggle->updateButtonStates();
      });
      visualToggle = sidebarMetricsToggle;

    } else if (param == "ModelUI") {
      FrogPilotParamManageControl *modelUIToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(modelUIToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          std::set<QString> modifiedModelUIKeysKeys = modelUIKeys;

          if (!hasOpenpilotLongitudinal || disableOpenpilotLongitudinal) {
            modifiedModelUIKeysKeys.erase("HideLeadMarker");
          }

          toggle->setVisible(modifiedModelUIKeysKeys.find(key.c_str()) != modifiedModelUIKeysKeys.end());
        }
      });
      visualToggle = modelUIToggle;
    } else if (param == "LaneLinesWidth" || param == "RoadEdgesWidth") {
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 24, std::map<int, QString>(), this, false, tr(" inches"));
    } else if (param == "PathEdgeWidth") {
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, std::map<int, QString>(), this, false, tr("%"));
    } else if (param == "PathWidth") {
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 100, std::map<int, QString>(), this, false, tr(" feet"), 10);

    } else if (param == "QOLVisuals") {
      FrogPilotParamManageControl *qolToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(qolToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(qolKeys.find(key.c_str()) != qolKeys.end());
        }
      });
      visualToggle = qolToggle;
    } else if (param == "CameraView") {
      std::vector<QString> cameraOptions{tr("Auto"), tr("Driver"), tr("Standard"), tr("Wide")};
      FrogPilotButtonParamControl *preferredCamera = new FrogPilotButtonParamControl(param, title, desc, icon, cameraOptions);
      visualToggle = preferredCamera;
    } else if (param == "BigMap") {
      std::vector<QString> mapToggles{"FullMap"};
      std::vector<QString> mapToggleNames{tr("Full Map")};
      visualToggle = new FrogPilotParamToggleControl(param, title, desc, icon, mapToggles, mapToggleNames);
    } else if (param == "HideSpeed") {
      std::vector<QString> hideSpeedToggles{"HideSpeedUI"};
      std::vector<QString> hideSpeedToggleNames{tr("Control Via UI")};
      visualToggle = new FrogPilotParamToggleControl(param, title, desc, icon, hideSpeedToggles, hideSpeedToggleNames);
    } else if (param == "MapStyle") {
      QMap<int, QString> styleMap = {
        {0, tr("Stock openpilot")},
        {1, tr("Mapbox Streets")},
        {2, tr("Mapbox Outdoors")},
        {3, tr("Mapbox Light")},
        {4, tr("Mapbox Dark")},
        {5, tr("Mapbox Satellite")},
        {6, tr("Mapbox Satellite Streets")},
        {7, tr("Mapbox Navigation Day")},
        {8, tr("Mapbox Navigation Night")},
        {9, tr("Mapbox Traffic Night")},
        {10, tr("mike854's (Satellite hybrid)")},
      };

      QStringList styles = styleMap.values();
      ButtonControl *mapStyleButton = new ButtonControl(title, tr("SELECT"), desc);
      QObject::connect(mapStyleButton, &ButtonControl::clicked, [=]() {
        QStringList styles = styleMap.values();
        QString selection = MultiOptionDialog::getSelection(tr("Select a map style"), styles, "", this);
        if (!selection.isEmpty()) {
          int selectedStyle = styleMap.key(selection);
          params.putIntNonBlocking("MapStyle", selectedStyle);
          mapStyleButton->setValue(selection);
          updateFrogPilotToggles();
        }
      });

      int currentStyle = params.getInt("MapStyle");
      mapStyleButton->setValue(styleMap[currentStyle]);

      visualToggle = mapStyleButton;

    } else if (param == "ScreenManagement") {
      FrogPilotParamManageControl *screenToggle = new FrogPilotParamManageControl(param, title, desc, icon, this);
      QObject::connect(screenToggle, &FrogPilotParamManageControl::manageButtonClicked, this, [this]() {
        openParentToggle();

        for (auto &[key, toggle] : toggles) {
          toggle->setVisible(screenKeys.find(key.c_str()) != screenKeys.end());
        }
      });
      visualToggle = screenToggle;
    } else if (param == "HideUIElements") {
      std::vector<QString> uiElementsToggles{"HideAlerts", "HideMapIcon", "HideMaxSpeed"};
      std::vector<QString> uiElementsToggleNames{tr("Alerts"), tr("Map Icon"), tr("Max Speed")};
      visualToggle = new FrogPilotParamToggleControl(param, title, desc, icon, uiElementsToggles, uiElementsToggleNames);
    } else if (param == "ScreenBrightness" || param == "ScreenBrightnessOnroad") {
      std::map<int, QString> brightnessLabels;
      if (param == "ScreenBrightnessOnroad") {
        for (int i = 0; i <= 101; i++) {
          brightnessLabels[i] = (i == 0) ? tr("Screen Off") : (i == 101) ? tr("Auto") : QString::number(i) + "%";
        }
        visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 0, 101, brightnessLabels, this, false);
      } else {
        for (int i = 1; i <= 101; i++) {
          brightnessLabels[i] = (i == 101) ? tr("Auto") : QString::number(i) + "%";
        }
        visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 1, 101, brightnessLabels, this, false);
      }
    } else if (param == "ScreenTimeout" || param == "ScreenTimeoutOnroad") {
      visualToggle = new FrogPilotParamValueControl(param, title, desc, icon, 5, 60, std::map<int, QString>(), this, false, tr(" seconds"));

    } else {
      visualToggle = new ParamControl(param, title, desc, icon, this);
    }

    addItem(visualToggle);
    toggles[param.toStdString()] = visualToggle;

    QObject::connect(static_cast<ToggleControl*>(visualToggle), &ToggleControl::toggleFlipped, &updateFrogPilotToggles);
    QObject::connect(static_cast<FrogPilotParamToggleControl*>(visualToggle), &FrogPilotParamToggleControl::buttonTypeClicked, &updateFrogPilotToggles);
    QObject::connect(static_cast<FrogPilotParamValueControl*>(visualToggle), &FrogPilotParamValueControl::valueChanged, [this]() {
      bool screen_management = params.getBool("ScreenManagement");
      if (!started) {
        uiState()->scene.screen_brightness = screen_management ? params.getInt("ScreenBrightness") : 101;
      } else {
        uiState()->scene.screen_brightness_onroad = screen_management ? params.getInt("ScreenBrightnessOnroad") : 101;
      }
      updateFrogPilotToggles();
    });

    QObject::connect(visualToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });

    QObject::connect(static_cast<FrogPilotParamManageControl*>(visualToggle), &FrogPilotParamManageControl::manageButtonClicked, [this]() {
      update();
    });
  }

  QObject::connect(parent, &SettingsWindow::closeParentToggle, this, &FrogPilotVisualsPanel::hideToggles);
  QObject::connect(parent, &SettingsWindow::closeSubParentToggle, this, &FrogPilotVisualsPanel::hideSubToggles);
  QObject::connect(parent, &SettingsWindow::updateMetric, this, &FrogPilotVisualsPanel::updateMetric);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &FrogPilotVisualsPanel::updateCarToggles);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotVisualsPanel::updateState);

  updateMetric();
}

void FrogPilotVisualsPanel::showEvent(QShowEvent *event) {
  disableOpenpilotLongitudinal = params.getBool("DisableOpenpilotLongitudinal");

  colorsDownloaded = params.get("DownloadableColors").empty();
  iconsDownloaded = params.get("DownloadableIcons").empty();
  signalsDownloaded = params.get("DownloadableSignals").empty();
  soundsDownloaded = params.get("DownloadableSounds").empty();
  wheelsDownloaded = params.get("DownloadableWheels").empty();
}

void FrogPilotVisualsPanel::updateState(const UIState &s) {
  if (!isVisible()) return;

  if (personalizeOpenpilotOpen) {
    if (themeDownloading) {
      QString progress = QString::fromStdString(paramsMemory.get("ThemeDownloadProgress"));
      bool downloadFailed = progress.contains(QRegularExpression("cancelled|exists|Failed|offline", QRegularExpression::CaseInsensitiveOption));

      if (progress != "Downloading...") {
        downloadStatusLabel->setText(progress);
      }

      if (progress == "Downloaded!" || downloadFailed) {
        QTimer::singleShot(2000, [=]() {
          if (!themeDownloading) {
            downloadStatusLabel->setText("Idle");
          }
        });
        paramsMemory.remove("ThemeDownloadProgress");
        colorDownloading = false;
        iconDownloading = false;
        signalDownloading = false;
        soundDownloading = false;
        themeDownloading = false;
        wheelDownloading = false;

        colorsDownloaded = params.get("DownloadableColors").empty();
        iconsDownloaded = params.get("DownloadableIcons").empty();
        signalsDownloaded = params.get("DownloadableSignals").empty();
        soundsDownloaded = params.get("DownloadableSounds").empty();
        wheelsDownloaded = params.get("DownloadableWheels").empty();
      }
    }

    manageCustomColorsBtn->setText(1, colorDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    manageCustomColorsBtn->setButtonEnabled(0, !themeDeleting && !themeDownloading);
    manageCustomColorsBtn->setButtonEnabled(1, s.scene.online && (!themeDownloading || colorDownloading) && !cancellingDownload && !themeDeleting && !colorsDownloaded);
    manageCustomColorsBtn->setButtonEnabled(2, !themeDeleting && !themeDownloading);

    manageCustomIconsBtn->setText(1, iconDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    manageCustomIconsBtn->setButtonEnabled(0, !themeDeleting && !themeDownloading);
    manageCustomIconsBtn->setButtonEnabled(1, s.scene.online && (!themeDownloading || iconDownloading) && !cancellingDownload && !themeDeleting && !iconsDownloaded);
    manageCustomIconsBtn->setButtonEnabled(2, !themeDeleting && !themeDownloading);

    manageCustomSignalsBtn->setText(1, signalDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    manageCustomSignalsBtn->setButtonEnabled(0, !themeDeleting && !themeDownloading);
    manageCustomSignalsBtn->setButtonEnabled(1, s.scene.online && (!themeDownloading || signalDownloading) && !cancellingDownload && !themeDeleting && !signalsDownloaded);
    manageCustomSignalsBtn->setButtonEnabled(2, !themeDeleting && !themeDownloading);

    manageCustomSoundsBtn->setText(1, soundDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    manageCustomSoundsBtn->setButtonEnabled(0, !themeDeleting && !themeDownloading);
    manageCustomSoundsBtn->setButtonEnabled(1, s.scene.online && (!themeDownloading || soundDownloading) && !cancellingDownload && !themeDeleting && !soundsDownloaded);
    manageCustomSoundsBtn->setButtonEnabled(2, !themeDeleting && !themeDownloading);

    manageWheelIconsBtn->setText(1, wheelDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    manageWheelIconsBtn->setButtonEnabled(0, !themeDeleting && !themeDownloading);
    manageWheelIconsBtn->setButtonEnabled(1, s.scene.online && (!themeDownloading || wheelDownloading) && !cancellingDownload && !themeDeleting && !wheelsDownloaded);
    manageWheelIconsBtn->setButtonEnabled(2, !themeDeleting && !themeDownloading);
  }

  started = s.scene.started;
}

void FrogPilotVisualsPanel::updateCarToggles() {
  auto carParams = params.get("CarParamsPersistent");
  if (!carParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(carParams.data(), carParams.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();
    auto carName = CP.getCarName();

    hasAutoTune = (carName == "hyundai" || carName == "toyota") && CP.getLateralTuning().which() == cereal::CarParams::LateralTuning::TORQUE;
    hasBSM = CP.getEnableBsm();
    hasOpenpilotLongitudinal = hasLongitudinalControl(CP);
  } else {
    hasAutoTune = true;
    hasBSM = true;
    hasOpenpilotLongitudinal = true;
  }

  hideToggles();
}

void FrogPilotVisualsPanel::updateMetric() {
  bool previousIsMetric = isMetric;
  isMetric = params.getBool("IsMetric");

  if (isMetric != previousIsMetric) {
    double distanceConversion = isMetric ? INCH_TO_CM : CM_TO_INCH;
    double speedConversion = isMetric ? FOOT_TO_METER : METER_TO_FOOT;

    params.putIntNonBlocking("LaneLinesWidth", std::nearbyint(params.getInt("LaneLinesWidth") * distanceConversion));
    params.putIntNonBlocking("RoadEdgesWidth", std::nearbyint(params.getInt("RoadEdgesWidth") * distanceConversion));

    params.putIntNonBlocking("PathWidth", std::nearbyint(params.getInt("PathWidth") * speedConversion));
  }

  FrogPilotParamValueControl *laneLinesWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["LaneLinesWidth"]);
  FrogPilotParamValueControl *roadEdgesWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["RoadEdgesWidth"]);
  FrogPilotParamValueControl *pathWidthToggle = static_cast<FrogPilotParamValueControl*>(toggles["PathWidth"]);

  if (isMetric) {
    laneLinesWidthToggle->setDescription(tr("Customize the lane line width.\n\nDefault matches the Vienna average of 10 centimeters."));
    roadEdgesWidthToggle->setDescription(tr("Customize the road edges width.\n\nDefault is 1/2 of the Vienna average lane line width of 10 centimeters."));

    laneLinesWidthToggle->updateControl(0, 60, tr(" centimeters"));
    roadEdgesWidthToggle->updateControl(0, 60, tr(" centimeters"));

    pathWidthToggle->updateControl(0, 30, tr(" meters"), 10);
  } else {
    laneLinesWidthToggle->setDescription(tr("Customize the lane line width.\n\nDefault matches the MUTCD average of 4 inches."));
    roadEdgesWidthToggle->setDescription(tr("Customize the road edges width.\n\nDefault is 1/2 of the MUTCD average lane line width of 4 inches."));

    laneLinesWidthToggle->updateControl(0, 24, tr(" inches"));
    roadEdgesWidthToggle->updateControl(0, 24, tr(" inches"));

    pathWidthToggle->updateControl(0, 100, tr(" feet"), 10);
  }

  laneLinesWidthToggle->refresh();
  roadEdgesWidthToggle->refresh();
}

void FrogPilotVisualsPanel::hideToggles() {
  personalizeOpenpilotOpen = false;

  for (auto &[key, toggle] : toggles) {
    bool subToggles = alertVolumeControlKeys.find(key.c_str()) != alertVolumeControlKeys.end() ||
                      bonusContentKeys.find(key.c_str()) != bonusContentKeys.end() ||
                      customAlertsKeys.find(key.c_str()) != customAlertsKeys.end() ||
                      customOnroadUIKeys.find(key.c_str()) != customOnroadUIKeys.end() ||
                      personalizeOpenpilotKeys.find(key.c_str()) != personalizeOpenpilotKeys.end() ||
                      developerUIKeys.find(key.c_str()) != developerUIKeys.end() ||
                      modelUIKeys.find(key.c_str()) != modelUIKeys.end() ||
                      qolKeys.find(key.c_str()) != qolKeys.end() ||
                      screenKeys.find(key.c_str()) != screenKeys.end();
    toggle->setVisible(!subToggles);
  }

  update();
}

void FrogPilotVisualsPanel::hideSubToggles() {
  if (personalizeOpenpilotOpen) {
    for (auto &[key, toggle] : toggles) {
      bool isVisible = bonusContentKeys.find(key.c_str()) != bonusContentKeys.end();
      toggle->setVisible(isVisible);
    }
  }

  update();
}
