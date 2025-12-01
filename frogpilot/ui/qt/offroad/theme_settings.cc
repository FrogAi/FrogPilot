#include "frogpilot/ui/qt/offroad/theme_settings.h"

bool isUserCreatedTheme(const QString &themeName) {
  return themeName.endsWith("-user_created");
}

void updateAssetParam(const QString &assetParam, Params &params, const QString &value) {
  QStringList assets = QString::fromStdString(params.get(assetParam.toStdString())).split(",", QString::SkipEmptyParts);
  if (!assets.contains(value)) {
    assets.append(value);
  }
  assets.sort();

  params.put(assetParam.toStdString(), assets.join(",").toStdString());
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

void deleteThemeAsset(const QDir &directory, const QString &subFolder, const QString &assetParam, const QString &assetKey, Params &params) {
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
      updateAssetParam(assetParam, params, formatThemeName(assetKey, subFolder.isEmpty()));
    }
  }
}

void downloadThemeAssets(const QStringList &inputs, const QString &component) {
  QStringList outputs;
  for (const QString &input : inputs) {
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

    outputs.append(output);
  }

  frogpilotUIState()->downloadTheme(component, outputs);
}

const QMap<QString, QString> &getBuiltinThemes() {
  static const QMap<QString, QString> builtinThemes = {
    {"april_fools", QObject::tr("April Fools")},
    {"christmas", QObject::tr("Christmas")},
    {"cinco_de_mayo", QObject::tr("Cinco de Mayo")},
    {"easter", QObject::tr("Easter")},
    {"fourth_of_july", QObject::tr("Fourth of July")},
    {"halloween", QObject::tr("Halloween")},
    {"may_the_fourth", QObject::tr("May the Fourth")},
    {"new_years", QObject::tr("New Year's")},
    {"none", QObject::tr("None")},
    {"st_patricks_day", QObject::tr("St. Patrick's Day")},
    {"stitch_day", QObject::tr("Stitch Day")},
    {"stock", QObject::tr("Stock")},
    {"thanksgiving", QObject::tr("Thanksgiving")},
    {"valentines_day", QObject::tr("Valentine's Day")},
    {"world_frog_day", QObject::tr("World Frog Day")}
  };
  return builtinThemes;
}

QStringList getBuiltinThemeNames(const QStringList &excludedKeys) {
  QStringList names;
  const QMap<QString, QString> &builtinThemes = getBuiltinThemes();
  for (QMap<QString, QString>::const_iterator it = builtinThemes.constBegin(); it != builtinThemes.constEnd(); ++it) {
    if (!excludedKeys.contains(it.key())) {
      names.append(it.value());
    }
  }
  return names;
}

QString getThemeName(const std::string &paramKey, Params &params) {
  const QString key = QString::fromStdString(params.get(paramKey));
  const QMap<QString, QString> &builtinThemes = getBuiltinThemes();
  if (builtinThemes.contains(key)) {
    return builtinThemes.value(key);
  }
  return formatThemeName(key, paramKey == "WheelIcon");
}

QString getActiveThemeAsset(const QString &subFolder) {
  if (subFolder.isEmpty()) {
    const QFileInfoList wheels = QDir("../../frogpilot/assets/active_theme/steering_wheel").entryInfoList(QDir::Files);
    return wheels.isEmpty() ? QString() : QFileInfo(wheels.first().symLinkTarget()).completeBaseName();
  }
  return QFileInfo(QFileInfo("../../frogpilot/assets/active_theme/" + subFolder).symLinkTarget()).dir().dirName();
}

QStringList getThemeList(bool randomThemes, const QDir &directory, const QString &subFolder, const QString &assetParam, Params &params,
                         QMap<QString, QString> &assetKeys) {
  const bool useFiles = subFolder.isEmpty();

  const QString currentAsset = randomThemes ? getActiveThemeAsset(subFolder) : QString::fromStdString(params.get(assetParam.toStdString()));

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
    if (assetKey != currentAsset && !themes.contains(displayName)) {
      themes.append(displayName);
    }
  }
  return themes;
}

QString normalizeThemeName(const QString &input) {
  const QString builtinKey = getBuiltinThemes().key(input);
  if (!builtinKey.isEmpty()) {
    return builtinKey;
  }

  QString output = input.toLower().remove("(").remove(")").remove("'").remove(".");
  output.replace(" ", input.contains("(") ? "-" : "_");
  output.replace("_🌟", "-user_created");
  return output.trimmed();
}

void appendCurrentTheme(QStringList &themes, const std::string &paramKey, Params &params, QMap<QString, QString> &assetKeys) {
  const QString currentKey = QString::fromStdString(params.get(paramKey));
  for (int i = 0; i < themes.size(); ++i) {
    const QString theme = themes[i];
    const QString assetKey = assetKeys.value(theme);
    const QString builtinKey = normalizeThemeName(theme);
    if (!assetKey.isEmpty() && assetKey != builtinKey && (themes.count(theme) > 1 || assetKey == currentKey)) {
      const QString downloadedTheme = theme + QString(" [%1]").arg(assetKey);
      if (themes.count(theme) > 1) {
        themes[i] = downloadedTheme;
      }
      assetKeys.insert(downloadedTheme, assetKey);
      assetKeys.insert(theme, builtinKey);
    }
  }

  if (currentKey.isEmpty()) {
    return;
  }

  QString current = assetKeys.key(currentKey);
  if (current.isEmpty() || !themes.contains(current)) {
    for (const QString &theme : themes) {
      if (!assetKeys.contains(theme) && normalizeThemeName(theme) == currentKey) {
        assetKeys.remove(current);
        current = theme;
        break;
      }
    }
  }

  if (current.isEmpty()) {
    current = getThemeName(paramKey, params);
    if (assetKeys.contains(current) && assetKeys.value(current) != currentKey) {
      current += QString(" [%1]").arg(currentKey);
    }
  }

  assetKeys.insert(current, currentKey);
  if (!themes.contains(current)) {
    themes.append(current);
  }
}

QString storeThemeName(const QString &input, const std::string &paramKey, Params &params, const QMap<QString, QString> &assetKeys) {
  if (assetKeys.contains(input)) {
    params.put(paramKey, assetKeys.value(input).toStdString());
  } else {
    params.put(paramKey, normalizeThemeName(input).toStdString());
  }
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

  themeAssets = {
    {"ColorScheme", {themePacksDirectory, "colors", "colors", "DownloadableColors",
                     tr("Select color schemes to delete"), tr("Delete the \"%1\" color scheme?"), tr("Delete the %1 selected color schemes?"),
                     tr("Select color schemes to download"), tr("Select a color scheme"), {"none"}}},
    {"DistanceIconPack", {themePacksDirectory, "distance_icons", "distance_icons", "DownloadableDistanceIcons",
                          tr("Select personality button packs to delete"), tr("Delete the \"%1\" personality button pack?"), tr("Delete the %1 selected personality button packs?"),
                          tr("Select personality button packs to download"), tr("Select a personality button pack"), {"april_fools", "easter", "none"}}},
    {"IconPack", {themePacksDirectory, "icons", "icons", "DownloadableIcons",
                  tr("Select icon packs to delete"), tr("Delete the \"%1\" icon pack?"), tr("Delete the %1 selected icon packs?"),
                  tr("Select icon packs to download"), tr("Select an icon pack"), {"none"}}},
    {"SignalAnimation", {themePacksDirectory, "signals", "signals", "DownloadableSignals",
                         tr("Select signal animations to delete"), tr("Delete the \"%1\" signal animation?"), tr("Delete the %1 selected signal animations?"),
                         tr("Select signal animations to download"), tr("Select a signal animation"), {"stock"}}},
    {"SoundPack", {themePacksDirectory, "sounds", "sounds", "DownloadableSounds",
                   tr("Select sound packs to delete"), tr("Delete the \"%1\" sound pack?"), tr("Delete the %1 selected sound packs?"),
                   tr("Select sound packs to download"), tr("Select a sound pack"), {"none"}}},
    {"WheelIcon", {wheelsDirectory, "", "steering_wheels", "DownloadableWheels",
                   tr("Select steering wheels to delete"), tr("Delete the \"%1\" steering wheel?"), tr("Delete the %1 selected steering wheels?"),
                   tr("Select steering wheels to download"), tr("Select a steering wheel"), {}}}
  };

  const std::vector<std::tuple<QString, QString, QString, QString>> themeToggles {
    {"CustomThemes", tr("Custom Themes"), tr("<b>Swap openpilot's colors, icons, sounds, turn signal animations, steering wheel picture and personality button for a theme pack you download.</b><br><br>You mix and match freely, so one theme's colors can run alongside another's sounds. Packs are made by other drivers, and you can build your own with the \"Theme Maker\" in \"The Pond\"."), "../../frogpilot/assets/toggle_icons/icon_frog.svg"},
    {"ColorScheme", tr("Color Scheme"), tr("<b>Change the colors openpilot draws on the driving screen, mainly the path ahead of you and the lane lines.</b><br><br>\"Stock\" is openpilot's normal green path with white lane lines. A scheme also recolors the marker on the car ahead and the sidebar boxes, but the road edges are always red and never change. Holiday options match the holiday they are named after, and a downloaded pack brings its own set of colors."), ""},
    {"IconPack", tr("Icon Pack"), tr("<b>Change the settings, home and flag buttons on openpilot's sidebar.</b><br><br>\"Stock\" puts the normal three back. A pack replaces all three at once and nothing else, so every other icon openpilot draws stays stock."), ""},
    {"DistanceIconPack", tr("Personality Button"), tr("<b>Change the icons on the driving personality button, the one you tap on the driving screen to switch between Aggressive, Standard and Relaxed.</b><br><br>Each pack draws four icons: one each for Aggressive, Standard and Relaxed, plus one that takes over while Traffic Mode is on. This row only appears while that button is switched on under \"Driving Personality Button\"."), ""},
    {"SoundPack", tr("Sound Pack"), tr("<b>Change the chimes openpilot plays for its alerts, like the sound when it starts driving or warns you about something.</b><br><br>\"Stock\" uses openpilot's normal chimes. A pack only replaces the sound files it actually ships and anything it leaves out stays stock, so the holiday packs mostly bring just their own engage and disengage chimes. How loud each one plays is set separately under \"Alert Volumes\" in \"Alerts and Sounds\"."), ""},
    {"WheelIcon", tr("Steering Wheel"), tr("<b>Change the steering wheel picture in the top right corner of the driving screen, which spins as openpilot steers.</b><br><br>\"Stock\" uses openpilot's normal wheel and \"None\" hides it completely. Some downloaded wheels are animated."), ""},
    {"SignalAnimation", tr("Turn Signal"), tr("<b>Play an animation across the driving screen for as long as your turn signal is on.</b><br><br>The animation runs toward whichever side you signalled. \"None\" turns it off, and each downloaded pack brings its own animation."), ""},
    {"DownloadStatusLabel", tr("Download Status"), "", ""},

    {"HolidayThemes", tr("Holiday Themes"), tr("<b>Dress openpilot up for thirteen holidays through the year, swapping the colors, icons, sounds, turn signals, steering wheel and personality button all at once.</b><br><br>Smaller ones like April Fools or Cinco de Mayo run on the day itself. Easter, Halloween, Thanksgiving and Christmas start on the Monday of that week and finish on the day, so they last anywhere from one day to a full week depending on where the date falls.<br><br>While a holiday is running it replaces the themes you picked, and your own choices come back the next day."), "../../frogpilot/assets/toggle_icons/icon_calendar.svg"},
    {"RainbowPath", tr("Rainbow Path"), tr("<b>Paint the driving path in shifting rainbow colors that scroll faster the quicker you go, like the Rainbow Road track from Mario Kart.</b><br><br>The rainbow replaces whatever color the path normally uses, including one that came with a theme you downloaded. With \"Acceleration Path\" also on, the green and red speed colors take over whenever openpilot speeds up or slows down, so the rainbow only shows while you hold a steady speed."), "../../frogpilot/assets/toggle_icons/icon_rainbow.svg"},
    {"RandomEvents", tr("Random Events"), tr("<b>Play a rare joke alert, with its own sound and sometimes its own steering wheel picture, when something unusual happens on a drive.</b><br><br>Taking off hard, a corner sharper than openpilot can steer through, or a collision warning can each set one off. Every alert can only happen once per drive, a swapped steering wheel goes back to normal after about five seconds, and none of them change how openpilot drives."), "../../frogpilot/assets/toggle_icons/icon_random.svg"},
    {"RandomThemes", tr("Random Themes"), tr("<b>Start every drive with a theme picked at random from the built-in Frog theme and the packs you have already downloaded.</b><br><br>A downloaded pack only joins the mix once you have its colors, icons and sounds, and the same theme can come up twice in a row. With \"Include Holiday Themes\" on, the holiday themes are in the mix too. While this is on, the rows inside \"Custom Themes\" stop offering \"SELECT\", and turning it back off gives you your own picks again."), "../../frogpilot/assets/toggle_icons/icon_random_themes.svg"},
    {"StartupAlert", tr("Startup Alert"), tr("<b>Change the two lines of text openpilot shows on screen at the start of every drive.</b><br><br>\"STOCK\" is openpilot's usual safety reminder and \"FROGPILOT\" is the frog version. \"CUSTOM\" lets you write your own, up to 35 characters on the top line and 45 on the bottom."), "../../frogpilot/assets/toggle_icons/icon_message.svg"}
  };

  for (const auto &[param, title, desc, icon] : themeToggles) {
    AbstractControl *themeToggle;

    if (param == "CustomThemes") {
      FrogPilotManageControl *customThemesToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(customThemesToggle, &FrogPilotManageControl::manageButtonClicked, [customThemesPanel, themesLayout]() {
        themesLayout->setCurrentWidget(customThemesPanel);
      });
      themeToggle = customThemesToggle;
    } else if (themeAssets.count(param)) {
      const std::string paramKey = param.toStdString();

      ThemeAsset &asset = themeAssets[param];
      asset.button = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(asset.button, &FrogPilotButtonsControl::buttonClicked, [this, paramKey, &asset](int id) {
        QMap<QString, QString> assetKeys;
        QStringList themes;
        if (id != 1) {
          themes = getThemeList(randomThemes, asset.directory, asset.subFolder, QString::fromStdString(paramKey), params, assetKeys);
        }

        if (id == 0) {
          QStringList themesToDelete = FrogPilotMultiOptionDialog::getSelections(asset.deleteTitle, themes, tr("Delete"), this);
          if (!themesToDelete.isEmpty()) {
            QString confirmation;
            if (themesToDelete.size() == 1) {
              confirmation = asset.deleteOneConfirmation.arg(themesToDelete.first());
            } else {
              confirmation = asset.deleteSeveralConfirmation.arg(themesToDelete.size());
            }

            if (ConfirmationDialog::confirm(confirmation, tr("Delete"), this)) {
              for (const QString &themeToDelete : themesToDelete) {
                deleteThemeAsset(asset.directory, asset.subFolder, asset.downloadableParam, assetKeys.value(themeToDelete), params);
              }
              asset.downloaded = params.get(asset.downloadableParam.toStdString()).empty();
            }
          }
        } else if (id == 1) {
          if (asset.downloading) {
            cancellingDownload = true;

            frogpilotUIState()->cancelThemeDownload();
          } else {
            QStringList downloadableThemes = QString::fromStdString(params.get(asset.downloadableParam.toStdString())).split(",", QString::SkipEmptyParts);
            const QStringList themesToDownload = FrogPilotMultiOptionDialog::getSelections(asset.downloadTitle, downloadableThemes, tr("Download"), this);
            if (!themesToDownload.isEmpty()) {
              asset.downloading = true;
              themeDownloading = true;

              downloadThemeAssets(themesToDownload, asset.component);

              downloadStatusLabel->setText(tr("Downloading..."));
            }
          }
        } else if (id == 2) {
          themes.append(getBuiltinThemeNames(asset.excludedBuiltinThemes));

          appendCurrentTheme(themes, paramKey, params, assetKeys);

          themes.sort();

          QString themeToSelect = MultiOptionDialog::getSelection(asset.selectTitle, themes,
            assetKeys.key(QString::fromStdString(params.get(paramKey))), this);
          if (!themeToSelect.isEmpty()) {
            asset.button->setValue(storeThemeName(themeToSelect, paramKey, params, assetKeys));
          }
        }
      });
      themeToggle = asset.button;
    } else if (param == "DownloadStatusLabel") {
      downloadStatusLabel = new LabelControl(title, tr("Idle"));
      themeToggle = downloadStatusLabel;

    } else if (param == "RandomThemes") {
      std::vector<QString> randomThemesToggles{"RandomThemesHolidays"};
      std::vector<QString> randomThemesToggleNames{tr("Include Holiday Themes")};
      themeToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, randomThemesToggles, randomThemesToggleNames);

    } else if (param == "StartupAlert") {
      startupAlertButton = new FrogPilotButtonsControl(title, desc, icon, {tr("STOCK"), tr("FROGPILOT"), tr("CUSTOM")}, true);

      QObject::connect(startupAlertButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        if (id == 0) {
          params.put("StartupMessageTop", params.getStockValue("StartupMessageTop").value());
          params.put("StartupMessageBottom", params.getStockValue("StartupMessageBottom").value());
        } else if (id == 1) {
          params.put("StartupMessageTop", params.getKeyDefaultValue("StartupMessageTop").value());
          params.put("StartupMessageBottom", params.getKeyDefaultValue("StartupMessageBottom").value());
        } else if (id == 2) {
          const int maxLengthTop = 35;
          const int maxLengthBottom = 45;

          QString currentTop = QString::fromStdString(params.get("StartupMessageTop"));
          QString newTop = InputDialog::getText(tr("Enter the text for the top half"), this, "", false, 1, currentTop, maxLengthTop).trimmed();
          if (!newTop.isEmpty()) {
            params.put("StartupMessageTop", newTop.toStdString());

            QString currentBottom = QString::fromStdString(params.get("StartupMessageBottom"));
            QString newBottom = InputDialog::getText(tr("Enter the text for the bottom half"), this, "", false, 1, currentBottom, maxLengthBottom).trimmed();
            if (!newBottom.isEmpty()) {
              params.put("StartupMessageBottom", newBottom.toStdString());
            }
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
  }

  QObject::connect(static_cast<ToggleControl*>(toggles["CustomThemes"]), &ToggleControl::toggleFlipped, this, &FrogPilotThemesPanel::updateToggles);
  QObject::connect(static_cast<ToggleControl*>(toggles["RandomThemes"]), &ToggleControl::toggleFlipped, [this](bool state) {
    if (state) {
      ConfirmationDialog::alert(tr("\"Random Themes\" picks from the built-in Frog theme, the holiday themes when \"Include Holiday Themes\" is on, and themes you've already downloaded, so grab the ones you want it to use!"), this);
    }
    updateThemeSelections(state);
  });

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [themesLayout, themesPanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    themesLayout->setCurrentWidget(themesPanel);
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotThemesPanel::updateState);
}

void FrogPilotThemesPanel::showEvent(QShowEvent *event) {
  updateStartupAlert();

  for (auto &[param, asset] : themeAssets) {
    asset.downloaded = params.get(asset.downloadableParam.toStdString()).empty();
  }

  updateThemeSelections(params.getBool("RandomThemes") && parent->tuningLevel >= parent->frogpilotToggleLevels.value("RandomThemes").toDouble());

  updateToggles();
}

void FrogPilotThemesPanel::updateStartupAlert() {
  const std::string currentTop = params.get("StartupMessageTop");
  const std::string currentBottom = params.get("StartupMessageBottom");

  if (currentTop == params.getStockValue("StartupMessageTop").value() && currentBottom == params.getStockValue("StartupMessageBottom").value()) {
    startupAlertButton->setCheckedButton(0);
  } else if (currentTop == params.getKeyDefaultValue("StartupMessageTop").value() && currentBottom == params.getKeyDefaultValue("StartupMessageBottom").value()) {
    startupAlertButton->setCheckedButton(1);
  } else {
    startupAlertButton->setCheckedButton(2);
  }
}

void FrogPilotThemesPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible() || finalizingDownload) {
    return;
  }

  const FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;

  if (themeDownloading) {
    QString progress = "Downloading...";
    bool downloadFinished = false;
    if (fs.download_theme_request_time <= frogpilotProcessState.getDownloadThemeRequestTime()) {
      downloadFinished = failedCount + successCount == downloadCount;
    }
    bool downloadStopped = progress == "Download cancelled..." || progress == "GitHub and GitLab are offline...";

    if (downloadFinished && !downloadStopped) {
      if (failedCount == 0) {
        downloadStatusLabel->setText(tr("Downloaded!"));
      } else if (downloadCount == 1) {
        downloadStatusLabel->setText(tr("Download failed..."));
      } else {
        downloadStatusLabel->setText(tr("%1 of %2 downloaded, %3 failed").arg(successCount).arg(downloadCount).arg(failedCount));
      }
    } else if (downloadCount > 1 && progress.endsWith("%")) {
      downloadStatusLabel->setText(tr("Downloading %1 of %2 (%3)").arg(failedCount + successCount + 1).arg(downloadCount).arg(progress));
    } else if (progress != "Downloading..." && progress != "Downloaded!") {
      static const QMap<QString, QString> progressTranslations = {
        {"Download cancelled...", tr("Download cancelled...")},
        {"Download failed...", tr("Download failed...")},
        {"Download invalid...", tr("Download invalid...")},
        {"GitHub and GitLab are offline...", tr("GitHub and GitLab are offline...")},
        {"Unpacking theme...", tr("Unpacking theme...")},
        {"Verifying authenticity...", tr("Verifying authenticity...")}
      };

      downloadStatusLabel->setText(progressTranslations.value(progress, progress));
    }

    if (downloadFinished || downloadStopped) {
      finalizingDownload = true;

      QTimer::singleShot(2500, this, [this]() {
        cancellingDownload = false;
        finalizingDownload = false;
        themeDownloading = false;

        for (auto &[param, asset] : themeAssets) {
          asset.downloaded = params.get(asset.downloadableParam.toStdString()).empty();
          asset.downloading = false;
        }

        downloadStatusLabel->setText(tr("Idle"));
      });
    }
  }

  bool parked = !s.scene.started || frogpilot_scene.parked || parent->isFrogsGoMoo;

  static const QString cancelText = tr("CANCEL");
  static const QString downloadText = tr("DOWNLOAD");

  for (auto &[param, asset] : themeAssets) {
    asset.button->setText(1, asset.downloading ? cancelText : downloadText);
    asset.button->setEnabledButtons(0, !themeDownloading);
    asset.button->setEnabledButtons(1, !cancellingDownload && !finalizingDownload &&
      (asset.downloading || (!themeDownloading && !asset.downloaded && frogpilot_scene.online && parked)));
    asset.button->setEnabledButtons(2, !themeDownloading);
  }

  parent->keepScreenOn = themeDownloading;
}

void FrogPilotThemesPanel::updateThemeSelections(bool randomThemesEnabled) {
  for (auto &[param, asset] : themeAssets) {
    if (randomThemesEnabled) {
      asset.button->setValue("");
    } else {
      asset.button->setValue(getThemeName(param.toStdString(), params));
    }
    asset.button->setVisibleButton(2, !randomThemesEnabled);
  }

  randomThemes = randomThemesEnabled;
}

void FrogPilotThemesPanel::updateToggles() {
  QSet<QString> visibleParents;

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      continue;
    }

    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels.value(key).toDouble();

    if (key == "DistanceIconPack") {
      setVisible &= parent->hasOpenpilotLongitudinal && params.getBool("CustomUI") && params.getBool("OnroadDistanceButton");
    }

    else if (key == "RandomThemes") {
      setVisible &= params.getBool("CustomThemes");
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (customThemeKeys.contains(key)) {
        visibleParents.insert("CustomThemes");
      }
    }
  }

  for (const QString &key : parentKeys) {
    toggles[key]->setVisible(visibleParents.contains(key));
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
