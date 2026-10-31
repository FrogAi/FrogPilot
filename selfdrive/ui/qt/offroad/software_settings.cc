#include "selfdrive/ui/qt/offroad/settings.h"

#include <cassert>
#include <cmath>
#include <string>

#include <QDebug>
#include <QLabel>

#include "common/params.h"
#include "common/util.h"
#include "selfdrive/ui/ui.h"
#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/widgets/controls.h"
#include "selfdrive/ui/qt/widgets/input.h"
#include "system/hardware/hw.h"

#include "frogpilot/ui/qt/offroad/telemetry_page.h"


void SoftwarePanel::checkForUpdates() {
  std::system("pkill -SIGUSR1 -f system.updated.updated");
}

SoftwarePanel::SoftwarePanel(QWidget* parent) : ListWidget(parent) {
  onroadLbl = new QLabel(tr("Updates are only downloaded while the car is off or in park."));
  onroadLbl->setStyleSheet("font-size: 50px; font-weight: 400; text-align: left; padding-top: 30px; padding-bottom: 30px;");
  addItem(onroadLbl);

  // current version
  versionLbl = new LabelControl(tr("Current Version"), "");
  addItem(versionLbl);

  // FrogPilot variables
  // automatic updates toggle
  ParamControl *automaticUpdatesToggle = new ParamControl("AutomaticUpdates", tr("Automatically Update FrogPilot"),
                                                       tr("Automatically update FrogPilot when the car is off and the device is on an unmetered connection, such as Wi-Fi. Automatic updates turn off after a FrogPilot backup is restored until you update manually."), "");
  automaticUpdatesToggle->setVisible(params.getBool("IsReleaseBranch") || frogsGoMoo());
  addItem(automaticUpdatesToggle);

  // telemetry toggle
  ParamControl *frogpilotTelemetryToggle = new ParamControl(
    "FrogPilotTelemetry",
    tr("Share FrogPilot Data"),
    FrogPilotTelemetryPage::description(),
    ""
  );
  frogpilotTelemetryToggle->setConfirmation(true, false);
  QObject::connect(frogpilotTelemetryToggle, &ToggleControl::toggleFlipped, [this](bool state) {
    if (state && params.getBool("FrogPilotTelemetry")) {
      params.putBool("FrogPilotTelemetryConfirmed", true);
    }
  });
  addItem(frogpilotTelemetryToggle);

  // download update btn
  downloadBtn = new ButtonControl(tr("Download"), tr("CHECK"));
  connect(downloadBtn, &ButtonControl::clicked, [=]() {
    downloadBtn->setEnabled(false);
    if (downloadBtn->text() == tr("CHECK")) {
      checkForUpdates();
    } else {
      std::system("pkill -SIGHUP -f system.updated.updated");
    }

    // FrogPilot variables
    frogpilotUIState()->runUpdateChecks();
  });
  addItem(downloadBtn);

  // install update btn
  installBtn = new ButtonControl(tr("Install Update"), tr("INSTALL"));
  connect(installBtn, &ButtonControl::clicked, [=]() {
    installBtn->setEnabled(false);
    params.putBool("DoReboot", true);
  });
  addItem(installBtn);

  // branch selecting
  targetBranchBtn = new ButtonControl(tr("Target Branch"), tr("SELECT"));
  connect(targetBranchBtn, &ButtonControl::clicked, [=]() {
    auto current = params.get("GitBranch");
    QStringList branches = QString::fromStdString(params.get("UpdaterAvailableBranches")).split(",");

    // FrogPilot variables
    if (!frogsGoMoo()) {
      for (int i = branches.size() - 1; i >= 0; --i) {
        if (branches[i].startsWith("FrogPilot-Development", Qt::CaseInsensitive)) {
          branches.removeAt(i);
        }
      }
      branches.removeAll("FrogPilot-Vetting");
      branches.removeAll("MAKE-PRS-HERE");
    }

    for (QString b : {current.c_str(), "devel-staging", "devel", "nightly", "master-ci", "master"}) {
      auto i = branches.indexOf(b);
      if (i >= 0) {
        branches.removeAt(i);
        branches.insert(0, b);
      }
    }

    QString cur = QString::fromStdString(params.get("UpdaterTargetBranch"));
    QString selection = MultiOptionDialog::getSelection(tr("Select a branch"), branches, cur, this);
    if (!selection.isEmpty()) {
      params.put("UpdaterTargetBranch", selection.toStdString());
      targetBranchBtn->setValue(QString::fromStdString(params.get("UpdaterTargetBranch")));
      checkForUpdates();

      // FrogPilot variables
      if (selection.toStdString() != current) {
        if (FrogPilotConfirmationDialog::yesorno(tr("This branch must be downloaded before switching. Would you like to download it now?"), this)) {
          std::system("pkill -SIGHUP -f system.updated.updated");
          frogpilotUIState()->runUpdateChecks();
        }
      }
    }
  });
  addItem(targetBranchBtn);

  // uninstall button
  auto uninstallBtn = new ButtonControl(tr("Uninstall %1").arg(getBrand()), tr("UNINSTALL"));
  connect(uninstallBtn, &ButtonControl::clicked, [&]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to uninstall?"), tr("Uninstall"), this)) {
      // FrogPilot variables
      if (FrogPilotConfirmationDialog::yesorno(tr("Do you want to perform a full factory reset? All saved assets and settings will be permanently deleted!"), this)) {
        if (FrogPilotConfirmationDialog::yesorno(tr("This is a complete factory reset and cannot be undone. Are you absolutely sure you want to continue?"), this)) {
          std::system("rm -rf /cache/params/.tmp_* /cache/params/d");
        }
      }

      params.putBool("DoUninstall", true);
    }
  });
  addItem(uninstallBtn);

  // FrogPilot variables
  // error log button
  ButtonControl *errorLogBtn = new ButtonControl(tr("Error Log"), tr("VIEW"), tr("View the error log for openpilot crashes."));
  connect(errorLogBtn, &ButtonControl::clicked, [=]() {
    std::string txt = util::read_file("/data/error_logs/error.txt");
    ConfirmationDialog::rich(QString::fromStdString(txt), this);
  });
  addItem(errorLogBtn);

  fs_watch = new ParamWatcher(this);
  QObject::connect(fs_watch, &ParamWatcher::paramChanged, [=](const QString &param_name, const QString &param_value) {
    updateLabels();
  });

  connect(uiState(), &UIState::offroadTransition, [=](bool offroad) {
    is_onroad = !offroad;
    updateLabels();
  });

  // FrogPilot variables
  connect(uiState(), &UIState::uiUpdate, [=](const UIState &s, const FrogPilotUIState &fs) {
    if (fs.frogpilot_scene.parked != previous_parked) {
      previous_parked = fs.frogpilot_scene.parked;
      updateLabels();
    }
  });

  updateLabels();
}

void SoftwarePanel::showEvent(QShowEvent *event) {
  // nice for testing on PC
  installBtn->setEnabled(true);

  updateLabels();

  // FrogPilot variables
  FrogPilotUIState &fs = *frogpilotUIState();
  FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;

  if (frogpilot_scene.online && !uiState()->scene.started && params.get("UpdaterState") == "idle") {
    checkForUpdates();
  }
}

void SoftwarePanel::updateLabels() {
  // FrogPilot variables
  FrogPilotUIState &fs = *frogpilotUIState();
  FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;

  // add these back in case the files got removed
  fs_watch->addParam("LastUpdateTime");
  fs_watch->addParam("UpdateFailedCount");
  fs_watch->addParam("UpdaterState");
  fs_watch->addParam("UpdateAvailable");

  // FrogPilot variables
  fs_watch->addParam("UpdaterDownloadProgress");

  if (!isVisible()) {
    // FrogPilot variables
    frogpilot_scene.downloading_update = false;
    return;
  }

  // updater only runs offroad or when parked
  // FrogPilot variables
  bool parked = frogpilot_scene.parked || frogsGoMoo();

  onroadLbl->setVisible(is_onroad && !parked);
  downloadBtn->setVisible(!is_onroad || parked);

  // download update
  QString updater_state = QString::fromStdString(params.get("UpdaterState"));
  bool failed = std::atoi(params.get("UpdateFailedCount").c_str()) > 0;

  // FrogPilot variables
  std::string download_progress = params.get("UpdaterDownloadProgress");

  if (updater_state != "idle" || !download_progress.empty()) {
    downloadBtn->setEnabled(false);

    // FrogPilot variables
    QString stateText = updater_state;
    if (updater_state == "downloading...") {
      stateText = tr("downloading…");
    } else if (updater_state == "checking...") {
      stateText = tr("checking…");
    } else if (updater_state == "finalizing update...") {
      stateText = tr("finalizing update…");
    }

    downloadBtn->setValue(formatDownloadProgress(download_progress, stateText));

    // FrogPilot variables
    frogpilot_scene.downloading_update = updater_state != "idle";
  } else {
    // FrogPilot variables
    frogpilot_scene.downloading_update = false;

    if (failed) {
      downloadBtn->setText(tr("CHECK"));
      downloadBtn->setValue(tr("failed to check for update"));
    } else if (params.getBool("UpdaterFetchAvailable")) {
      downloadBtn->setText(tr("DOWNLOAD"));
      downloadBtn->setValue(tr("update available"));
    } else {
      QString lastUpdate = tr("never");
      auto tm = params.get("LastUpdateTime");
      if (!tm.empty()) {
        lastUpdate = timeAgo(QDateTime::fromString(QString::fromStdString(tm + "Z"), Qt::ISODate));
      }
      downloadBtn->setText(tr("CHECK"));
      downloadBtn->setValue(tr("up to date, last checked %1").arg(lastUpdate));
    }
    downloadBtn->setEnabled(true);
  }
  targetBranchBtn->setValue(QString::fromStdString(params.get("UpdaterTargetBranch")));

  // current + new versions
  versionLbl->setText(QString::fromStdString(params.get("UpdaterCurrentDescription")));
  versionLbl->setDescription(QString::fromStdString(params.get("UpdaterCurrentReleaseNotes")));

  installBtn->setVisible((!is_onroad || parked) && params.getBool("UpdateAvailable") && download_progress.empty());
  installBtn->setValue(QString::fromStdString(params.get("UpdaterNewDescription")));
  installBtn->setDescription(QString::fromStdString(params.get("UpdaterNewReleaseNotes")));

  update();
}
