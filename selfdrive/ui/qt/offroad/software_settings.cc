#include "selfdrive/ui/qt/offroad/settings.h"

#include <cassert>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

#include <QDebug>
#include <QLabel>
#include <QProcess>

#include "common/params.h"
#include "common/util.h"
#include "selfdrive/ui/ui.h"
#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/widgets/controls.h"
#include "selfdrive/ui/qt/widgets/input.h"
#include "system/hardware/hw.h"

#include "selfdrive/frogpilot/ui/frogpilot_ui_functions.h"

void SoftwarePanel::checkForUpdates() {
  std::system("pkill -SIGUSR1 -f selfdrive.updated");
}

SoftwarePanel::SoftwarePanel(QWidget* parent) : ListWidget(parent), scene(uiState()->scene) {
  onroadLbl = new QLabel(tr("Updates are only downloaded while the car is off or in park."));
  onroadLbl->setStyleSheet("font-size: 50px; font-weight: 400; text-align: left; padding-top: 30px; padding-bottom: 30px;");
  addItem(onroadLbl);

  // current version
  versionLbl = new LabelControl(tr("Current Version"), "");
  addItem(versionLbl);

  // Update scheduler
  std::vector<QString> scheduleOptions{tr("Manually"), tr("Daily"), tr("Weekly")};
  FrogPilotButtonParamControl *preferredSchedule = new FrogPilotButtonParamControl("UpdateSchedule", tr("Update Scheduler"),
                                          tr("Choose the frequency to automatically update FrogPilot.\n\n"
                                          "This feature will handle the download, installation, and device reboot for a seamless 'Set and Forget' update experience.\n\n"
                                          "Weekly updates start at midnight every Sunday."),
                                          "",
                                          scheduleOptions);
  schedule = params.getInt("UpdateSchedule");
  QObject::connect(preferredSchedule, &FrogPilotButtonParamControl::buttonClicked, [this](int id) {
    schedule = id;
    updateLabels();
  });
  addItem(preferredSchedule);

  updateTime = new ButtonControl(tr("Update Time"), tr("SELECT"));
  QStringList hours;
  for (int h = 0; h < 24; h++) {
    int displayHour = (h % 12 == 0) ? 12 : h % 12;
    QString meridiem = (h < 12) ? "AM" : "PM";
    hours << QString("%1:00 %2").arg(displayHour).arg(meridiem)
          << QString("%1:30 %2").arg(displayHour).arg(meridiem);
  }

  QObject::connect(updateTime, &ButtonControl::clicked, [=]() {
    int currentHourIndex = params.getInt("UpdateTime");
    QString currentHourLabel = hours[currentHourIndex];

    QString selection = MultiOptionDialog::getSelection(tr("Select a time to automatically update"), hours, currentHourLabel, this);
    if (!selection.isEmpty()) {
      int selectedHourIndex = hours.indexOf(selection);
      params.putInt("UpdateTime", selectedHourIndex);
      updateTime->setValue(selection);
    }
  });
  time = params.getInt("UpdateTime");
  updateTime->setValue(hours[time]);
  updateTime->setVisible(schedule != 0);
  addItem(updateTime);

  // download update btn
  downloadBtn = new ButtonControl(tr("Download"), tr("CHECK"));
  connect(downloadBtn, &ButtonControl::clicked, [=]() {
    downloadBtn->setEnabled(false);
    if (downloadBtn->text() == tr("CHECK")) {
      if (schedule == 0) {
        params.putBool("ManualUpdateInitiated", true);
      }
      checkForUpdates();
    } else {
      std::system("pkill -SIGHUP -f selfdrive.updated");
    }
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
    }
  });
  if (!params.getBool("IsTestedBranch")) {
    addItem(targetBranchBtn);
  }

  // uninstall button
  auto uninstallBtn = new ButtonControl(tr("Uninstall %1").arg(getBrand()), tr("UNINSTALL"));
  connect(uninstallBtn, &ButtonControl::clicked, [&]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to uninstall?"), tr("Uninstall"), this)) {
      params.putBool("DoUninstall", true);
    }
  });
  addItem(uninstallBtn);

  // error log button
  errorLogBtn = new ButtonControl(tr("Error Log"), tr("VIEW"), "View the error log for debugging purposes when openpilot crashes.");
  connect(errorLogBtn, &ButtonControl::clicked, [=]() {
    std::string txt = util::read_file("/data/community/crashes/error.txt");
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

  QObject::connect(uiState(), &UIState::uiUpdate, this, &SoftwarePanel::automaticUpdate);

  updateLabels();
}

void SoftwarePanel::showEvent(QShowEvent *event) {
  // nice for testing on PC
  installBtn->setEnabled(true);

  updateLabels();
}

void SoftwarePanel::updateLabels() {
  // add these back in case the files got removed
  fs_watch->addParam("LastUpdateTime");
  fs_watch->addParam("UpdateFailedCount");
  fs_watch->addParam("UpdaterState");
  fs_watch->addParam("UpdateAvailable");

  if (!isVisible()) {
    return;
  }

  // updater only runs offroad or when parked
  bool parked = scene.parked;

  onroadLbl->setVisible(is_onroad && !parked);
  downloadBtn->setVisible(!is_onroad || parked);

  // download update
  QString updater_state = QString::fromStdString(params.get("UpdaterState"));
  bool failed = std::atoi(params.get("UpdateFailedCount").c_str()) > 0;
  if (updater_state != "idle") {
    downloadBtn->setEnabled(false);
    downloadBtn->setValue(updater_state);
  } else {
    if (failed && schedule != 0) {
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

  installBtn->setVisible((!is_onroad || parked) && params.getBool("UpdateAvailable"));
  installBtn->setValue(QString::fromStdString(params.get("UpdaterNewDescription")));
  installBtn->setDescription(QString::fromStdString(params.get("UpdaterNewReleaseNotes")));

  updateTime->setVisible(params.getInt("UpdateSchedule"));

  update();
}

void SoftwarePanel::automaticUpdate() {
  // Variable declarations
  static bool isDownloadCompleted = false;
  static bool updateCheckedToday = false;

  std::time_t currentTimeT = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm now = *std::localtime(&currentTimeT);

  // Check to make sure we're not onroad and have a WiFi connection
  bool isWifiConnected = (*uiState()->sm)["deviceState"].getDeviceState().getNetworkType() == cereal::DeviceState::NetworkType::WIFI;
  if (schedule == 0 || is_onroad || !isWifiConnected || isVisible()) return;

  // Reboot if an automatic update was completed
  if (isDownloadCompleted) {
    if (installBtn->isVisible()) Hardware::reboot();
    return;
  }

  // Format "Updated" to a useable format
  std::tm lastUpdate;
  std::istringstream ss(params.get("Updated"));
  ss >> std::get_time(&lastUpdate, "%Y-%m-%d %H:%M:%S");
  std::time_t lastUpdateTimeT = std::mktime(&lastUpdate);

  // Check if an update was already performed today
  static int lastCheckedDay = now.tm_yday;
  if (lastCheckedDay != now.tm_yday) {
    updateCheckedToday = false;
    lastCheckedDay = now.tm_yday;
  } else if (lastUpdate.tm_yday == now.tm_yday) {
    return;
  }

  // Check if it's time to update
  std::chrono::hours durationSinceLastUpdate = std::chrono::duration_cast<std::chrono::hours>(std::chrono::system_clock::now() - std::chrono::system_clock::from_time_t(lastUpdateTimeT));
  int daysSinceLastUpdate = durationSinceLastUpdate.count() / 24;

  if ((schedule == 1 && daysSinceLastUpdate >= 1) || (schedule == 2 && (now.tm_yday / 7) != (std::localtime(&lastUpdateTimeT)->tm_yday / 7))) {
    if (downloadBtn->text() == tr("CHECK") && !updateCheckedToday) {
      checkForUpdates();
      updateCheckedToday = true;
    } else {
      std::system("pkill -SIGHUP -f selfdrive.updated");
      isDownloadCompleted = true;
    }
  }
}
