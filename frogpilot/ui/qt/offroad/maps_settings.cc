#include <regex>

#include <QtConcurrent>

#include "frogpilot/ui/qt/offroad/maps_settings.h"

FrogPilotMapsPanel::FrogPilotMapsPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

  QStackedLayout *mapsLayout = new QStackedLayout();
  addItem(mapsLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);

  std::vector<QString> scheduleOptions{tr("Manually"), tr("Weekly"), tr("Monthly")};
  preferredSchedule = new ButtonParamControl("PreferredSchedule", tr("Automatically Update Maps"),
                                          tr("<b>How often maps update</b> from \"OpenStreetMap (OSM)\" with the latest speed limit information. "
                                             "Weekly updates run every Sunday; monthly updates run on the 1st."),
                                             "",
                                             scheduleOptions);
  settingsList->addItem(preferredSchedule);

  downloadMapsButton = new ButtonControl(tr("Download Maps"), tr("DOWNLOAD"), tr("<b>Manually update your selected map sources</b> so \"Speed Limit Controller\" has the latest speed limit information."));
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

  settingsList->addItem(lastMapsDownload = new LabelControl(tr("Last Updated"), params.get("LastMapsUpdate").empty() ? "Never" : QString::fromStdString(params.get("LastMapsUpdate"))));

  selectMaps = new FrogPilotButtonsControl(tr("Map Sources"),
                                           tr("<b>Select the countries or U.S. states to use with \"Speed Limit Controller\".</b>") ,
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

  removeMapsButton = new ButtonControl(tr("Remove Maps"), tr("REMOVE"), tr("<b>Delete downloaded map data</b> to free up storage space."));
  QObject::connect(removeMapsButton, &ButtonControl::clicked, [this] {
    if (FrogPilotConfirmationDialog::yesorno(tr("Delete all downloaded maps?"), this)) {
      std::thread([this] {
        mapsSize->setText("0 MB");

        mapsFolderPath.removeRecursively();
      }).detach();
    }
  });
  settingsList->addItem(removeMapsButton);

  resetMapdButton = new ButtonControl(tr("Reset Downloader"), tr("RESET"),
                                   tr("<b>Reset the map downloader.</b> Use this if downloads are stuck or failing."));
  QObject::connect(resetMapdButton, &ButtonControl::clicked, [parent, this]() {
    if (ConfirmationDialog::confirm(tr("Reset the map downloader? Your device will reboot afterward."), tr("Reset"), this)) {
      std::thread([parent, this]() {
        parent->keepScreenOn = true;

        resetMapdButton->setEnabled(false);
        resetMapdButton->setValue(tr("Resetting..."));

        std::system("pkill mapd");

        QDir("/data/media/0/osm").removeRecursively();

        resetMapdButton->setValue(tr("Reset!"));

        util::sleep_for(2500);

        resetMapdButton->setValue(tr("Rebooting..."));

        util::sleep_for(2500);

        Hardware::reboot();
      }).detach();
    }
  });
  settingsList->addItem(resetMapdButton);

  settingsList->addItem(mapsSize = new LabelControl(tr("Storage Used"), calculateDirectorySize(mapsFolderPath)));

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
    countriesList->addItem(new MapSelectionControl(country.second, true));
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
    statesList->addItem(new MapSelectionControl(state.second));
  }

  ScrollView *stateMapsPanel = new ScrollView(statesList, this);
  mapsLayout->addWidget(stateMapsPanel);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [mapsLayout, settingsPanel, this] {
    if (forceOpenDescriptions) {
      downloadMapsButton->showDescription();
      preferredSchedule->showDescription();
      removeMapsButton->showDescription();
      resetMapdButton->showDescription();
      selectMaps->showDescription();
    }

    std::string mapsSelected = params.get("MapsSelected");
    hasMapsSelected = !QJsonDocument::fromJson(QByteArray::fromStdString(mapsSelected)).object().value("nations").toArray().isEmpty();
    hasMapsSelected |= !QJsonDocument::fromJson(QByteArray::fromStdString(mapsSelected)).object().value("states").toArray().isEmpty();

    mapsLayout->setCurrentWidget(settingsPanel);
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotMapsPanel::updateState);
}

void FrogPilotMapsPanel::showEvent(QShowEvent *event) {
  if (forceOpenDescriptions) {
    downloadMapsButton->showDescription();
    preferredSchedule->showDescription();
    removeMapsButton->showDescription();
    resetMapdButton->showDescription();
    selectMaps->showDescription();
  }

  FrogPilotUIState &fs = *frogpilotUIState();
  UIState &s = *uiState();

  std::string mapsSelected = params.get("MapsSelected");
  hasMapsSelected = !QJsonDocument::fromJson(QByteArray::fromStdString(mapsSelected)).object().value("nations").toArray().isEmpty();
  hasMapsSelected |= !QJsonDocument::fromJson(QByteArray::fromStdString(mapsSelected)).object().value("states").toArray().isEmpty();

  bool parked = !s.scene.started || fs.frogpilot_scene.parked || fs.frogpilot_toggles.value("frogs_go_moo").toBool();

  removeMapsButton->setVisible(mapsFolderPath.exists());

  std::string osmDownloadProgress = params.get("OSMDownloadProgress");
  if (!osmDownloadProgress.empty()) {
    downloadMapsButton->setText(tr("CANCEL"));
    downloadStatus->setText("Calculating...");

    downloadStatus->setVisible(true);

    lastMapsDownload->setVisible(false);
    removeMapsButton->setVisible(false);
    resetMapdButton->setVisible(false);

    updateDownloadLabels(osmDownloadProgress);
  } else {
    downloadMapsButton->setEnabled(!cancellingDownload && hasMapsSelected && fs.frogpilot_scene.online && parked);
    downloadMapsButton->setValue(fs.frogpilot_scene.online ? (parked ? "" : "Not parked") : tr("Offline..."));
  }
}


void FrogPilotMapsPanel::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible() || s.sm->frame % (UI_FREQ / 2) != 0) {
    return;
  }

  bool parked = !s.scene.started || fs.frogpilot_scene.parked || fs.frogpilot_toggles.value("frogs_go_moo").toBool();

  std::string osmDownloadProgress = params.get("OSMDownloadProgress");
  if (!osmDownloadProgress.empty() && !cancellingDownload) {
    updateDownloadLabels(osmDownloadProgress);
  } else {
    downloadMapsButton->setEnabled(!cancellingDownload && hasMapsSelected && fs.frogpilot_scene.online && parked);
    downloadMapsButton->setValue(fs.frogpilot_scene.online ? (parked ? "" : "Not parked") : tr("Offline..."));
  }

  parent->keepScreenOn = !osmDownloadProgress.empty();
}

void FrogPilotMapsPanel::cancelDownload() {
  cancellingDownload = true;

  downloadMapsButton->setEnabled(false);

  downloadETA->setText("Cancelling...");
  downloadMapsButton->setText(tr("CANCELLED"));
  downloadStatus->setText("Cancelling...");
  downloadTimeElapsed->setText("Cancelling...");

  params.remove("OSMDownloadProgress");
  params_memory.remove("OSMDownloadLocations");

  std::system("pkill mapd");

  QTimer::singleShot(2500, [this]() {
    cancellingDownload = false;

    downloadMapsButton->setEnabled(true);

    downloadMapsButton->setText(tr("DOWNLOAD"));

    downloadETA->setVisible(false);
    downloadStatus->setVisible(false);
    downloadTimeElapsed->setVisible(false);

    lastMapsDownload->setVisible(true);
    removeMapsButton->setVisible(mapsFolderPath.exists());
    resetMapdButton->setVisible(true);

    update();
  });
}

void FrogPilotMapsPanel::startDownload() {
  downloadETA->setText("Calculating...");
  downloadMapsButton->setText(tr("CANCEL"));
  downloadStatus->setText("Calculating...");
  downloadTimeElapsed->setText("Calculating...");

  downloadETA->setVisible(true);
  downloadStatus->setVisible(true);
  downloadTimeElapsed->setVisible(true);

  lastMapsDownload->setVisible(false);
  removeMapsButton->setVisible(false);
  resetMapdButton->setVisible(false);

  elapsedTime.start();
  startTime = QDateTime::currentDateTime();

  params_memory.put("OSMDownloadLocations", params.get("MapsSelected"));
}

void FrogPilotMapsPanel::updateDownloadLabels(std::string &osmDownloadProgress) {
  static std::regex fileStatusRegex(R"("total_files":(\d+),.*"downloaded_files":(\d+))");

  std::smatch match;
  if (std::regex_search(osmDownloadProgress, match, fileStatusRegex)) {
    int totalFiles = std::stoi(match[1].str());
    int downloadedFiles = std::stoi(match[2].str());

    if (downloadedFiles == totalFiles) {
      downloadMapsButton->setText(tr("DOWNLOAD"));
      lastMapsDownload->setText(formatCurrentDate());

      downloadETA->setVisible(false);
      downloadStatus->setVisible(false);
      downloadTimeElapsed->setVisible(false);

      lastMapsDownload->setVisible(true);
      removeMapsButton->setVisible(true);
      resetMapdButton->setVisible(true);

      params.put("LastMapsUpdate", formatCurrentDate().toStdString());
      params.remove("OSMDownloadProgress");

      update();

      return;
    }

    static int previousDownloadedFiles = 0;
    if (downloadedFiles != previousDownloadedFiles) {
      std::thread([this]() {
        mapsSize->setText(calculateDirectorySize(mapsFolderPath));
      }).detach();
    }

    downloadETA->setText(QString("%1").arg(formatETA(elapsedTime.elapsed(), downloadedFiles, previousDownloadedFiles, totalFiles, startTime)));
    downloadStatus->setText(QString("%1 / %2 (%3%)").arg(downloadedFiles).arg(totalFiles).arg((downloadedFiles * 100) / totalFiles));
    downloadTimeElapsed->setText(formatElapsedTime(elapsedTime.elapsed()));

    previousDownloadedFiles = downloadedFiles;
  }
}
