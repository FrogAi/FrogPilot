#include <fcntl.h>
#include <sys/file.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <QProcess>
#include <QStorageInfo>

#include "frogpilot/ui/qt/offroad/data_settings.h"

namespace {
  int lockRecordings() {
    int fd = ::open("/data/media/screen_recordings.lock", O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0664);
    if (fd >= 0 && ::flock(fd, LOCK_EX | LOCK_NB) != 0) {
      ::close(fd);
      return -1;
    }
    return fd;
  }

  void removeRecordingCompanions(const QDir &directory, const QString &name) {
    const QString stem = name.left(name.lastIndexOf('.'));
    QFile::remove(directory.absoluteFilePath(stem + ".png"));
    QFile::remove(directory.absoluteFilePath(stem + ".gif"));
  }

  void renameRecordingCompanions(const QDir &directory, const QString &oldName, const QString &newName) {
    const QString oldStem = oldName.left(oldName.lastIndexOf('.'));
    const QString newStem = newName.left(newName.lastIndexOf('.'));
    for (const QString &extension : {QString(".png"), QString(".gif")}) {
      const QString oldPath = directory.absoluteFilePath(oldStem + extension);
      if (QFile::exists(oldPath)) {
        QFile::rename(oldPath, directory.absoluteFilePath(newStem + extension));
      }
    }
  }

  const QStringList PROTECTED_PARAMS = {
    "CalibrationParams", "DongleId", "GithubSshKeys", "GithubUsername",
    "HardwareSerial", "IMEI", "LiveDelay", "LiveParameters", "LiveTorqueParameters"
  };

  bool validBackupName(const QString &name) {
    static const QRegularExpression validCharacters("^[A-Za-z0-9._-]+$");
    return validCharacters.match(name).hasMatch() && !name.contains("..") && !name.startsWith("-") && !name.contains("_in_progress") && !name.contains("_auto");
  }

  int runCommand(const QString &program, const QStringList &arguments) {
    return QProcess::execute(program, arguments);
  }

  QStringList protectedParamArgs() {
    QStringList arguments;
    for (const QString &key : PROTECTED_PARAMS) {
      arguments << "--exclude" << key;
    }
    return arguments;
  }
}

FrogPilotDataPanel::FrogPilotDataPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  QStackedLayout *dataLayout = new QStackedLayout();
  addItem(dataLayout);

  FrogPilotListWidget *dataMainList = new FrogPilotListWidget(this);
  ScrollView *dataMainPanel = new ScrollView(dataMainList, this);
  dataLayout->addWidget(dataMainPanel);

  FrogPilotListWidget *statsLabelsList = new FrogPilotListWidget(this);
  ScrollView *statsLabelsPanel = new ScrollView(statsLabelsList, this);
  dataLayout->addWidget(statsLabelsPanel);

  ButtonControl *deleteDrivingDataButton = new ButtonControl(tr("Delete Driving Data"), tr("DELETE"), tr("<b>Delete every recorded drive to free up space and clear personal footage off the device.</b><br><br>Only the one-minute chunk of footage containing the moment you flagged is kept, not the rest of that drive, and preserving a drive in \"The Pond\" keeps it the same one minute at a time."));
  QObject::connect(deleteDrivingDataButton, &ButtonControl::clicked, [=]() {
    if (ConfirmationDialog::confirm(tr("Delete all driving footage and data? Flagged and preserved drives will be kept."), tr("Delete"), this)) {
      std::thread([=]() {
        runOnUIThread(deleteDrivingDataButton, [=]() {
          parent->keepScreenOn = true;

          deleteDrivingDataButton->setEnabled(false);
          deleteDrivingDataButton->setValue(tr("Deleting..."));
        });

        bool success = true;

        std::vector<QString> drivePaths = {"/data/media/0/realdata/", "/data/media/0/realdata_HD/", "/data/media/0/realdata_konik/"};
        for (const QString &path : drivePaths) {
          QDir dir(path);
          if (!dir.exists()) {
            continue;
          }

          for (const QFileInfo &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            char preserveValue[10] = {0};
            bool isPreserved = (getxattr(entry.absoluteFilePath().toUtf8().constData(), "user.preserve", preserveValue, sizeof(preserveValue)) > 0 && strcmp(preserveValue, "1") == 0);
            if (!isPreserved) {
              success &= QDir(entry.absoluteFilePath()).removeRecursively();
            }
          }
        }

        runOnUIThread(deleteDrivingDataButton, [=]() {
          deleteDrivingDataButton->setValue(success ? tr("Deleted!") : tr("Delete failed..."));
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
  if (forceOpenDescriptions) {
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
  if (forceOpenDescriptions) {
    deleteErrorLogsButton->showDescription();
  }
  dataMainList->addItem(deleteErrorLogsButton);

  FrogPilotButtonsControl *screenRecordingsButton = new FrogPilotButtonsControl(tr("Screen Recordings"), tr("<b>Delete or rename your recordings of the driving screen.</b><br><br>Recordings are made with the \"Screen Recorder\" button on the driving screen. \"DELETE ALL\" removes every recording at once."), "", {tr("DELETE"), tr("DELETE ALL"), tr("RENAME")});
  QObject::connect(screenRecordingsButton, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir recordingsDir("/data/media/screen_recordings");
    QStringList recordingsNames = recordingsDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    std::sort(recordingsNames.begin(), recordingsNames.end(), std::greater<QString>());

    QStringList friendlyNames;
    QMap<QString, QString> recordingMap;

    for (const QString &name : recordingsNames) {
      if (!name.endsWith(".mp4", Qt::CaseInsensitive)) {
        continue;
      }

      QString friendlyName = name;
      QString cleanName = QString(name).remove(".mp4");

      QStringList parts = cleanName.split(cleanName.contains("--") ? "--" : "_");

      if (parts.size() >= 2) {
        QDate date = QDate::fromString(parts[0], "yyyy-MM-dd");
        QTime time = QTime::fromString(parts[1], "HH-mm-ss");

        if (date.isValid() && time.isValid()) {
          int day = date.day();
          QString suffix = (day >= 11 && day <= 13) ? "th" :
                           (day % 10 == 1) ? "st" :
                           (day % 10 == 2) ? "nd" :
                           (day % 10 == 3) ? "rd" : "th";

          friendlyName = QString("%1 %2%3, %4 (%5)")
            .arg(date.toString("MMMM"))
            .arg(day)
            .arg(suffix)
            .arg(date.year())
            .arg(time.toString("h:mm AP"));
        }
      }

      if (friendlyName == name) {
        friendlyName = cleanName;
        friendlyName.replace("_", " ");
      }

      friendlyNames.append(friendlyName);
      recordingMap[friendlyName] = name;
    }

    if (id == 0) {
      QString selection = MultiOptionDialog::getSelection(tr("Choose a screen recording to delete"), friendlyNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Delete this screen recording?"), tr("Delete"), this)) {
          std::thread([=]() {
            runOnUIThread(screenRecordingsButton, [=]() {
              parent->keepScreenOn = true;

              screenRecordingsButton->setButtonsEnabled(false);
              screenRecordingsButton->setValue(tr("Deleting..."));

              screenRecordingsButton->setVisibleButton(1, false);
              screenRecordingsButton->setVisibleButton(2, false);
            });

            const QString recordingName = recordingMap[selection];
            int lockFd = lockRecordings();
            bool success = false;
            if (lockFd >= 0) {
              success = QFile::remove(recordingsDir.absoluteFilePath(recordingName));
              if (success) {
                removeRecordingCompanions(recordingsDir, recordingName);
              }
              ::close(lockFd);
            }

            runOnUIThread(screenRecordingsButton, [=]() {
              screenRecordingsButton->setValue(lockFd < 0 ? tr("Recording in progress...") : success ? tr("Deleted!") : tr("Delete failed..."));
            });

            util::sleep_for(2500);

            runOnUIThread(screenRecordingsButton, [=]() {
              screenRecordingsButton->setButtonsEnabled(true);
              screenRecordingsButton->setValue("");

              screenRecordingsButton->setVisibleButton(1, true);
              screenRecordingsButton->setVisibleButton(2, true);

              parent->keepScreenOn = false;
            });
          }).detach();
        }
      }

    } else if (id == 1) {
      if (ConfirmationDialog::confirm(tr("Delete all screen recordings?"), tr("Delete All"), this)) {
        std::thread([=]() mutable {
          runOnUIThread(screenRecordingsButton, [=]() {
            parent->keepScreenOn = true;

            screenRecordingsButton->setButtonsEnabled(false);
            screenRecordingsButton->setValue(tr("Deleting..."));

            screenRecordingsButton->setVisibleButton(0, false);
            screenRecordingsButton->setVisibleButton(2, false);
          });

          int lockFd = lockRecordings();
          bool success = false;
          if (lockFd >= 0) {
            success = recordingsDir.removeRecursively();
            success &= QDir("/data/media/screen_recordings.in_progress").removeRecursively();
            success &= recordingsDir.mkpath(".");
            ::close(lockFd);
          }

          runOnUIThread(screenRecordingsButton, [=]() {
            screenRecordingsButton->setValue(lockFd < 0 ? tr("Recording in progress...") : success ? tr("Deleted!") : tr("Delete failed..."));
          });

          util::sleep_for(2500);

          runOnUIThread(screenRecordingsButton, [=]() {
            screenRecordingsButton->setButtonsEnabled(true);
            screenRecordingsButton->setValue("");

            screenRecordingsButton->setVisibleButton(0, true);
            screenRecordingsButton->setVisibleButton(2, true);

            parent->keepScreenOn = false;
          });
        }).detach();
      }

    } else if (id == 2) {
      QString selection = MultiOptionDialog::getSelection(tr("Choose a screen recording to rename"), friendlyNames, "", this);
      if (!selection.isEmpty()) {
        QString newBase = InputDialog::getText(tr("Enter a new name"), this, tr("Rename Screen Recording")).trimmed().replace(" ", "_");
        if (!newBase.isEmpty()) {
          QString newName = newBase + ".mp4";
          if (!validBackupName(newBase)) {
            ConfirmationDialog::alert(tr("That name can't be used. Names can only use letters, numbers, dashes, periods, and underscores."), this);
            return;
          }
          if (recordingsNames.contains(newName)) {
            ConfirmationDialog::alert(tr("Name already in use. Please choose a different name."), this);
            return;
          }
          std::thread([=]() {
            runOnUIThread(screenRecordingsButton, [=]() {
              parent->keepScreenOn = true;

              screenRecordingsButton->setButtonsEnabled(false);
              screenRecordingsButton->setValue(tr("Renaming..."));

              screenRecordingsButton->setVisibleButton(0, false);
              screenRecordingsButton->setVisibleButton(1, false);
            });

            QString newPath = recordingsDir.absoluteFilePath(newName);
            const QString oldName = recordingMap[selection];
            QString oldPath = recordingsDir.absoluteFilePath(oldName);
            int lockFd = lockRecordings();
            bool success = false;
            if (lockFd >= 0) {
              success = QFile::rename(oldPath, newPath);
              if (success) {
                renameRecordingCompanions(recordingsDir, oldName, newName);
              }
              ::close(lockFd);
            }

            runOnUIThread(screenRecordingsButton, [=]() {
              screenRecordingsButton->setValue(lockFd < 0 ? tr("Recording in progress...") : success ? tr("Renamed!") : tr("Rename failed..."));
            });

            util::sleep_for(2500);

            runOnUIThread(screenRecordingsButton, [=]() {
              screenRecordingsButton->setButtonsEnabled(true);
              screenRecordingsButton->setValue("");

              screenRecordingsButton->setVisibleButton(0, true);
              screenRecordingsButton->setVisibleButton(1, true);

              parent->keepScreenOn = false;
            });
          }).detach();
        }
      }
    }
  });
  if (forceOpenDescriptions) {
    screenRecordingsButton->showDescription();
  }
  dataMainList->addItem(screenRecordingsButton);

  FrogPilotButtonsControl *frogpilotBackupButton = new FrogPilotButtonsControl(tr("FrogPilot Backups"), tr("<b>Back up the FrogPilot software, restore a backup to go back to that version, or delete ones you no longer need.</b><br><br>Restoring reboots the device on its own and puts the software back exactly as it was when the backup was made, without changing your settings. Automatic updates turn off after a restore until you update manually. \"DELETE ALL\" also removes the backups FrogPilot makes automatically."), "", {tr("BACKUP"), tr("DELETE"), tr("DELETE ALL"), tr("RESTORE")});
  QObject::connect(frogpilotBackupButton, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir backupDir("/data/backups");

    QFileInfoList backupList = backupDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    std::sort(backupList.begin(), backupList.end(), [](const QFileInfo &a, const QFileInfo &b) {
      return a.lastModified() > b.lastModified();
    });

    QStringList friendlyNames;
    QMap<QString, QString> backupMap;

    for (const QFileInfo &fileInfo : backupList) {
      QString fileName = fileInfo.fileName();

      if (fileName.contains("in_progress")) {
        continue;
      }

      QString friendlyName = fileName;

      if (fileName.endsWith("_auto.tar.zst")) {
        QStringList parts = QString(fileName).remove(".tar.zst").split("_");

        if (parts.size() >= 3) {
          QDate date = fileInfo.lastModified().date();

          if (date.isValid()) {
            int day = date.day();
            QString suffix = (day >= 11 && day <= 13) ? "th" :
                             (day % 10 == 1) ? "st" :
                             (day % 10 == 2) ? "nd" :
                             (day % 10 == 3) ? "rd" : "th";

            friendlyName = QString("%1 %2%3, %4 (%5)")
              .arg(date.toString("MMMM"))
              .arg(day)
              .arg(suffix)
              .arg(date.year())
              .arg(parts[1]);
          }
        }
      }

      if (friendlyName == fileName) {
        friendlyName.remove(".tar.zst");
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

        QStringList distinctFileNames = backupDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        if (distinctFileNames.contains(name + ".tar.zst")) {
          ConfirmationDialog::alert(tr("Name already in use. Please choose a different name."), this);
          return;
        }

        std::thread([=]() {
          runOnUIThread(frogpilotBackupButton, [=]() {
            parent->keepScreenOn = true;

            frogpilotBackupButton->setButtonsEnabled(false);
            frogpilotBackupButton->setValue(tr("Backing up..."));

            frogpilotBackupButton->setVisibleButton(1, false);
            frogpilotBackupButton->setVisibleButton(2, false);
            frogpilotBackupButton->setVisibleButton(3, false);
          });

          QString inProgressPath = backupDir.absoluteFilePath(name + "_in_progress.tar.zst");
          QString finalPath = backupDir.absoluteFilePath(name + ".tar.zst");

          QFile::remove(inProgressPath);
          int tarStatus = runCommand("tar", {"--use-compress-program=zstd", "-cf", inProgressPath, "/data/openpilot"});

          bool success = tarStatus == 0 && QFileInfo(inProgressPath).size() > 0;
          if (success) {
            success = QFile::rename(inProgressPath, finalPath);
          }
          if (!success) {
            QFile::remove(inProgressPath);
          }

          runOnUIThread(frogpilotBackupButton, [=]() {
            frogpilotBackupButton->setValue(success ? tr("Backup created!") : tr("Backup failed..."));
          });

          util::sleep_for(2500);

          runOnUIThread(frogpilotBackupButton, [=]() {
            frogpilotBackupButton->setButtonsEnabled(true);
            frogpilotBackupButton->setValue("");

            frogpilotBackupButton->setVisibleButton(1, true);
            frogpilotBackupButton->setVisibleButton(2, true);
            frogpilotBackupButton->setVisibleButton(3, true);

            parent->keepScreenOn = false;
          });
        }).detach();
      }

    } else if (id == 1) {
      QString selection = MultiOptionDialog::getSelection(tr("Choose a backup to delete"), friendlyNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Delete this backup?"), tr("Delete"), this)) {
          std::thread([=]() {
            runOnUIThread(frogpilotBackupButton, [=]() {
              parent->keepScreenOn = true;

              frogpilotBackupButton->setButtonsEnabled(false);
              frogpilotBackupButton->setValue(tr("Deleting..."));

              frogpilotBackupButton->setVisibleButton(0, false);
              frogpilotBackupButton->setVisibleButton(2, false);
              frogpilotBackupButton->setVisibleButton(3, false);
            });

            bool success = QFile::remove(backupDir.absoluteFilePath(backupMap[selection]));

            runOnUIThread(frogpilotBackupButton, [=]() {
              frogpilotBackupButton->setValue(success ? tr("Deleted!") : tr("Delete failed..."));
            });

            util::sleep_for(2500);

            runOnUIThread(frogpilotBackupButton, [=]() {
              frogpilotBackupButton->setButtonsEnabled(true);
              frogpilotBackupButton->setValue("");

              frogpilotBackupButton->setVisibleButton(0, true);
              frogpilotBackupButton->setVisibleButton(2, true);
              frogpilotBackupButton->setVisibleButton(3, true);

              parent->keepScreenOn = false;
            });
          }).detach();
        }
      }

    } else if (id == 2) {
      if (ConfirmationDialog::confirm(tr("Delete all backups? This includes the backups FrogPilot makes automatically."), tr("Delete All"), this)) {
        std::thread([=]() mutable {
          runOnUIThread(frogpilotBackupButton, [=]() {
            parent->keepScreenOn = true;

            frogpilotBackupButton->setButtonsEnabled(false);
            frogpilotBackupButton->setValue(tr("Deleting..."));

            frogpilotBackupButton->setVisibleButton(0, false);
            frogpilotBackupButton->setVisibleButton(1, false);
            frogpilotBackupButton->setVisibleButton(3, false);
          });

          bool success = backupDir.removeRecursively();
          backupDir.mkpath(".");

          runOnUIThread(frogpilotBackupButton, [=]() {
            frogpilotBackupButton->setValue(success ? tr("Deleted!") : tr("Delete failed..."));
          });

          util::sleep_for(2500);

          runOnUIThread(frogpilotBackupButton, [=]() {
            frogpilotBackupButton->setButtonsEnabled(true);
            frogpilotBackupButton->setValue("");

            frogpilotBackupButton->setVisibleButton(0, true);
            frogpilotBackupButton->setVisibleButton(1, true);
            frogpilotBackupButton->setVisibleButton(3, true);

            parent->keepScreenOn = false;
          });
        }).detach();
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
              parent->keepScreenOn = true;

              frogpilotBackupButton->setButtonsEnabled(false);
              frogpilotBackupButton->setValue(tr("Restoring..."));

              frogpilotBackupButton->setVisibleButton(0, false);
              frogpilotBackupButton->setVisibleButton(1, false);
              frogpilotBackupButton->setVisibleButton(2, false);
            });

            QString archivePath = backupDir.absoluteFilePath(backupMap[selection]);
            QString extractDirectory = "/data/restore_temp";

            QDir(extractDirectory).removeRecursively();
            QDir().mkpath(extractDirectory);

            bool success = QStorageInfo("/data").bytesAvailable() > QFileInfo(archivePath).size() * 4;

            if (success) {
              success = runCommand("tar", {"--use-compress-program=zstd", "-xf", archivePath, "-C", extractDirectory}) == 0;
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
                if (QFileInfo::exists(candidate + "/launch_openpilot.sh") && QFileInfo::exists(candidate + "/launch_chffrplus.sh") &&
                    QDir(candidate + "/selfdrive").exists() && QDir(candidate + "/system").exists()) {
                  sourceRoot = candidate;
                  break;
                }
              }
              success = !sourceRoot.isEmpty();
            }

            if (success) {
              success = runCommand("rsync", {"-a", "--delete", "-l", sourceRoot + "/", "/data/openpilot/"}) == 0;
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
                frogpilotBackupButton->setButtonsEnabled(true);
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
  if (forceOpenDescriptions) {
    frogpilotBackupButton->showDescription();
  }
  dataMainList->addItem(frogpilotBackupButton);

  FrogPilotButtonsControl *toggleBackupButton = new FrogPilotButtonsControl(tr("Settings Backups"), tr("<b>Save a copy of your current settings, restore a saved copy, or delete ones you no longer need.</b><br><br>Restoring applies the settings right away with no reboot needed. FrogPilot also saves a copy automatically whenever you change a setting, but it only keeps the newest few and deletes the older ones. FrogPilot also saves a copy automatically whenever you change a setting, and those show up in the list by date and time."), "", {tr("BACKUP"), tr("DELETE"), tr("DELETE ALL"), tr("RESTORE")});
  QObject::connect(toggleBackupButton, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir backupDir("/data/toggle_backups");

    QStringList backupNames = backupDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    std::sort(backupNames.begin(), backupNames.end(), std::greater<QString>());

    QMap<QString, QString> backupMap;
    for (const QString &dirName : backupNames) {
      if (dirName.contains("in_progress")) {
        continue;
      }

      QString friendlyName = dirName;

      if (dirName.endsWith("_auto")) {
        QStringList parts = QString(dirName).remove("_auto").split("_");

        if (parts.size() >= 2) {
          QDate date = QDate::fromString(parts[0], "yyyy-MM-dd");
          QTime time = QTime::fromString(parts[1], "HH-mm-ss");

          if (date.isValid() && time.isValid()) {
            int day = date.day();
            QString suffix = (day >= 11 && day <= 13) ? "th" :
                             (day % 10 == 1) ? "st" :
                             (day % 10 == 2) ? "nd" :
                             (day % 10 == 3) ? "rd" : "th";

            friendlyName = QString("%1 %2%3, %4 (%5)")
              .arg(date.toString("MMMM"))
              .arg(day)
              .arg(suffix)
              .arg(date.year())
              .arg(time.toString("h:mm AP"));
          }
        }
      }

      if (friendlyName == dirName) {
        friendlyName.replace("_", " ");
      }

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

        std::thread([=]() {
          runOnUIThread(toggleBackupButton, [=]() {
            parent->keepScreenOn = true;

            toggleBackupButton->setButtonsEnabled(false);
            toggleBackupButton->setValue(tr("Backing up..."));

            toggleBackupButton->setVisibleButton(1, false);
            toggleBackupButton->setVisibleButton(2, false);
            toggleBackupButton->setVisibleButton(3, false);
          });

          QString inProgressPath = backupDir.absoluteFilePath(name + "_in_progress");
          QString finalPath = backupDir.absoluteFilePath(name);

          bool success = runCommand("rsync", QStringList{"-a", "/data/params/d/", inProgressPath + "/"} + protectedParamArgs()) == 0;
          if (success) {
            success = QDir().rename(inProgressPath, finalPath);
          }
          if (!success) {
            QDir(inProgressPath).removeRecursively();
          }

          runOnUIThread(toggleBackupButton, [=]() {
            toggleBackupButton->setValue(success ? tr("Backup created!") : tr("Backup failed..."));
          });

          util::sleep_for(2500);

          runOnUIThread(toggleBackupButton, [=]() {
            toggleBackupButton->setButtonsEnabled(true);
            toggleBackupButton->setValue("");

            toggleBackupButton->setVisibleButton(1, true);
            toggleBackupButton->setVisibleButton(2, true);
            toggleBackupButton->setVisibleButton(3, true);

            parent->keepScreenOn = false;
          });
        }).detach();
      }

    } else if (id == 1) {
      QString selection = MultiOptionDialog::getSelection(tr("Choose a backup to delete"), backupMap.keys(), "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Delete this backup?"), tr("Delete"), this)) {
          std::thread([=]() {
            runOnUIThread(toggleBackupButton, [=]() {
              parent->keepScreenOn = true;

              toggleBackupButton->setButtonsEnabled(false);
              toggleBackupButton->setValue(tr("Deleting..."));

              toggleBackupButton->setVisibleButton(0, false);
              toggleBackupButton->setVisibleButton(2, false);
              toggleBackupButton->setVisibleButton(3, false);
            });

            bool success = QDir(backupDir.absoluteFilePath(backupMap[selection])).removeRecursively();

            runOnUIThread(toggleBackupButton, [=]() {
              toggleBackupButton->setValue(success ? tr("Deleted!") : tr("Delete failed..."));
            });

            util::sleep_for(2500);

            runOnUIThread(toggleBackupButton, [=]() {
              toggleBackupButton->setButtonsEnabled(true);
              toggleBackupButton->setValue("");

              toggleBackupButton->setVisibleButton(0, true);
              toggleBackupButton->setVisibleButton(2, true);
              toggleBackupButton->setVisibleButton(3, true);

              parent->keepScreenOn = false;
            });
          }).detach();
        }
      }

    } else if (id == 2) {
      if (ConfirmationDialog::confirm(tr("Delete all settings backups? This includes the copies FrogPilot saves automatically."), tr("Delete All"), this)) {
        std::thread([=]() mutable {
          runOnUIThread(toggleBackupButton, [=]() {
            parent->keepScreenOn = true;

            toggleBackupButton->setButtonsEnabled(false);
            toggleBackupButton->setValue(tr("Deleting..."));

            toggleBackupButton->setVisibleButton(0, false);
            toggleBackupButton->setVisibleButton(1, false);
            toggleBackupButton->setVisibleButton(3, false);
          });

          bool success = backupDir.removeRecursively();
          backupDir.mkpath(".");

          runOnUIThread(toggleBackupButton, [=]() {
            toggleBackupButton->setValue(success ? tr("Deleted!") : tr("Delete failed..."));
          });

          util::sleep_for(2500);

          runOnUIThread(toggleBackupButton, [=]() {
            toggleBackupButton->setButtonsEnabled(true);
            toggleBackupButton->setValue("");

            toggleBackupButton->setVisibleButton(0, true);
            toggleBackupButton->setVisibleButton(1, true);
            toggleBackupButton->setVisibleButton(3, true);

            parent->keepScreenOn = false;
          });
        }).detach();
      }

    } else if (id == 3) {
      QString selection = MultiOptionDialog::getSelection(tr("Choose a backup to restore"), backupMap.keys(), "", this);
      if (!selection.isEmpty()) {
        if (FrogPilotConfirmationDialog::yesorno(tr("Restore this backup? This overwrites your current settings."), this)) {
          std::thread([=]() {
            runOnUIThread(toggleBackupButton, [=]() {
              parent->keepScreenOn = true;

              toggleBackupButton->setButtonsEnabled(false);
              toggleBackupButton->setValue(tr("Restoring..."));

              toggleBackupButton->setVisibleButton(0, false);
              toggleBackupButton->setVisibleButton(1, false);
              toggleBackupButton->setVisibleButton(2, false);
            });

            bool success = runCommand("rsync", QStringList{"-a", "-l", backupDir.absoluteFilePath(backupMap[selection]) + "/", "/data/params/d/"} + protectedParamArgs()) == 0;

            if (success) {
              updateFrogPilotToggles();
            }

            runOnUIThread(toggleBackupButton, [=]() {
              if (success) {
                parent->updateTuningLevel();
              }

              toggleBackupButton->setValue(success ? tr("Restored!") : tr("Restore failed..."));
            });

            util::sleep_for(2500);

            runOnUIThread(toggleBackupButton, [=]() {
              toggleBackupButton->setButtonsEnabled(true);
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
  if (forceOpenDescriptions) {
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
  if (forceOpenDescriptions) {
    viewStatsButton->showDescription();
  }
  dataMainList->addItem(viewStatsButton);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [dataLayout, dataMainPanel] {
    dataLayout->setCurrentWidget(dataMainPanel);
  });
  QObject::connect(parent, &FrogPilotSettingsWindow::updateMetric, [statsLabelsList, this](bool metric) {
    isMetric = metric;
    updateStatsLabels(statsLabelsList);
  });
  QObject::connect(uiState(), &UIState::offroadTransition, [statsLabelsList, this](bool offroad) {
    if (offroad) {
      updateStatsLabels(statsLabelsList);
    }
  });
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
    {"Month", {tr("Month"), "other"}},
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
    {"hal9000", tr("HAL 9000 Denials")},
    {"openpilotCrashedRandomEvent", tr("openpilot Crashes")},
    {"thisIsFineSteerSaturated", tr("This Is Fine Moments")},
    {"toBeContinued", tr("To Be Continued Moments")},
    {"vCruise69", tr("Noices")},
    {"yourFrogTriedToKillMe", tr("Attempted Frog Murders")},
    {"youveGotMail", tr("Total Mail Received")}
  };

  static QSet<QString> ignoredKeys = {
    "Month"
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
    static int secondsInDay = 60 * 60 * 24;
    static int secondsInHour = 60 * 60;

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

  double trackedTime = stats.contains("TrackedTime") ? stats.value("TrackedTime").toDouble() : 0.0;

  for (const QString &key : keys) {
    if (ignoredKeys.contains(key)) {
      continue;
    }

    QJsonValue value = stats.contains(key) ? stats.value(key) : QJsonValue(0);
    QString labelText = keyMap.value(key).first;
    QString type = keyMap.value(key).second;

    if ((key == "DayTime" || key == "NightTime") && value.toDouble() <= 0.0) {
      continue;
    }

    if (key == "AEBEvents") {
      QJsonObject totalEvents = stats.value("TotalEvents").toObject();

      QString trimmedLabel = labelText;
      QString prefix = tr("Total ");
      if (trimmedLabel.startsWith(prefix)) {
        trimmedLabel = trimmedLabel.mid(prefix.length());
      }
      QString displayValue = format_number(totalEvents.value("stockAeb").toInt(0) + totalEvents.value("fcw").toInt(0)) + " " + trimmedLabel;

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
        } else if (key == "WeatherTimes") {
          QStringList words = subkey.split('_');
          for (QString &word : words) {
            if (!word.isEmpty()) {
              word[0] = word[0].toUpper();
            }
          }
          displaySubKey = words.join(' ');
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
        QString trimmedLabel = labelText;
        QString prefix = tr("Total ");
        if (trimmedLabel.startsWith(prefix)) {
          trimmedLabel = trimmedLabel.mid(prefix.length());
        }
        displayValue = format_number(value.toInt()) + " " + trimmedLabel;
      } else if (type == "distance") {
        displayValue = format_distance(value.toDouble());
      } else if (type == "speed") {
        displayValue = "--";
      } else if (type == "time" || type == "timePercent") {
        displayValue = format_time(value.toDouble());
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
