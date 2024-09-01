#include <filesystem>

#include "selfdrive/frogpilot/ui/qt/offroad/data_settings.h"

FrogPilotDataPanel::FrogPilotDataPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  ButtonControl *deleteDrivingDataBtn = new ButtonControl(tr("Delete Driving Footage and Data"), tr("DELETE"), tr("This button provides a swift and secure way to permanently delete all stored driving footage and data from your device. Ideal for maintaining privacy or freeing up space."));
  QObject::connect(deleteDrivingDataBtn, &ButtonControl::clicked, [=]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to permanently delete all of your driving footage and data?"), tr("Delete"), this)) {
      std::thread([=] {
        deleteDrivingDataBtn->setEnabled(false);
        deleteDrivingDataBtn->setValue(tr("Deleting..."));

        std::system("rm -rf /data/media/0/realdata");

        deleteDrivingDataBtn->setValue(tr("Deleted!"));

        util::sleep_for(2000);
        deleteDrivingDataBtn->setValue("");
        deleteDrivingDataBtn->setEnabled(true);
      }).detach();
    }
  });
  addItem(deleteDrivingDataBtn);

  FrogPilotButtonsControl *screenRecordingsBtn = new FrogPilotButtonsControl(tr("Screen Recordings"), tr("Manage your screen recordings."), {tr("DELETE"), tr("RENAME")});
  QObject::connect(screenRecordingsBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir recordingsDir("/data/media/0/videos");
    QStringList recordingsNames = recordingsDir.entryList(QDir::Files | QDir::NoDotAndDotDot);

    if (id == 0) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a recording to delete"), recordingsNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Are you sure you want to delete this recording?"), tr("Delete"), this)) {
          std::thread([=]() {
            screenRecordingsBtn->setEnabled(false);
            screenRecordingsBtn->setValue(tr("Deleting..."));

            QFile fileToDelete(recordingsDir.absoluteFilePath(selection));
            if (fileToDelete.remove()) {
              screenRecordingsBtn->setValue(tr("Deleted!"));
            } else {
              screenRecordingsBtn->setValue(tr("Failed..."));
            }

            util::sleep_for(2000);
            screenRecordingsBtn->setValue("");
            screenRecordingsBtn->setEnabled(true);
          }).detach();
        }
      }

    } else if (id == 1) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a recording to rename"), recordingsNames, "", this);
      if (!selection.isEmpty()) {
        QString newName = InputDialog::getText(tr("Enter a new name"), this, tr("Rename Recording"));
        if (!newName.isEmpty()) {
          std::thread([=]() {
            screenRecordingsBtn->setEnabled(false);
            screenRecordingsBtn->setValue(tr("Renaming..."));

            QString oldPath = recordingsDir.absoluteFilePath(selection);
            QString newPath = recordingsDir.absoluteFilePath(newName);
            if (QFile::rename(oldPath, newPath)) {
              screenRecordingsBtn->setValue(tr("Renamed!"));
            } else {
              screenRecordingsBtn->setValue(tr("Failed..."));
            }

            util::sleep_for(2000);
            screenRecordingsBtn->setValue("");
            screenRecordingsBtn->setEnabled(true);
          }).detach();
        }
      }
    }
  });
  addItem(screenRecordingsBtn);

  FrogPilotButtonsControl *frogpilotBackupBtn = new FrogPilotButtonsControl(tr("FrogPilot Backups"), tr("Manage your FrogPilot backups."), {tr("BACKUP"), tr("DELETE"), tr("RESTORE")});
  QObject::connect(frogpilotBackupBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir backupDir("/data/backups");
    QStringList backupNames = backupDir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name).filter(QRegularExpression("^(?!.*_in_progress$).*$"));

    if (id == 0) {
      QString nameSelection = InputDialog::getText(tr("Name your backup"), this, "", false, 1);
      if (!nameSelection.isEmpty()) {
        bool compressed = FrogPilotConfirmationDialog::yesorno(tr("Do you want to compress this backup? The end file size will be 2.25x smaller, but can take 10+ minutes."), this);
        std::thread([=]() {
          device()->resetInteractiveTimeout(300);

          frogpilotBackupBtn->setEnabled(false);
          frogpilotBackupBtn->setValue(tr("Backing..."));

          std::string fullBackupPath = backupDir.absolutePath().toStdString() + "/" + nameSelection.toStdString();
          std::string inProgressBackupPath = fullBackupPath + "_in_progress";
          int result = std::system(("mkdir -p " + inProgressBackupPath + " && rsync -av /data/openpilot/ " + inProgressBackupPath + "/").c_str());

          if (result == 0 && compressed) {
            frogpilotBackupBtn->setValue(tr("Compressing..."));
            result = std::system(("tar -czf " + fullBackupPath + "_in_progress.tar.gz -C " + inProgressBackupPath + " . && rm -rf " + inProgressBackupPath).c_str());
            if (result == 0) {
              result = std::system(("mv " + fullBackupPath + "_in_progress.tar.gz " + fullBackupPath + ".tar.gz").c_str());
            }
          } else if (result == 0) {
            result = std::system(("mv " + inProgressBackupPath + " " + fullBackupPath).c_str());
          }

          if (result == 0) {
            frogpilotBackupBtn->setValue(tr("Success!"));
          } else {
            frogpilotBackupBtn->setValue(tr("Failed..."));
            std::system(("rm -rf " + inProgressBackupPath).c_str());
          }

          util::sleep_for(2000);
          frogpilotBackupBtn->setValue("");
          frogpilotBackupBtn->setEnabled(true);

          device()->resetInteractiveTimeout(30);
        }).detach();
      }

    } else if (id == 1) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a backup to delete"), backupNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Are you sure you want to delete this backup?"), tr("Delete"), this)) {
          std::thread([=]() {
            frogpilotBackupBtn->setEnabled(false);
            frogpilotBackupBtn->setValue(tr("Deleting..."));

            QDir dirToDelete(backupDir.absoluteFilePath(selection));
            if (selection.endsWith(".tar.gz")) {
              if (QFile::remove(dirToDelete.absolutePath())) {
                frogpilotBackupBtn->setValue(tr("Deleted!"));
              } else {
                frogpilotBackupBtn->setValue(tr("Failed..."));
              }
            } else {
              if (dirToDelete.removeRecursively()) {
                frogpilotBackupBtn->setValue(tr("Deleted!"));
              } else {
                frogpilotBackupBtn->setValue(tr("Failed..."));
              }
            }

            util::sleep_for(2000);
            frogpilotBackupBtn->setValue("");
            frogpilotBackupBtn->setEnabled(true);
          }).detach();
        }
      }

    } else if (id == 2) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a restore point"), backupNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Are you sure you want to restore this version of FrogPilot?"), tr("Restore"), this)) {
          std::thread([=]() {
            device()->resetInteractiveTimeout(300);

            frogpilotBackupBtn->setEnabled(false);
            frogpilotBackupBtn->setValue(tr("Restoring..."));

            std::string sourcePath = backupDir.absolutePath().toStdString() + "/" + selection.toStdString();
            std::string targetPath = "/data/safe_staging/finalized";
            std::string extractDirectory = "/data/restore_temp";

            if (selection.endsWith(".tar.gz")) {
              frogpilotBackupBtn->setValue(tr("Extracting..."));

              if (std::system(("mkdir -p " + extractDirectory).c_str()) != 0 || std::system(("tar --strip-components=1 -xzf " + sourcePath + " -C " + extractDirectory).c_str()) != 0) {
                frogpilotBackupBtn->setValue(tr("Failed..."));
                util::sleep_for(2000);
                frogpilotBackupBtn->setValue("");
                frogpilotBackupBtn->setEnabled(true);
                return;
              }

              sourcePath = extractDirectory;
              frogpilotBackupBtn->setValue(tr("Restoring..."));
            }

            if (std::system(("rsync -av --delete -l --exclude='.overlay_consistent' " + sourcePath + "/ " + targetPath + "/").c_str()) == 0) {
              std::ofstream consistentFile(targetPath + "/.overlay_consistent");
              if (consistentFile) {
                frogpilotBackupBtn->setValue(tr("Restored!"));
                params.putBool("AutomaticUpdates", false);
                util::sleep_for(2000);

                frogpilotBackupBtn->setValue(tr("Rebooting..."));
                consistentFile.close();
                std::filesystem::remove_all(extractDirectory);

                util::sleep_for(2000);
                Hardware::reboot();
              } else {
                frogpilotBackupBtn->setValue(tr("Failed..."));
                util::sleep_for(2000);
                frogpilotBackupBtn->setValue("");
                frogpilotBackupBtn->setEnabled(true);

                device()->resetInteractiveTimeout(30);
              }
            } else {
              frogpilotBackupBtn->setValue(tr("Failed..."));
              util::sleep_for(2000);
              frogpilotBackupBtn->setValue("");
              frogpilotBackupBtn->setEnabled(true);

              device()->resetInteractiveTimeout(30);
            }
          }).detach();
        }
      }
    }
  });
  addItem(frogpilotBackupBtn);

  FrogPilotButtonsControl *toggleBackupBtn = new FrogPilotButtonsControl(tr("Toggle Backups"), tr("Manage your toggle backups."), {tr("BACKUP"), tr("DELETE"), tr("RESTORE")});
  QObject::connect(toggleBackupBtn, &FrogPilotButtonsControl::buttonClicked, [=](int id) {
    QDir backupDir("/data/toggle_backups");
    QStringList backupNames = backupDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    if (id == 0) {
      QString nameSelection = InputDialog::getText(tr("Name your backup"), this, "", false, 1);
      if (!nameSelection.isEmpty()) {
        std::thread([=]() {
          toggleBackupBtn->setEnabled(false);
          toggleBackupBtn->setValue(tr("Backing..."));

          std::string command = "mkdir -p " + backupDir.absolutePath().toStdString() + "/" + nameSelection.toStdString() + " && rsync -av /data/params/d/ " + backupDir.absolutePath().toStdString() + "/" + nameSelection.toStdString();
          int result = std::system(command.c_str());

          if (result == 0) {
            toggleBackupBtn->setValue(tr("Success!"));
          } else {
            toggleBackupBtn->setValue(tr("Failed..."));
          }

          util::sleep_for(2000);
          toggleBackupBtn->setValue("");
          toggleBackupBtn->setEnabled(true);
        }).detach();
      }

    } else if (id == 1) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a backup to delete"), backupNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Are you sure you want to delete this backup?"), tr("Delete"), this)) {
          std::thread([=]() {
            toggleBackupBtn->setEnabled(false);
            toggleBackupBtn->setValue(tr("Deleting..."));

            QDir dirToDelete(backupDir.absoluteFilePath(selection));
            if (dirToDelete.removeRecursively()) {
              toggleBackupBtn->setValue(tr("Deleted!"));
            } else {
              toggleBackupBtn->setValue(tr("Failed..."));
            }

            util::sleep_for(2000);
            toggleBackupBtn->setValue("");
            toggleBackupBtn->setEnabled(true);
          }).detach();
        }
      }

    } else if (id == 2) {
      QString selection = MultiOptionDialog::getSelection(tr("Select a restore point"), backupNames, "", this);
      if (!selection.isEmpty()) {
        if (ConfirmationDialog::confirm(tr("Are you sure you want to restore this toggle backup?"), tr("Restore"), this)) {
          std::thread([=]() {
            toggleBackupBtn->setEnabled(false);
            toggleBackupBtn->setValue(tr("Restoring..."));

            std::string targetPath = "/data/params/d/";
            std::string tempBackupPath = "/data/params/d_backup/";

            int backupResult = std::system(("rsync -av --delete -l " + targetPath + " " + tempBackupPath).c_str());

            if (backupResult == 0) {
              toggleBackupBtn->setValue(tr("Restoring..."));

              std::string restoreCommand = "rsync -av --delete -l " + backupDir.absolutePath().toStdString() + "/" + selection.toStdString() + "/ " + targetPath;
              int restoreResult = std::system(restoreCommand.c_str());

              if (restoreResult == 0) {
                toggleBackupBtn->setValue(tr("Success!"));
              } else {
                toggleBackupBtn->setValue(tr("Failed..."));

                std::system(("rsync -av --delete -l " + tempBackupPath + " " + targetPath).c_str());
              }

              std::system(("rm -rf " + tempBackupPath).c_str());
            } else {
              toggleBackupBtn->setValue(tr("Failed..."));
            }

            util::sleep_for(2000);
            toggleBackupBtn->setValue("");
            toggleBackupBtn->setEnabled(true);
          }).detach();
        }
      }
    }
  });
  addItem(toggleBackupBtn);
}
