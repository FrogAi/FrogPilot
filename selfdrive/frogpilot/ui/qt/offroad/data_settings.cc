#include <filesystem>

#include "selfdrive/frogpilot/ui/qt/offroad/data_settings.h"

FrogPilotDataPanel::FrogPilotDataPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  ButtonControl *deleteDrivingDataBtn = new ButtonControl(tr("Delete Driving Footage and Data"), tr("DELETE"), tr("Permanently deletes all stored driving footage and data from your device. Ideal for maintaining privacy or freeing up space."));
  QObject::connect(deleteDrivingDataBtn, &ButtonControl::clicked, [=]() {
    QDir realdataDir(QString::fromStdString(Path::log_root()));

    if (ConfirmationDialog::confirm(tr("Are you sure you want to permanently delete all of your driving footage and data?"), tr("Delete"), this)) {
      std::thread([=]() mutable {
        parent->keepScreenOn = true;

        deleteDrivingDataBtn->setEnabled(false);
        deleteDrivingDataBtn->setValue(tr("Deleting..."));

        realdataDir.removeRecursively();
        util::create_directories(Path::log_root(), 0775);

        deleteDrivingDataBtn->setValue(tr("Deleted!"));
        util::sleep_for(2500);
        deleteDrivingDataBtn->setEnabled(true);
        deleteDrivingDataBtn->setValue("");

        parent->keepScreenOn = false;
      }).detach();
    }
  });
  addItem(deleteDrivingDataBtn);

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

    QMap<QString, QString> backupFriendlyMap;
    QRegularExpression autoRegex("^(.*)_(\\d{4}-\\d{2}-\\d{2})_auto(?:\\..*)?$");
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
        bool compressed = FrogPilotConfirmationDialog::yesorno(tr("Do you want to compress this backup? The final result will be significantly smaller and will run in the background."), this);
        std::thread([=]() {
          parent->keepScreenOn = true;

          frogpilotBackupBtn->setEnabled(false);
          frogpilotBackupBtn->setValue(tr("Backing up..."));

          frogpilotBackupBtn->setVisibleButton(1, false);
          frogpilotBackupBtn->setVisibleButton(2, false);
          frogpilotBackupBtn->setVisibleButton(3, false);

          std::string fullBackupPath = backupDir.filePath(nameSelection).toStdString();
          std::string inProgressBackupPath = fullBackupPath + "_in_progress";

          std::filesystem::create_directories(inProgressBackupPath);
          std::system(("rsync -av /data/openpilot/ " + inProgressBackupPath + "/").c_str());

          if (compressed) {
            frogpilotBackupBtn->setValue(tr("Compressing..."));

            std::system(("tar -cf - -C " + inProgressBackupPath + " . | zstd -2 -T0 -o " + fullBackupPath + "_in_progress.tar.zst").c_str());
            std::filesystem::remove_all(inProgressBackupPath);
            std::filesystem::rename(fullBackupPath + "_in_progress.tar.zst", fullBackupPath + ".tar.zst");
          } else {
            std::filesystem::rename(inProgressBackupPath, fullBackupPath);
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

            std::string extractDirectory = "/data/restore_temp";
            std::string sourcePath = backupDir.filePath(selection).toStdString();
            std::string targetPath = "/data/safe_staging/finalized";

            if (selection.endsWith(".tar.gz")) {
              frogpilotBackupBtn->setValue(tr("Extracting..."));

              std::filesystem::create_directories(extractDirectory);
              std::system(("tar --strip-components=1 -xzf " + sourcePath + " -C " + extractDirectory).c_str());

              sourcePath = extractDirectory;
            } else if (selection.endsWith(".tar.zst")) {
              frogpilotBackupBtn->setValue(tr("Extracting..."));

              std::filesystem::create_directories(extractDirectory);
              std::system(("zstd -d " + sourcePath + " -o " + extractDirectory + "/backup.tar").c_str());
              std::system(("tar --strip-components=1 -xf " + extractDirectory + "/backup.tar -C " + extractDirectory).c_str());
              std::filesystem::remove(extractDirectory + "/backup.tar");

              sourcePath = extractDirectory;
            }

            std::filesystem::create_directories(targetPath);
            std::system(("rsync -av --delete -l " + sourcePath + "/ " + targetPath + "/").c_str());

            std::filesystem::path overlayFile = targetPath + "/.overlay_consistent";
            std::ofstream(overlayFile).close();

            if (std::filesystem::exists(extractDirectory)) {
              std::filesystem::remove_all(extractDirectory);
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

    QMap<QString, QString> backupFriendlyMap;
    QRegularExpression autoRegex("^(\\d{4}-\\d{2}-\\d{2})_(\\d{1,2}-\\d{2}[ap]m)_auto(?:\\..*)?$");
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
        std::thread([=]() {
          parent->keepScreenOn = true;

          toggleBackupBtn->setEnabled(false);
          toggleBackupBtn->setValue(tr("Backing up..."));

          toggleBackupBtn->setVisibleButton(1, false);
          toggleBackupBtn->setVisibleButton(2, false);
          toggleBackupBtn->setVisibleButton(3, false);

          std::string fullBackupPath = backupDir.filePath(nameSelection).toStdString();
          std::string inProgressBackupPath = fullBackupPath + "_in_progress";

          std::filesystem::create_directories(inProgressBackupPath);

          std::system(("rsync -av /data/params/d/ " + inProgressBackupPath + "/").c_str());

          std::filesystem::rename(inProgressBackupPath, fullBackupPath);

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
      QString selectionFriendly = MultiOptionDialog::getSelection(tr("Select a backup to delete"), backupFriendlyMap.keys(), "", this);
      if (!selectionFriendly.isEmpty()) {
        QString selection = backupFriendlyMap.value(selectionFriendly);
        if (ConfirmationDialog::confirm(tr("Are you sure you want to delete this backup?"), tr("Delete"), this)) {
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
      QString selectionFriendly = MultiOptionDialog::getSelection(tr("Select a restore point"), backupFriendlyMap.keys(), "", this);
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

            std::string sourcePath = backupDir.filePath(selection).toStdString();
            std::string targetPath = "/data/params/d";

            std::filesystem::create_directories(targetPath);

            std::system(("rsync -av -l " + sourcePath + "/ " + targetPath + "/").c_str());

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
