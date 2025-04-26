#include "selfdrive/frogpilot/ui/qt/offroad/data_settings.h"

FrogPilotDataPanel::FrogPilotDataPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  ButtonControl *deleteDrivingDataBtn = new ButtonControl(tr("Delete Driving Footage and Data"), tr("DELETE"), tr("Deletes all stored driving footage and data from your device. Ideal for maintaining privacy or for simply freeing up space."));
  QObject::connect(deleteDrivingDataBtn, &ButtonControl::clicked, [=]() {
    QDir realdataDir(QString::fromStdString(Path::log_root()));

    if (ConfirmationDialog::confirm(tr("Are you sure you want to delete all of your driving footage and data?"), tr("Delete"), this)) {
      std::thread([=]() mutable {
        parent->keepScreenOn = true;

        deleteDrivingDataBtn->setEnabled(false);
        deleteDrivingDataBtn->setValue(tr("Deleting..."));

        realdataDir.removeRecursively();
        realdataDir.mkpath(".");

        deleteDrivingDataBtn->setValue(tr("Deleted!"));

        util::sleep_for(2500);

        deleteDrivingDataBtn->setEnabled(true);
        deleteDrivingDataBtn->setValue("");

        parent->keepScreenOn = false;
      }).detach();
    }
  });
  addItem(deleteDrivingDataBtn);

  ButtonControl *deleteErrorLogsBtn = new ButtonControl(tr("Delete Error Logs"), tr("DELETE"), tr("Deletes all stored error logs from your device. Ideal for freeing up space."));
  QObject::connect(deleteErrorLogsBtn, &ButtonControl::clicked, [=]() {
    QDir errorLogsDir("/data/error_logs");

    if (ConfirmationDialog::confirm(tr("Are you sure you want to delete all of the error logs?"), tr("Delete"), this)) {
      std::thread([=]() mutable {
        parent->keepScreenOn = true;

        deleteErrorLogsBtn->setEnabled(false);
        deleteErrorLogsBtn->setValue(tr("Deleting..."));

        errorLogsDir.removeRecursively();
        errorLogsDir.mkpath(".");

        deleteErrorLogsBtn->setValue(tr("Deleted!"));

        util::sleep_for(2500);

        deleteErrorLogsBtn->setEnabled(true);
        deleteErrorLogsBtn->setValue("");

        parent->keepScreenOn = false;
      }).detach();
    }
  });
  addItem(deleteErrorLogsBtn);

  FrogPilotButtonsControl *screenRecordingsBtn = new FrogPilotButtonsControl(tr("Screen Recordings"), tr("Manage your screen recordings."), "", {tr("DELETE"), tr("DELETE ALL"), tr("RENAME")});
  QObject::connect(screenRecordingsBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir recordingsDir("/data/media/screen_recordings");
    QStringList recordingsNames = recordingsDir.entryList(QDir::Files | QDir::NoDotAndDotDot);

    if (id == 0) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a recording to delete"), recordingsNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Are you sure you want to delete this recording?"), tr("Delete"), this)) {
          std::thread([=]() {
            parent->keepScreenOn = true;

            screenRecordingsBtn->setEnabled(false);
            screenRecordingsBtn->setValue(tr("Deleting..."));

            screenRecordingsBtn->setVisibleButton(1, false);
            screenRecordingsBtn->setVisibleButton(2, false);

            QFile::remove(recordingsDir.absoluteFilePath(selection));

            screenRecordingsBtn->setValue(tr("Deleted!"));

            util::sleep_for(2500);

            screenRecordingsBtn->setEnabled(true);
            screenRecordingsBtn->setValue("");

            screenRecordingsBtn->setVisibleButton(1, true);
            screenRecordingsBtn->setVisibleButton(2, true);

            parent->keepScreenOn = false;
          }).detach();
        }
      }

    } else if (id == 1) {
      if (ConfirmationDialog::confirm(tr("Are you sure you want to delete all screen recordings?"), tr("Delete All"), this)) {
        std::thread([=]() mutable {
          parent->keepScreenOn = true;

          screenRecordingsBtn->setEnabled(false);
          screenRecordingsBtn->setValue(tr("Deleting..."));

          screenRecordingsBtn->setVisibleButton(0, false);
          screenRecordingsBtn->setVisibleButton(2, false);

          recordingsDir.removeRecursively();
          recordingsDir.mkpath(".");

          screenRecordingsBtn->setValue(tr("Deleted!"));

          util::sleep_for(2500);

          screenRecordingsBtn->setEnabled(true);
          screenRecordingsBtn->setValue("");

          screenRecordingsBtn->setVisibleButton(0, true);
          screenRecordingsBtn->setVisibleButton(2, true);

          parent->keepScreenOn = false;
        }).detach();
      }

    } else if (id == 2) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a recording to rename"), recordingsNames, "", this);
      if (!selection.isEmpty()) {
        QString newName = InputDialog::getText(tr("Enter a new name"), this, tr("Rename Recording")).trimmed().replace(" ", "_");
        if (!newName.isEmpty()) {
          if (recordingsNames.contains(newName)) {
            ConfirmationDialog::alert(tr("A recording with this name already exists. Please choose a different name."), this);
            return;
          }
          std::thread([=]() {
            parent->keepScreenOn = true;

            screenRecordingsBtn->setEnabled(false);
            screenRecordingsBtn->setValue(tr("Renaming..."));

            screenRecordingsBtn->setVisibleButton(0, false);
            screenRecordingsBtn->setVisibleButton(1, false);

            QString newPath = recordingsDir.absoluteFilePath(newName);
            QString oldPath = recordingsDir.absoluteFilePath(selection);
            QFile::rename(oldPath, newPath);

            screenRecordingsBtn->setValue(tr("Renamed!"));

            util::sleep_for(2500);

            screenRecordingsBtn->setEnabled(true);
            screenRecordingsBtn->setValue("");

            screenRecordingsBtn->setVisibleButton(0, true);
            screenRecordingsBtn->setVisibleButton(1, true);

            parent->keepScreenOn = false;
          }).detach();
        }
      }
    }
  });
  addItem(screenRecordingsBtn);

  FrogPilotButtonsControl *frogpilotBackupBtn = new FrogPilotButtonsControl(tr("FrogPilot Backups"), tr("Manage your FrogPilot backups."), "", {tr("BACKUP"), tr("DELETE"), tr("DELETE ALL"), tr("RESTORE")});
  QObject::connect(frogpilotBackupBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
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
      QString nameSelection = InputDialog::getText(tr("Name your backup"), this, "", false, 1).trimmed().replace(" ", "_");
      if (!nameSelection.isEmpty()) {
        if (backupNames.contains(nameSelection)) {
          ConfirmationDialog::alert(tr("A backup with this name already exists. Please choose a different name."), this);
          return;
        }
        bool compressed = FrogPilotConfirmationDialog::yesorno(tr("Do you want to compress this backup? This will take a few minutes, but the final result will be smaller and run in the background."), this);
        std::thread([=]() {
          parent->keepScreenOn = true;

          frogpilotBackupBtn->setEnabled(false);
          frogpilotBackupBtn->setValue(tr("Backing up..."));

          frogpilotBackupBtn->setVisibleButton(1, false);
          frogpilotBackupBtn->setVisibleButton(2, false);
          frogpilotBackupBtn->setVisibleButton(3, false);

          QString fullBackupPath = backupDir.filePath(nameSelection);
          QString inProgressBackupPath = fullBackupPath + "_in_progress";

          QDir().mkpath(inProgressBackupPath);
          std::system(qPrintable("rsync -av /data/openpilot/ " + inProgressBackupPath + "/"));

          if (compressed) {
            frogpilotBackupBtn->setValue(tr("Compressing..."));

            std::system(qPrintable("tar -cf - -C " + inProgressBackupPath + " . | zstd -2 -T0 -o " + fullBackupPath + "_in_progress.tar.zst"));

            QDir(inProgressBackupPath).removeRecursively();

            QString oldTar = fullBackupPath + "_in_progress.tar.zst";
            QString newTar = fullBackupPath + ".tar.zst";
            QFile::rename(oldTar, newTar);
          } else {
            QDir().rename(inProgressBackupPath, fullBackupPath);
          }

          frogpilotBackupBtn->setValue(tr("Backup created!"));

          util::sleep_for(2500);

          frogpilotBackupBtn->setEnabled(true);
          frogpilotBackupBtn->setValue("");

          frogpilotBackupBtn->setVisibleButton(1, true);
          frogpilotBackupBtn->setVisibleButton(2, true);
          frogpilotBackupBtn->setVisibleButton(3, true);

          parent->keepScreenOn = false;
        }).detach();
      }

    } else if (id == 1) {
      QString selectionFriendly = MultiOptionDialog::getSelection(tr("Select a backup to delete"), backupFriendlyMap.keys(), "", this);
      if (!selectionFriendly.isEmpty()) {
        QString selection = backupFriendlyMap.value(selectionFriendly);
        if (ConfirmationDialog::confirm(tr("Are you sure you want to delete this backup?"), tr("Delete"), this)) {
          std::thread([=]() {
            parent->keepScreenOn = true;

            frogpilotBackupBtn->setEnabled(false);
            frogpilotBackupBtn->setValue(tr("Deleting..."));

            frogpilotBackupBtn->setVisibleButton(0, false);
            frogpilotBackupBtn->setVisibleButton(2, false);
            frogpilotBackupBtn->setVisibleButton(3, false);

            QDir dirToDelete(backupDir.filePath(selection));
            if (selection.endsWith(".tar.gz") || selection.endsWith(".tar.zst")) {
              QFile::remove(dirToDelete.absolutePath());
            } else {
              dirToDelete.removeRecursively();
            }

            frogpilotBackupBtn->setValue(tr("Deleted!"));

            util::sleep_for(2500);

            frogpilotBackupBtn->setEnabled(true);
            frogpilotBackupBtn->setValue("");

            frogpilotBackupBtn->setVisibleButton(0, true);
            frogpilotBackupBtn->setVisibleButton(2, true);
            frogpilotBackupBtn->setVisibleButton(3, true);

            parent->keepScreenOn = false;
          }).detach();
        }
      }

    } else if (id == 2) {
      if (ConfirmationDialog::confirm(tr("Are you sure you want to delete all FrogPilot backups?"), tr("Delete All"), this)) {
        std::thread([=]() mutable {
          parent->keepScreenOn = true;

          frogpilotBackupBtn->setEnabled(false);
          frogpilotBackupBtn->setValue(tr("Deleting..."));

          frogpilotBackupBtn->setVisibleButton(0, false);
          frogpilotBackupBtn->setVisibleButton(1, false);
          frogpilotBackupBtn->setVisibleButton(3, false);

          backupDir.removeRecursively();
          backupDir.mkpath(".");

          frogpilotBackupBtn->setValue(tr("Deleted!"));

          util::sleep_for(2500);

          frogpilotBackupBtn->setEnabled(true);
          frogpilotBackupBtn->setValue("");

          frogpilotBackupBtn->setVisibleButton(0, true);
          frogpilotBackupBtn->setVisibleButton(1, true);
          frogpilotBackupBtn->setVisibleButton(3, true);

          parent->keepScreenOn = false;
        }).detach();
      }

    } else if (id == 3) {
      QString selectionFriendly = MultiOptionDialog::getSelection(tr("Select a restore point"), backupFriendlyMap.keys(), "", this);
      if (!selectionFriendly.isEmpty()) {
        QString selection = backupFriendlyMap.value(selectionFriendly);
        if (ConfirmationDialog::confirm(tr("Are you sure you want to restore this version of FrogPilot?"), tr("Restore"), this)) {
          std::thread([=]() {
            parent->keepScreenOn = true;

            frogpilotBackupBtn->setEnabled(false);
            frogpilotBackupBtn->setValue(tr("Restoring..."));

            frogpilotBackupBtn->setVisibleButton(0, false);
            frogpilotBackupBtn->setVisibleButton(1, false);
            frogpilotBackupBtn->setVisibleButton(2, false);

            QString extractDirectory = "/data/restore_temp";
            QString sourcePath = backupDir.filePath(selection);
            QString targetPath = "/data/safe_staging/finalized";

            QDir().mkpath(extractDirectory);

            std::system(qPrintable("tar --strip-components=1 -xzf " + sourcePath + " -C " + extractDirectory));

            if (selection.endsWith(".tar.zst")) {
              frogpilotBackupBtn->setValue(tr("Extracting..."));

              QDir().mkpath(extractDirectory);

              std::system(qPrintable("zstd -d " + sourcePath + " -o " + extractDirectory + "/backup.tar"));
              std::system(qPrintable("tar --strip-components=1 -xf " + extractDirectory + "/backup.tar -C " + extractDirectory));

              QFile::remove(extractDirectory + "/backup.tar");
            }

            QDir().mkpath(targetPath);

            std::system(qPrintable("rsync -av --delete -l " + extractDirectory + "/ " + targetPath + "/"));

            QFile overlayFile(targetPath + "/.overlay_consistent");
            overlayFile.open(QIODevice::WriteOnly);
            overlayFile.close();

            if (QFileInfo::exists(extractDirectory)) {
              QDir(extractDirectory).removeRecursively();
            }

            params.putBool("AutomaticUpdates", false);

            frogpilotBackupBtn->setValue(tr("Restored!"));

            util::sleep_for(2500);

            frogpilotBackupBtn->setValue(tr("Rebooting..."));

            util::sleep_for(2500);

            Hardware::reboot();
          }).detach();
        }
      }
    }
  });
  addItem(frogpilotBackupBtn);

  FrogPilotButtonsControl *toggleBackupBtn = new FrogPilotButtonsControl(tr("Toggle Backups"), tr("Manage your toggle backups."), "", {tr("BACKUP"), tr("DELETE"), tr("DELETE ALL"), tr("RESTORE")});
  QObject::connect(toggleBackupBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
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
      QString nameSelection = InputDialog::getText(tr("Name your toggle backup"), this, "", false, 1).trimmed().replace(" ", "_");
      if (!nameSelection.isEmpty()) {
        if (backupNames.contains(nameSelection)) {
          ConfirmationDialog::alert(tr("A toggle backup with this name already exists. Please choose a different name."), this);
          return;
        }
        std::thread([=]() {
          parent->keepScreenOn = true;

          toggleBackupBtn->setEnabled(false);
          toggleBackupBtn->setValue(tr("Backing up..."));

          toggleBackupBtn->setVisibleButton(1, false);
          toggleBackupBtn->setVisibleButton(2, false);
          toggleBackupBtn->setVisibleButton(3, false);

          QString fullBackupPath = backupDir.filePath(nameSelection);
          QString inProgressBackupPath = fullBackupPath + "_in_progress";

          QDir().mkpath(inProgressBackupPath);

          std::system(qPrintable("rsync -av /data/params/d/ " + inProgressBackupPath + "/"));

          QDir().rename(inProgressBackupPath, fullBackupPath);

          toggleBackupBtn->setValue(tr("Backup created!"));

          util::sleep_for(2500);

          toggleBackupBtn->setEnabled(true);
          toggleBackupBtn->setValue("");

          toggleBackupBtn->setVisibleButton(1, true);
          toggleBackupBtn->setVisibleButton(2, true);
          toggleBackupBtn->setVisibleButton(3, true);

          parent->keepScreenOn = false;
        }).detach();
      }

    } else if (id == 1) {
      QString selectionFriendly = MultiOptionDialog::getSelection(tr("Select a toggle backup to delete"), backupFriendlyMap.keys(), "", this);
      if (!selectionFriendly.isEmpty()) {
        QString selection = backupFriendlyMap.value(selectionFriendly);
        if (ConfirmationDialog::confirm(tr("Are you sure you want to delete this toggle backup?"), tr("Delete"), this)) {
          std::thread([=]() {
            parent->keepScreenOn = true;

            toggleBackupBtn->setEnabled(false);
            toggleBackupBtn->setValue(tr("Deleting..."));

            toggleBackupBtn->setVisibleButton(0, false);
            toggleBackupBtn->setVisibleButton(2, false);
            toggleBackupBtn->setVisibleButton(3, false);

            QDir dirToDelete(backupDir.filePath(selection));
            dirToDelete.removeRecursively();

            toggleBackupBtn->setValue(tr("Deleted!"));

            util::sleep_for(2500);

            toggleBackupBtn->setEnabled(true);
            toggleBackupBtn->setValue("");

            toggleBackupBtn->setVisibleButton(0, true);
            toggleBackupBtn->setVisibleButton(2, true);
            toggleBackupBtn->setVisibleButton(3, true);

            parent->keepScreenOn = false;
          }).detach();
        }
      }

    } else if (id == 2) {
      if (ConfirmationDialog::confirm(tr("Are you sure you want to delete all toggle backups?"), tr("Delete All"), this)) {
        std::thread([=]() mutable {
          parent->keepScreenOn = true;

          toggleBackupBtn->setEnabled(false);
          toggleBackupBtn->setValue(tr("Deleting..."));

          toggleBackupBtn->setVisibleButton(0, false);
          toggleBackupBtn->setVisibleButton(1, false);
          toggleBackupBtn->setVisibleButton(3, false);

          backupDir.removeRecursively();
          backupDir.mkpath(".");

          toggleBackupBtn->setValue(tr("Deleted!"));

          util::sleep_for(2500);

          toggleBackupBtn->setEnabled(true);
          toggleBackupBtn->setValue("");

          toggleBackupBtn->setVisibleButton(0, true);
          toggleBackupBtn->setVisibleButton(1, true);
          toggleBackupBtn->setVisibleButton(3, true);

          parent->keepScreenOn = false;
        }).detach();
      }

    } else if (id == 3) {
      QString selectionFriendly = MultiOptionDialog::getSelection(tr("Select a toggle restore point"), backupFriendlyMap.keys(), "", this);
      if (!selectionFriendly.isEmpty()) {
        QString selection = backupFriendlyMap.value(selectionFriendly);
        if (ConfirmationDialog::confirm(tr("Are you sure you want to restore this toggle backup?"), tr("Restore"), this)) {
          std::thread([=]() {
            parent->keepScreenOn = true;

            toggleBackupBtn->setEnabled(false);
            toggleBackupBtn->setValue(tr("Restoring..."));

            toggleBackupBtn->setVisibleButton(0, false);
            toggleBackupBtn->setVisibleButton(1, false);
            toggleBackupBtn->setVisibleButton(2, false);

            QString sourcePath = backupDir.filePath(selection);
            QString targetPath = "/data/params/d";

            QDir().mkpath(targetPath);

            std::system(qPrintable("rsync -av -l " + sourcePath + "/ " + targetPath + "/"));

            updateFrogPilotToggles();

            toggleBackupBtn->setValue(tr("Restored!"));

            util::sleep_for(2500);

            toggleBackupBtn->setEnabled(true);
            toggleBackupBtn->setValue("");

            toggleBackupBtn->setVisibleButton(0, true);
            toggleBackupBtn->setVisibleButton(1, true);
            toggleBackupBtn->setVisibleButton(2, true);

            parent->keepScreenOn = false;
          }).detach();
        }
      }
    }
  });
  addItem(toggleBackupBtn);
}
