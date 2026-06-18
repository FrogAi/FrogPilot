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
  output.replace(" - by: ", "~");
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
      displayName = (parts.size() <= 1 || useFiles || !baseName.contains("-")) ? parts.join(" ") : QString("%1 (%2)").arg(parts[0], parts.mid(1).join(" "));
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

FrogPilotThemesPanel::FrogPilotThemesPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  QStackedLayout *themesLayout = new QStackedLayout();
  addItem(themesLayout);

  FrogPilotListWidget *themesList = new FrogPilotListWidget(this);

  ScrollView *themesPanel = new ScrollView(themesList, this);

  themesLayout->addWidget(themesPanel);

  FrogPilotListWidget *customThemesList = new FrogPilotListWidget(this);

  ScrollView *customThemesPanel = new ScrollView(customThemesList, this);

  themesLayout->addWidget(customThemesPanel);

  const std::vector<std::tuple<QString, QString, QString, QString>> themeToggles {
    {"CustomThemes", tr("Custom Themes"), tr("<b>Swap openpilot's colors, icons, sounds, turn signal animations, steering wheel picture and personality button for a theme pack you download.</b><br><br>You mix and match freely, so one theme's colors can run alongside another's sounds. Packs are made by other drivers, and you can build your own with the \"Theme Maker\" in \"The Pond\"."), "../../frogpilot/assets/toggle_icons/icon_frog.png"},
    {"ColorScheme", tr("Color Scheme"), tr("<b>Change the colors openpilot draws on the driving screen, mainly the path ahead of you and the lane lines.</b><br><br>\"Stock\" is openpilot's normal green path with white lane lines. A scheme also recolors the marker on the car ahead and the sidebar boxes, but the road edges are always red and never change. Holiday options match the holiday they are named after, and a downloaded pack brings its own set of colors."), ""},
    {"DistanceIconPack", tr("Distance Button"), tr("<b>Change the icons on the driving personality button, the one you tap on the driving screen to switch between Aggressive, Standard and Relaxed.</b><br><br>Each pack draws four icons: one each for Aggressive, Standard and Relaxed, plus one that takes over while Traffic Mode is on. This row only appears while that button is switched on under \"Driving Personality Button\"."), ""},
    {"IconPack", tr("Icon Pack"), tr("<b>Change the settings, home and flag buttons on openpilot's sidebar.</b><br><br>\"Stock\" puts the normal three back. A pack replaces all three at once and nothing else, so every other icon openpilot draws stays stock."), ""},
    {"SignalAnimation", tr("Turn Signal"), tr("<b>Play an animation across the driving screen for as long as your turn signal is on.</b><br><br>The animation runs toward whichever side you signalled. \"None\" turns it off, and each downloaded pack brings its own animation."), ""},
    {"SoundPack", tr("Sound Pack"), tr("<b>Change the chimes openpilot plays for its alerts, like the sound when it starts driving or warns you about something.</b><br><br>\"Stock\" uses openpilot's normal chimes. A pack only replaces the sound files it actually ships and anything it leaves out stays stock, so the holiday packs mostly bring just their own engage and disengage chimes. How loud each one plays is set separately under \"Alert Volumes\" in \"Alerts and Sounds\"."), ""},
    {"WheelIcon", tr("Steering Wheel"), tr("<b>Change the steering wheel picture in the top right corner of the driving screen, which spins as openpilot steers.</b><br><br>\"Stock\" uses openpilot's normal wheel and \"None\" hides it completely. Some downloaded wheels are animated."), ""},
    {"DownloadStatusLabel", tr("Download Status"), "", ""},

    {"HolidayThemes", tr("Holiday Themes"), tr("<b>Dress openpilot up for thirteen holidays through the year, swapping the colors, icons, sounds, turn signals, steering wheel and personality button all at once.</b><br><br>Smaller ones like April Fools or Cinco de Mayo run on the day itself. Easter, Halloween, Thanksgiving and Christmas start on the Monday of that week and finish on the day, so they last anywhere from one day to a full week depending on where the date falls.<br><br>While a holiday is running it replaces the themes you picked, and your own choices come back the next day."), "../../frogpilot/assets/toggle_icons/icon_calendar.png"},
    {"RainbowPath", tr("Rainbow Path"), tr("<b>Paint the driving path in shifting rainbow colors that scroll faster the quicker you go, like the Rainbow Road track from Mario Kart.</b><br><br>The rainbow replaces whatever color the path normally uses, including one that came with a theme you downloaded. With \"Acceleration Path\" also on, the green and red speed colors take over whenever openpilot speeds up or slows down, so the rainbow only shows while you hold a steady speed."), "../../frogpilot/assets/toggle_icons/icon_rainbow.png"},
    {"RandomEvents", tr("Random Events"), tr("<b>Play a rare joke alert, with its own sound and sometimes its own steering wheel picture, when something unusual happens on a drive.</b><br><br>Taking off hard, a corner sharper than openpilot can steer through, or a collision warning can each set one off. Every alert can only happen once per drive, a swapped steering wheel goes back to normal after about five seconds, and none of them change how openpilot drives."), "../../frogpilot/assets/toggle_icons/icon_random.png"},
    {"RandomThemes", tr("Random Themes"), tr("<b>Start every drive with a different theme, picked at random from the packs you have already downloaded.</b><br><br>Nothing happens until you download at least one pack. While this is on, the rows inside \"Custom Themes\" stop offering \"SELECT\", and turning it back off gives you your own picks again."), "../../frogpilot/assets/toggle_icons/icon_random_themes.png"},
    {"StartupAlert", tr("Startup Alert"), tr("<b>Change the two lines of text openpilot shows on screen at the start of every drive.</b><br><br>\"STOCK\" is openpilot's usual safety reminder and \"FROGPILOT\" is the frog version. \"CUSTOM\" lets you write your own, up to 35 characters on the top line and 45 on the bottom, and \"CLEAR\" leaves the screen blank."), "../../frogpilot/assets/toggle_icons/icon_message.png"}
  };

  for (const auto &[param, title, desc, icon] : themeToggles) {
    AbstractControl *themeToggle;

    if (param == "CustomThemes") {
      FrogPilotManageControl *personalizeOpenpilotToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(personalizeOpenpilotToggle, &FrogPilotManageControl::manageButtonClicked, [customThemesPanel, themesLayout]() {
        themesLayout->setCurrentWidget(customThemesPanel);
      });
      themeToggle = personalizeOpenpilotToggle;
    } else if (param == "ColorScheme") {
      manageColorSchemeButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageColorSchemeButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList colorSchemes = getThemeList(randomThemes, QDir(themePacksDirectory.path()), "colors", "ColorScheme", params);

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

            QTimer::singleShot(2500, this, [this]() {
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

              downloadStatusLabel->setText(tr("Downloading..."));
            }
          }
        } else if (id == 2) {
          colorSchemes.append("Stock");
          colorSchemes.append(getHolidayThemes());
          colorSchemes.sort();

          QString colorSchemeToSelect = MultiOptionDialog::getSelection(tr("Select a color scheme"), colorSchemes, getThemeName("ColorScheme", params), this);
          if (!colorSchemeToSelect.isEmpty()) {
            manageColorSchemeButton->setValue(storeThemeName(colorSchemeToSelect, "ColorScheme", params));
          }
        }
      });
      manageColorSchemeButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageColorSchemeButton;
    } else if (param == "DistanceIconPack") {
      manageDistanceIconPackButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageDistanceIconPackButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList distanceIconPacks = getThemeList(randomThemes, QDir(themePacksDirectory.path()), "distance_icons", "DistanceIconPack", params);

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

            QTimer::singleShot(2500, this, [this]() {
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

              downloadStatusLabel->setText(tr("Downloading..."));
            }
          }
        } else if (id == 2) {
          distanceIconPacks.append("Stock");
          distanceIconPacks.append(getHolidayThemes());
          distanceIconPacks.sort();

          QString distanceIconPackToSelect = MultiOptionDialog::getSelection(tr("Select a distance icon pack"), distanceIconPacks, getThemeName("DistanceIconPack", params), this);
          if (!distanceIconPackToSelect.isEmpty()) {
            manageDistanceIconPackButton->setValue(storeThemeName(distanceIconPackToSelect, "DistanceIconPack", params));
          }
        }
      });
      manageDistanceIconPackButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageDistanceIconPackButton;
    } else if (param == "IconPack") {
      manageIconPackButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageIconPackButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList iconPacks = getThemeList(randomThemes, QDir(themePacksDirectory.path()), "icons", "IconPack", params);

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

            QTimer::singleShot(2500, this, [this]() {
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

              downloadStatusLabel->setText(tr("Downloading..."));
            }
          }
        } else if (id == 2) {
          iconPacks.append("Stock");
          iconPacks.append(getHolidayThemes());
          iconPacks.sort();

          QString iconPackToSelect = MultiOptionDialog::getSelection(tr("Select an icon pack"), iconPacks, getThemeName("IconPack", params), this);
          if (!iconPackToSelect.isEmpty()) {
            manageIconPackButton->setValue(storeThemeName(iconPackToSelect, "IconPack", params));
          }
        }
      });
      manageIconPackButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageIconPackButton;
    } else if (param == "SignalAnimation") {
      manageSignalAnimationButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageSignalAnimationButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList signalAnimations = getThemeList(randomThemes, QDir(themePacksDirectory.path()), "signals", "SignalAnimation", params);

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

            QTimer::singleShot(2500, this, [this]() {
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

              downloadStatusLabel->setText(tr("Downloading..."));
            }
          }
        } else if (id == 2) {
          signalAnimations.append("None");
          signalAnimations.append(getHolidayThemes());
          signalAnimations.sort();

          QString signalAnimationToSelect = MultiOptionDialog::getSelection(tr("Select a signal animation"), signalAnimations, getThemeName("SignalAnimation", params), this);
          if (!signalAnimationToSelect.isEmpty()) {
            manageSignalAnimationButton->setValue(storeThemeName(signalAnimationToSelect, "SignalAnimation", params));
          }
        }
      });
      manageSignalAnimationButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageSignalAnimationButton;
    } else if (param == "SoundPack") {
      manageSoundPackButton = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageSoundPackButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList soundPacks = getThemeList(randomThemes, QDir(themePacksDirectory.path()), "sounds", "SoundPack", params);

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

            QTimer::singleShot(2500, this, [this]() {
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

              downloadStatusLabel->setText(tr("Downloading..."));
            }
          }
        } else if (id == 2) {
          soundPacks.append("Stock");
          soundPacks.append(getHolidayThemes());
          soundPacks.sort();

          QString soundPackToSelect = MultiOptionDialog::getSelection(tr("Select a sound pack"), soundPacks, getThemeName("SoundPack", params), this);
          if (!soundPackToSelect.isEmpty()) {
            manageSoundPackButton->setValue(storeThemeName(soundPackToSelect, "SoundPack", params));
          }
        }
      });
      manageSoundPackButton->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageSoundPackButton;
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

            QTimer::singleShot(2500, this, [this]() {
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

              downloadStatusLabel->setText(tr("Downloading..."));
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
      downloadStatusLabel = new LabelControl(title, tr("Idle"));
      themeToggle = downloadStatusLabel;

    } else if (param == "RandomThemes") {
      std::vector<QString> randomThemesToggles{"RandomThemesHolidays"};
      std::vector<QString> randomThemesToggleNames{tr("Include Holiday Themes")};
      themeToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, randomThemesToggles, randomThemesToggleNames);

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
          QString currentTop = QString::fromStdString(params.get("StartupMessageTop"));
          QString newTop = InputDialog::getText(tr("Enter the text for the top half"), this, tr("Characters: 0/%1").arg(maxLengthTop), false, 1, currentTop, maxLengthTop).trimmed();
          if (!newTop.isEmpty()) {
            params.put("StartupMessageTop", newTop.toStdString());

            QString currentBottom = QString::fromStdString(params.get("StartupMessageBottom"));
            QString newBottom = InputDialog::getText(tr("Enter the text for the bottom half"), this, tr("Characters: 0/%1").arg(maxLengthBottom), false, 1, currentBottom, maxLengthBottom).trimmed();
            if (!newBottom.isEmpty()) {
              params.put("StartupMessageBottom", newBottom.toStdString());
            }
          }
        } else if (id == 3) {
          if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to completely reset your startup message?"), this)) {
            params.remove("StartupMessageTop");
            params.remove("StartupMessageBottom");

            startupAlertButton->clearCheckedButtons();
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

      if (param == "CustomThemes") {
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

  QObject::connect(static_cast<ToggleControl *>(toggles["CustomThemes"]), &ToggleControl::toggleFlipped, this, &FrogPilotThemesPanel::updateToggles);
  QObject::connect(static_cast<ToggleControl*>(toggles["RandomThemes"]), &ToggleControl::toggleFlipped, [this](bool state) {
    if (state) {
      ConfirmationDialog::alert(tr("\"Random Themes\" only works with downloaded themes, so make sure you download the themes you want it to use!"), this);

      manageColorSchemeButton->setValue("");
      manageColorSchemeButton->setVisibleButton(2, false);

      manageDistanceIconPackButton->setValue("");
      manageDistanceIconPackButton->setVisibleButton(2, false);

      manageIconPackButton->setValue("");
      manageIconPackButton->setVisibleButton(2, false);

      manageSignalAnimationButton->setValue("");
      manageSignalAnimationButton->setVisibleButton(2, false);

      manageSoundPackButton->setValue("");
      manageSoundPackButton->setVisibleButton(2, false);

      manageWheelIconsButton->setValue("");
      manageWheelIconsButton->setVisibleButton(2, false);
    } else {
      manageColorSchemeButton->setValue(getThemeName("ColorScheme", params));
      manageColorSchemeButton->setVisibleButton(2, true);

      manageDistanceIconPackButton->setValue(getThemeName("DistanceIconPack", params));
      manageDistanceIconPackButton->setVisibleButton(2, true);

      manageIconPackButton->setValue(getThemeName("IconPack", params));
      manageIconPackButton->setVisibleButton(2, true);

      manageSignalAnimationButton->setValue(getThemeName("SignalAnimation", params));
      manageSignalAnimationButton->setVisibleButton(2, true);

      manageSoundPackButton->setValue(getThemeName("SoundPack", params));
      manageSoundPackButton->setVisibleButton(2, true);

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

  if (params.getBool("RandomThemes")) {
    manageColorSchemeButton->setValue("");
    manageColorSchemeButton->setVisibleButton(2, false);

    manageDistanceIconPackButton->setValue("");
    manageDistanceIconPackButton->setVisibleButton(2, false);

    manageIconPackButton->setValue("");
    manageIconPackButton->setVisibleButton(2, false);

    manageSignalAnimationButton->setValue("");
    manageSignalAnimationButton->setVisibleButton(2, false);

    manageSoundPackButton->setValue("");
    manageSoundPackButton->setVisibleButton(2, false);

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

  const FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;

  if (themeDownloading) {
    QString progress = QString::fromStdString(params_memory.get("ThemeDownloadProgress"));
    bool downloadFailed = progress.contains(QRegularExpression("cancelled|exists|failed|offline", QRegularExpression::CaseInsensitiveOption));

   if (progress != "Downloading...") {
      static const QMap<QString, QString> progressTranslations = {
        {"Download cancelled...", tr("Download cancelled...")},
        {"Download failed...", tr("Download failed...")},
        {"Downloaded!", tr("Downloaded!")},
        {"GitHub and GitLab are offline...", tr("GitHub and GitLab are offline...")},
        {"Repository unavailable", tr("Repository unavailable")},
        {"Unpacking theme...", tr("Unpacking theme...")},
        {"Verifying authenticity...", tr("Verifying authenticity...")}
      };

      if (progressTranslations.contains(progress)) {
        downloadStatusLabel->setText(progressTranslations[progress]);
      } else if (progress.endsWith("%")) {
        downloadStatusLabel->setText(progress);
      } else {
        downloadStatusLabel->setText(tr("Idle"));
      }
    }

    if (progress == "Downloaded!" || downloadFailed) {
      finalizingDownload = true;

      QTimer::singleShot(2500, this, [this]() {
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

  bool parked = !s.scene.started || frogpilot_scene.parked || parent->isFrogsGoMoo;

  manageColorSchemeButton->setText(1, colorDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageColorSchemeButton->setEnabledButtons(0, !themeDownloading);
  manageColorSchemeButton->setEnabledButtons(1, frogpilot_scene.online && (!themeDownloading || colorDownloading) && !cancellingDownload && !finalizingDownload && !colorsDownloaded && parked);
  manageColorSchemeButton->setEnabledButtons(2, !themeDownloading);

  manageDistanceIconPackButton->setText(1, distanceIconDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageDistanceIconPackButton->setEnabledButtons(0, !themeDownloading);
  manageDistanceIconPackButton->setEnabledButtons(1, frogpilot_scene.online && (!themeDownloading || distanceIconDownloading) && !cancellingDownload && !finalizingDownload && !distanceIconsDownloaded && parked);
  manageDistanceIconPackButton->setEnabledButtons(2, !themeDownloading);

  manageIconPackButton->setText(1, iconDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageIconPackButton->setEnabledButtons(0, !themeDownloading);
  manageIconPackButton->setEnabledButtons(1, frogpilot_scene.online && (!themeDownloading || iconDownloading) && !cancellingDownload && !finalizingDownload && !iconsDownloaded && parked);
  manageIconPackButton->setEnabledButtons(2, !themeDownloading);

  manageSignalAnimationButton->setText(1, signalDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageSignalAnimationButton->setEnabledButtons(0, !themeDownloading);
  manageSignalAnimationButton->setEnabledButtons(1, frogpilot_scene.online && (!themeDownloading || signalDownloading) && !cancellingDownload && !finalizingDownload && !signalsDownloaded && parked);
  manageSignalAnimationButton->setEnabledButtons(2, !themeDownloading);

  manageSoundPackButton->setText(1, soundDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageSoundPackButton->setEnabledButtons(0, !themeDownloading);
  manageSoundPackButton->setEnabledButtons(1, frogpilot_scene.online && (!themeDownloading || soundDownloading) && !cancellingDownload && !finalizingDownload && !soundsDownloaded && parked);
  manageSoundPackButton->setEnabledButtons(2, !themeDownloading);

  manageWheelIconsButton->setText(1, wheelDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageWheelIconsButton->setEnabledButtons(0, !themeDownloading);
  manageWheelIconsButton->setEnabledButtons(1, frogpilot_scene.online && (!themeDownloading || wheelDownloading) && !cancellingDownload && !finalizingDownload && !wheelsDownloaded && parked);
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

    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels[key].toDouble();

    if (key == "DistanceIconPack") {
      setVisible &= params.getBool("QOLVisuals") && params.getBool("OnroadDistanceButton");
    }

    else if (key == "RandomThemes") {
      setVisible &= params.getBool("CustomThemes");
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (customThemeKeys.contains(key)) {
        toggles["CustomThemes"]->setVisible(true);
      }
    }
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
