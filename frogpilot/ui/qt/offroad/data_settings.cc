#include <cerrno>
#include <sys/file.h>
#include <sys/xattr.h>

#include <QDirIterator>
#include <QProcess>
#include <QSet>
#include <QStorageInfo>

#include "frogpilot/ui/qt/offroad/data_settings.h"
#include "frogpilot/ui/qt/widgets/account_popup.h"

namespace {
  QString friendlyDate(const QDate &date, const QString &detail) {
    int day = date.day();
    QString suffix = QCoreApplication::translate("FrogPilotDataPanel", "th", "ordinal suffix");
    if (day < 11 || day > 13) {
      if (day % 10 == 1) {
        suffix = QCoreApplication::translate("FrogPilotDataPanel", "st", "ordinal suffix");
      } else if (day % 10 == 2) {
        suffix = QCoreApplication::translate("FrogPilotDataPanel", "nd", "ordinal suffix");
      } else if (day % 10 == 3) {
        suffix = QCoreApplication::translate("FrogPilotDataPanel", "rd", "ordinal suffix");
      }
    }

    return QCoreApplication::translate("FrogPilotDataPanel", "%1 %2%3, %4 (%5)")
      .arg(date.toString("MMMM"))
      .arg(day)
      .arg(suffix)
      .arg(date.year())
      .arg(detail);
  }

  qint64 restoreSourceSize(const QFileInfo &source) {
    if (source.isFile()) {
      return source.size();
    }

    qint64 size = 0;
    QDirIterator iterator(source.absoluteFilePath(), QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
      iterator.next();
      size += iterator.fileInfo().size();
    }
    return size;
  }

  bool hasRestoreSpace(const QString &sourcePath) {
    QStorageInfo storage("/data");
    qint64 sourceSize = restoreSourceSize(QFileInfo(sourcePath));
    return storage.isValid() && storage.isReady() && sourceSize > 0 && storage.bytesAvailable() / 4 > sourceSize;
  }

  void removeRecordingCompanions(const QDir &directory, const QString &name) {
    const QString stem = name.left(name.lastIndexOf('.'));
    QFile::remove(directory.absoluteFilePath(stem + ".png"));
    QFile::remove(directory.absoluteFilePath(stem + ".gif"));
  }

  bool renameRecording(const QDir &directory, const QString &oldName, const QString &newName) {
    const QString oldStem = oldName.left(oldName.lastIndexOf('.'));
    const QString newStem = newName.left(newName.lastIndexOf('.'));
    const QStringList sources = {
      directory.absoluteFilePath(oldName), directory.absoluteFilePath(oldStem + ".png"), directory.absoluteFilePath(oldStem + ".gif")
    };
    const QStringList targets = {
      directory.absoluteFilePath(newName), directory.absoluteFilePath(newStem + ".png"), directory.absoluteFilePath(newStem + ".gif")
    };

    if (!QFile::exists(sources[0])) {
      return false;
    }

    for (const QString &target : targets) {
      if (QFile::exists(target)) {
        return false;
      }
    }

    for (int i = 0; i < sources.size(); ++i) {
      if (!QFile::exists(sources[i])) {
        continue;
      }

      if (!QFile::rename(sources[i], targets[i])) {
        for (int j = i - 1; j >= 0; --j) {
          QFile::rename(targets[j], sources[j]);
        }
        return false;
      }
    }
    return true;
  }

  void runDataOperation(FrogPilotSettingsWindow *parent, FrogPilotButtonsControl *button, std::vector<int> hiddenButtons,
                        QString busyText, QString successText, QString failureText, std::function<bool()> operation) {
    std::thread([=]() {
      runOnUIThread(button, [=]() {
        parent->keepScreenOn = true;

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

        parent->keepScreenOn = false;
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

  int runCommand(const QString &program, const QStringList &arguments) {
    return QProcess::execute(program, arguments);
  }

  bool runSettingsRequest(const std::string &key, const QString &path) {
    Params params_memory{"/dev/shm/params"};

    std::string request = path.toStdString();
    params_memory.put(key, request);

    int elapsed = 0;
    while (params_memory.get(key) == request && elapsed < 120000) {
      util::sleep_for(100);
      elapsed += 100;
    }

    bool success = params_memory.get(key).empty();
    params_memory.remove(key);
    return success;
  }
}

FrogPilotDataPanel::FrogPilotDataPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  QStackedLayout *dataLayout = new QStackedLayout();
  addItem(dataLayout);

  FrogPilotListWidget *dataMainList = new FrogPilotListWidget(this);
  ScrollView *dataMainPanel = new ScrollView(dataMainList, this);
  dataLayout->addWidget(dataMainPanel);

  FrogPilotListWidget *statsLabelsList = new FrogPilotListWidget(this);
  ScrollView *statsLabelsPanel = new ScrollView(statsLabelsList, this);
  dataLayout->addWidget(statsLabelsPanel);

  ButtonControl *deleteDrivingDataButton = new ButtonControl(tr("Delete Driving Data"), tr("DELETE"),
    tr("<b>Delete recorded drives to free up space and clear personal footage off the device.</b><br><br>"
       "Whole routes preserved in \"The Pond\" are kept. Existing event flags keep their marked segments."));
  QObject::connect(deleteDrivingDataButton, &ButtonControl::clicked, [=]() {
    QDir hdDataDir("/data/media/0/realdata_HD/");
    QDir konikDataDir("/data/media/0/realdata_konik/");
    QDir realDataDir("/data/media/0/realdata/");
    QDir speedLimitCapturesDir("/data/media/speed_limit_captures/");

    if (ConfirmationDialog::confirm(tr("Delete all driving footage and data? Flagged segments and preserved routes will be kept."), tr("Delete"), this)) {
      std::thread([=]() mutable {
        runOnUIThread(deleteDrivingDataButton, [=]() {
          parent->keepScreenOn = true;

          deleteDrivingDataButton->setEnabled(false);
          deleteDrivingDataButton->setValue(tr("Deleting..."));
        });

        QString result = tr("Delete failed...");
        int descriptor = ::open("/data/media/0/.route_mutation.lock", O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0664);
        if (descriptor >= 0) {
          if (::flock(descriptor, LOCK_EX | LOCK_NB) == 0) {
            bool success = true;
            QFileInfoList entries;
            QList<QDir> footageDirs = {hdDataDir, konikDataDir, realDataDir};
            for (const QDir &footageDir : footageDirs) {
              if (footageDir.exists()) {
                entries.append(footageDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot));
              }
            }

            QSet<QString> preservedRoutes;
            for (const QFileInfo &entry : entries) {
              char value = 0;
              if (getxattr(entry.absoluteFilePath().toUtf8().constData(), "user.preserve_route", &value, sizeof(value)) == 1 && value == '1') {
                preservedRoutes.insert(entry.fileName().section("--", 0, -2));
              }
            }
            for (const QFileInfo &entry : entries) {
              if (preservedRoutes.contains(entry.fileName().section("--", 0, -2))) {
                continue;
              }
              char value[10] = {0};
              if (!(getxattr(entry.absoluteFilePath().toUtf8().constData(), "user.preserve", value, sizeof(value)) > 0 && strcmp(value, "1") == 0)) {
                success &= QDir(entry.absoluteFilePath()).removeRecursively();
              }
            }
            success &= speedLimitCapturesDir.removeRecursively();
            if (success) {
              result = tr("Deleted!");
            }
          } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
            result = tr("Driving data busy...");
          }
          ::close(descriptor);
        }

        runOnUIThread(deleteDrivingDataButton, [=]() {
          deleteDrivingDataButton->setValue(result);
        });

        util::sleep_for(2500);

        runOnUIThread(deleteDrivingDataButton, [=]() {
          deleteDrivingDataButton->setEnabled(true);
          deleteDrivingDataButton->setValue("");

          parent->keepScreenOn = false;
        });
      }).detach();
    }
  });
  if (forceOpen) {
    deleteDrivingDataButton->showDescription();
  }
  dataMainList->addItem(deleteDrivingDataButton);

  ButtonControl *deleteErrorLogsButton = new ButtonControl(tr("Delete Error Logs"), tr("DELETE"), tr("<b>Delete openpilot's saved crash logs.</b><br><br>Bug reports sent after deleting won't include crash details until a new crash happens."));
  QObject::connect(deleteErrorLogsButton, &ButtonControl::clicked, [=]() {
    QDir errorLogsDir("/data/error_logs");

    if (ConfirmationDialog::confirm(tr("Delete all error logs?"), tr("Delete"), this)) {
      std::thread([=]() mutable {
        runOnUIThread(deleteErrorLogsButton, [=]() {
          parent->keepScreenOn = true;

          deleteErrorLogsButton->setEnabled(false);
          deleteErrorLogsButton->setValue(tr("Deleting..."));
        });

        bool success = errorLogsDir.removeRecursively();
        errorLogsDir.mkpath(".");

        runOnUIThread(deleteErrorLogsButton, [=]() {
          deleteErrorLogsButton->setValue(success ? tr("Deleted!") : tr("Delete failed..."));
        });

        util::sleep_for(2500);

        runOnUIThread(deleteErrorLogsButton, [=]() {
          deleteErrorLogsButton->setEnabled(true);
          deleteErrorLogsButton->setValue("");

          parent->keepScreenOn = false;
        });
      }).detach();
    }
  });
  if (forceOpen) {
    deleteErrorLogsButton->showDescription();
  }
  dataMainList->addItem(deleteErrorLogsButton);

  FrogPilotButtonsControl *screenRecordingsButton = new FrogPilotButtonsControl(tr("Screen Recordings"), tr("<b>Delete or rename your recordings of the driving screen.</b><br><br>Recordings are made with the \"Screen Recorder\" button on the driving screen. \"DELETE ALL\" removes every recording at once."), "", {tr("DELETE"), tr("DELETE ALL"), tr("RENAME")});
  QObject::connect(screenRecordingsButton, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir recordingsDir("/data/media/screen_recordings");
    QStringList recordingsNames = recordingsDir.entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
    std::sort(recordingsNames.begin(), recordingsNames.end(), std::greater<QString>());

    QStringList friendlyNames;
    QMap<QString, QString> recordingMap;

    for (const QString &name : recordingsNames) {
      if (!name.endsWith(".mp4", Qt::CaseInsensitive)) {
        continue;
      }

      QString friendlyName = name;
      QString cleanName = QString(name).remove(".mp4");

      QStringList parts = cleanName.split("_");

      if (parts.size() >= 2) {
        QDate date = QDate::fromString(parts[0], "yyyy-MM-dd");
        QTime time = QTime::fromString(parts[1], "HH-mm-ss");

        if (date.isValid() && time.isValid()) {
          friendlyName = friendlyDate(date, formatShortTime(time));
        }
      }

      if (friendlyName == name) {
        friendlyName = cleanName;
        friendlyName.replace("_", " ");
      }

      if (cleanName.contains("_replay_")) {
        friendlyName += tr(" (Replay)");
      }

      const QString baseName = friendlyName;
      int duplicate = 2;

      while (recordingMap.contains(friendlyName)) {
        friendlyName = QString("%1 (%2)").arg(baseName).arg(duplicate);
        duplicate++;
      }

      friendlyNames.append(friendlyName);
      recordingMap[friendlyName] = name;
    }

    if (id == 0) {
      QStringList selections = FrogPilotMultiOptionDialog::getSelections(tr("Choose screen recordings to delete"), friendlyNames, tr("Delete"), this);
      if (!selections.isEmpty()) {
        QString confirmation;
        if (selections.size() == 1) {
          confirmation = tr("Delete this screen recording?");
        } else {
          confirmation = tr("Delete the %1 selected screen recordings?").arg(selections.size());
        }

        if (ConfirmationDialog::confirm(confirmation, tr("Delete"), this)) {
          runDataOperation(parent, screenRecordingsButton, {1, 2}, tr("Deleting..."), tr("Deleted!"), tr("Delete failed..."), [=]() {
            bool success = true;
            for (const QString &selection : selections) {
              const QString recordingName = recordingMap[selection];
              if (QFile::remove(recordingsDir.absoluteFilePath(recordingName))) {
                removeRecordingCompanions(recordingsDir, recordingName);
              } else {
                success = false;
              }
            }
            return success;
          });
        }
      }

    } else if (id == 1) {
      if (ConfirmationDialog::confirm(tr("Delete all screen recordings?"), tr("Delete All"), this)) {
        runDataOperation(parent, screenRecordingsButton, {0, 2}, tr("Deleting..."), tr("Deleted!"), tr("Delete failed..."), [=]() {
          bool success = true;
          for (const QString &name : recordingsDir.entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot)) {
            success &= QFile::remove(recordingsDir.absoluteFilePath(name));
          }
          return success;
        });
      }

    } else if (id == 2) {
      QString selection = MultiOptionDialog::getSelection(tr("Choose a screen recording to rename"), friendlyNames, "", this);
      if (!selection.isEmpty()) {
        QString newBase = InputDialog::getText(tr("Enter a new name"), this, tr("Rename Screen Recording")).trimmed().replace(" ", "_");
        if (!newBase.isEmpty()) {
          QString newName = newBase + ".mp4";
          if (!validName(newBase)) {
            ConfirmationDialog::alert(tr("That name can't be used. Names can only use letters, numbers, dashes, periods, and underscores."), this);
            return;
          }
          if (recordingsNames.contains(newName)) {
            ConfirmationDialog::alert(tr("Name already in use. Please choose a different name."), this);
            return;
          }
          runDataOperation(parent, screenRecordingsButton, {0, 1}, tr("Renaming..."), tr("Renamed!"), tr("Rename failed..."), [=]() {
            return renameRecording(recordingsDir, recordingMap[selection], newName);
          });
        }
      }
    }
  });
  if (forceOpen) {
    screenRecordingsButton->showDescription();
  }
  dataMainList->addItem(screenRecordingsButton);

  FrogPilotButtonsControl *frogpilotBackupButton = new FrogPilotButtonsControl(tr("FrogPilot Backups"), tr("<b>Back up the FrogPilot software, restore a backup to go back to that version, or delete ones you no longer need.</b><br><br>Restoring reboots the device on its own and puts the software back exactly as it was when the backup was made, without changing your settings. Automatic updates turn off after a restore until you update manually. \"DELETE ALL\" also removes the backups FrogPilot makes automatically."), "", {tr("BACKUP"), tr("DELETE"), tr("DELETE ALL"), tr("RESTORE")});
  QObject::connect(frogpilotBackupButton, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir backupDir("/data/backups");
    QStringList backupNames = backupDir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name).filter(QRegularExpression("^(?!.*_in_progress(?:\\..*)?$).*$"));

    QRegularExpression autoRegex("^(.*)_(\\d{4}-\\d{2}-\\d{2})_auto(?:\\..*)?$");

    QMap<QString, QString> backupFriendlyMap;
    for (const QString &name : backupNames) {
      QString friendly = name;

      QRegularExpressionMatch match = autoRegex.match(name);
      if (match.hasMatch()) {
        friendly = match.captured(1) + ": " + match.captured(2);
      }

      backupFriendlyMap.insert(friendly, name);
    }

    if (id == 0) {
      QString nameSelection = InputDialog::getText(tr("Enter a name for this backup"), this, "", false, 1).trimmed().replace(" ", "_");
      if (!nameSelection.isEmpty()) {
        if (!validBackupName(nameSelection)) {
          ConfirmationDialog::alert(tr("That name can't be used. Names can only use letters, numbers, dashes, periods, and underscores, and \"_auto\" and \"_in_progress\" are reserved."), this);
          return;
        }
        QStringList existingNames = backupNames;
        for (QString &name : existingNames) {
          if (name.endsWith(".tar.gz")) {
            name.chop(7);
          } else if (name.endsWith(".tar.zst")) {
            name.chop(8);
          }
        }
        if (existingNames.contains(nameSelection)) {
          ConfirmationDialog::alert(tr("Name already in use. Please choose a different name."), this);
          return;
        }
        runDataOperation(parent, frogpilotBackupButton, {1, 2, 3}, tr("Backing up..."), tr("Backup created!"), tr("Backup failed..."), [=]() {
          QString inProgressPath = backupDir.filePath(nameSelection + "_in_progress.tar.zst");
          QString finalPath = backupDir.filePath(nameSelection + ".tar.zst");

          QFile::remove(inProgressPath);
          int tarStatus = runCommand("tar", {"--use-compress-program=zstd", "-cf", inProgressPath, "-C", "/data", "openpilot"});

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
      QString selectionFriendly = MultiOptionDialog::getSelection(tr("Choose a FrogPilot backup to delete"), backupFriendlyMap.keys(), "", this);
      if (!selectionFriendly.isEmpty()) {
        QString selection = backupFriendlyMap.value(selectionFriendly);
        if (ConfirmationDialog::confirm(tr("Delete this backup?"), tr("Delete"), this)) {
          runDataOperation(parent, frogpilotBackupButton, {0, 2, 3}, tr("Deleting..."), tr("Deleted!"), tr("Delete failed..."), [=]() {
            bool success = false;
            if (selection.endsWith(".tar.gz") || selection.endsWith(".tar.zst")) {
              success = QFile::remove(backupDir.filePath(selection));
            } else {
              success = QDir(backupDir.filePath(selection)).removeRecursively();
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
      QString selectionFriendly = MultiOptionDialog::getSelection(tr("Choose a backup to restore"), backupFriendlyMap.keys(), "", this);
      if (!selectionFriendly.isEmpty()) {
        QString selection = backupFriendlyMap.value(selectionFriendly);
        if (ConfirmationDialog::confirm(tr("Restore this backup? The device will reboot on its own once the restore finishes."), tr("Restore"), this)) {
          if (params.get("UpdaterState") != "idle") {
            ConfirmationDialog::alert(tr("An update check is running. Wait for it to finish and try again."), this);
            return;
          }

          bool createdBackupFlag = !QFile::exists("/cache/on_backup");
          QFile("/cache/on_backup").open(QIODevice::WriteOnly);
          frogpilotUIState()->updateToggles();

          std::thread([=]() {
            runOnUIThread(frogpilotBackupButton, [=]() {
              parent->keepScreenOn = true;

              frogpilotBackupButton->setEnabled(false);
              frogpilotBackupButton->setValue(tr("Restoring..."));

              frogpilotBackupButton->setVisibleButton(0, false);
              frogpilotBackupButton->setVisibleButton(1, false);
              frogpilotBackupButton->setVisibleButton(2, false);
            });

            QString extractDirectory = "/data/restore_temp";
            QString sourcePath = backupDir.filePath(selection);
            QString targetPath = "/data/safe_staging/finalized";

            QDir(extractDirectory).removeRecursively();
            QDir().mkpath(extractDirectory);

            QString restoreDirectory = extractDirectory;

            bool success = hasRestoreSpace(sourcePath);
            if (success && selection.endsWith(".tar.gz")) {
              runOnUIThread(frogpilotBackupButton, [=]() {
                frogpilotBackupButton->setValue(tr("Extracting..."));
              });

              success = runCommand("tar", {"--strip-components=1", "-xpzf", sourcePath, "-C", extractDirectory}) == 0;
            } else if (success && selection.endsWith(".tar.zst")) {
              runOnUIThread(frogpilotBackupButton, [=]() {
                frogpilotBackupButton->setValue(tr("Extracting..."));
              });

              success = runCommand("tar", {"-I", "zstd", "--strip-components=1", "-xpf", sourcePath, "-C", extractDirectory}) == 0;
            } else {
              restoreDirectory = sourcePath;
            }

            QString sourceRoot;
            if (success) {
              QDir extracted(restoreDirectory);
              QStringList candidates{restoreDirectory, extracted.absoluteFilePath("data/openpilot")};
              for (const QString &entry : extracted.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                candidates << extracted.absoluteFilePath(entry);
              }

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
              QDir().mkpath(targetPath);

              success = runCommand("rsync", {"-av", "--delete", "-l", sourceRoot + "/", targetPath + "/"}) == 0;
            }

            if (success) {
              QDir("/data/safe_staging/old_openpilot").removeRecursively();
              runCommand("touch", {"/data/openpilot/.overlay_init"});

              QFile overlayFile(targetPath + "/.overlay_consistent");
              overlayFile.open(QIODevice::WriteOnly);
              overlayFile.close();

              QDir(extractDirectory).removeRecursively();

              runOnUIThread(frogpilotBackupButton, [=]() {
                frogpilotBackupButton->setValue(tr("Restored!"));
              });

              util::sleep_for(2500);

              runOnUIThread(frogpilotBackupButton, [=]() {
                frogpilotBackupButton->setValue(tr("Rebooting..."));
              });

              util::sleep_for(2500);

              runOnUIThread(frogpilotBackupButton, [=]() {
                if (uiState()->scene.started) {
                  frogpilotBackupButton->setValue(tr("Reboots when the car is off"));

                  parent->keepScreenOn = false;

                  QObject::connect(uiState(), &UIState::offroadTransition, [](bool offroad) {
                    if (offroad) {
                      Hardware::reboot();
                    }
                  });
                } else {
                  Hardware::reboot();
                }
              });
            } else {
              QDir(extractDirectory).removeRecursively();

              runOnUIThread(frogpilotBackupButton, [=]() {
                if (createdBackupFlag) {
                  QFile::remove("/cache/on_backup");
                  frogpilotUIState()->updateToggles();
                }

                frogpilotBackupButton->setValue(tr("Restore failed..."));
              });

              util::sleep_for(2500);

              runOnUIThread(frogpilotBackupButton, [=]() {
                frogpilotBackupButton->setEnabled(true);
                frogpilotBackupButton->setValue("");

                frogpilotBackupButton->setVisibleButton(0, true);
                frogpilotBackupButton->setVisibleButton(1, true);
                frogpilotBackupButton->setVisibleButton(2, true);

                parent->keepScreenOn = false;
              });
            }
          }).detach();
        }
      }
    }
  });
  if (forceOpen) {
    frogpilotBackupButton->showDescription();
  }
  dataMainList->addItem(frogpilotBackupButton);

  FrogPilotButtonsControl *toggleBackupButton = new FrogPilotButtonsControl(tr("Settings Backups"), tr("<b>Save a copy of your current settings, restore a saved copy, or delete ones you no longer need.</b><br><br>Restoring applies most settings right away, but a few, such as \"High-Quality Recording\" and \"Use Konik Server\", need a reboot. FrogPilot also saves a copy automatically whenever you change a setting, but it only keeps the newest few and deletes the older ones. Those show up in the list by date and time."), "", {tr("BACKUP"), tr("DELETE"), tr("DELETE ALL"), tr("RESTORE")});
  QObject::connect(toggleBackupButton, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir backupDir("/data/toggle_backups");
    QStringList backupNames = backupDir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name).filter(QRegularExpression("^(?!.*_in_progress$).*$"));

    QRegularExpression autoRegex("^(\\d{4}-\\d{2}-\\d{2})_(\\d{2}-\\d{2}[APMapm]{2})_auto(?:\\..*)?$");

    QMap<QString, QString> backupFriendlyMap;
    for (const QString &name : backupNames) {
      QRegularExpressionMatch match = autoRegex.match(name);

      QString friendly = name;
      if (match.hasMatch()) {
        QString datePart = match.captured(1);
        QString timePart = match.captured(2);

        timePart.replace("-", ":");
        if (timePart.endsWith("pm", Qt::CaseInsensitive)) {
          timePart = timePart.left(timePart.size() - 2) + " PM";
        } else if (timePart.endsWith("am", Qt::CaseInsensitive)) {
          timePart = timePart.left(timePart.size() - 2) + " AM";
        }

        friendly = datePart + " - " + timePart;
      }
      backupFriendlyMap.insert(friendly, name);
    }

    if (id == 0) {
      QString nameSelection = InputDialog::getText(tr("Enter a name for this backup"), this, "", false, 1).trimmed().replace(" ", "_");
      if (!nameSelection.isEmpty()) {
        if (!validBackupName(nameSelection)) {
          ConfirmationDialog::alert(tr("That name can't be used. Names can only use letters, numbers, dashes, periods, and underscores, and \"_auto\" and \"_in_progress\" are reserved."), this);
          return;
        }
        if (backupNames.contains(nameSelection)) {
          ConfirmationDialog::alert(tr("Name already in use. Please choose a different name."), this);
          return;
        }
        runDataOperation(parent, toggleBackupButton, {1, 2, 3}, tr("Backing up..."), tr("Backup created!"), tr("Backup failed..."), [=]() {
          QString fullBackupPath = backupDir.filePath(nameSelection);
          QString inProgressBackupPath = fullBackupPath + "_in_progress";

          bool success = (!QFileInfo::exists(inProgressBackupPath) || QDir(inProgressBackupPath).removeRecursively());
          success &= QDir().mkpath(inProgressBackupPath);
          if (success) {
            success = runSettingsRequest("BackupSettings", inProgressBackupPath);
          }

          if (success) {
            success = QDir().rename(inProgressBackupPath, fullBackupPath);
          } else {
            QDir(inProgressBackupPath).removeRecursively();
          }
          return success;
        });
      }

    } else if (id == 1) {
      QString selectionFriendly = MultiOptionDialog::getSelection(tr("Choose a backup to delete"), backupFriendlyMap.keys(), "", this);
      if (!selectionFriendly.isEmpty()) {
        QString selection = backupFriendlyMap.value(selectionFriendly);
        if (ConfirmationDialog::confirm(tr("Delete this backup?"), tr("Delete"), this)) {
          runDataOperation(parent, toggleBackupButton, {0, 2, 3}, tr("Deleting..."), tr("Deleted!"), tr("Delete failed..."), [=]() {
            return QDir(backupDir.filePath(selection)).removeRecursively();
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
      QString selectionFriendly = MultiOptionDialog::getSelection(tr("Choose a backup to restore"), backupFriendlyMap.keys(), "", this);
      if (!selectionFriendly.isEmpty()) {
        QString selection = backupFriendlyMap.value(selectionFriendly);
        if (ConfirmationDialog::confirm(tr("Restore this backup?"), tr("Restore"), this)) {
          std::thread([=]() {
            runOnUIThread(toggleBackupButton, [=]() {
              parent->keepScreenOn = true;

              toggleBackupButton->setEnabled(false);
              toggleBackupButton->setValue(tr("Restoring..."));

              toggleBackupButton->setVisibleButton(0, false);
              toggleBackupButton->setVisibleButton(1, false);
              toggleBackupButton->setVisibleButton(2, false);
            });

            bool success = runSettingsRequest("RestoreSettings", backupDir.filePath(selection));

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

              parent->keepScreenOn = false;
            });
          }).detach();
        }
      }
    }
  });
  if (forceOpen) {
    toggleBackupButton->showDescription();
  }
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

        updateStatsLabels(statsLabelsList);
      }
    } else if (id == 1) {
      emit openSubPanel();
      dataLayout->setCurrentWidget(statsLabelsPanel);
    }
  });
  if (forceOpen) {
    viewStatsButton->showDescription();
  }
  dataMainList->addItem(viewStatsButton);

  accountButton = new FrogPilotButtonsControl(tr("FrogPilot.com Account"), tr("<b>Link this device to your FrogPilot.com account, or delete the data FrogPilot keeps about it.</b><br><br>A linked device's stats and reports show in your account. Both show a code to scan with your phone."), "", {tr("LINK"), tr("DELETE"), tr("UNLINK")});
  QObject::connect(accountButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 0) {
      FrogPilotAccountPopup popup("pair", this);
      popup.exec();
      frogpilotUIState()->requestAccountLink();
    } else if (id == 1) {
      if (ConfirmationDialog::confirm(tr("This shows a code to scan with your phone. On FrogPilot.com you'll see exactly what will be deleted before anything is."), tr("Continue"), this)) {
        FrogPilotAccountPopup popup("delete", this);
        popup.exec();
      }
    } else if (id == 2) {
      if (ConfirmationDialog::confirm(tr("Unlink this device from your FrogPilot.com account?"), tr("Unlink"), this)) {
        accountButton->setEnabled(false);
        accountButton->setValue(tr("Unlinking..."));
        frogpilotUIState()->unlinkAccount();
      }
    }
  });
  accountButton->setVisible(false);
  if (forceOpen) {
    accountButton->showDescription();
  }
  dataMainList->addItem(accountButton);

  QSizePolicy accountButtonPolicy = accountButton->sizePolicy();
  accountButtonPolicy.setRetainSizeWhenHidden(true);
  accountButton->setSizePolicy(accountButtonPolicy);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [dataLayout, dataMainPanel] {
    dataLayout->setCurrentWidget(dataMainPanel);
  });
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, [statsLabelsList, this](bool metric) {
    isMetric = metric;
    updateStatsLabels(statsLabelsList);
  });
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::statsSaved, this, [statsLabelsList, this] {
    updateStatsLabels(statsLabelsList);
  });

  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotDataPanel::updateState);
}

void FrogPilotDataPanel::showEvent(QShowEvent *event) {
  frogpilotUIState()->requestAccountLink();
}

void FrogPilotDataPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible() || s.sm->frame % UI_FREQ != 0) {
    return;
  }

  const cereal::FrogPilotAccountState::Reader &frogpilotAccountState = (*fs.sm)["frogpilotAccountState"].getFrogpilotAccountState();
  if (frogpilotAccountState.getLinkRequestTime() < fs.account_link_request_time) {
    return;
  }

  QJsonObject accountLink = QJsonDocument::fromJson(frogpilotAccountState.getLink().cStr()).object();
  bool linked = accountLink.value("linked").toBool();

  accountButton->setEnabled(true);
  accountButton->setVisible(linked || accountLink.value("available").toBool());
  accountButton->setVisibleButton(0, !linked);
  accountButton->setVisibleButton(1, !linked);
  accountButton->setVisibleButton(2, linked);

  if (linked) {
    accountButton->setValue(tr("Linked as @%1").arg(accountLink.value("username").toString()));
  } else {
    accountButton->setValue("");
  }
}

void FrogPilotDataPanel::updateStatsLabels(FrogPilotListWidget *labelsList) {
  labelsList->clear();

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
    {"snow", tr("Snow")},
    {"unknown", tr("Unknown")}
  };

  QStringList keys = keyMap.keys();
  std::sort(keys.begin(), keys.end(), [&](const QString &a, const QString &b) {
    return keyMap.value(a).first.toLower() < keyMap.value(b).first.toLower();
  });

  std::function<QString(double)> format_number = [&](double number) {
    return QLocale().toString(number);
  };

  double trackedTime = stats.contains("TrackedTime") ? stats.value("TrackedTime").toDouble() : 0.0;

  for (const QString &key : keys) {
    QJsonValue value = stats.contains(key) ? stats.value(key) : QJsonValue(0);
    if (key == "CurrentMonthsMeters" && stats.value("Month").toInt() != QDateTime::currentDateTimeUtc().date().month()) {
      value = QJsonValue(0);
    }
    QString labelText = keyMap.value(key).first;
    QString type = keyMap.value(key).second;

    if ((key == "DayTime" || key == "NightTime") && value.toDouble() <= 0.0) {
      continue;
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

      labelsList->addItem(new LabelControl(labelText, displaySpeed + " (" + formatDuration(maxTime) + ")", "", this));
    } else if (type == "parent") {
      labelsList->addItem(new LabelControl(labelText, "", "", this));

      QJsonObject subObject = value.toObject();
      QStringList subKeys;

      if (key == "RandomEvents") {
        subKeys = randomEventNames().keys();
      } else {
        subKeys = subObject.keys();
      }

      std::sort(subKeys.begin(), subKeys.end(), [&](const QString &a, const QString &b) {
        QString displayA, displayB;
        if (key == "RandomEvents") {
          displayA = randomEventNames().value(a, a);
          displayB = randomEventNames().value(b, b);
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
          displaySubKey = processModelName(subkey);
        } else if (key == "RandomEvents") {
          displaySubKey = randomEventNames().value(subkey, subkey);
        } else if (key == "PersonalityTimes") {
          displaySubKey = personalityNames.value(subkey, subkey);
        } else if (key == "WeatherTimes") {
          displaySubKey = weatherNames.value(subkey, subkey);
        } else {
          displaySubKey = subkey;
        }

        QString subvalue;
        if (key.endsWith("Times")) {
          subvalue = formatDuration(subObject.value(subkey).toDouble());
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
        displayValue = formatDistance(value.toDouble(), isMetric);
      } else if (type == "speed") {
        displayValue = "--";
      } else if (type == "time" || type == "timePercent") {
        displayValue = formatDuration(value.toDouble());
      } else {
        QString stringValue = value.toVariant().toString();
        displayValue = stringValue.isEmpty() ? "0" : stringValue;
      }

      labelsList->addItem(new LabelControl(labelText, displayValue, "", this));

      if (type == "timePercent") {
        int percent = 0;
        if (trackedTime > 0.0) {
          percent = (value.toDouble() * 100.0) / trackedTime;
        }

        labelsList->addItem(new LabelControl(tr("% of ") + labelText, format_number(percent) + "%", "", this));
      }
    }
  }
}
