#include "selfdrive/frogpilot/ui/qt/offroad/theme_settings.h"

FrogPilotThemesPanel::FrogPilotThemesPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> themeToggles {
    {"PersonalizeOpenpilot", tr("Custom Theme"), tr("Custom openpilot themes."), "../frogpilot/assets/toggle_icons/frog.png"},
    {"CustomColors", tr("Color Scheme"), tr("Changes out openpilot's color scheme.\n\nWant to submit your own color scheme? Share it in the 'custom-themes' channel on the FrogPilot Discord!"), ""},
    {"CustomDistanceIcons", "Distance Button", "Changes out openpilot's distance button icons.\n\nWant to submit your own icon pack? Share it in the 'custom-themes' channel on the FrogPilot Discord!", ""},
    {"CustomIcons", tr("Icon Pack"), tr("Changes out openpilot's icon pack.\n\nWant to submit your own icons? Share them in the 'custom-themes' channel on the FrogPilot Discord!"), ""},
    {"CustomSounds", tr("Sound Pack"), tr("Changes out openpilot's sound effects.\n\nWant to submit your own sounds? Share them in the 'custom-themes' channel on the FrogPilot Discord!"), ""},
    {"WheelIcon", tr("Steering Wheel"), tr("Enables a custom steering wheel icon in the top right of the screen."), ""},
    {"CustomSignals", tr("Turn Signal Animation"), tr("Enables themed turn signal animations.\n\nWant to submit your own animations? Share them in the 'custom-themes' channel on the FrogPilot Discord!"), ""},
    {"DownloadStatusLabel", tr("Download Status"), "", ""},

    {"HolidayThemes", tr("Holiday Themes"), tr("Changes the openpilot theme based on the current holiday. Minor holidays last one day, while major holidays (Easter, Christmas, Halloween, etc.) last the entire week."), "../frogpilot/assets/toggle_icons/icon_calendar.png"},

    {"RandomEvents", tr("Random Events"), tr("Enables random cosmetic events that happen during certain driving conditions. These events are purely for fun and don't affect driving controls!"), "../frogpilot/assets/toggle_icons/icon_random.png"},

    {"StartupAlert", tr("Startup Alert"), tr("Controls the text of the 'Startup' alert message that appears when you start the drive."), "../frogpilot/assets/toggle_icons/icon_message.png"}
  };

  for (const auto &[param, title, desc, icon] : themeToggles) {
    AbstractControl *themeToggle;

    if (param == "PersonalizeOpenpilot") {
      FrogPilotParamManageControl *personalizeOpenpilotToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(personalizeOpenpilotToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        personalizeOpenpilotOpen = true;
        showToggles(customThemeKeys);
      });
      themeToggle = personalizeOpenpilotToggle;
    } else if (param == "CustomColors") {
      manageCustomColorsBtn = new FrogPilotButtonsControl(title, desc, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});

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
        for (int i = 1; i < currentColor.length(); ++i) {
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
          if (colorDownloading) {
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
      for (int i = 1; i < currentColor.length(); ++i) {
        if (currentColor[i - 1] == ' ' || currentColor[i - 1] == '(') {
          currentColor[i] = currentColor[i].toUpper();
        }
      }
      if (currentColor.contains(" (")) {
        currentColor.append(')');
      }
      manageCustomColorsBtn->setValue(currentColor);
      themeToggle = manageCustomColorsBtn;
    } else if (param == "CustomDistanceIcons") {
      manageDistanceIconsBtn = new FrogPilotButtonsControl(title, desc, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});

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

      QObject::connect(manageDistanceIconsBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        QDir themesDir{"/data/themes/distance_icons"};
        QFileInfoList dirList = themesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

        QString currentDistanceIcon = QString::fromStdString(params.get("CustomDistanceIcons")).replace('_', ' ').replace('-', " (").toLower();
        currentDistanceIcon[0] = currentDistanceIcon[0].toUpper();
        for (int i = 1; i < currentDistanceIcon.length(); ++i) {
          if (currentDistanceIcon[i - 1] == ' ' || currentDistanceIcon[i - 1] == '(') {
            currentDistanceIcon[i] = currentDistanceIcon[i].toUpper();
          }
        }
        if (currentDistanceIcon.contains(" (")) {
          currentDistanceIcon.append(')');
        }

        QStringList availableIcons;
        for (const QFileInfo &dirInfo : dirList) {
          QString iconPackDir = dirInfo.absoluteFilePath();

          availableIcons << formatIconName(dirInfo.fileName());
        }
        availableIcons.append("Stock");
        std::sort(availableIcons.begin(), availableIcons.end());

        if (id == 0) {
          QStringList iconPackList = availableIcons;
          iconPackList.removeAll("Stock");
          iconPackList.removeAll(currentDistanceIcon);

          QString iconPackToDelete = MultiOptionDialog::getSelection(tr("Select an icon pack to delete"), iconPackList, "", this);
          if (!iconPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' icon pack?").arg(iconPackToDelete), tr("Delete"), this)) {
            themeDeleting = true;
            distanceIconsDownloaded = false;

            QString selectedIconPack = formatIconNameForStorage(iconPackToDelete);
            for (const QFileInfo &dirInfo : dirList) {
              if (dirInfo.fileName() == selectedIconPack) {
                QDir iconPackDir(dirInfo.absoluteFilePath());
                if (iconPackDir.exists()) {
                  iconPackDir.removeRecursively();
                }
              }
            }

            QStringList downloadableIcons = QString::fromStdString(params.get("DownloadableDistanceIcons")).split(",");
            downloadableIcons << iconPackToDelete;
            downloadableIcons.removeDuplicates();
            downloadableIcons.removeAll("");
            std::sort(downloadableIcons.begin(), downloadableIcons.end());

            params.put("DownloadableDistanceIcons", downloadableIcons.join(",").toStdString());
            themeDeleting = false;
          }
        } else if (id == 1) {
          if (distanceIconDownloading) {
            paramsMemory.putBool("CancelThemeDownload", true);
            cancellingDownload = true;

            QTimer::singleShot(2000, [=]() {
              paramsMemory.putBool("CancelThemeDownload", false);
              cancellingDownload = false;
              distanceIconDownloading = false;
              themeDownloading = false;
            });
          } else {
            QStringList downloadableIcons = QString::fromStdString(params.get("DownloadableDistanceIcons")).split(",");
            QString iconPackToDownload = MultiOptionDialog::getSelection(tr("Select an icon pack to download"), downloadableIcons, "", this);

            if (!iconPackToDownload.isEmpty()) {
              QString convertedIconPack = formatIconNameForStorage(iconPackToDownload);
              paramsMemory.put("DistanceIconToDownload", convertedIconPack.toStdString());
              downloadStatusLabel->setText("Downloading...");
              paramsMemory.put("ThemeDownloadProgress", "Downloading...");
              distanceIconDownloading = true;
              themeDownloading = true;

              downloadableIcons.removeAll(iconPackToDownload);
              params.put("DownloadableDistanceIcons", downloadableIcons.join(",").toStdString());
            }
          }
        } else if (id == 2) {
          QString iconPackToSelect = MultiOptionDialog::getSelection(tr("Select an icon pack"), availableIcons, currentDistanceIcon, this);
          if (!iconPackToSelect.isEmpty()) {
            params.put("CustomDistanceIcons", formatIconNameForStorage(iconPackToSelect).toStdString());
            manageDistanceIconsBtn->setValue(iconPackToSelect);
            paramsMemory.putBool("UpdateTheme", true);
          }
        }
      });

      QString currentDistanceIcon = QString::fromStdString(params.get("CustomDistanceIcons")).replace('_', ' ').replace('-', " (").toLower();
      currentDistanceIcon[0] = currentDistanceIcon[0].toUpper();
      for (int i = 1; i < currentDistanceIcon.length(); ++i) {
        if (currentDistanceIcon[i - 1] == ' ' || currentDistanceIcon[i - 1] == '(') {
          currentDistanceIcon[i] = currentDistanceIcon[i].toUpper();
        }
      }
      if (currentDistanceIcon.contains(" (")) {
        currentDistanceIcon.append(')');
      }
      manageDistanceIconsBtn->setValue(currentDistanceIcon);
      themeToggle = reinterpret_cast<AbstractControl*>(manageDistanceIconsBtn);
    } else if (param == "CustomIcons") {
      manageCustomIconsBtn = new FrogPilotButtonsControl(title, desc, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});

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
        for (int i = 1; i < currentIcon.length(); ++i) {
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
          if (iconDownloading) {
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
      for (int i = 1; i < currentIcon.length(); ++i) {
        if (currentIcon[i - 1] == ' ' || currentIcon[i - 1] == '(') {
          currentIcon[i] = currentIcon[i].toUpper();
        }
      }
      if (currentIcon.contains(" (")) {
        currentIcon.append(')');
      }
      manageCustomIconsBtn->setValue(currentIcon);
      themeToggle = manageCustomIconsBtn;
    } else if (param == "CustomSignals") {
      manageCustomSignalsBtn = new FrogPilotButtonsControl(title, desc, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});

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
        for (int i = 1; i < currentSignal.length(); ++i) {
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
          if (signalDownloading) {
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
      for (int i = 1; i < currentSignal.length(); ++i) {
        if (currentSignal[i - 1] == ' ' || currentSignal[i - 1] == '(') {
          currentSignal[i] = currentSignal[i].toUpper();
        }
      }
      if (currentSignal.contains(" (")) {
        currentSignal.append(')');
      }
      manageCustomSignalsBtn->setValue(currentSignal);
      themeToggle = manageCustomSignalsBtn;
    } else if (param == "CustomSounds") {
      manageCustomSoundsBtn = new FrogPilotButtonsControl(title, desc, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});

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
        for (int i = 1; i < currentSound.length(); ++i) {
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
          if (soundDownloading) {
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
      for (int i = 1; i < currentSound.length(); ++i) {
        if (currentSound[i - 1] == ' ' || currentSound[i - 1] == '(') {
          currentSound[i] = currentSound[i].toUpper();
        }
      }
      if (currentSound.contains(" (")) {
        currentSound.append(')');
      }
      manageCustomSoundsBtn->setValue(currentSound);
      themeToggle = manageCustomSoundsBtn;
    } else if (param == "WheelIcon") {
      manageWheelIconsBtn = new FrogPilotButtonsControl(title, desc, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});

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
        for (int i = 1; i < currentWheel.length(); ++i) {
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
          if (wheelDownloading) {
            paramsMemory.putBool("CancelThemeDownload", true);
            cancellingDownload = true;

            QTimer::singleShot(2000, [=]() {
              paramsMemory.putBool("CancelThemeDownload", false);
              cancellingDownload = false;
              wheelDownloading = false;
              themeDownloading = false;
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
      for (int i = 1; i < currentWheel.length(); ++i) {
        if (currentWheel[i - 1] == ' ' || currentWheel[i - 1] == '(') {
          currentWheel[i] = currentWheel[i].toUpper();
        }
      }
      if (currentWheel.contains(" (")) {
        currentWheel.append(')');
      }
      manageWheelIconsBtn->setValue(currentWheel);
      themeToggle = manageWheelIconsBtn;
    } else if (param == "DownloadStatusLabel") {
      downloadStatusLabel = new LabelControl(title, "Idle");
      themeToggle = reinterpret_cast<AbstractControl*>(downloadStatusLabel);
    } else if (param == "StartupAlert") {
      FrogPilotButtonsControl *startupAlertButton = new FrogPilotButtonsControl(title, desc, {tr("STOCK"), tr("FROGPILOT"), tr("CUSTOM"), tr("CLEAR")}, false, true, icon);
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
          QString newTop = InputDialog::getText(tr("Enter the text for the top half"), this, tr("Characters: 0/%1").arg(maxLengthTop), false, -1, currentTop, maxLengthTop).trimmed();
          if (newTop.length() > 0) {
            params.putNonBlocking("StartupMessageTop", newTop.toStdString());
            QString newBottom = InputDialog::getText(tr("Enter the text for the bottom half"), this, tr("Characters: 0/%1").arg(maxLengthBottom), false, -1, currentBottom, maxLengthBottom).trimmed();
            if (newBottom.length() > 0) {
              params.putNonBlocking("StartupMessageBottom", newBottom.toStdString());
            }
          }
        } else if (id == 3) {
          params.remove("StartupMessageTop");
          params.remove("StartupMessageBottom");
        }

        updateFrogPilotToggles();
      });
      themeToggle = startupAlertButton;

    } else {
      themeToggle = new ParamControl(param, title, desc, icon);
    }

    addItem(themeToggle);
    toggles[param] = themeToggle;

    makeConnections(themeToggle);

    if (FrogPilotParamManageControl *frogPilotManageToggle = qobject_cast<FrogPilotParamManageControl*>(themeToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotParamManageControl::manageButtonClicked, this, &FrogPilotThemesPanel::openParentToggle);
    }

    QObject::connect(themeToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  QObject::connect(static_cast<ToggleControl*>(toggles["HolidayThemes"]), &ToggleControl::toggleFlipped, [this] {
    paramsMemory.putBool("UpdateTheme", true);
  });

  QObject::connect(static_cast<ToggleControl*>(toggles["PersonalizeOpenpilot"]), &ToggleControl::toggleFlipped, [this] {
    paramsMemory.putBool("UpdateTheme", true);
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, this, &FrogPilotThemesPanel::hideToggles);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotThemesPanel::updateState);
}

void FrogPilotThemesPanel::showEvent(QShowEvent *event) {
  colorsDownloaded = params.get("DownloadableColors").empty();
  distanceIconsDownloaded = params.get("DownloadableDistanceIcons").empty();
  iconsDownloaded = params.get("DownloadableIcons").empty();
  signalsDownloaded = params.get("DownloadableSignals").empty();
  soundsDownloaded = params.get("DownloadableSounds").empty();
  wheelsDownloaded = params.get("DownloadableWheels").empty();

  customizationLevel = parent->customizationLevel;

  toggles["RandomEvents"]->setVisible(customizationLevel != 0);
  toggles["StartupAlert"]->setVisible(customizationLevel == 2);
}

void FrogPilotThemesPanel::updateState(const UIState &s) {
  if (!isVisible()) {
    return;
  }

  uiState()->scene.keep_screen_on = personalizeOpenpilotOpen && themeDownloading;

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
        distanceIconDownloading = false;
        iconDownloading = false;
        signalDownloading = false;
        soundDownloading = false;
        themeDownloading = false;
        wheelDownloading = false;

        colorsDownloaded = params.get("DownloadableColors").empty();
        distanceIconsDownloaded = params.get("DownloadableDistanceIcons").empty();
        iconsDownloaded = params.get("DownloadableIcons").empty();
        signalsDownloaded = params.get("DownloadableSignals").empty();
        soundsDownloaded = params.get("DownloadableSounds").empty();
        wheelsDownloaded = params.get("DownloadableWheels").empty();
      }
    }

    manageCustomColorsBtn->setText(1, colorDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    manageCustomColorsBtn->setEnabledButtons(0, !themeDeleting && !themeDownloading);
    manageCustomColorsBtn->setEnabledButtons(1, s.scene.online && (!themeDownloading || colorDownloading) && !cancellingDownload && !themeDeleting && !colorsDownloaded);
    manageCustomColorsBtn->setEnabledButtons(2, !themeDeleting && !themeDownloading);

    manageCustomIconsBtn->setText(1, iconDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    manageCustomIconsBtn->setEnabledButtons(0, !themeDeleting && !themeDownloading);
    manageCustomIconsBtn->setEnabledButtons(1, s.scene.online && (!themeDownloading || iconDownloading) && !cancellingDownload && !themeDeleting && !iconsDownloaded);
    manageCustomIconsBtn->setEnabledButtons(2, !themeDeleting && !themeDownloading);

    manageCustomSignalsBtn->setText(1, signalDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    manageCustomSignalsBtn->setEnabledButtons(0, !themeDeleting && !themeDownloading);
    manageCustomSignalsBtn->setEnabledButtons(1, s.scene.online && (!themeDownloading || signalDownloading) && !cancellingDownload && !themeDeleting && !signalsDownloaded);
    manageCustomSignalsBtn->setEnabledButtons(2, !themeDeleting && !themeDownloading);

    manageCustomSoundsBtn->setText(1, soundDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    manageCustomSoundsBtn->setEnabledButtons(0, !themeDeleting && !themeDownloading);
    manageCustomSoundsBtn->setEnabledButtons(1, s.scene.online && (!themeDownloading || soundDownloading) && !cancellingDownload && !themeDeleting && !soundsDownloaded);
    manageCustomSoundsBtn->setEnabledButtons(2, !themeDeleting && !themeDownloading);

    manageDistanceIconsBtn->setText(1, distanceIconDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    manageDistanceIconsBtn->setEnabledButtons(0, !themeDeleting && !themeDownloading);
    manageDistanceIconsBtn->setEnabledButtons(1, s.scene.online && (!themeDownloading || distanceIconDownloading) && !cancellingDownload && !themeDeleting && !distanceIconsDownloaded);
    manageDistanceIconsBtn->setEnabledButtons(2, !themeDeleting && !themeDownloading);

    manageWheelIconsBtn->setText(1, wheelDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
    manageWheelIconsBtn->setEnabledButtons(0, !themeDeleting && !themeDownloading);
    manageWheelIconsBtn->setEnabledButtons(1, s.scene.online && (!themeDownloading || wheelDownloading) && !cancellingDownload && !themeDeleting && !wheelsDownloaded);
    manageWheelIconsBtn->setEnabledButtons(2, !themeDeleting && !themeDownloading);
  }
}

void FrogPilotThemesPanel::showToggles(const std::set<QString> &keys) {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(keys.find(key) != keys.end());
  }

  setUpdatesEnabled(true);
  update();
}

void FrogPilotThemesPanel::hideToggles() {
  setUpdatesEnabled(false);

  personalizeOpenpilotOpen = false;

  for (auto &[key, toggle] : toggles) {
    bool subToggles = customThemeKeys.find(key) != customThemeKeys.end();

    toggle->setVisible(!subToggles);
  }

  toggles["RandomEvents"]->setVisible(customizationLevel != 0);
  toggles["StartupAlert"]->setVisible(customizationLevel == 2);

  setUpdatesEnabled(true);
  update();
}
