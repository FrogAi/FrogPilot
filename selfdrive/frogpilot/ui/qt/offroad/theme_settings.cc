#include "selfdrive/frogpilot/ui/qt/offroad/theme_settings.h"

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

  QString themeName = themeToDelete.toLower().replace(" (", "-").replace(")", "").replace(' ', '-');
  if (useFiles) {
    for (const QString &file : directory.entryList(QDir::Files)) {
      QString fileName = QFileInfo(file).baseName().toLower().replace('_', '-');
      if (fileName == themeName) {
        QFile::remove(directory.filePath(file));
        break;
      }
    }
  } else {
    QDir targetDir(directory.filePath(QDir(themeName).filePath(subFolder)));
    if (targetDir.exists()) {
      targetDir.removeRecursively();
    }
  }

  updateAssetParam(assetParam, params, themeToDelete, true);
}

void downloadThemeAsset(const QString &input, const std::string &paramKey, const QString &assetParam, Params &params, Params &params_memory) {
  QString output = input.toLower().remove('(').remove(')');
  output.replace(' ', input.contains('(') ? '-' : '_');
  params_memory.put(paramKey, output.toStdString());

  updateAssetParam(assetParam, params, input, false);
}

QStringList getThemeList(const QDir &themePacksDirectory, const QString &subFolder, const QString &assetParam, Params &params) {
  bool useFiles = subFolder.isEmpty();

  QString currentAsset = QString::fromStdString(params.get(assetParam.toStdString()));
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

    QChar delimiter = entry.baseName().contains('-') ? '-' : '_';
    QStringList parts = entry.baseName().split(delimiter, QString::SkipEmptyParts);
    for (QString &part : parts) {
      part[0] = part[0].toUpper();
    }

    themeList.append(parts.size() <= 1 || useFiles ? parts.join(' ') : QString("%1 (%2)").arg(parts[0], parts.mid(1).join(' ')));
  }

  return themeList;
}

QString getThemeName(const std::string &paramKey, Params &params) {
  QString value = QString::fromStdString(params.get(paramKey));
  QChar delimiter = value.contains('-') ? '-' : '_';

  QStringList parts = value.split(delimiter, QString::SkipEmptyParts);
  for (QString &part : parts) {
    part[0] = part[0].toUpper();
  }

  if (value.contains('-') && parts.size() > 1) {
    return QString("%1 (%2)").arg(parts[0], parts.mid(1).join(' '));
  }
  return parts.join(' ');
}

QString storeThemeName(const QString &input, const std::string &paramKey, Params &params) {
  QString output = input.toLower().remove('(').remove(')');
  output.replace(' ', input.contains('(') ? '-' : '_');
  params.put(paramKey, output.toStdString());
  return getThemeName(paramKey, params);
}

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

    {"RainbowPath", tr("Rainbow Path"), tr("Swap out the path in the onroad UI for a Mario Kart inspired 'Rainbow Path'."), "../frogpilot/assets/toggle_icons/icon_rainbow.png"},

    {"RandomEvents", tr("Random Events"), tr("Enables random cosmetic events that happen during certain driving conditions. These events are purely for fun and don't affect driving controls!"), "../frogpilot/assets/toggle_icons/icon_random.png"},

    {"StartupAlert", tr("Startup Alert"), tr("Controls the text of the 'Startup' alert message that appears when you start the drive."), "../frogpilot/assets/toggle_icons/icon_message.png"}
  };

  for (const auto &[param, title, desc, icon] : themeToggles) {
    AbstractControl *themeToggle;

    if (param == "PersonalizeOpenpilot") {
      FrogPilotManageControl *personalizeOpenpilotToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(personalizeOpenpilotToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        showToggles(customThemeKeys);
      });
      themeToggle = personalizeOpenpilotToggle;
    } else if (param == "CustomColors") {
      manageCustomColorsBtn = new FrogPilotButtonsControl(title, desc, "", {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageCustomColorsBtn, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList colorSchemes = getThemeList(QDir(themePacksDirectory.path()), "colors", "CustomColors", params);

        if (id == 0) {
          QString colorSchemeToDelete = MultiOptionDialog::getSelection(tr("Select a color scheme to delete"), colorSchemes, "", this);
          if (!colorSchemeToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' color scheme?").arg(colorSchemeToDelete), tr("Delete"), this)) {
            colorsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "colors", "DownloadableColors", colorSchemeToDelete, params);
          }
        } else if (id == 1) {
          if (colorDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);

            updateAssetParam("DownloadableColors", params, colorSchemeToDownload, true);

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
          colorSchemes.sort();
          QString colorSchemeToSelect = MultiOptionDialog::getSelection(tr("Select a color scheme"), colorSchemes, getThemeName("CustomColors", params), this);
          if (!colorSchemeToSelect.isEmpty()) {
            manageCustomColorsBtn->setValue(storeThemeName(colorSchemeToSelect, "CustomColors", params));
          }
        }
      });
      manageCustomColorsBtn->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageCustomColorsBtn;
    } else if (param == "CustomDistanceIcons") {
      manageDistanceIconsBtn = new FrogPilotButtonsControl(title, desc, "", {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageDistanceIconsBtn, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList distanceIconPacks = getThemeList(QDir(themePacksDirectory.path()), "distance_icons", "CustomDistanceIcons", params);

        if (id == 0) {
          QString distanceIconPackToDelete = MultiOptionDialog::getSelection(tr("Select a distance icon pack to delete"), distanceIconPacks, "", this);
          if (!distanceIconPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' distance icon pack?").arg(distanceIconPackToDelete), tr("Delete"), this)) {
            distanceIconsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "distance_icons", "DownloadableDistanceIcons", distanceIconPackToDelete, params);
          }
        } else if (id == 1) {
          if (distanceIconDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);

            updateAssetParam("DownloadableDistanceIcons", params, distanceIconPackToDownload, true);

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
          distanceIconPacks.sort();
          QString distanceIconPackToSelect = MultiOptionDialog::getSelection(tr("Select a distance icon pack"), distanceIconPacks, getThemeName("CustomDistanceIcons", params), this);
          if (!distanceIconPackToSelect.isEmpty()) {
            manageDistanceIconsBtn->setValue(storeThemeName(distanceIconPackToSelect, "CustomDistanceIcons", params));
          }
        }
      });
      manageDistanceIconsBtn->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageDistanceIconsBtn;
    } else if (param == "CustomIcons") {
      manageCustomIconsBtn = new FrogPilotButtonsControl(title, desc, "", {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageCustomIconsBtn, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList iconPacks = getThemeList(QDir(themePacksDirectory.path()), "icons", "CustomIcons", params);

        if (id == 0) {
          QString iconPackToDelete = MultiOptionDialog::getSelection(tr("Select an icon pack to delete"), iconPacks, "", this);
          if (!iconPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' icon pack?").arg(iconPackToDelete), tr("Delete"), this)) {
            iconsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "icons", "DownloadableIcons", iconPackToDelete, params);
          }
        } else if (id == 1) {
          if (iconDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);

            updateAssetParam("DownloadableIcons", params, colorSchemeToDownload, true);

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
          iconPacks.sort();
          QString iconPackToSelect = MultiOptionDialog::getSelection(tr("Select an icon pack"), iconPacks, getThemeName("CustomIcons", params), this);
          if (!iconPackToSelect.isEmpty()) {
            manageCustomIconsBtn->setValue(storeThemeName(iconPackToSelect, "CustomIcons", params));
          }
        }
      });
      manageCustomIconsBtn->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageCustomIconsBtn;
    } else if (param == "CustomSignals") {
      manageCustomSignalsBtn = new FrogPilotButtonsControl(title, desc, "", {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageCustomSignalsBtn, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList signalAnimations = getThemeList(QDir(themePacksDirectory.path()), "signals", "CustomSignals", params);

        if (id == 0) {
          QString signalAnimationToDelete = MultiOptionDialog::getSelection(tr("Select a signal animation to delete"), signalAnimations, "", this);
          if (!signalAnimationToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' signal animation?").arg(signalAnimationToDelete), tr("Delete"), this)) {
            signalsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "signals", "DownloadableSignals", signalAnimationToDelete, params);
          }
        } else if (id == 1) {
          if (signalDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);

            updateAssetParam("DownloadableSignals", params, colorSchemeToDownload, true);

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
          signalAnimations.sort();
          QString signalAnimationToSelect = MultiOptionDialog::getSelection(tr("Select a signal animation"), signalAnimations, getThemeName("CustomSignals", params), this);
          if (!signalAnimationToSelect.isEmpty()) {
            manageCustomSignalsBtn->setValue(storeThemeName(signalAnimationToSelect, "CustomSignals", params));
          }
        }
      });
      manageCustomSignalsBtn->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageCustomSignalsBtn;
    } else if (param == "CustomSounds") {
      manageCustomSoundsBtn = new FrogPilotButtonsControl(title, desc, "", {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageCustomSoundsBtn, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList soundPacks = getThemeList(QDir(themePacksDirectory.path()), "sounds", "CustomSounds", params);

        if (id == 0) {
          QString soundPackToDelete = MultiOptionDialog::getSelection(tr("Select a sound pack to delete"), soundPacks, "", this);
          if (!soundPackToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' sound pack?").arg(soundPackToDelete), tr("Delete"), this)) {
            soundsDownloaded = false;

            deleteThemeAsset(themePacksDirectory, "sounds", "DownloadableSounds", soundPackToDelete, params);
          }
        } else if (id == 1) {
          if (soundDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);

            updateAssetParam("DownloadableSounds", params, colorSchemeToDownload, true);

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
          soundPacks.sort();
          QString soundPackToSelect = MultiOptionDialog::getSelection(tr("Select a sound pack"), soundPacks, getThemeName("CustomSounds", params), this);
          if (!soundPackToSelect.isEmpty()) {
            manageCustomSoundsBtn->setValue(storeThemeName(soundPackToSelect, "CustomSounds", params));
          }
        }
      });
      manageCustomSoundsBtn->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageCustomSoundsBtn;
    } else if (param == "WheelIcon") {
      manageWheelIconsBtn = new FrogPilotButtonsControl(title, desc, "", {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
      QObject::connect(manageWheelIconsBtn, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
        QStringList wheelIcons = getThemeList(QDir(wheelsDirectory.path()), "", "WheelIcon", params);

        if (id == 0) {
          QString wheelIconToDelete = MultiOptionDialog::getSelection(tr("Select a steering wheel to delete"), wheelIcons, "", this);
          if (!wheelIconToDelete.isEmpty() && ConfirmationDialog::confirm(tr("Are you sure you want to delete the '%1' steering wheel?").arg(wheelIconToDelete), tr("Delete"), this)) {
            wheelsDownloaded = false;

            deleteThemeAsset(wheelsDirectory, "", "DownloadableWheels", wheelIconToDelete, params);
          }
        } else if (id == 1) {
          if (wheelDownloading) {
            cancellingDownload = true;

            params_memory.putBool("CancelThemeDownload", true);

            updateAssetParam("DownloadableWheels", params, colorSchemeToDownload, true);

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
          wheelIcons.sort();
          QString steeringWheelToSelect = MultiOptionDialog::getSelection(tr("Select a steering wheel"), wheelIcons, getThemeName("WheelIcon", params), this);
          if (!steeringWheelToSelect.isEmpty()) {
            manageWheelIconsBtn->setValue(storeThemeName(steeringWheelToSelect, "WheelIcon", params));
          }
        }
      });
      manageWheelIconsBtn->setValue(getThemeName(param.toStdString(), params));
      themeToggle = manageWheelIconsBtn;
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
          params.remove("StartupMessageTop");
          params.remove("StartupMessageBottom");

          startupAlertButton->clearCheckedButtons();
        }
      });
      themeToggle = startupAlertButton;

    } else {
      themeToggle = new ParamControl(param, title, desc, icon);
    }

    addItem(themeToggle);
    toggles[param] = themeToggle;

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(themeToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, this, &FrogPilotThemesPanel::openParentToggle);
    }
  }

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

  frogpilotToggleLevels = parent->frogpilotToggleLevels;
  tuningLevel = parent->tuningLevel;

  hideToggles();
}

void FrogPilotThemesPanel::updateState(const UIState &s) {
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
        params_memory.remove("ColorToDownload");
        params_memory.remove("DistanceIconToDownload");
        params_memory.remove("IconToDownload");
        params_memory.remove("SignalToDownload");
        params_memory.remove("SoundToDownload");
        params_memory.remove("ThemeDownloadProgress");
        params_memory.remove("WheelToDownload");

        downloadStatusLabel->setText("Idle");
      });
    }
  }

  bool parked = !s.scene.started || s.scene.parked || s.scene.frogs_go_moo;

  manageCustomColorsBtn->setText(1, colorDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageCustomColorsBtn->setEnabledButtons(0, !themeDownloading);
  manageCustomColorsBtn->setEnabledButtons(1, s.scene.online && (!themeDownloading || colorDownloading) && !cancellingDownload && !colorsDownloaded && parked);
  manageCustomColorsBtn->setEnabledButtons(2, !themeDownloading);

  manageCustomIconsBtn->setText(1, iconDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageCustomIconsBtn->setEnabledButtons(0, !themeDownloading);
  manageCustomIconsBtn->setEnabledButtons(1, s.scene.online && (!themeDownloading || iconDownloading) && !cancellingDownload && !iconsDownloaded && parked);
  manageCustomIconsBtn->setEnabledButtons(2, !themeDownloading);

  manageCustomSignalsBtn->setText(1, signalDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageCustomSignalsBtn->setEnabledButtons(0, !themeDownloading);
  manageCustomSignalsBtn->setEnabledButtons(1, s.scene.online && (!themeDownloading || signalDownloading) && !cancellingDownload && !signalsDownloaded && parked);
  manageCustomSignalsBtn->setEnabledButtons(2, !themeDownloading);

  manageCustomSoundsBtn->setText(1, soundDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageCustomSoundsBtn->setEnabledButtons(0, !themeDownloading);
  manageCustomSoundsBtn->setEnabledButtons(1, s.scene.online && (!themeDownloading || soundDownloading) && !cancellingDownload && !soundsDownloaded && parked);
  manageCustomSoundsBtn->setEnabledButtons(2, !themeDownloading);

  manageDistanceIconsBtn->setText(1, distanceIconDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageDistanceIconsBtn->setEnabledButtons(0, !themeDownloading);
  manageDistanceIconsBtn->setEnabledButtons(1, s.scene.online && (!themeDownloading || distanceIconDownloading) && !cancellingDownload && !distanceIconsDownloaded && parked);
  manageDistanceIconsBtn->setEnabledButtons(2, !themeDownloading);

  manageWheelIconsBtn->setText(1, wheelDownloading ? tr("CANCEL") : tr("DOWNLOAD"));
  manageWheelIconsBtn->setEnabledButtons(0, !themeDownloading);
  manageWheelIconsBtn->setEnabledButtons(1, s.scene.online && (!themeDownloading || wheelDownloading) && !cancellingDownload && !wheelsDownloaded && parked);
  manageWheelIconsBtn->setEnabledButtons(2, !themeDownloading);

  parent->keepScreenOn = themeDownloading;
}

void FrogPilotThemesPanel::showToggles(const std::set<QString> &keys) {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(keys.find(key) != keys.end() && tuningLevel >= frogpilotToggleLevels[key].toDouble());
  }

  setUpdatesEnabled(true);

  update();
}

void FrogPilotThemesPanel::hideToggles() {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    bool subToggles = customThemeKeys.find(key) != customThemeKeys.end();

    toggle->setVisible(!subToggles && tuningLevel >= frogpilotToggleLevels[key].toDouble());
  }

  setUpdatesEnabled(true);

  update();
}
