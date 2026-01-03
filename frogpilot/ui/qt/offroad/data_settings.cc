#include "frogpilot/ui/qt/offroad/data_settings.h"
#include "frogpilot/ui/qt/offroad/utilities.h"

namespace {
  QString friendlyDate(const QDate &date, const QString &detail) {
    int day = date.day();
    QString suffix = (day >= 11 && day <= 13) ? QCoreApplication::translate("FrogPilotDataPanel", "th", "ordinal suffix") :
                     (day % 10 == 1) ? QCoreApplication::translate("FrogPilotDataPanel", "st", "ordinal suffix") :
                     (day % 10 == 2) ? QCoreApplication::translate("FrogPilotDataPanel", "nd", "ordinal suffix") :
                     (day % 10 == 3) ? QCoreApplication::translate("FrogPilotDataPanel", "rd", "ordinal suffix") : QCoreApplication::translate("FrogPilotDataPanel", "th", "ordinal suffix");

    return QCoreApplication::translate("FrogPilotDataPanel", "%1 %2%3, %4 (%5)")
      .arg(date.toString("MMMM"))
      .arg(day)
      .arg(suffix)
      .arg(date.year())
      .arg(detail);
  }

  bool hasRestoreSpace(const QString &sourcePath) {
    QStorageInfo storage("/data");
    qint64 sourceSize = QFileInfo(sourcePath).size();
    return storage.isValid() && storage.isReady() && sourceSize > 0 && storage.bytesAvailable() / 4 > sourceSize;
  }

  void runDataOperation(FrogPilotSettingsWindow *parent, FrogPilotButtonsControl *button, std::vector<int> hiddenButtons,
                        QString busyText, QString successText, QString failureText, std::function<bool()> operation) {
    std::thread([=]() {
      runOnUIThread(button, [=]() {
        parent->activeOperations++;

        button->setEnabled(false);
        button->setValue(busyText);

        for (int id : hiddenButtons) {
          button->setVisibleButton(id, false);
        }
      });

      bool success = operation();

      runOnUIThread(button, [=]() {
        button->setValue(success ? successText : failureText);
      });

      util::sleep_for(2500);

      runOnUIThread(button, [=]() {
        button->setEnabled(true);
        button->setValue("");

        for (int id : hiddenButtons) {
          button->setVisibleButton(id, true);
        }

        parent->activeOperations--;
      });
    }).detach();
  }

  bool validName(const QString &name) {
    static const QRegularExpression validCharacters("^[A-Za-z0-9._-]+$");
    return validCharacters.match(name).hasMatch() && !name.contains("..") && !name.startsWith("-");
  }

  bool validBackupName(const QString &name) {
    return validName(name) && !name.contains("_in_progress") && !name.contains("_auto");
  }

  const std::set<std::string> backedUpKeys = {"DiscordUsername", "MapboxPublicKey", "MapboxSecretKey", "MapdSettings", "SecOCKeys", "Timezone", "WeatherToken"};

  QStringList protectedParamArgs() {
    QStringList arguments = {"--exclude", "Offroad_*"};
    for (const std::string &key : excluded_keys) {
      if (!backedUpKeys.count(key)) {
        arguments << "--exclude" << QString::fromStdString(key);
      }
    }
    return arguments;
  }
}

FrogPilotDataPanel::FrogPilotDataPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent) {
  QStackedLayout *dataLayout = new QStackedLayout();
  addItem(dataLayout);

  FrogPilotListWidget *dataMainList = new FrogPilotListWidget(this);
  ScrollView *dataMainPanel = new ScrollView(dataMainList, this);
  dataLayout->addWidget(dataMainPanel);

  FrogPilotListWidget *statsLabelsList = new FrogPilotListWidget(this);
  ScrollView *statsLabelsPanel = new ScrollView(statsLabelsList, this);
  dataLayout->addWidget(statsLabelsPanel);

  FrogPilotButtonsControl *frogpilotBackupButton = new FrogPilotButtonsControl(tr("FrogPilot Backups"), tr("<b>Back up the FrogPilot software, restore a backup to go back to that version, or delete ones you no longer need.</b><br><br>Restoring reboots the device on its own and puts the software back exactly as it was when the backup was made, without changing your settings. Automatic updates turn off after a restore until you update manually. \"DELETE ALL\" also removes the backups FrogPilot makes automatically."), "", {tr("BACKUP"), tr("DELETE"), tr("DELETE ALL"), tr("RESTORE")});
  QObject::connect(frogpilotBackupButton, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir backupDir("/data/backups");

    QFileInfoList backupList = backupDir.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
    std::sort(backupList.begin(), backupList.end(), [](const QFileInfo &a, const QFileInfo &b) {
      return a.lastModified() > b.lastModified();
    });

    QStringList friendlyNames;
    QMap<QString, QString> backupMap;

    for (const QFileInfo &fileInfo : backupList) {
      QString fileName = fileInfo.fileName();

      if (fileName.contains("_in_progress")) {
        continue;
      }

      QString friendlyName = fileName;

      if (fileName.endsWith("_auto.tar.zst")) {
        QStringList parts = QString(fileName).remove(".tar.zst").split("_");

        if (parts.size() >= 3) {
          QDate date = fileInfo.lastModified().date();

          if (date.isValid()) {
            friendlyName = friendlyDate(date, parts[1]);
          }
        }
      }

      if (friendlyName == fileName) {
        if (friendlyName.endsWith(".tar.zst")) {
          friendlyName.chop(8);
        }
        friendlyName.replace("_", " ");
      }

      friendlyNames.append(friendlyName);
      backupMap[friendlyName] = fileName;
    }

    if (id == 0) {
      QString name = InputDialog::getText(tr("Name your backup"), this, tr("Backup Name")).trimmed().replace(" ", "_");
      if (!name.isEmpty()) {
        if (!validBackupName(name)) {
          ConfirmationDialog::alert(tr("That name can't be used. Names can only use letters, numbers, dashes, periods, and underscores, and \"_auto\" and \"_in_progress\" are reserved."), this);
          return;
        }

        if (QFileInfo(backupDir.absoluteFilePath(name + ".tar.zst")).isFile()) {
          ConfirmationDialog::alert(tr("Name already in use. Please choose a different name."), this);
          return;
        }

        runDataOperation(parent, frogpilotBackupButton, {1, 2, 3}, tr("Backing up..."), tr("Backup created!"), tr("Backup failed..."), [=]() {
          QString inProgressPath = backupDir.absoluteFilePath(name + "_in_progress.tar.zst");
          QString finalPath = backupDir.absoluteFilePath(name + ".tar.zst");

          QFile::remove(inProgressPath);
          int tarStatus = QProcess::execute("tar", {"--use-compress-program=zstd", "-cf", inProgressPath, "/data/openpilot"});

          bool success = tarStatus == 0 && QFileInfo(inProgressPath).size() > 0;
          if (success) {
            success = QFile::rename(inProgressPath, finalPath);
          }
          if (!success) {
            QFile::remove(inProgressPath);
          }
          return success;
        });
      }

    } else if (id == 1) {
      QStringList selections = FrogPilotMultiOptionDialog::getSelections(tr("Choose backups to delete"), friendlyNames, tr("Delete"), this);
      if (!selections.isEmpty()) {
        QString confirmation;
        if (selections.size() == 1) {
          confirmation = tr("Delete this backup?");
        } else {
          confirmation = tr("Delete the %1 selected backups?").arg(selections.size());
        }

        if (ConfirmationDialog::confirm(confirmation, tr("Delete"), this)) {
          runDataOperation(parent, frogpilotBackupButton, {0, 2, 3}, tr("Deleting..."), tr("Deleted!"), tr("Delete failed..."), [=]() {
            bool success = true;
            for (const QString &selection : selections) {
              success &= QFile::remove(backupDir.absoluteFilePath(backupMap[selection]));
            }
            return success;
          });
        }
      }

    } else if (id == 2) {
      if (ConfirmationDialog::confirm(tr("Delete all backups? This includes the backups FrogPilot makes automatically."), tr("Delete All"), this)) {
        runDataOperation(parent, frogpilotBackupButton, {0, 1, 3}, tr("Deleting..."), tr("Deleted!"), tr("Delete failed..."), [=]() mutable {
          bool success = backupDir.removeRecursively();
          backupDir.mkpath(".");
          return success;
        });
      }

    } else if (id == 3) {
      if (uiState()->scene.started) {
        ConfirmationDialog::alert(tr("Backups can't be restored while the car is on. Turn the car off and try again."), this);
        return;
      }

      QString selection = MultiOptionDialog::getSelection(tr("Choose a backup to restore"), friendlyNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Restore this backup? The device will reboot on its own once the restore finishes."), tr("Restore"), this)) {
          std::thread([=]() {
            runOnUIThread(frogpilotBackupButton, [=]() {
              parent->activeOperations++;

              frogpilotBackupButton->setEnabled(false);
              frogpilotBackupButton->setValue(tr("Restoring..."));

              frogpilotBackupButton->setVisibleButton(0, false);
              frogpilotBackupButton->setVisibleButton(1, false);
              frogpilotBackupButton->setVisibleButton(2, false);
            });

            QString archivePath = backupDir.absoluteFilePath(backupMap[selection]);
            QString extractDirectory = "/data/restore_temp";

            QDir(extractDirectory).removeRecursively();
            QDir().mkpath(extractDirectory);

            bool success = hasRestoreSpace(archivePath);

            if (success) {
              success = QProcess::execute("tar", {"--use-compress-program=zstd", "-xf", archivePath, "-C", extractDirectory}) == 0;
            }

            QString sourceRoot;
            if (success) {
              QDir extracted(extractDirectory);

              QStringList candidates{extracted.absoluteFilePath("data/openpilot")};
              for (const QString &entry : extracted.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                candidates << extracted.absoluteFilePath(entry);
              }
              candidates << extractDirectory;

              for (const QString &candidate : candidates) {
                if (QFileInfo(candidate + "/launch_openpilot.sh").isFile() && QFileInfo(candidate + "/launch_chffrplus.sh").isFile() &&
                    QDir(candidate + "/selfdrive").exists() && QDir(candidate + "/system").exists()) {
                  sourceRoot = candidate;
                  break;
                }
              }
              success = !sourceRoot.isEmpty();
            }

            if (success) {
              success = QProcess::execute("rsync", {"-a", "--delete", "-l", sourceRoot + "/", "/data/openpilot/"}) == 0;
            }

            if (success) {
              QDir(extractDirectory).removeRecursively();

              QFile("/cache/on_backup").open(QIODevice::WriteOnly);

              runOnUIThread(frogpilotBackupButton, [=]() {
                frogpilotBackupButton->setValue(tr("Restored!"));
              });

              util::sleep_for(2500);

              runOnUIThread(frogpilotBackupButton, [=]() {
                frogpilotBackupButton->setValue(tr("Rebooting..."));
              });

              util::sleep_for(2500);

              Hardware::reboot();
            } else {
              QDir(extractDirectory).removeRecursively();

              runOnUIThread(frogpilotBackupButton, [=]() {
                frogpilotBackupButton->setValue(tr("Restore failed..."));
              });

              util::sleep_for(2500);

              runOnUIThread(frogpilotBackupButton, [=]() {
                frogpilotBackupButton->setEnabled(true);
                frogpilotBackupButton->setValue("");

                frogpilotBackupButton->setVisibleButton(0, true);
                frogpilotBackupButton->setVisibleButton(1, true);
                frogpilotBackupButton->setVisibleButton(2, true);

                parent->activeOperations--;
              });
            }
          }).detach();
        }
      }
    }
  });
  dataMainList->addItem(frogpilotBackupButton);

  FrogPilotButtonsControl *toggleBackupButton = new FrogPilotButtonsControl(tr("Settings Backups"), tr("<b>Save a copy of your current settings, restore a saved copy, or delete ones you no longer need.</b><br><br>Restoring applies most settings right away, but a few, such as \"High-Quality Recording\" and \"Use Konik Server\", need a reboot. FrogPilot also saves a copy automatically whenever you change a setting, but it only keeps the newest few and deletes the older ones. Those show up in the list by date and time."), "", {tr("BACKUP"), tr("DELETE"), tr("DELETE ALL"), tr("RESTORE")});
  QObject::connect(toggleBackupButton, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir backupDir("/data/toggle_backups");

    QStringList backupNames = backupDir.entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    std::sort(backupNames.begin(), backupNames.end(), [](const QString &a, const QString &b) {
      const bool aIsAutomatic = a.endsWith("_auto");
      const bool bIsAutomatic = b.endsWith("_auto");
      if (aIsAutomatic != bIsAutomatic) {
        return bIsAutomatic;
      }
      if (aIsAutomatic) {
        return a > b;
      }
      return QString(a).replace("_", " ").compare(QString(b).replace("_", " "), Qt::CaseInsensitive) < 0;
    });

    QStringList friendlyNames;
    QMap<QString, QString> backupMap;
    for (const QString &dirName : backupNames) {
      if (dirName.contains("_in_progress")) {
        continue;
      }

      QString friendlyName = dirName;

      if (dirName.endsWith("_auto")) {
        QStringList parts = QString(dirName).remove("_auto").split("_");

        if (parts.size() >= 2) {
          QDate date = QDate::fromString(parts[0], "yyyy-MM-dd");
          QTime time = QTime::fromString(parts[1], "HH-mm-ss");

          if (date.isValid() && time.isValid()) {
            friendlyName = friendlyDate(date, formatShortTime(time));
          }
        }
      }

      if (friendlyName == dirName) {
        friendlyName.replace("_", " ");
      }

      const QString baseName = friendlyName;
      int duplicate = 2;

      while (backupMap.contains(friendlyName)) {
        friendlyName = QString("%1 (%2)").arg(baseName).arg(duplicate);
        duplicate++;
      }

      friendlyNames.append(friendlyName);
      backupMap[friendlyName] = dirName;
    }

    if (id == 0) {
      QString name = InputDialog::getText(tr("Name your backup"), this, tr("Backup Name")).trimmed().replace(" ", "_");
      if (!name.isEmpty()) {
        if (!validBackupName(name)) {
          ConfirmationDialog::alert(tr("That name can't be used. Names can only use letters, numbers, dashes, periods, and underscores, and \"_auto\" and \"_in_progress\" are reserved."), this);
          return;
        }

        if (backupNames.contains(name)) {
          ConfirmationDialog::alert(tr("Name already in use. Please choose a different name."), this);
          return;
        }

        runDataOperation(parent, toggleBackupButton, {1, 2, 3}, tr("Backing up..."), tr("Backup created!"), tr("Backup failed..."), [=]() {
          QString inProgressPath = backupDir.absoluteFilePath(name + "_in_progress");
          QString finalPath = backupDir.absoluteFilePath(name);

          bool success = QProcess::execute("rsync", QStringList{"-a", "/data/params/d/", inProgressPath + "/"} + protectedParamArgs()) == 0;
          if (success) {
            success = QDir().rename(inProgressPath, finalPath);
          }
          if (!success) {
            QDir(inProgressPath).removeRecursively();
          }
          return success;
        });
      }

    } else if (id == 1) {
      QStringList selections = FrogPilotMultiOptionDialog::getSelections(tr("Choose backups to delete"), friendlyNames, tr("Delete"), this);
      if (!selections.isEmpty()) {
        QString confirmation;
        if (selections.size() == 1) {
          confirmation = tr("Delete this backup?");
        } else {
          confirmation = tr("Delete the %1 selected backups?").arg(selections.size());
        }

        if (ConfirmationDialog::confirm(confirmation, tr("Delete"), this)) {
          runDataOperation(parent, toggleBackupButton, {0, 2, 3}, tr("Deleting..."), tr("Deleted!"), tr("Delete failed..."), [=]() {
            bool success = true;
            for (const QString &selection : selections) {
              success &= QDir(backupDir.absoluteFilePath(backupMap[selection])).removeRecursively();
            }
            return success;
          });
        }
      }

    } else if (id == 2) {
      if (ConfirmationDialog::confirm(tr("Delete all settings backups? This includes the copies FrogPilot saves automatically."), tr("Delete All"), this)) {
        runDataOperation(parent, toggleBackupButton, {0, 1, 3}, tr("Deleting..."), tr("Deleted!"), tr("Delete failed..."), [=]() mutable {
          bool success = backupDir.removeRecursively();
          backupDir.mkpath(".");
          return success;
        });
      }

    } else if (id == 3) {
      QString selection = MultiOptionDialog::getSelection(tr("Choose a backup to restore"), friendlyNames, "", this);
      if (!selection.isEmpty()) {
        if (FrogPilotConfirmationDialog::yesorno(tr("Restore this backup? This overwrites your current settings."), this)) {
          std::thread([=]() {
            runOnUIThread(toggleBackupButton, [=]() {
              parent->activeOperations++;

              toggleBackupButton->setEnabled(false);
              toggleBackupButton->setValue(tr("Restoring..."));

              toggleBackupButton->setVisibleButton(0, false);
              toggleBackupButton->setVisibleButton(1, false);
              toggleBackupButton->setVisibleButton(2, false);
            });

            bool success = QProcess::execute("rsync", QStringList{"-a", "-l", backupDir.absoluteFilePath(backupMap[selection]) + "/", "/data/params/d/"} + protectedParamArgs()) == 0;
            if (success) {
              QDir restoredBackup(backupDir.absoluteFilePath(backupMap[selection]));
              for (const QFileInfo &file : restoredBackup.entryInfoList(QDir::Files)) {
                std::string key = file.fileName().toStdString();
                if (file.size() == 0 && (!excluded_keys.count(key) || backedUpKeys.count(key))) {
                  params.remove(key);
                }
              }
            }

            runOnUIThread(toggleBackupButton, [=]() {
              if (success) {
                frogpilotUIState()->updateToggles();
                parent->updateMetric(params.getBool("IsMetric"), true);
                parent->updateTuningLevel();
              }

              toggleBackupButton->setValue(success ? tr("Restored!") : tr("Restore failed..."));
            });

            util::sleep_for(2500);

            runOnUIThread(toggleBackupButton, [=]() {
              toggleBackupButton->setEnabled(true);
              toggleBackupButton->setValue("");

              toggleBackupButton->setVisibleButton(0, true);
              toggleBackupButton->setVisibleButton(1, true);
              toggleBackupButton->setVisibleButton(2, true);

              parent->activeOperations--;
            });
          }).detach();
        }
      }
    }
  });
  dataMainList->addItem(toggleBackupButton);

  FrogPilotButtonsControl *viewStatsButton = new FrogPilotButtonsControl(tr("FrogPilot Stats"), tr("<b>See everything FrogPilot has tracked about your driving, or reset the numbers and start over.</b><br><br>Stats can only be reset while the car is off."), "", {tr("RESET"), tr("VIEW")});
  QObject::connect(viewStatsButton, &FrogPilotButtonsControl::buttonClicked, [dataLayout, statsLabelsList, statsLabelsPanel, this](int id) {
    if (id == 0) {
      if (uiState()->scene.started) {
        ConfirmationDialog::alert(tr("Stats can't be reset while the car is on. Turn the car off and try again."), this);
        return;
      }

      if (ConfirmationDialog::confirm(tr("Are you sure you want to reset all of your FrogPilot stats?"), tr("Reset"), this)) {
        params.remove("FrogPilotStats");
      }
    } else if (id == 1) {
      updateStatsLabels(statsLabelsList);

      emit openSubPanel();
      dataLayout->setCurrentWidget(statsLabelsPanel);
    }
  });
  dataMainList->addItem(viewStatsButton);

  if (forceOpen) {
    for (AbstractControl *control : std::initializer_list<AbstractControl*>{deleteDrivingDataButton, deleteErrorLogsButton, screenRecordingsButton, frogpilotBackupButton, toggleBackupButton, viewStatsButton}) {
      control->showDescription();
    }
  }

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [dataLayout, dataMainPanel] {
    dataLayout->setCurrentWidget(dataMainPanel);
  });
}

void FrogPilotDataPanel::updateStatsLabels(FrogPilotListWidget *labelsList) {
  labelsList->clear();

  bool isMetric = params.getBool("IsMetric");
  QJsonObject stats = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("FrogPilotStats"))).object();

  static QMap<QString, QPair<QString, QString>> keyMap = {
    {"AEBEvents", {tr("Total Collision Alerts"), "count"}},
    {"AOLTime", {tr("Time Using \"Always On Lateral\""), "timePercent"}},
    {"CruiseSpeedTimes", {tr("Favorite Set Speed"), "speed"}},
    {"CurrentMonthsMeters", {tr("Distance Driven This Month"), "distance"}},
    {"DayTime", {tr("Time Driving (Daytime)"), "timePercent"}},
    {"Disengages", {tr("Total Disengagements"), "count"}},
    {"Engages", {tr("Total Engagements"), "count"}},
    {"ExperimentalModeTime", {tr("Time Using \"Experimental Mode\""), "timePercent"}},
    {"FrogChirps", {tr("Total Frog Chirps"), "count"}},
    {"FrogHops", {tr("Total Frog Hops"), "count"}},
    {"FrogPilotDrives", {tr("Total Drives"), "count"}},
    {"FrogPilotMeters", {tr("Total Distance Driven"), "distance"}},
    {"FrogPilotSeconds", {tr("Total Driving Time"), "time"}},
    {"FrogSqueaks", {tr("Total Frog Squeaks"), "count"}},
    {"GoatScreams", {tr("Total Goat Screams"), "count"}},
    {"LateralTime", {tr("Time openpilot Was Steering"), "timePercent"}},
    {"LongestDistanceWithoutOverride", {tr("Longest Distance Without an Override"), "distance"}},
    {"LongitudinalTime", {tr("Time openpilot Controlled the Speed"), "timePercent"}},
    {"MaxAcceleration", {tr("Highest openpilot Acceleration"), "accel"}},
    {"ModelTimes", {tr("Driving Models:"), "parent"}},
    {"NightTime", {tr("Time Driving (Nighttime)"), "timePercent"}},
    {"Overrides", {tr("Total Overrides"), "count"}},
    {"OverrideTime", {tr("Time Driving Manually"), "timePercent"}},
    {"PersonalityTimes", {tr("Driving Personalities:"), "parent"}},
    {"RandomEvents", {tr("Random Events:"), "parent"}},
    {"StandstillTime", {tr("Time Stopped"), "timePercent"}},
    {"StopLightTime", {tr("Time Spent at Stoplights"), "timePercent"}},
    {"WeatherTimes", {tr("Time Driven (Weather):"), "parent"}}
  };

  static QMap<QString, QString> randomEventsMap = {
    {"accel30", tr("UwUs")},
    {"accel35", tr("Loch Ness Encounters")},
    {"accel40", tr("Visits to 1955")},
    {"dejaVuCurve", tr("Deja Vu Moments")},
    {"firefoxSteerSaturated", tr("Internet Explorer Weeeeeeees")},
    {"goatSteerSaturated", tr("Goat Screams")},
    {"hal9000", tr("HAL 9000 Denials")},
    {"openpilotCrashedRandomEvent", tr("openpilot Crashes")},
    {"thisIsFineSteerSaturated", tr("This Is Fine Moments")},
    {"toBeContinued", tr("To Be Continued Moments")},
    {"vCruise69", tr("Noices")},
    {"yourFrogTriedToKillMe", tr("Attempted Frog Murders")},
    {"youveGotMail", tr("Total Mail Received")}
  };

  static QMap<QString, QString> countNames = {
    {"AEBEvents", tr("Collision Alerts")},
    {"Disengages", tr("Disengagements")},
    {"Engages", tr("Engagements")},
    {"FrogChirps", tr("Frog Chirps")},
    {"FrogHops", tr("Frog Hops")},
    {"FrogPilotDrives", tr("Drives")},
    {"FrogSqueaks", tr("Frog Squeaks")},
    {"GoatScreams", tr("Goat Screams")},
    {"Overrides", tr("Overrides")}
  };

  static QMap<QString, QString> personalityNames = {
    {"Aggressive", tr("Aggressive")},
    {"Relaxed", tr("Relaxed")},
    {"Standard", tr("Standard")}
  };

  static QMap<QString, QString> weatherNames = {
    {"clear", tr("Clear")},
    {"low_visibility", tr("Low Visibility")},
    {"rain", tr("Rain")},
    {"rain_storm", tr("Rain Storm")},
    {"snow", tr("Snow")}
  };

  QStringList keys = keyMap.keys();
  std::sort(keys.begin(), keys.end(), [&](const QString &a, const QString &b) {
    return keyMap.value(a).first.toLower() < keyMap.value(b).first.toLower();
  });

  std::function<QString(double)> format_number = [&](double number) {
    return QLocale().toString(number);
  };

  std::function<QString(double)> format_distance = [&](double meters) {
    int value;
    QString unit;
    if (isMetric) {
      value = qRound(meters / 1000.0);
      unit = (value == 1) ? tr(" kilometer") : tr(" kilometers");
    } else {
      value = qRound(meters * METER_TO_MILE);
      unit = (value == 1) ? tr(" mile") : tr(" miles");
    }
    return format_number(value) + unit;
  };

  std::function<QString(int)> format_time = [&](int seconds) {
    const int secondsInDay = 60 * 60 * 24;
    const int secondsInHour = 60 * 60;

    int days = seconds / secondsInDay;
    int hours = (seconds % secondsInDay) / secondsInHour;
    int minutes = (seconds % secondsInHour) / 60;

    QString result;
    if (days > 0) {
      result += format_number(days) + (days == 1 ? tr(" day ") : tr(" days "));
    }
    if (hours > 0 || days > 0) {
      result += format_number(hours) + (hours == 1 ? tr(" hour ") : tr(" hours "));
    }
    result += format_number(minutes) + (minutes == 1 ? tr(" minute") : tr(" minutes"));
    return result.trimmed();
  };

  double trackedTime = stats.value("TrackedTime").toDouble();

  for (const QString &key : keys) {
    QJsonValue value = stats.value(key);
    QString labelText = keyMap.value(key).first;
    QString type = keyMap.value(key).second;

    if ((key == "DayTime" || key == "NightTime") && value.toDouble() <= 0.0) {
      continue;
    }

    if (key == "CurrentMonthsMeters" && stats.value("Month").toInt() != QDateTime::currentDateTimeUtc().date().month()) {
      value = QJsonValue(0);
    }

    if (key == "AEBEvents") {
      QJsonObject totalEvents = stats.value("TotalEvents").toObject();

      QString displayValue = format_number(totalEvents.value("stockAeb").toInt(0) + totalEvents.value("fcw").toInt(0)) + " " + countNames.value(key);

      labelsList->addItem(new LabelControl(labelText, displayValue, "", this));
    } else if (key == "CruiseSpeedTimes" && value.isObject() && !value.toObject().isEmpty()) {
      QJsonObject speeds = value.toObject();

      double maxTime = -1.0;
      QString bestSpeed;
      for (const QString &speedKey : speeds.keys()) {
        double time = speeds.value(speedKey).toDouble();
        if (time > maxTime) {
          bestSpeed = speedKey;
          maxTime = time;
        }
      }

      QString displaySpeed;
      if (isMetric) {
        displaySpeed = QString::number(qRound(bestSpeed.toDouble() * MS_TO_KPH)) + " " + tr("km/h");
      } else {
        displaySpeed = QString::number(qRound(bestSpeed.toDouble() * MS_TO_MPH)) + " " + tr("mph");
      }

      labelsList->addItem(new LabelControl(labelText, displaySpeed + " (" + format_time(maxTime) + ")", "", this));
    } else if (type == "parent") {
      labelsList->addItem(new LabelControl(labelText, "", "", this));

      QJsonObject subObject = value.toObject();
      QStringList subKeys;

      if (key == "RandomEvents") {
        subKeys = randomEventsMap.keys();
      } else {
        subKeys = subObject.keys();
      }

      std::sort(subKeys.begin(), subKeys.end(), [&](const QString &a, const QString &b) {
        QString displayA, displayB;
        if (key == "RandomEvents") {
          displayA = randomEventsMap.value(a, a);
          displayB = randomEventsMap.value(b, b);
        } else {
          displayA = a;
          displayB = b;
        }
        return displayA.toLower() < displayB.toLower();
      });

      for (const QString &subkey : subKeys) {
        if (subkey.compare("unknown", Qt::CaseInsensitive) == 0) {
          continue;
        }

        QString displaySubKey;
        if (key == "ModelTimes") {
          displaySubKey = cleanModelName(subkey);
        } else if (key == "RandomEvents") {
          displaySubKey = randomEventsMap.value(subkey, subkey);
        } else if (key == "PersonalityTimes") {
          displaySubKey = personalityNames.value(subkey, subkey);
        } else if (key == "WeatherTimes") {
          displaySubKey = weatherNames.value(subkey, subkey);
        } else {
          displaySubKey = subkey;
        }

        QString subvalue;
        if (key.endsWith("Times")) {
          subvalue = format_time(subObject.value(subkey).toDouble());
        } else {
          subvalue = format_number(subObject.value(subkey).toInt(0));
        }

        labelsList->addItem(new LabelControl("     " + displaySubKey, subvalue, "", this));
      }
    } else {
      QString displayValue;
      if (type == "accel") {
        displayValue = QString::number(value.toDouble(), 'f', 2) + " " + tr("m/s²");
      } else if (type == "count") {
        displayValue = format_number(value.toInt()) + " " + countNames.value(key);
      } else if (type == "distance") {
        displayValue = format_distance(value.toDouble());
      } else if (type == "speed") {
        displayValue = "--";
      } else if (type == "time" || type == "timePercent") {
        displayValue = format_time(value.toDouble());
      }

      labelsList->addItem(new LabelControl(labelText, displayValue, "", this));

      if (type == "timePercent") {
        int percent = 0;
        if (trackedTime > 0.0) {
          percent = (value.toDouble() * 100.0) / trackedTime;
        }

        labelsList->addItem(new LabelControl(tr("% of %1").arg(labelText), format_number(percent) + "%", "", this));
      }
    }
  }
}
