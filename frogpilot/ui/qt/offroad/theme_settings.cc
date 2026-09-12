#include "frogpilot/ui/qt/offroad/theme_settings.h"

bool isUserCreatedTheme(const QString &themeName) {
  return themeName.endsWith("-user_created");
}

QString themeAssetKey(const QString &input) {
  QString output = input;
  bool userCreated = output.contains("ðŸŒŸ") || output.contains("🌟");
  output.replace(" - by: ", "~");
  int tilde = output.indexOf("~");
  if (tilde >= 0) {
    output = output.left(tilde).toLower() + "~" + output.mid(tilde + 1);
  } else {
    output = output.toLower();
  }
  output.remove("(").remove(")").remove("'").remove(".");
  output.replace(" ", input.contains("(") ? "-" : "_");
  output.replace("_ðŸŒŸ", "");
  output.replace("_🌟", "");
  output.replace("-🌟", "");
  output.remove("ðŸŒŸ").remove("🌟");
  output = output.trimmed();
  if (userCreated) {
    output += "-user_created";
  }

  return output;
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

QString formatThemeName(QString key, bool useFiles);

void deleteThemeAsset(QDir &directory, const QString &subFolder, const QString &assetParam, const QString &assetKey, Params &params) {
  if (params.getBool("RandomThemes")) {
    return;
  }

  bool deleted = false;
  if (subFolder.isEmpty()) {
    for (const QFileInfo &entry : directory.entryInfoList(QDir::Files)) {
      if (entry.completeBaseName() == assetKey) {
        deleted = QFile::remove(entry.absoluteFilePath());
        if (deleted) {
          break;
        }
      }
    }
  } else {
    QDir assetDirectory(directory.filePath(assetKey));
    deleted = QDir(assetDirectory.filePath(subFolder)).removeRecursively();
  }

  if (deleted) {
    params.remove("ThemesDownloaded");

    if (!isUserCreatedTheme(assetKey)) {
      updateAssetParam(assetParam, params, formatThemeName(assetKey, subFolder.isEmpty()), true);
    }
  }
}

void downloadThemeAsset(const QString &input, const std::string &paramKey, const QString &assetParam, Params &params, Params &params_memory) {
  params_memory.remove("CancelThemeDownload");
  params_memory.put(paramKey, themeAssetKey(input).toStdString());
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

QString formatThemeName(QString key, bool useFiles) {
  bool userCreated = isUserCreatedTheme(key);
  if (userCreated) {
    key.chop(QString("-user_created").size());
  }

  int tildeIndex = key.indexOf("~");
  QString creator;
  if (tildeIndex >= 0) {
    creator = key.mid(tildeIndex + 1);
    key = key.left(tildeIndex);
  }

  QStringList parts = key.split(key.contains("-") ? "-" : "_", QString::SkipEmptyParts);
  for (QString &part : parts) {
    part[0] = part[0].toUpper();
  }

  QString displayName;
  if (!userCreated && !useFiles && key.contains("-") && parts.size() > 1) {
    displayName = QString("%1 (%2)").arg(parts[0], parts.mid(1).join(" "));
  } else {
    displayName = parts.join(" ");
  }
  if (userCreated) {
    displayName += " 🌟";
  }
  if (!creator.isEmpty()) {
    displayName += " - by: " + creator;
  }
  return displayName;
}

QString getThemeName(const std::string &paramKey, Params &params) {
  return formatThemeName(QString::fromStdString(params.get(paramKey)), paramKey == "WheelIcon");
}

QStringList getThemeList(bool randomThemes, const QDir &directory, const QString &subFolder, const QString &assetParam, Params &params, QMap<QString, QString> &assetKeys) {
  const bool useFiles = subFolder.isEmpty();

  const QString currentAsset = QString::fromStdString(params.get(assetParam.toStdString()));

  QStringList themes;
  for (const QFileInfo &entry : directory.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
    QString assetKey;
    if (useFiles) {
      if (!entry.isFile()) {
        continue;
      }
      assetKey = entry.completeBaseName();
    } else {
      if (!entry.isDir() || !QDir(entry.filePath()).exists(subFolder)) {
        continue;
      }
      assetKey = entry.fileName();
    }

    QString displayName = formatThemeName(assetKey, useFiles);
    if (assetKeys.contains(displayName) && assetKeys.value(displayName) != assetKey) {
      displayName += QString(" [%1]").arg(assetKey);
    }

    assetKeys.insert(displayName, assetKey);
    if ((randomThemes || assetKey != currentAsset) && !themes.contains(displayName)) {
      themes.append(displayName);
    }
  }
  return themes;
}

void appendCurrentTheme(QStringList &themes, const std::string &paramKey, Params &params, QMap<QString, QString> &assetKeys) {
  const QString currentKey = QString::fromStdString(params.get(paramKey));
  if (currentKey.isEmpty()) {
    return;
  }

  QString current = assetKeys.key(currentKey);
  if (current.isEmpty()) {
    current = getThemeName(paramKey, params);
    if (assetKeys.contains(current) && assetKeys.value(current) != currentKey) {
      current += QString(" [%1]").arg(currentKey);
    }

    assetKeys.insert(current, currentKey);
  }

  if (!themes.contains(current)) {
    themes.append(current);
  }
}

QString storeThemeName(const QString &input, const std::string &paramKey, Params &params, const QMap<QString, QString> &assetKeys) {
  if (assetKeys.contains(input)) {
    params.put(paramKey, assetKeys.value(input).toStdString());
  } else {
    params.put(paramKey, themeAssetKey(input).toStdString());
  }
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
    {"PersonalizeOpenpilot", tr("Custom Themes"), tr("<b>Swap openpilot's colors, icons, sounds, turn signal animations, steering wheel picture and personality button for a theme pack you download.</b><br><br>You mix and match freely, so one theme's colors can run alongside another's sounds. Packs are made by other drivers, and you can build your own with the \"Theme Maker\" in \"The Pond\"."), "../../frogpilot/assets/toggle_icons/icon_frog.png"},
    {"CustomColors", tr("Color Scheme"), tr("<b>Change the colors openpilot draws on the driving screen, mainly the path ahead of you and the lane lines.</b><br><br>\"Stock\" is openpilot's normal green path with white lane lines. A scheme also recolors the marker on the car ahead and the sidebar boxes, but the road edges are always red and never change. Holiday options match the holiday they are named after, and a downloaded pack brings its own set of colors."), ""},
    {"DownloadStatusLabel", tr("Download Status"), "", ""},
    {"CustomIcons", tr("Icon Pack"), tr("<b>Change the settings, home and flag buttons on openpilot's sidebar.</b><br><br>\"Stock\" puts the normal three back. A pack replaces all three at once and nothing else, so every other icon openpilot draws stays stock."), ""},
    {"CustomDistanceIcons", tr("Personality Button"), tr("<b>Change the icons on the driving personality button, the one you tap on the driving screen to switch between Aggressive, Standard and Relaxed.</b><br><br>Each pack draws four icons: one each for Aggressive, Standard and Relaxed, plus one that takes over while Traffic Mode is on. This row only appears while that button is switched on under \"Driving Personality Button\"."), ""},
    {"CustomSounds", tr("Sound Pack"), tr("<b>Change the chimes openpilot plays for its alerts, like the sound when it starts driving or warns you about something.</b><br><br>\"Stock\" uses openpilot's normal chimes. A pack only replaces the sound files it actually ships and anything it leaves out stays stock, so the holiday packs mostly bring just their own engage and disengage chimes. How loud each one plays is set separately under \"Alert Volumes\" in \"Alerts and Sounds\"."), ""},
    {"WheelIcon", tr("Steering Wheel"), tr("<b>Change the steering wheel picture in the top right corner of the driving screen, which spins as openpilot steers.</b><br><br>\"Stock\" uses openpilot's normal wheel and \"None\" hides it completely. Some downloaded wheels are animated."), ""},
    {"CustomSignals", tr("Turn Signal"), tr("<b>Play an animation across the driving screen for as long as your turn signal is on.</b><br><br>The animation runs toward whichever side you signalled. \"None\" turns it off, and each downloaded pack brings its own animation."), ""},

    {"HolidayThemes", tr("Holiday Themes"), tr("<b>Dress openpilot up for thirteen holidays through the year, swapping the colors, icons, sounds, turn signals, steering wheel and personality button all at once.</b><br><br>Smaller ones like April Fools or Cinco de Mayo run on the day itself. Easter, Halloween, Thanksgiving and Christmas start on the Monday of that week and finish on the day, so they last anywhere from one day to a full week depending on where the date falls.<br><br>While a holiday is running it replaces the themes you picked, and your own choices come back the next day."), "../../frogpilot/assets/toggle_icons/icon_calendar.png"},
    {"RainbowPath", tr("Rainbow Path"), tr("<b>Paint the driving path in shifting rainbow colors that scroll faster the quicker you go, like the Rainbow Road track from Mario Kart.</b><br><br>The rainbow replaces whatever color the path normally uses, including one that came with a theme you downloaded. With \"Acceleration Path\" also on, the green and red speed colors take over whenever openpilot speeds up or slows down, so the rainbow only shows while you hold a steady speed."), "../../frogpilot/assets/toggle_icons/icon_rainbow.png"},
    {"RandomEvents", tr("Random Events"), tr("<b>Play a rare joke alert, with its own sound and sometimes its own steering wheel picture, when something unusual happens on a drive.</b><br><br>Taking off hard, a corner sharper than openpilot can steer through, or a collision warning can each set one off. Every alert can only happen once per drive, a swapped steering wheel goes back to normal after about five seconds, and none of them change how openpilot drives."), "../../frogpilot/assets/toggle_icons/icon_random.png"},
    {"RandomThemes", tr("Random Themes"), tr("<b>Start every drive with a different theme, picked at random from the packs you have already downloaded.</b><br><br>Nothing happens until you download at least one pack. While this is on, the rows inside \"Custom Themes\" stop offering \"SELECT\", and turning it back off gives you your own picks again."), "../../frogpilot/assets/toggle_icons/icon_random_themes.png"},
    {"StartupAlert", tr("Startup Alert"), tr("<b>Change the two lines of text openpilot shows on screen at the start of every drive.</b><br><br>\"STOCK\" is openpilot's usual safety reminder and \"FROGPILOT\" is the frog version. \"CUSTOM\" lets you write your own, up to 35 characters on the top line and 45 on the bottom, and \"CLEAR\" leaves the screen blank."), "../../frogpilot/assets/toggle_icons/icon_message.png"}
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
        QMap<QString, QString> assetKeys;
        QStringList colorSchemes = getThemeList(randomThemes, themePacksDirectory, "colors", "CustomColors", params, assetKeys);

        if (id == 0) {
          QString colorSchemeToDelete = MultiOptionDialog::getSelection(tr("Select a color scheme to delete"), colorSchemes, "", this);
          if (!colorSchemeToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Delete the \"%1\" color scheme?").arg(colorSchemeToDelete), tr("Delete"), this)) {
            colorsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "colors", "DownloadableColors", assetKeys.value(colorSchemeToDelete), params);
          }
        } else if (id == 1) {
          if (colorDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);
          } else {
            QStringList downloadableColorSchemes = QString::fromStdString(params.get("DownloadableColors")).split(",", QString::SkipEmptyParts);
            colorSchemeToDownload = MultiOptionDialog::getSelection(tr("Select a color scheme to download"), downloadableColorSchemes, "", this);
            if (!colorSchemeToDownload.isEmpty()) {
              colorDownloading = true;
              themeDownloading = true;

              params_memory.put("ThemeDownloadProgress", "Downloading...");

              downloadThemeAsset(colorSchemeToDownload, "ColorToDownload", "DownloadableColors", params, params_memory);

              downloadStatusLabel->setText(tr("Downloading..."));
            }
          }
        } else if (id == 2) {
          colorSchemes.append("Stock");
          colorSchemes.append(getHolidayThemes());

          appendCurrentTheme(colorSchemes, "CustomColors", params, assetKeys);

          colorSchemes.sort();

          QString colorSchemeToSelect = MultiOptionDialog::getSelection(tr("Select a color scheme"), colorSchemes, assetKeys.key(QString::fromStdString(params.get("CustomColors"))), this);
          if (!colorSchemeToSelect.isEmpty()) {
            manageCustomColorsButton->setValue(storeThemeName(colorSchemeToSelect, "CustomColors", params, assetKeys));
          }
        }
      });
      manageCustomColorsButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageCustomColorsButton;
    } else if (param == "CustomDistanceIcons") {
      manageDistanceIconsButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageDistanceIconsButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QMap<QString, QString> assetKeys;
        QStringList distanceIconPacks = getThemeList(randomThemes, themePacksDirectory, "distance_icons", "CustomDistanceIcons", params, assetKeys);

        if (id == 0) {
          QString distanceIconPackToDelete = MultiOptionDialog::getSelection(tr("Select a personality button pack to delete"), distanceIconPacks, "", this);
          if (!distanceIconPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Delete the \"%1\" personality button pack?").arg(distanceIconPackToDelete), tr("Delete"), this)) {
            distanceIconsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "distance_icons", "DownloadableDistanceIcons", assetKeys.value(distanceIconPackToDelete), params);
          }
        } else if (id == 1) {
          if (distanceIconDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);
          } else {
            QStringList downloadableDistanceIconPacks = QString::fromStdString(params.get("DownloadableDistanceIcons")).split(",", QString::SkipEmptyParts);
            distanceIconPackToDownload = MultiOptionDialog::getSelection(tr("Select a personality button pack to download"), downloadableDistanceIconPacks, "", this);
            if (!distanceIconPackToDownload.isEmpty()) {
              distanceIconDownloading = true;
              themeDownloading = true;

              params_memory.put("ThemeDownloadProgress", "Downloading...");

              downloadThemeAsset(distanceIconPackToDownload, "DistanceIconToDownload", "DownloadableDistanceIcons", params, params_memory);

              downloadStatusLabel->setText(tr("Downloading..."));
            }
          }
        } else if (id == 2) {
          distanceIconPacks.append("Stock");
          QStringList distanceIconHolidays = getHolidayThemes();
          distanceIconHolidays.removeAll("April Fools");
          distanceIconHolidays.removeAll("Easter");

          distanceIconPacks.append(distanceIconHolidays);

          appendCurrentTheme(distanceIconPacks, "CustomDistanceIcons", params, assetKeys);

          distanceIconPacks.sort();

          QString distanceIconPackToSelect = MultiOptionDialog::getSelection(tr("Select a personality button pack"), distanceIconPacks, assetKeys.key(QString::fromStdString(params.get("CustomDistanceIcons"))), this);
          if (!distanceIconPackToSelect.isEmpty()) {
            manageDistanceIconsButton->setValue(storeThemeName(distanceIconPackToSelect, "CustomDistanceIcons", params, assetKeys));
          }
        }
      });
      manageDistanceIconsButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageDistanceIconsButton;
    } else if (param == "CustomIcons") {
      manageCustomIconsButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageCustomIconsButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QMap<QString, QString> assetKeys;
        QStringList iconPacks = getThemeList(randomThemes, themePacksDirectory, "icons", "CustomIcons", params, assetKeys);

        if (id == 0) {
          QString iconPackToDelete = MultiOptionDialog::getSelection(tr("Select an icon pack to delete"), iconPacks, "", this);
          if (!iconPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Delete the \"%1\" icon pack?").arg(iconPackToDelete), tr("Delete"), this)) {
            iconsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "icons", "DownloadableIcons", assetKeys.value(iconPackToDelete), params);
          }
        } else if (id == 1) {
          if (iconDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);
          } else {
            QStringList downloadableIconPacks = QString::fromStdString(params.get("DownloadableIcons")).split(",", QString::SkipEmptyParts);
            iconPackToDownload = MultiOptionDialog::getSelection(tr("Select an icon pack to download"), downloadableIconPacks, "", this);
            if (!iconPackToDownload.isEmpty()) {
              iconDownloading = true;
              themeDownloading = true;

              params_memory.put("ThemeDownloadProgress", "Downloading...");

              downloadThemeAsset(iconPackToDownload, "IconToDownload", "DownloadableIcons", params, params_memory);

              downloadStatusLabel->setText(tr("Downloading..."));
            }
          }
        } else if (id == 2) {
          iconPacks.append("Stock");
          iconPacks.append(getHolidayThemes());

          appendCurrentTheme(iconPacks, "CustomIcons", params, assetKeys);

          iconPacks.sort();

          QString iconPackToSelect = MultiOptionDialog::getSelection(tr("Select an icon pack"), iconPacks, assetKeys.key(QString::fromStdString(params.get("CustomIcons"))), this);
          if (!iconPackToSelect.isEmpty()) {
            manageCustomIconsButton->setValue(storeThemeName(iconPackToSelect, "CustomIcons", params, assetKeys));
          }
        }
      });
      manageCustomIconsButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageCustomIconsButton;
    } else if (param == "CustomSignals") {
      manageCustomSignalsButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageCustomSignalsButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QMap<QString, QString> assetKeys;
        QStringList signalAnimations = getThemeList(randomThemes, themePacksDirectory, "signals", "CustomSignals", params, assetKeys);

        if (id == 0) {
          QString signalAnimationToDelete = MultiOptionDialog::getSelection(tr("Select a signal animation to delete"), signalAnimations, "", this);
          if (!signalAnimationToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Delete the \"%1\" signal animation?").arg(signalAnimationToDelete), tr("Delete"), this)) {
            signalsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "signals", "DownloadableSignals", assetKeys.value(signalAnimationToDelete), params);
          }
        } else if (id == 1) {
          if (signalDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);
          } else {
            QStringList downloadableSignalAnimations = QString::fromStdString(params.get("DownloadableSignals")).split(",", QString::SkipEmptyParts);
            signalAnimationToDownload = MultiOptionDialog::getSelection(tr("Select a signal animation to download"), downloadableSignalAnimations, "", this);
            if (!signalAnimationToDownload.isEmpty()) {
              signalDownloading = true;
              themeDownloading = true;

              params_memory.put("ThemeDownloadProgress", "Downloading...");

              downloadThemeAsset(signalAnimationToDownload, "SignalToDownload", "DownloadableSignals", params, params_memory);

              downloadStatusLabel->setText(tr("Downloading..."));
            }
          }
        } else if (id == 2) {
          signalAnimations.append("None");
          signalAnimations.append(getHolidayThemes());

          appendCurrentTheme(signalAnimations, "CustomSignals", params, assetKeys);

          signalAnimations.sort();

          QString signalAnimationToSelect = MultiOptionDialog::getSelection(tr("Select a signal animation"), signalAnimations, assetKeys.key(QString::fromStdString(params.get("CustomSignals"))), this);
          if (!signalAnimationToSelect.isEmpty()) {
            manageCustomSignalsButton->setValue(storeThemeName(signalAnimationToSelect, "CustomSignals", params, assetKeys));
          }
        }
      });
      manageCustomSignalsButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageCustomSignalsButton;
    } else if (param == "CustomSounds") {
      manageCustomSoundsButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageCustomSoundsButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QMap<QString, QString> assetKeys;
        QStringList soundPacks = getThemeList(randomThemes, themePacksDirectory, "sounds", "CustomSounds", params, assetKeys);

        if (id == 0) {
          QString soundPackToDelete = MultiOptionDialog::getSelection(tr("Select a sound pack to delete"), soundPacks, "", this);
          if (!soundPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Delete the \"%1\" sound pack?").arg(soundPackToDelete), tr("Delete"), this)) {
            soundsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "sounds", "DownloadableSounds", assetKeys.value(soundPackToDelete), params);
          }
        } else if (id == 1) {
          if (soundDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);
          } else {
            QStringList downloadableSoundPacks = QString::fromStdString(params.get("DownloadableSounds")).split(",", QString::SkipEmptyParts);
            soundPackToDownload = MultiOptionDialog::getSelection(tr("Select a sound pack to download"), downloadableSoundPacks, "", this);
            if (!soundPackToDownload.isEmpty()) {
              soundDownloading = true;
              themeDownloading = true;

              params_memory.put("ThemeDownloadProgress", "Downloading...");

              downloadThemeAsset(soundPackToDownload, "SoundToDownload", "DownloadableSounds", params, params_memory);

              downloadStatusLabel->setText(tr("Downloading..."));
            }
          }
        } else if (id == 2) {
          soundPacks.append("Stock");
          soundPacks.append(getHolidayThemes());

          appendCurrentTheme(soundPacks, "CustomSounds", params, assetKeys);

          soundPacks.sort();

          QString soundPackToSelect = MultiOptionDialog::getSelection(tr("Select a sound pack"), soundPacks, assetKeys.key(QString::fromStdString(params.get("CustomSounds"))), this);
          if (!soundPackToSelect.isEmpty()) {
            manageCustomSoundsButton->setValue(storeThemeName(soundPackToSelect, "CustomSounds", params, assetKeys));
          }
        }
      });
      manageCustomSoundsButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageCustomSoundsButton;
    } else if (param == "WheelIcon") {
      manageWheelIconsButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageWheelIconsButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QMap<QString, QString> assetKeys;
        QStringList wheelIcons = getThemeList(randomThemes, wheelsDirectory, "", "WheelIcon", params, assetKeys);

        if (id == 0) {
          QString wheelIconToDelete = MultiOptionDialog::getSelection(tr("Select a steering wheel to delete"), wheelIcons, "", this);
          if (!wheelIconToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Delete the \"%1\" steering wheel?").arg(wheelIconToDelete), tr("Delete"), this)) {
            wheelsDownloaded = false;

            deleteThemeAsset(wheelsDirectory, "", "DownloadableWheels", assetKeys.value(wheelIconToDelete), params);
          }
        } else if (id == 1) {
          if (wheelDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);
          } else {
            QStringList downloadableWheels = QString::fromStdString(params.get("DownloadableWheels")).split(",", QString::SkipEmptyParts);
            wheelToDownload = MultiOptionDialog::getSelection(tr("Select a steering wheel to download"), downloadableWheels, "", this);
            if (!wheelToDownload.isEmpty()) {
              wheelDownloading = true;
              themeDownloading = true;

              params_memory.put("ThemeDownloadProgress", "Downloading...");

              downloadThemeAsset(wheelToDownload, "WheelToDownload", "DownloadableWheels", params, params_memory);

              downloadStatusLabel->setText(tr("Downloading..."));
            }
          }
        } else if (id == 2) {
          wheelIcons.append("None");
          wheelIcons.append("Stock");
          wheelIcons.append(getHolidayThemes());

          appendCurrentTheme(wheelIcons, "WheelIcon", params, assetKeys);

          wheelIcons.sort();

          QString steeringWheelToSelect = MultiOptionDialog::getSelection(tr("Select a steering wheel"), wheelIcons, assetKeys.key(QString::fromStdString(params.get("WheelIcon"))), this);
          if (!steeringWheelToSelect.isEmpty()) {
            manageWheelIconsButton->setValue(storeThemeName(steeringWheelToSelect, "WheelIcon", params, assetKeys));
          }
        }
      });
      manageWheelIconsButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageWheelIconsButton;
    } else if (param == "DownloadStatusLabel") {
      downloadStatusLabel = new LabelControl(title, tr("Idle"));
      themeToggle = downloadStatusLabel;
    } else if (param == "StartupAlert") {
      startupAlertButton = new FrogPilotButtonsControl(title, desc, icon, {tr("STOCK"), tr("FROGPILOT"), tr("CUSTOM"), tr("CLEAR")}, true);

      QObject::connect(startupAlertButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        int maxLengthTop = 35;
        int maxLengthBottom = 45;

        if (id == 0) {
          params.put("StartupMessageTop", "Be ready to take over at any time");
          params.put("StartupMessageBottom", "Always keep hands on wheel and eyes on road");
        } else if (id == 1) {
          params.put("StartupMessageTop", "Hop in and buckle up!");
          params.put("StartupMessageBottom", "Human-tested, frog-approved 🐸");
        } else if (id == 2) {
          QString currentTop = QString::fromStdString(params.get("StartupMessageTop"));
          QString newTop = InputDialog::getText(tr("Enter the text for the top half"), this, tr("Characters: 0/%1").arg(maxLengthTop), false, -1, currentTop, maxLengthTop).trimmed();
          if (!newTop.isEmpty()) {
            params.put("StartupMessageTop", newTop.toStdString());

            QString currentBottom = QString::fromStdString(params.get("StartupMessageBottom"));
            QString newBottom = InputDialog::getText(tr("Enter the text for the bottom half"), this, tr("Characters: 0/%1").arg(maxLengthBottom), false, -1, currentBottom, maxLengthBottom).trimmed();
            if (!newBottom.isEmpty()) {
              params.put("StartupMessageBottom", newBottom.toStdString());
            }
          }
        } else if (id == 3) {
          if (FrogPilotConfirmationDialog::yesorno(tr("Clear your startup message? Nothing will be shown at the start of a drive."), this)) {
            params.remove("StartupMessageTop");
            params.remove("StartupMessageBottom");
          }
        }
        updateStartupAlert();
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
      ConfirmationDialog::alert(tr("\"Random Themes\" only picks from themes you've already downloaded, so grab the ones you want it to use!"), this);

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
  updateStartupAlert();

  colorsDownloaded = params.get("DownloadableColors").empty();
  distanceIconsDownloaded = params.get("DownloadableDistanceIcons").empty();
  iconsDownloaded = params.get("DownloadableIcons").empty();
  signalsDownloaded = params.get("DownloadableSignals").empty();
  soundsDownloaded = params.get("DownloadableSounds").empty();
  wheelsDownloaded = params.get("DownloadableWheels").empty();

  frogpilotToggleLevels = parent->frogpilotToggleLevels;

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

    randomThemes = false;
  }

  updateToggles();
}

void FrogPilotThemesPanel::updateStartupAlert() {
  const QString currentTop = QString::fromStdString(params.get("StartupMessageTop"));
  const QString currentBottom = QString::fromStdString(params.get("StartupMessageBottom"));

  if (currentTop == "Be ready to take over at any time" && currentBottom == "Always keep hands on wheel and eyes on road") {
    startupAlertButton->setCheckedButton(0);
  } else if (currentTop == "Hop in and buckle up!" && currentBottom == "Human-tested, frog-approved 🐸") {
    startupAlertButton->setCheckedButton(1);
  } else if (!currentTop.isEmpty() || !currentBottom.isEmpty()) {
    startupAlertButton->setCheckedButton(2);
  } else {
    startupAlertButton->clearCheckedButtons(true);
  }
}

void FrogPilotThemesPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible() || finalizingDownload) {
    return;
  }

  if (themeDownloading) {
    QString progress = QString::fromStdString(params_memory.get("ThemeDownloadProgress"));
    bool downloadFailed = progress.contains(QRegularExpression("cancelled|failed|offline", QRegularExpression::CaseInsensitiveOption));

    if (progress != "Downloading...") {
      static const QMap<QString, QString> progressTranslations = {
        {"Unpacking theme...", tr("Unpacking theme...")},
        {"Downloaded!", tr("Downloaded!")},
        {"Download cancelled...", tr("Download cancelled...")},
        {"Download failed...", tr("Download failed...")},
        {"GitHub and GitLab are offline...", tr("GitHub and GitLab are offline...")}
      };
      downloadStatusLabel->setText(progressTranslations.value(progress, progress));
    }

    if (progress == "Downloaded!" || downloadFailed) {
      finalizingDownload = true;

      QTimer::singleShot(2500, this, [this]() {
        cancellingDownload = false;
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

        downloadStatusLabel->setText(tr("Idle"));
      });
    }
  }

  bool parked = !s.scene.started || fs.frogpilot_scene.parked || fs.frogpilot_toggles.value("frogs_go_moo").toBool();

  manageCustomColorsButton->setText(1, colorDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageCustomColorsButton->setEnabledButtons(0, !themeDownloading && !randomThemes);
  manageCustomColorsButton->setEnabledButtons(1, !cancellingDownload && !finalizingDownload && (colorDownloading || (!themeDownloading && !colorsDownloaded && fs.frogpilot_scene.online && parked)));
  manageCustomColorsButton->setEnabledButtons(2, !themeDownloading);

  manageCustomIconsButton->setText(1, iconDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageCustomIconsButton->setEnabledButtons(0, !themeDownloading && !randomThemes);
  manageCustomIconsButton->setEnabledButtons(1, !cancellingDownload && !finalizingDownload && (iconDownloading || (!themeDownloading && !iconsDownloaded && fs.frogpilot_scene.online && parked)));
  manageCustomIconsButton->setEnabledButtons(2, !themeDownloading);

  manageCustomSignalsButton->setText(1, signalDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageCustomSignalsButton->setEnabledButtons(0, !themeDownloading && !randomThemes);
  manageCustomSignalsButton->setEnabledButtons(1, !cancellingDownload && !finalizingDownload && (signalDownloading || (!themeDownloading && !signalsDownloaded && fs.frogpilot_scene.online && parked)));
  manageCustomSignalsButton->setEnabledButtons(2, !themeDownloading);

  manageCustomSoundsButton->setText(1, soundDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageCustomSoundsButton->setEnabledButtons(0, !themeDownloading && !randomThemes);
  manageCustomSoundsButton->setEnabledButtons(1, !cancellingDownload && !finalizingDownload && (soundDownloading || (!themeDownloading && !soundsDownloaded && fs.frogpilot_scene.online && parked)));
  manageCustomSoundsButton->setEnabledButtons(2, !themeDownloading);

  manageDistanceIconsButton->setText(1, distanceIconDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageDistanceIconsButton->setEnabledButtons(0, !themeDownloading && !randomThemes);
  manageDistanceIconsButton->setEnabledButtons(1, !cancellingDownload && !finalizingDownload && (distanceIconDownloading || (!themeDownloading && !distanceIconsDownloaded && fs.frogpilot_scene.online && parked)));
  manageDistanceIconsButton->setEnabledButtons(2, !themeDownloading);

  manageWheelIconsButton->setText(1, wheelDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageWheelIconsButton->setEnabledButtons(0, !themeDownloading && !randomThemes);
  manageWheelIconsButton->setEnabledButtons(1, !cancellingDownload && !finalizingDownload && (wheelDownloading || (!themeDownloading && !wheelsDownloaded && fs.frogpilot_scene.online && parked)));
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

    bool setVisible = parent->tuningLevel >= frogpilotToggleLevels[key].toDouble();

    if (key == "CustomDistanceIcons") {
      setVisible &= params.getBool("CustomUI") && params.getBool("OnroadDistanceButton");
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
