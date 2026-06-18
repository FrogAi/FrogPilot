#include <thread>

#include "frogpilot/ui/qt/offroad/maps_settings.h"

FrogPilotMapsPanel::FrogPilotMapsPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  QStackedLayout *mapsLayout = new QStackedLayout();
  addItem(mapsLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);

  std::vector<QString> scheduleOptions{tr("Manually"), tr("Weekly"), tr("Monthly")};
  preferredSchedule = new ButtonParamControl("PreferredSchedule", tr("Automatically Update Maps"),
                                          tr("<b>How often openpilot re-downloads the speed limit map data for the places you picked under \"Map Sources\". \"Weekly\" runs every Sunday, \"Monthly\" runs on the 1st, and \"Manually\" waits until you press \"DOWNLOAD\" yourself.</b><br><br>"
                                             "There is one exception. Whenever the map data is missing from the device, openpilot starts the download on its own, usually within the hour, and that one is not held back until you park."),
                                             "",
                                             scheduleOptions);
  settingsList->addItem(preferredSchedule);

  downloadMapsButton = new ButtonControl(tr("Download Maps"), tr("DOWNLOAD"), tr("<b>Start downloading the speed limit map data for the places you picked under \"Map Sources\".</b><br><br>Your car has to be parked and online. Large areas can take hours and use several gigabytes."));
  QObject::connect(downloadMapsButton, &ButtonControl::clicked, [this] {
    if (downloadMapsButton->text() == tr("CANCEL")) {
      if (FrogPilotConfirmationDialog::yesorno(tr("Cancel the download?"), this)) {
        cancelDownload();
      }
    } else {
      startDownload();
    }
  });
  settingsList->addItem(downloadMapsButton);

  settingsList->addItem(lastMapsDownload = new LabelControl(tr("Last Updated")));

  selectMaps = new FrogPilotButtonsControl(tr("Map Sources"),
                                           tr("<b>Pick the countries or U.S. states you drive in, so openpilot knows their speed limits.</b><br><br>Only what you pick here gets downloaded, so pick as little as covers your driving.") ,
                                              "", {tr("COUNTRIES"), tr("STATES")});
  QObject::connect(selectMaps, &FrogPilotButtonsControl::buttonClicked, [mapsLayout, this](int id) {
    mapsLayout->setCurrentIndex(id + 1);

    openSubPanel();
  });
  settingsList->addItem(selectMaps);

  settingsList->addItem(downloadStatus = new LabelControl(tr("Progress")));
  settingsList->addItem(downloadTimeElapsed = new LabelControl(tr("Time Elapsed")));
  settingsList->addItem(downloadETA = new LabelControl(tr("Time Remaining")));

  downloadETA->setVisible(false);
  downloadStatus->setVisible(false);
  downloadTimeElapsed->setVisible(false);

  removeMapsButton = new ButtonControl(tr("Remove Maps"), tr("REMOVE"), tr("<b>Delete your downloaded map data and clear the places you picked under \"Map Sources\", to free up storage.</b><br><br>Nothing comes back on its own, so \"Speed Limit Controller\" has no map speed limits until you pick your places again and start a new download."));
  QObject::connect(removeMapsButton, &ButtonControl::clicked, [this] {
    if (FrogPilotConfirmationDialog::yesorno(tr("Delete all downloaded maps and clear your selected map sources?"), this)) {
      hasMapsSelected = false;

      params.remove("MapsSelected");
      params.remove("LastMapsUpdate");

      for (MapSelectionControl *control : mapSelectionControls) {
        control->reloadSelectedMaps();
      }

      lastMapsDownload->setText(tr("Never"));

      QDir mapsFolder = mapsFolderPath;

      mapsSize->setText(tr("0 MB"));

      std::thread([mapsFolder]() mutable {
        mapsFolder.removeRecursively();
      }).detach();
    }
  });
  settingsList->addItem(removeMapsButton);

  settingsList->addItem(mapsSize = new LabelControl(tr("Storage Used")));

  ScrollView *settingsPanel = new ScrollView(settingsList, this);
  mapsLayout->addWidget(settingsPanel);

  FrogPilotListWidget *countriesList = new FrogPilotListWidget(this);
  std::vector<std::pair<QString, QMap<QString, QString>>> countries = {
    {tr("Africa"), africaMap},
    {tr("Antarctica"), antarcticaMap},
    {tr("Asia"), asiaMap},
    {tr("Europe"), europeMap},
    {tr("North America"), northAmericaMap},
    {tr("Oceania"), oceaniaMap},
    {tr("South America"), southAmericaMap}
  };

  for (std::pair<QString, QMap<QString, QString>> country : countries) {
    countriesList->addItem(new LabelControl(country.first, ""));
    MapSelectionControl *control = new MapSelectionControl(country.second, true);
    mapSelectionControls.push_back(control);
    countriesList->addItem(control);
  }

  ScrollView *countryMapsPanel = new ScrollView(countriesList, this);
  mapsLayout->addWidget(countryMapsPanel);

  FrogPilotListWidget *statesList = new FrogPilotListWidget(this);
  std::vector<std::pair<QString, QMap<QString, QString>>> states = {
    {tr("United States - Midwest"), midwestMap},
    {tr("United States - Northeast"), northeastMap},
    {tr("United States - South"), southMap},
    {tr("United States - West"), westMap},
    {tr("United States - Territories"), territoriesMap}
  };

  for (std::pair<QString, QMap<QString, QString>> state : states) {
    statesList->addItem(new LabelControl(state.first, ""));
    MapSelectionControl *control = new MapSelectionControl(state.second);
    mapSelectionControls.push_back(control);
    statesList->addItem(control);
  }

  ScrollView *stateMapsPanel = new ScrollView(statesList, this);
  mapsLayout->addWidget(stateMapsPanel);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [mapsLayout, settingsPanel, this] {
    if (forceOpenDescriptions) {
      downloadMapsButton->showDescription();
      preferredSchedule->showDescription();
      removeMapsButton->showDescription();
      selectMaps->showDescription();
    }

    hasMapsSelected = !params.get("MapsSelected").empty();

    mapsLayout->setCurrentWidget(settingsPanel);
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotMapsPanel::updateState);
}

void FrogPilotMapsPanel::showEvent(QShowEvent *event) {
  if (forceOpenDescriptions) {
    downloadMapsButton->showDescription();
    preferredSchedule->showDescription();
    removeMapsButton->showDescription();
    selectMaps->showDescription();
  }

  hasMapsSelected = !params.get("MapsSelected").empty();
  mapDownloadStarted = false;
  wasDownloadingMaps = false;

  refreshMapInfo();
  updateState(*uiState(), *frogpilotUIState());
}

void FrogPilotMapsPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible()) {
    return;
  }

  const FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;
  const UIScene &scene = s.scene;
  const SubMaster &fpsm = *(fs.sm);

  const cereal::MapdExtendedOut::Reader &mapdExtendedOut = fpsm["mapdExtendedOut"].getMapdExtendedOut();
  const cereal::MapdDownloadProgress::Reader &downloadProgress = mapdExtendedOut.getDownloadProgress();

  const bool mapDownloadActive = downloadProgress.getActive();
  const bool mapDownloadPending = params_memory.getBool("DownloadMaps");
  const bool downloadingMaps = mapDownloadActive || mapDownloadPending;
  const bool parked = !scene.started || frogpilot_scene.parked || parent->isFrogsGoMoo;

  const int mapDownloadDownloaded = downloadProgress.getDownloadedFiles();
  const int mapDownloadTotal = downloadProgress.getTotalFiles();

  if (downloadingMaps && !wasDownloadingMaps) {
    previousDownloadedFiles = 0;
    elapsedTime.start();
    startTime = QDateTime::currentDateTime();
  } else if (!downloadingMaps && wasDownloadingMaps) {
    refreshMapInfo();
    if (mapDownloadStarted && !downloadProgress.getCancelled() && mapDownloadDownloaded == mapDownloadTotal && mapDownloadTotal > 0) {
      lastMapsDownload->setText(formatCurrentDate());
    }
    mapDownloadStarted = false;
  }
  wasDownloadingMaps = downloadingMaps;

  if (downloadingMaps) {
    downloadMapsButton->setEnabled(!cancellingDownload);
    downloadMapsButton->setText(tr("CANCEL"));

    downloadETA->setVisible(true);
    downloadStatus->setVisible(true);
    downloadTimeElapsed->setVisible(true);

    lastMapsDownload->setVisible(false);
    removeMapsButton->setVisible(false);

    if (mapDownloadActive) {
      mapDownloadStarted = true;
      updateDownloadLabels(mapDownloadDownloaded, mapDownloadTotal);
    } else {
      downloadETA->setText(tr("Calculating..."));
      downloadStatus->setText(tr("Calculating..."));
      downloadTimeElapsed->setText(tr("Calculating..."));
    }
  } else {
    downloadMapsButton->setText(tr("DOWNLOAD"));

    downloadETA->setVisible(false);
    downloadStatus->setVisible(false);
    downloadTimeElapsed->setVisible(false);

    lastMapsDownload->setVisible(true);
    removeMapsButton->setVisible(mapsFolderPath.exists());

    downloadMapsButton->setEnabled(!cancellingDownload && hasMapsSelected && frogpilot_scene.online && parked);
    downloadMapsButton->setValue(frogpilot_scene.online ? (parked ? (hasMapsSelected ? "" : tr("Select your map sources")) : tr("Not parked")) : tr("Offline..."));
  }

  parent->keepScreenOn = downloadingMaps;
}

void FrogPilotMapsPanel::cancelDownload() {
  cancellingDownload = true;

  downloadMapsButton->setEnabled(false);

  params_memory.putBool("CancelDownloadMaps", true);
  params_memory.remove("DownloadMaps");

  QTimer::singleShot(2500, this, [this]() {
    cancellingDownload = false;
  });
}

void FrogPilotMapsPanel::refreshMapInfo() {
  const std::string lastMapsUpdate = params.get("LastMapsUpdate");
  lastMapsDownload->setText(lastMapsUpdate.empty() ? tr("Never") : QString::fromStdString(lastMapsUpdate));
  mapsSize->setText(calculateDirectorySize(mapsFolderPath));
}

void FrogPilotMapsPanel::startDownload() {
  params_memory.putBool("DownloadMaps", true);
}

void FrogPilotMapsPanel::updateDownloadLabels(int downloadedFiles, int totalFiles) {
  if (downloadedFiles > 0) {
    downloadETA->setText(formatETA(elapsedTime.elapsed(), downloadedFiles, previousDownloadedFiles, totalFiles, startTime));
  } else {
    downloadETA->setText(tr("Calculating..."));
  }
  downloadStatus->setText(QString("%1 / %2 (%3%)").arg(downloadedFiles).arg(totalFiles).arg((downloadedFiles * 100) / (totalFiles == 0 ? 1 : totalFiles)));
  downloadTimeElapsed->setText(formatElapsedTime(elapsedTime.elapsed()));

  previousDownloadedFiles = downloadedFiles;
}
