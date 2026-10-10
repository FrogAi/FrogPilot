#include <QtConcurrent>

#include "frogpilot/ui/qt/offroad/maps_settings.h"

FrogPilotMapsPanel::FrogPilotMapsPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  QStackedLayout *mapsLayout = new QStackedLayout();
  addItem(mapsLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);

  std::vector<QString> scheduleOptions{tr("Manually"), tr("Weekly"), tr("Monthly")};
  preferredSchedule = new ButtonParamControl("PreferredSchedule", tr("Automatically Update Maps"),
                                             tr("<b>How often openpilot re-downloads the speed limit map data for the places you picked under \"Map Sources\". \"Weekly\" runs every Sunday, \"Monthly\" runs on the 1st, and \"Manually\" waits until you press \"DOWNLOAD\" yourself.</b><br><br>"
                                                "There is one exception. Whenever the map data is missing from the device, openpilot starts the download on its own, usually within the hour. Automatic downloads only start on an unmetered connection, such as Wi-Fi, and unlike \"DOWNLOAD\" they do not wait until you park."),
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
      frogpilotUIState()->downloadMaps();
    }
  });
  settingsList->addItem(downloadMapsButton);

  settingsList->addItem(lastMapsDownload = new LabelControl(tr("Last Updated")));

  selectMaps = new FrogPilotButtonsControl(tr("Map Sources"),
                                           tr("<b>Pick the countries or U.S. states you drive in, so openpilot knows their speed limits.</b><br><br>Map data within about 60 miles (100 km) of where you park downloads on its own, so pick only what covers the rest of your driving."),
                                           "", {tr("COUNTRIES"), tr("STATES")});
  QObject::connect(selectMaps, &FrogPilotButtonsControl::buttonClicked, [mapsLayout, this](int id) {
    const std::vector<MapSelectionControl *> &selectionControls = id == 0 ? countrySelectionControls : stateSelectionControls;
    for (MapSelectionControl *control : selectionControls) {
      control->reloadSelectedMaps();
    }

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

  removeMapsButton = new ButtonControl(tr("Remove Maps"), tr("REMOVE"), tr("<b>Delete your downloaded map data and clear the places you picked under \"Map Sources\", to free up storage.</b><br><br>Only the map data around where you park comes back on its own, the next time the device is on Wi-Fi. Everywhere else, \"Speed Limit Controller\" has no map speed limits until you pick your places again and start a new download."));
  QObject::connect(removeMapsButton, &ButtonControl::clicked, [this] {
    if (FrogPilotConfirmationDialog::yesorno(tr("Delete all downloaded maps and clear your selected map sources?"), this)) {
      hasMapsSelected = false;
      removingMaps = true;

      downloadMapsButton->setEnabled(false);
      removeMapsButton->setEnabled(false);
      selectMaps->setEnabled(false);

      removeMapsButton->setValue(tr("Removing..."));

      params.remove("MapsSelected");
      params.remove("LastMapsUpdate");

      lastMapsDownload->setText(tr("Never"));

      QDir mapsFolder = mapsFolderPath;

      QFutureWatcher<bool> *removalWatcher = new QFutureWatcher<bool>(this);
      QObject::connect(removalWatcher, &QFutureWatcher<bool>::finished, this, [this, removalWatcher]() {
        const bool removed = removalWatcher->result();
        removalWatcher->deleteLater();

        removingMaps = false;

        removeMapsButton->setValue("");

        removeMapsButton->setEnabled(true);
        selectMaps->setEnabled(true);

        refreshMapInfo();
        updateState(*uiState(), *frogpilotUIState());

        if (!removed && isVisible()) {
          ConfirmationDialog::alert(tr("Some map data could not be removed. Try again."), this);
        }
      });

      removalWatcher->setFuture(QtConcurrent::run([mapsFolder]() mutable {
        return mapsFolder.removeRecursively();
      }));
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

  for (const std::pair<QString, QMap<QString, QString>> &country : countries) {
    countriesList->addItem(new LabelControl(country.first, ""));
    MapSelectionControl *control = new MapSelectionControl(country.second, true);
    countrySelectionControls.push_back(control);
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

  for (const std::pair<QString, QMap<QString, QString>> &state : states) {
    statesList->addItem(new LabelControl(state.first, ""));
    MapSelectionControl *control = new MapSelectionControl(state.second);
    stateSelectionControls.push_back(control);
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

  refreshMapInfo();
  updateState(*uiState(), *frogpilotUIState());

  const SubMaster &fpsm = *(frogpilotUIState()->sm);
  const cereal::MapdDownloadProgress::Reader &downloadProgress = fpsm["mapdExtendedOut"].getMapdExtendedOut().getDownloadProgress();
  if (downloadProgress.getActive()) {
    updateDownloadLabels(downloadProgress.getDownloadedFiles(), downloadProgress.getTotalFiles());
  }
}

void FrogPilotMapsPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  const SubMaster &fpsm = *(fs.sm);

  const cereal::FrogPilotProcessState::Reader &frogpilotProcessState = fpsm["frogpilotProcessState"].getFrogpilotProcessState();
  const cereal::MapdExtendedOut::Reader &mapdExtendedOut = fpsm["mapdExtendedOut"].getMapdExtendedOut();
  const cereal::MapdDownloadProgress::Reader &downloadProgress = mapdExtendedOut.getDownloadProgress();

  const bool mapDownloadActive = downloadProgress.getActive();
  const bool mapDownloadPending = fs.download_maps_request_time > frogpilotProcessState.getDownloadMapsRequestTime() || frogpilotProcessState.getDownloadingMaps();
  const bool downloadingMaps = mapDownloadActive || mapDownloadPending;

  if (downloadingMaps && !wasDownloadingMaps) {
    previousDownloadedFiles = 0;
    elapsedTime.start();
    startTime = QDateTime::currentDateTime();
  } else if (!downloadingMaps && wasDownloadingMaps && isVisible()) {
    refreshMapInfo();
  }
  wasDownloadingMaps = downloadingMaps;

  if (!isVisible()) {
    return;
  }

  const FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;
  const UIScene &scene = s.scene;

  const bool parked = !scene.started || frogpilot_scene.parked || parent->isFrogsGoMoo;

  const int mapDownloadDownloaded = downloadProgress.getDownloadedFiles();
  const int mapDownloadTotal = downloadProgress.getTotalFiles();

  if (downloadingMaps) {
    downloadMapsButton->setEnabled(!removingMaps && !cancellingDownload);
    downloadMapsButton->setText(tr("CANCEL"));
    downloadMapsButton->setValue("");

    downloadETA->setVisible(true);
    downloadStatus->setVisible(true);
    downloadTimeElapsed->setVisible(true);

    lastMapsDownload->setVisible(false);
    removeMapsButton->setVisible(false);

    if (mapDownloadActive) {
      if (s.sm->frame % (UI_FREQ / 2) == 0) {
        updateDownloadLabels(mapDownloadDownloaded, mapDownloadTotal);
      }
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
    removeMapsButton->setVisible(mapsFolderExists);

    downloadMapsButton->setEnabled(!removingMaps && !cancellingDownload && hasMapsSelected && frogpilot_scene.online && parked);
    downloadMapsButton->setValue(frogpilot_scene.online ? (parked ? (hasMapsSelected ? "" : tr("Select your map sources")) : tr("Not parked")) : tr("Offline..."));
  }

  parent->keepScreenOn = downloadingMaps || removingMaps;
}

void FrogPilotMapsPanel::cancelDownload() {
  cancellingDownload = true;

  downloadMapsButton->setEnabled(false);

  frogpilotUIState()->cancelMapsDownload();

  QTimer::singleShot(2500, this, [this]() {
    cancellingDownload = false;
  });
}

void FrogPilotMapsPanel::refreshMapInfo() {
  const std::string lastMapsUpdate = params.get("LastMapsUpdate");
  lastMapsDownload->setText(lastMapsUpdate.empty() ? tr("Never") : QString::fromStdString(lastMapsUpdate));

  mapsFolderExists = mapsFolderPath.exists();

  QFutureWatcher<QString> *sizeWatcher = new QFutureWatcher<QString>(this);
  QObject::connect(sizeWatcher, &QFutureWatcher<QString>::finished, this, [this, sizeWatcher]() {
    mapsSize->setText(sizeWatcher->result());
    sizeWatcher->deleteLater();
  });
  sizeWatcher->setFuture(QtConcurrent::run(calculateDirectorySize, mapsFolderPath));
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
