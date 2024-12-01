#include <filesystem>

#include "selfdrive/frogpilot/ui/qt/offroad/data_settings.h"

FrogPilotDataPanel::FrogPilotDataPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent) {
  ButtonControl *deleteDrivingDataBtn = new ButtonControl(tr("Delete Driving Footage and Data"), tr("DELETE"), tr("Permanently deletes all stored driving footage and data from your device. Ideal for maintaining privacy or freeing up space."));
  QObject::connect(deleteDrivingDataBtn, &ButtonControl::clicked, [=]() {
    QDir realdataDir("/data/media/0/realdata");

    if (ConfirmationDialog::confirm(tr("Are you sure you want to permanently delete all of your driving footage and data?"), tr("Delete"), this)) {
      std::thread([=]() mutable {
        deleteDrivingDataBtn->setEnabled(false);

        deleteDrivingDataBtn->setValue(tr("Deleting..."));

        realdataDir.removeRecursively();
        realdataDir.mkpath(".");

        deleteDrivingDataBtn->setValue(tr("Deleted!"));

        util::sleep_for(2500);

        deleteDrivingDataBtn->setEnabled(true);

        deleteDrivingDataBtn->setValue("");
      }).detach();
    }
  });
  addItem(deleteDrivingDataBtn);

  FrogPilotButtonsControl *screenRecordingsBtn = new FrogPilotButtonsControl(tr("Screen Recordings"), tr("Manage your screen recordings."), {tr("DELETE"), tr("DELETE ALL"), tr("RENAME")});
  QObject::connect(screenRecordingsBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir recordingsDir("/data/media/screen_recordings");
    QStringList recordingsNames = recordingsDir.entryList(QDir::Files | QDir::NoDotAndDotDot);

    if (id == 0) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a recording to delete"), recordingsNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Are you sure you want to delete this recording?"), tr("Delete"), this)) {
          std::thread([=]() {
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
          }).detach();
        }
      }

    } else if (id == 1) {
      if (ConfirmationDialog::confirm(tr("Are you sure you want to delete all screen recordings?"), tr("Delete All"), this)) {
        std::thread([=]() mutable {
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
          }).detach();
        }
      }
    }
  });
  addItem(screenRecordingsBtn);

  FrogPilotButtonsControl *frogpilotBackupBtn = new FrogPilotButtonsControl(tr("FrogPilot Backups"), tr("Manage your FrogPilot backups."), {tr("BACKUP"), tr("DELETE"), tr("DELETE ALL"), tr("RESTORE")});
  QObject::connect(frogpilotBackupBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir backupDir("/data/backups");
    QStringList backupNames = backupDir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name).filter(QRegularExpression("^(?!.*_in_progress$).*$"));

    if (id == 0) {
      QString nameSelection = InputDialog::getText(tr("Name your backup"), this, "", false, 1).trimmed().replace(" ", "_");
      if (!nameSelection.isEmpty()) {
        if (backupNames.contains(nameSelection)) {
          ConfirmationDialog::alert(tr("A backup with this name already exists. Please choose a different name."), this);
          return;
        }
        bool compressed = FrogPilotConfirmationDialog::yesorno(tr("Do you want to compress this backup? The final result will be 2.25x smaller and will run in the background, but can take 10+ minutes."), this);
        std::thread([=]() {
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

            std::system(("tar -czf " + fullBackupPath + "_in_progress.tar.gz -C " + inProgressBackupPath + " .").c_str());
            std::filesystem::remove_all(inProgressBackupPath);
            std::filesystem::rename(fullBackupPath + "_in_progress.tar.gz", fullBackupPath + ".tar.gz");
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
        }).detach();
      }

    } else if (id == 1) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a backup to delete"), backupNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Are you sure you want to delete this backup?"), tr("Delete"), this)) {
          std::thread([=]() {
            frogpilotBackupBtn->setVisibleButton(0, false);
            frogpilotBackupBtn->setVisibleButton(2, false);
            frogpilotBackupBtn->setVisibleButton(3, false);

            frogpilotBackupBtn->setValue(tr("Deleting..."));

            QDir dirToDelete(backupDir.filePath(selection));
            if (selection.endsWith(".tar.gz")) {
              QFile::remove(dirToDelete.absolutePath());
            } else {
              dirToDelete.removeRecursively();
            }

            util::sleep_for(2500);

            frogpilotBackupBtn->setValue("");

            frogpilotBackupBtn->setEnabled(true);

            frogpilotBackupBtn->setVisibleButton(0, true);
            frogpilotBackupBtn->setVisibleButton(2, true);
            frogpilotBackupBtn->setVisibleButton(3, true);
          }).detach();
        }
      }

    } else if (id == 2) {
      if (ConfirmationDialog::confirm(tr("Are you sure you want to delete all FrogPilot backups?"), tr("Delete All"), this)) {
        std::thread([=]() mutable {
          frogpilotBackupBtn->setEnabled(false);

          frogpilotBackupBtn->setVisibleButton(0, false);
          frogpilotBackupBtn->setVisibleButton(1, false);
          frogpilotBackupBtn->setVisibleButton(3, false);

          frogpilotBackupBtn->setValue(tr("Deleting..."));

          backupDir.removeRecursively();
          backupDir.mkpath(".");

          frogpilotBackupBtn->setValue(tr("Deleted!"));

          util::sleep_for(2500);

          frogpilotBackupBtn->setValue("");

          frogpilotBackupBtn->setEnabled(true);

          frogpilotBackupBtn->setVisibleButton(0, true);
          frogpilotBackupBtn->setVisibleButton(1, true);
          frogpilotBackupBtn->setVisibleButton(3, true);
        }).detach();
      }

    } else if (id == 3) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a restore point"), backupNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Are you sure you want to restore this version of FrogPilot?"), tr("Restore"), this)) {
          std::thread([=]() {
            frogpilotBackupBtn->setEnabled(false);

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
            }

            frogpilotBackupBtn->setValue(tr("Restoring..."));

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

  FrogPilotButtonsControl *toggleBackupBtn = new FrogPilotButtonsControl(tr("Toggle Backups"), tr("Manage your toggle backups."), {tr("BACKUP"), tr("DELETE"), tr("DELETE ALL"), tr("RESTORE")});
  QObject::connect(toggleBackupBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir backupDir("/data/toggle_backups");
    QStringList backupNames = backupDir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name).filter(QRegularExpression("^(?!.*_in_progress$).*$"));

    if (id == 0) {
      QString nameSelection = InputDialog::getText(tr("Name your backup"), this, "", false, 1).trimmed().replace(" ", "_");
      if (!nameSelection.isEmpty()) {
        if (backupNames.contains(nameSelection)) {
          ConfirmationDialog::alert(tr("A backup with this name already exists. Please choose a different name."), this);
          return;
        }
        std::thread([=]() {
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
        }).detach();
      }

    } else if (id == 1) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a backup to delete"), backupNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Are you sure you want to delete this backup?"), tr("Delete"), this)) {
          std::thread([=]() {
            toggleBackupBtn->setVisibleButton(0, false);
            toggleBackupBtn->setVisibleButton(2, false);
            toggleBackupBtn->setVisibleButton(3, false);

            toggleBackupBtn->setValue(tr("Deleting..."));

            QDir dirToDelete(backupDir.filePath(selection));
            dirToDelete.removeRecursively();

            util::sleep_for(2500);

            toggleBackupBtn->setValue("");

            toggleBackupBtn->setEnabled(true);

            toggleBackupBtn->setVisibleButton(0, true);
            toggleBackupBtn->setVisibleButton(2, true);
            toggleBackupBtn->setVisibleButton(3, true);
          }).detach();
        }
      }

    } else if (id == 2) {
      if (ConfirmationDialog::confirm(tr("Are you sure you want to delete all toggle backups?"), tr("Delete All"), this)) {
        std::thread([=]() mutable {
          toggleBackupBtn->setEnabled(false);

          toggleBackupBtn->setVisibleButton(0, false);
          toggleBackupBtn->setVisibleButton(1, false);
          toggleBackupBtn->setVisibleButton(3, false);

          toggleBackupBtn->setValue(tr("Deleting..."));

          backupDir.removeRecursively();
          backupDir.mkpath(".");

          toggleBackupBtn->setValue(tr("Deleted!"));

          util::sleep_for(2500);

          toggleBackupBtn->setValue("");

          toggleBackupBtn->setEnabled(true);

          toggleBackupBtn->setVisibleButton(0, true);
          toggleBackupBtn->setVisibleButton(1, true);
          toggleBackupBtn->setVisibleButton(3, true);
        }).detach();
      }

    } else if (id == 3) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a restore point"), backupNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Are you sure you want to restore this toggle backup?"), tr("Restore"), this)) {
          std::thread([=]() {
            toggleBackupBtn->setEnabled(false);

            toggleBackupBtn->setVisibleButton(0, false);
            toggleBackupBtn->setVisibleButton(1, false);
            toggleBackupBtn->setVisibleButton(2, false);

            toggleBackupBtn->setValue(tr("Restoring..."));

            std::string sourcePath = backupDir.filePath(selection).toStdString();
            std::string targetPath = "/data/params/d";

            std::filesystem::create_directories(targetPath);

            std::system(("rsync -av -l " + sourcePath + "/ " + targetPath + "/").c_str());

            updateFrogPilotToggles();

            toggleBackupBtn->setValue(tr("Restored!"));

            util::sleep_for(2500);

            toggleBackupBtn->setValue("");

            toggleBackupBtn->setEnabled(true);

            toggleBackupBtn->setVisibleButton(0, true);
            toggleBackupBtn->setVisibleButton(1, true);
            toggleBackupBtn->setVisibleButton(2, true);
          }).detach();
        }
      }
    }
  });
  addItem(toggleBackupBtn);
}
