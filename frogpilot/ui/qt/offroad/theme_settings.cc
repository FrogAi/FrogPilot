#include "frogpilot/ui/qt/offroad/theme_settings.h"

bool isUserCreatedTheme(const QString &themeName) {
  return themeName.endsWith("-user_created");
}

void updateAssetParam(const QString &assetParam, Params &params, const QString &value, bool add) {
  QStringList assets = QString::fromStdString(params.get(assetParam.toStdString())).split(",", QString::SkipEmptyParts);
  if (add) {
    if (!assets.contains(value)) {
      assets.append(value);
    }
  } else {
    assets.removeAll(value);
  }
  assets.sort();

  params.put(assetParam.toStdString(), assets.join(",").toStdString());
}

void deleteThemeAsset(QDir &directory, const QString &subFolder, const QString &assetParam, const QString &themeToDelete, Params &params) {
  bool useFiles = subFolder.isEmpty();

  QString baseName = themeToDelete.toLower();
  baseName.replace("(", "-").replace(")", "").replace(" ", "-");
  baseName.remove(QRegularExpression("[^a-z0-9\\-]"));
  while (baseName.endsWith("-")) {
    baseName.chop(1);
  }

  QString baseUnderscore = baseName;
  baseUnderscore.replace("-", "_");

  QStringList candidateNames = {
    baseName,
    baseName + "-user-created",
    baseUnderscore,
    baseUnderscore + "-user_created"
  };

  if (useFiles) {
    QStringList files = directory.entryList(QDir::Files);
    for (QString &file : files) {
      QString normalizedFile = QFileInfo(file).baseName().toLower();
      normalizedFile.replace("_", "-");
      normalizedFile.remove(QRegularExpression("[^a-z0-9\\-~]"));

      if (candidateNames.contains(normalizedFile)) {
        QFile::remove(directory.filePath(file));
        break;
      }
    }
  } else {
    for (QString &candidate : candidateNames) {
      QString fullSubPath = QDir(candidate).filePath(subFolder);
      QDir targetDir(directory.filePath(fullSubPath));

      if (targetDir.exists()) {
        targetDir.removeRecursively();
        break;
      }
    }
  }

  updateAssetParam(assetParam, params, themeToDelete, true);
}

void downloadThemeAsset(const QString &input, const std::string &paramKey, const QString &assetParam, Params &params, Params &params_memory) {
  QString output = input;
  int tilde = output.indexOf("~");
  if (tilde >= 0) {
    output = output.left(tilde).toLower() + "~" + output.mid(tilde + 1);
  } else {
    output = output.toLower();
  }
  output.remove("(").remove(")");
  output.replace(" ", input.contains("(") ? "-" : "_");

  params_memory.put(paramKey, output.toStdString());
}

QStringList getHolidayThemes() {
  return QStringList()
         << "New Year's"
         << "Valentine's Day"
         << "St. Patrick's Day"
         << "World Frog Day"
         << "April Fools"
         << "Easter"
         << "May the Fourth"
         << "Cinco de Mayo"
         << "Stitch Day"
         << "Fourth of July"
         << "Halloween"
         << "Thanksgiving"
         << "Christmas";
}

QStringList getThemeList(const bool &randomThemes, const QDir &themePacksDirectory, const QString &subFolder, const QString &assetParam, Params &params) {
  bool useFiles = subFolder.isEmpty();

  QString currentAsset = randomThemes ? "" : QString::fromStdString(params.get(assetParam.toStdString()));

  QStringList themeList;
  for (const QFileInfo &entry : themePacksDirectory.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
    if (entry.baseName() == currentAsset) {
      continue;
    }

    if (useFiles && entry.isDir()) {
      continue;
    }

    if (!useFiles) {
      QString targetPath = QDir(entry.filePath()).filePath(subFolder);
      if (!QFileInfo(targetPath).exists()) {
        continue;
      }
    }

    QString baseName = entry.baseName();
    bool userCreated = isUserCreatedTheme(baseName);
    if (userCreated) {
      baseName = baseName.replace("-user_created", "");
    }

    int tildeIdx = baseName.indexOf("~");
    QString creator;
    if (tildeIdx >= 0) {
      creator = baseName.mid(tildeIdx + 1);
      baseName = baseName.left(tildeIdx);
    }

    QStringList parts = baseName.split(baseName.contains("-") ? "-" : "_", QString::SkipEmptyParts);
    for (QString &part : parts) {
      part[0] = part[0].toUpper();
    }

    QString displayName;
    if (userCreated) {
      displayName = parts.join(" ");
    } else {
      displayName = (parts.size() <= 1 || useFiles) ? parts.join(" ") : QString("%1 (%2)").arg(parts[0], parts.mid(1).join(" "));
    }

    if (userCreated) {
      displayName += " 🌟";
    }
    if (!creator.isEmpty()) {
      displayName += " - by: " + creator;
    }

    themeList.append(displayName);
  }

  return themeList;
}

QString getThemeName(const std::string &paramKey, Params &params) {
  QString value = QString::fromStdString(params.get(paramKey));

  QString baseName = value;

  int tildeIdx = baseName.indexOf("~");
  QString creator;
  if (tildeIdx >= 0) {
    creator = baseName.mid(tildeIdx + 1);
    baseName = baseName.left(tildeIdx);
  }

  QStringList parts = baseName.split(baseName.contains("-") ? "-" : "_", QString::SkipEmptyParts);
  for (QString &part : parts) {
    part[0] = part[0].toUpper();
  }

  QString displayName;
  if (baseName.contains("-") && parts.size() > 1) {
    displayName = QString("%1 (%2)").arg(parts[0], parts.mid(1).join(" "));
  } else {
    displayName = parts.join(" ");
  }

  if (isUserCreatedTheme(value)) {
    displayName = displayName.split(" (")[0] + " 🌟";
  }
  if (!creator.isEmpty()) {
    displayName += " - by: " + creator;
  }

  return displayName;
}

QString storeThemeName(const QString &input, const std::string &paramKey, Params &params) {
  QString output = input.toLower().remove("(").remove(")").remove("'").remove(".");
  output.replace(" ", input.contains("(") ? "-" : "_");
  output.replace("_🌟", "-user_created");
  output = output.trimmed();

  params.put(paramKey, output.toStdString());

  return getThemeName(paramKey, params);
}

FrogPilotThemesPanel::FrogPilotThemesPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

  QStackedLayout *themesLayout = new QStackedLayout();
  addItem(themesLayout);

  FrogPilotListWidget *themesList = new FrogPilotListWidget(this);

  ScrollView *themesPanel = new ScrollView(themesList, this);

  themesLayout->addWidget(themesPanel);

  FrogPilotListWidget *customThemesList = new FrogPilotListWidget(this);

  ScrollView *customThemesPanel = new ScrollView(customThemesList, this);

  themesLayout->addWidget(customThemesPanel);

  const std::vector<std::tuple<QString, QString, QString, QString>> themeToggles {
    {"PersonalizeOpenpilot", tr("Custom Themes"), tr("<b>The overall look and feel of openpilot.</b> Use the \"Theme Maker\" in \"The Pond\" to create and share your own themes!"), "../../frogpilot/assets/toggle_icons/icon_frog.png"},
    {"CustomColors", tr("Color Scheme"), tr("<b>The color scheme used throughout openpilot.</b> Use the \"Theme Maker\" in \"The Pond\" to create and share your own themes!"), ""},
    {"CustomDistanceIcons", tr("Distance Button"), tr("<b>The distance button icons shown on the driving screen.</b> Use the \"Theme Maker\" in \"The Pond\" to create and share your own themes!"), ""},
    {"CustomIcons", tr("Icon Pack"), tr("<b>The icon style used across openpilot.</b> Use the \"Theme Maker\" in \"The Pond\" to create and share your own themes!"), ""},
    {"CustomSounds", tr("Sound Pack"), tr("<b>The sound pack used by openpilot.</b> Use the \"Theme Maker\" in \"The Pond\" to create and share your own themes!"), ""},
    {"WheelIcon", tr("Steering Wheel"), tr("<b>The steering-wheel icon</b> shown at the top-right of the driving screen. Use the \"Theme Maker\" in \"The Pond\" to create and share your own themes!"), ""},
    {"CustomSignals", tr("Turn Signal"), tr("<b>Themed turn-signal animations.</b> Use the \"Theme Maker\" in \"The Pond\" to create and share your own themes!"), ""},
    {"DownloadStatusLabel", tr("Download Status"), "", ""},

    {"HolidayThemes", tr("Holiday Themes"), tr("<b>Themes based on U.S. holidays.</b> Minor holidays last one day; major holidays (Christmas, Easter, Halloween) run for a full week."), "../../frogpilot/assets/toggle_icons/icon_calendar.png"},
    {"RainbowPath", tr("Rainbow Path"), tr("<b>Color the driving path like a Mario Kart–style \"Rainbow Road\".</b>"), "../../frogpilot/assets/toggle_icons/icon_rainbow.png"},
    {"RandomEvents", tr("Random Events"), tr("<b>Occasional on-screen effects triggered by driving conditions.</b> These are purely a visual and don't impact how openpilot drives!"), "../../frogpilot/assets/toggle_icons/icon_random.png"},
    {"RandomThemes", tr("Random Themes"), tr("<b>Pick a random theme between each drive</b> from the themes you have downloaded. Great for variety without changing settings while driving."), "../../frogpilot/assets/toggle_icons/icon_random_themes.png"},
    {"StartupAlert", tr("Startup Alert"), tr("<b>Customize the \"Startup Alert\" message</b> shown at the start of each drive."), "../../frogpilot/assets/toggle_icons/icon_message.png"}
  };

  for (const auto &[param, title, desc, icon] : themeToggles) {
    AbstractControl *themeToggle;

    if (param == "PersonalizeOpenpilot") {
      FrogPilotManageControl *personalizeOpenpilotToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(personalizeOpenpilotToggle, &FrogPilotManageControl::manageButtonClicked, [customThemesPanel, themesLayout]() {
        themesLayout->setCurrentWidget(customThemesPanel);
      });
      themeToggle = personalizeOpenpilotToggle;
    } else if (param == "CustomColors") {
      manageCustomColorsButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageCustomColorsButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList colorSchemes = getThemeList(randomThemes, QDir(themePacksDirectory.path()), "colors", "CustomColors", params);

        if (id == 0) {
          QString colorSchemeToDelete = MultiOptionDialog::getSelection(tr("Select a color scheme to delete"), colorSchemes, "", this);
          if (!colorSchemeToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Delete the \"%1\" color scheme?").arg(colorSchemeToDelete), tr("Delete"), this)) {
            colorsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "colors", "DownloadableColors", colorSchemeToDelete, params);
          }
        } else if (id == 1) {
          if (colorDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);

            QTimer::singleShot(2500, [this]() {
              cancellingDownload = false;
              colorDownloading = false;
              themeDownloading = false;

              params_memory.putBool("CancelThemeDownload", false);
            });
          } else {
            QStringList downloadableColorSchemes = QString::fromStdString(params.get("DownloadableColors")).split(",");
            colorSchemeToDownload = MultiOptionDialog::getSelection(tr("Select a color scheme to download"), downloadableColorSchemes, "", this);
            if (!colorSchemeToDownload.isEmpty()) {
              colorDownloading = true;
              themeDownloading = true;

              params_memory.put("ThemeDownloadProgress", "Downloading...");

              downloadThemeAsset(colorSchemeToDownload, "ColorToDownload", "DownloadableColors", params, params_memory);

              downloadStatusLabel->setText("Downloading...");
            }
          }
        } else if (id == 2) {
          colorSchemes.append("Stock");
          colorSchemes.append(getHolidayThemes());
          colorSchemes.sort();

          QString colorSchemeToSelect = MultiOptionDialog::getSelection(tr("Select a color scheme"), colorSchemes, getThemeName("CustomColors", params), this);
          if (!colorSchemeToSelect.isEmpty()) {
            manageCustomColorsButton->setValue(storeThemeName(colorSchemeToSelect, "CustomColors", params));
          }
        }
      });
      manageCustomColorsButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageCustomColorsButton;
    } else if (param == "CustomDistanceIcons") {
      manageDistanceIconsButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageDistanceIconsButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList distanceIconPacks = getThemeList(randomThemes, QDir(themePacksDirectory.path()), "distance_icons", "CustomDistanceIcons", params);

        if (id == 0) {
          QString distanceIconPackToDelete = MultiOptionDialog::getSelection(tr("Select a distance icon pack to delete"), distanceIconPacks, "", this);
          if (!distanceIconPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Delete the \"%1\" distance icon pack?").arg(distanceIconPackToDelete), tr("Delete"), this)) {
            distanceIconsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "distance_icons", "DownloadableDistanceIcons", distanceIconPackToDelete, params);
          }
        } else if (id == 1) {
          if (distanceIconDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);

            QTimer::singleShot(2500, [this]() {
              cancellingDownload = false;
              distanceIconDownloading = false;
              themeDownloading = false;

              params_memory.putBool("CancelThemeDownload", false);
            });
          } else {
            QStringList downloadableDistanceIconPacks = QString::fromStdString(params.get("DownloadableDistanceIcons")).split(",");
            distanceIconPackToDownload = MultiOptionDialog::getSelection(tr("Select a distance icon pack to download"), downloadableDistanceIconPacks, "", this);
            if (!distanceIconPackToDownload.isEmpty()) {
              distanceIconDownloading = true;
              themeDownloading = true;

              params_memory.put("ThemeDownloadProgress", "Downloading...");

              downloadThemeAsset(distanceIconPackToDownload, "DistanceIconToDownload", "DownloadableDistanceIcons", params, params_memory);

              downloadStatusLabel->setText("Downloading...");
            }
          }
        } else if (id == 2) {
          distanceIconPacks.append("Stock");
          distanceIconPacks.append(getHolidayThemes());
          distanceIconPacks.sort();

          QString distanceIconPackToSelect = MultiOptionDialog::getSelection(tr("Select a distance icon pack"), distanceIconPacks, getThemeName("CustomDistanceIcons", params), this);
          if (!distanceIconPackToSelect.isEmpty()) {
            manageDistanceIconsButton->setValue(storeThemeName(distanceIconPackToSelect, "CustomDistanceIcons", params));
          }
        }
      });
      manageDistanceIconsButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageDistanceIconsButton;
    } else if (param == "CustomIcons") {
      manageCustomIconsButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageCustomIconsButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList iconPacks = getThemeList(randomThemes, QDir(themePacksDirectory.path()), "icons", "CustomIcons", params);

        if (id == 0) {
          QString iconPackToDelete = MultiOptionDialog::getSelection(tr("Select an icon pack to delete"), iconPacks, "", this);
          if (!iconPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Delete the \"%1\" icon pack?").arg(iconPackToDelete), tr("Delete"), this)) {
            iconsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "icons", "DownloadableIcons", iconPackToDelete, params);
          }
        } else if (id == 1) {
          if (iconDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);

            QTimer::singleShot(2500, [this]() {
              cancellingDownload = false;
              iconDownloading = false;
              themeDownloading = false;

              params_memory.putBool("CancelThemeDownload", false);
            });
          } else {
            QStringList downloadableIconPacks = QString::fromStdString(params.get("DownloadableIcons")).split(",");
            iconPackToDownload = MultiOptionDialog::getSelection(tr("Select an icon pack to download"), downloadableIconPacks, "", this);
            if (!iconPackToDownload.isEmpty()) {
              iconDownloading = true;
              themeDownloading = true;

              params_memory.put("ThemeDownloadProgress", "Downloading...");

              downloadThemeAsset(iconPackToDownload, "IconToDownload", "DownloadableIcons", params, params_memory);

              downloadStatusLabel->setText("Downloading...");
            }
          }
        } else if (id == 2) {
          iconPacks.append("Stock");
          iconPacks.append(getHolidayThemes());
          iconPacks.sort();

          QString iconPackToSelect = MultiOptionDialog::getSelection(tr("Select an icon pack"), iconPacks, getThemeName("CustomIcons", params), this);
          if (!iconPackToSelect.isEmpty()) {
            manageCustomIconsButton->setValue(storeThemeName(iconPackToSelect, "CustomIcons", params));
          }
        }
      });
      manageCustomIconsButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageCustomIconsButton;
    } else if (param == "CustomSignals") {
      manageCustomSignalsButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageCustomSignalsButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList signalAnimations = getThemeList(randomThemes, QDir(themePacksDirectory.path()), "signals", "CustomSignals", params);

        if (id == 0) {
          QString signalAnimationToDelete = MultiOptionDialog::getSelection(tr("Select a signal animation to delete"), signalAnimations, "", this);
          if (!signalAnimationToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Delete the \"%1\" signal animation?").arg(signalAnimationToDelete), tr("Delete"), this)) {
            signalsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "signals", "DownloadableSignals", signalAnimationToDelete, params);
          }
        } else if (id == 1) {
          if (signalDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);

            QTimer::singleShot(2500, [this]() {
              cancellingDownload = false;
              signalDownloading = false;
              themeDownloading = false;

              params_memory.putBool("CancelThemeDownload", false);
            });
          } else {
            QStringList downloadableSignalAnimations = QString::fromStdString(params.get("DownloadableSignals")).split(",");
            signalAnimationToDownload = MultiOptionDialog::getSelection(tr("Select a signal animation to download"), downloadableSignalAnimations, "", this);
            if (!signalAnimationToDownload.isEmpty()) {
              signalDownloading = true;
              themeDownloading = true;

              params_memory.put("ThemeDownloadProgress", "Downloading...");

              downloadThemeAsset(signalAnimationToDownload, "SignalToDownload", "DownloadableSignals", params, params_memory);

              downloadStatusLabel->setText("Downloading...");
            }
          }
        } else if (id == 2) {
          signalAnimations.append("None");
          signalAnimations.append(getHolidayThemes());
          signalAnimations.sort();

          QString signalAnimationToSelect = MultiOptionDialog::getSelection(tr("Select a signal animation"), signalAnimations, getThemeName("CustomSignals", params), this);
          if (!signalAnimationToSelect.isEmpty()) {
            manageCustomSignalsButton->setValue(storeThemeName(signalAnimationToSelect, "CustomSignals", params));
          }
        }
      });
      manageCustomSignalsButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageCustomSignalsButton;
    } else if (param == "CustomSounds") {
      manageCustomSoundsButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageCustomSoundsButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList soundPacks = getThemeList(randomThemes, QDir(themePacksDirectory.path()), "sounds", "CustomSounds", params);

        if (id == 0) {
          QString soundPackToDelete = MultiOptionDialog::getSelection(tr("Select a sound pack to delete"), soundPacks, "", this);
          if (!soundPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Delete the \"%1\" sound pack?").arg(soundPackToDelete), tr("Delete"), this)) {
            soundsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "sounds", "DownloadableSounds", soundPackToDelete, params);
          }
        } else if (id == 1) {
          if (soundDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);

            QTimer::singleShot(2500, [this]() {
              cancellingDownload = false;
              soundDownloading = false;
              themeDownloading = false;

              params_memory.putBool("CancelThemeDownload", false);
            });
          } else {
            QStringList downloadableSoundPacks = QString::fromStdString(params.get("DownloadableSounds")).split(",");
            soundPackToDownload = MultiOptionDialog::getSelection(tr("Select a sound pack to download"), downloadableSoundPacks, "", this);
            if (!soundPackToDownload.isEmpty()) {
              soundDownloading = true;
              themeDownloading = true;

              params_memory.put("ThemeDownloadProgress", "Downloading...");

              downloadThemeAsset(soundPackToDownload, "SoundToDownload", "DownloadableSounds", params, params_memory);

              downloadStatusLabel->setText("Downloading...");
            }
          }
        } else if (id == 2) {
          soundPacks.append("Stock");
          soundPacks.append(getHolidayThemes());
          soundPacks.sort();

          QString soundPackToSelect = MultiOptionDialog::getSelection(tr("Select a sound pack"), soundPacks, getThemeName("CustomSounds", params), this);
          if (!soundPackToSelect.isEmpty()) {
            manageCustomSoundsButton->setValue(storeThemeName(soundPackToSelect, "CustomSounds", params));
          }
        }
      });
      manageCustomSoundsButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageCustomSoundsButton;
    } else if (param == "WheelIcon") {
      manageWheelIconsButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageWheelIconsButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList wheelIcons = getThemeList(randomThemes, QDir(wheelsDirectory.path()), "", "WheelIcon", params);

        if (id == 0) {
          QString wheelIconToDelete = MultiOptionDialog::getSelection(tr("Select a steering wheel to delete"), wheelIcons, "", this);
          if (!wheelIconToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Delete the \"%1\" steering wheel?").arg(wheelIconToDelete), tr("Delete"), this)) {
            wheelsDownloaded = false;

            deleteThemeAsset(wheelsDirectory, "", "DownloadableWheels", wheelIconToDelete, params);
          }
        } else if (id == 1) {
          if (wheelDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);

            QTimer::singleShot(2500, [this]() {
              cancellingDownload = false;
              wheelDownloading = false;
              themeDownloading = false;

              params_memory.putBool("CancelThemeDownload", false);
            });
          } else {
            QStringList downloadableWheels = QString::fromStdString(params.get("DownloadableWheels")).split(",");
            wheelToDownload = MultiOptionDialog::getSelection(tr("Select a steering wheel to download"), downloadableWheels, "", this);
            if (!wheelToDownload.isEmpty()) {
              wheelDownloading = true;
              themeDownloading = true;

              params_memory.put("ThemeDownloadProgress", "Downloading...");

              downloadThemeAsset(wheelToDownload, "WheelToDownload", "DownloadableWheels", params, params_memory);

              downloadStatusLabel->setText("Downloading...");
            }
          }
        } else if (id == 2) {
          wheelIcons.append("None");
          wheelIcons.append("Stock");
          wheelIcons.append(getHolidayThemes());
          wheelIcons.sort();

          QString steeringWheelToSelect = MultiOptionDialog::getSelection(tr("Select a steering wheel"), wheelIcons, getThemeName("WheelIcon", params), this);
          if (!steeringWheelToSelect.isEmpty()) {
            manageWheelIconsButton->setValue(storeThemeName(steeringWheelToSelect, "WheelIcon", params));
          }
        }
      });
      manageWheelIconsButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageWheelIconsButton;
    } else if (param == "DownloadStatusLabel") {
      downloadStatusLabel = new LabelControl(title, "Idle");
      themeToggle = downloadStatusLabel;
    } else if (param == "StartupAlert") {
      FrogPilotButtonsControl *startupAlertButton = new FrogPilotButtonsControl(title, desc, icon, {tr("STOCK"), tr("FROGPILOT"), tr("CUSTOM"), tr("CLEAR")}, true);

      QString currentTop = QString::fromStdString(params.get("StartupMessageTop"));
      QString currentBottom = QString::fromStdString(params.get("StartupMessageBottom"));

      QString stockTop = "Be ready to take over at any time";
      QString stockBottom = "Always keep hands on wheel and eyes on road";

      QString frogpilotTop = "Hop in and buckle up!";
      QString frogpilotBottom = "Human-tested, frog-approved 🐸";

      if (currentTop == stockTop && currentBottom == stockBottom) {
        startupAlertButton->setCheckedButton(0);
      } else if (currentTop == frogpilotTop && currentBottom == frogpilotBottom) {
        startupAlertButton->setCheckedButton(1);
      } else if (!currentTop.isEmpty() || !currentBottom.isEmpty()) {
        startupAlertButton->setCheckedButton(2);
      }

      QObject::connect(startupAlertButton, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
        int maxLengthTop = 35;
        int maxLengthBottom = 45;

        if (id == 0) {
          params.put("StartupMessageTop", stockTop.toStdString());
          params.put("StartupMessageBottom", stockBottom.toStdString());
        } else if (id == 1) {
          params.put("StartupMessageTop", frogpilotTop.toStdString());
          params.put("StartupMessageBottom", frogpilotBottom.toStdString());
        } else if (id == 2) {
          QString newTop = InputDialog::getText(tr("Enter the text for the top half"), this, tr("Characters: 0/%1").arg(maxLengthTop), false, -1, currentTop, maxLengthTop).trimmed();
          if (!newTop.isEmpty()) {
            params.put("StartupMessageTop", newTop.toStdString());
            QString newBottom = InputDialog::getText(tr("Enter the text for the bottom half"), this, tr("Characters: 0/%1").arg(maxLengthBottom), false, -1, currentBottom, maxLengthBottom).trimmed();
            if (!newBottom.isEmpty()) {
              params.put("StartupMessageBottom", newBottom.toStdString());
            }
          }
        } else if (id == 3) {
          if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your startup message?"), this)) {
            params.remove("StartupMessageTop");
            params.remove("StartupMessageBottom");

            startupAlertButton->clearCheckedButtons(true);
          }
        }
      });
      themeToggle = startupAlertButton;

    } else {
      themeToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = themeToggle;

    if (customThemeKeys.contains(param)) {
      customThemesList->addItem(themeToggle);
    } else {
      themesList->addItem(themeToggle);

      if (param == "PersonalizeOpenpilot") {
        parentKeys.insert(param);
      }
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(themeToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        emit openSubPanel();
        openDescriptions(forceOpenDescriptions, toggles);
      });
    }

    QObject::connect(themeToggle, &AbstractControl::hideDescriptionEvent, [this]() {
      update();
    });
    QObject::connect(themeToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  openDescriptions(forceOpenDescriptions, toggles);

  QObject::connect(static_cast<ToggleControl *>(toggles["PersonalizeOpenpilot"]), &ToggleControl::toggleFlipped, this, &FrogPilotThemesPanel::updateToggles);
  QObject::connect(static_cast<ToggleControl*>(toggles["RandomThemes"]), &ToggleControl::toggleFlipped, [this](bool state) {
    if (state) {
      ConfirmationDialog::alert(tr("\"Random Themes\" only works with downloaded themes, so make sure you download the themes you want it to use!"), this);

      manageCustomColorsButton->setValue("");
      manageCustomColorsButton->setVisibleButton(2, false);

      manageCustomIconsButton->setValue("");
      manageCustomIconsButton->setVisibleButton(2, false);

      manageCustomSignalsButton->setValue("");
      manageCustomSignalsButton->setVisibleButton(2, false);

      manageCustomSoundsButton->setValue("");
      manageCustomSoundsButton->setVisibleButton(2, false);

      manageDistanceIconsButton->setValue("");
      manageDistanceIconsButton->setVisibleButton(2, false);

      manageWheelIconsButton->setValue("");
      manageWheelIconsButton->setVisibleButton(2, false);
    } else {
      manageCustomColorsButton->setValue(getThemeName("CustomColors", params));
      manageCustomColorsButton->setVisibleButton(2, true);

      manageCustomIconsButton->setValue(getThemeName("CustomIcons", params));
      manageCustomIconsButton->setVisibleButton(2, true);

      manageCustomSignalsButton->setValue(getThemeName("CustomSignals", params));
      manageCustomSignalsButton->setVisibleButton(2, true);

      manageCustomSoundsButton->setValue(getThemeName("CustomSounds", params));
      manageCustomSoundsButton->setVisibleButton(2, true);

      manageDistanceIconsButton->setValue(getThemeName("CustomDistanceIcons", params));
      manageDistanceIconsButton->setVisibleButton(2, true);

      manageWheelIconsButton->setValue(getThemeName("WheelIcon", params));
      manageWheelIconsButton->setVisibleButton(2, true);
    }

    randomThemes = state;
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [themesLayout, themesPanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    themesLayout->setCurrentWidget(themesPanel);
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotThemesPanel::updateState);
}

void FrogPilotThemesPanel::showEvent(QShowEvent *event) {
  colorsDownloaded = params.get("DownloadableColors").empty();
  distanceIconsDownloaded = params.get("DownloadableDistanceIcons").empty();
  iconsDownloaded = params.get("DownloadableIcons").empty();
  signalsDownloaded = params.get("DownloadableSignals").empty();
  soundsDownloaded = params.get("DownloadableSounds").empty();
  wheelsDownloaded = params.get("DownloadableWheels").empty();

  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  tuningLevel = parent->tuningLevel;

  if (params.getBool("RandomThemes")) {
    manageCustomColorsButton->setValue("");
    manageCustomColorsButton->setVisibleButton(2, false);

    manageCustomIconsButton->setValue("");
    manageCustomIconsButton->setVisibleButton(2, false);

    manageCustomSignalsButton->setValue("");
    manageCustomSignalsButton->setVisibleButton(2, false);

    manageCustomSoundsButton->setValue("");
    manageCustomSoundsButton->setVisibleButton(2, false);

    manageDistanceIconsButton->setValue("");
    manageDistanceIconsButton->setVisibleButton(2, false);

    manageWheelIconsButton->setValue("");
    manageWheelIconsButton->setVisibleButton(2, false);

    randomThemes = true;
  }

  updateToggles();
}

void FrogPilotThemesPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible() || finalizingDownload) {
    return;
  }

  if (themeDownloading) {
    QString progress = QString::fromStdString(params_memory.get("ThemeDownloadProgress"));
    bool downloadFailed = progress.contains(QRegularExpression("cancelled|exists|failed|offline", QRegularExpression::CaseInsensitiveOption));

    if (progress != "Downloading...") {
      downloadStatusLabel->setText(progress);
    }

    if (progress == "Downloaded!" || downloadFailed) {
      finalizingDownload = true;

      QTimer::singleShot(2500, [this]() {
        colorDownloading = false;
        distanceIconDownloading = false;
        finalizingDownload = false;
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

        params_memory.remove("CancelThemeDownload");
        params_memory.remove("ThemeDownloadProgress");

        downloadStatusLabel->setText("Idle");
      });
    }
  }

  bool parked = !s.scene.started || fs.frogpilot_scene.parked || fs.frogpilot_toggles.value("frogs_go_moo").toBool();

  manageCustomColorsButton->setText(1, colorDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageCustomColorsButton->setEnabledButtons(0, !themeDownloading);
  manageCustomColorsButton->setEnabledButtons(1, fs.frogpilot_scene.online && (!themeDownloading || colorDownloading) && !cancellingDownload && !finalizingDownload && !colorsDownloaded && parked);
  manageCustomColorsButton->setEnabledButtons(2, !themeDownloading);

  manageCustomIconsButton->setText(1, iconDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageCustomIconsButton->setEnabledButtons(0, !themeDownloading);
  manageCustomIconsButton->setEnabledButtons(1, fs.frogpilot_scene.online && (!themeDownloading || iconDownloading) && !cancellingDownload && !finalizingDownload && !iconsDownloaded && parked);
  manageCustomIconsButton->setEnabledButtons(2, !themeDownloading);

  manageCustomSignalsButton->setText(1, signalDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageCustomSignalsButton->setEnabledButtons(0, !themeDownloading);
  manageCustomSignalsButton->setEnabledButtons(1, fs.frogpilot_scene.online && (!themeDownloading || signalDownloading) && !cancellingDownload && !finalizingDownload && !signalsDownloaded && parked);
  manageCustomSignalsButton->setEnabledButtons(2, !themeDownloading);

  manageCustomSoundsButton->setText(1, soundDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageCustomSoundsButton->setEnabledButtons(0, !themeDownloading);
  manageCustomSoundsButton->setEnabledButtons(1, fs.frogpilot_scene.online && (!themeDownloading || soundDownloading) && !cancellingDownload && !finalizingDownload && !soundsDownloaded && parked);
  manageCustomSoundsButton->setEnabledButtons(2, !themeDownloading);

  manageDistanceIconsButton->setText(1, distanceIconDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageDistanceIconsButton->setEnabledButtons(0, !themeDownloading);
  manageDistanceIconsButton->setEnabledButtons(1, fs.frogpilot_scene.online && (!themeDownloading || distanceIconDownloading) && !cancellingDownload && !finalizingDownload && !distanceIconsDownloaded && parked);
  manageDistanceIconsButton->setEnabledButtons(2, !themeDownloading);

  manageWheelIconsButton->setText(1, wheelDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageWheelIconsButton->setEnabledButtons(0, !themeDownloading);
  manageWheelIconsButton->setEnabledButtons(1, fs.frogpilot_scene.online && (!themeDownloading || wheelDownloading) && !cancellingDownload && !finalizingDownload && !wheelsDownloaded && parked);
  manageWheelIconsButton->setEnabledButtons(2, !themeDownloading);

  parent->keepScreenOn = themeDownloading;
}

void FrogPilotThemesPanel::updateToggles() {
  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      toggle->setVisible(false);
    }
  }

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      continue;
    }

    bool setVisible = tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (key == "CustomDistanceIcons") {
      setVisible &= params.getBool("QOLVisuals") && params.getBool("OnroadDistanceButton");
    }

    else if (key == "RandomThemes") {
      setVisible &= params.getBool("PersonalizeOpenpilot");
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (customThemeKeys.contains(key)) {
        toggles["PersonalizeOpenpilot"]->setVisible(true);
      }
    }
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
