#include <regex>

#include <QtConcurrent>

#include "selfdrive/frogpilot/navigation/ui/maps_settings.h"

FrogPilotMapsPanel::FrogPilotMapsPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent), mapsFolderPath{"/data/media/0/osm/offline"} {
  QVBoxLayout *mainLayout = new QVBoxLayout();
  addItem(mainLayout);

  mapsLayout = new QStackedLayout();
  mainLayout->addLayout(mapsLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);

  std::vector<QString> scheduleOptions{tr("Manually"), tr("Weekly"), tr("Monthly")};
  ButtonParamControl *preferredSchedule = new ButtonParamControl("PreferredSchedule", tr("Automatically Update Maps"),
                                          tr("Controls the frequency at which maps update with the latest OpenStreetMap (OSM) changes. "
                                             "Weekly updates begin at midnight every Sunday, while monthly updates start at midnight on the 1st of each month."),
                                             "",
                                             scheduleOptions);
  settingsList->addItem(preferredSchedule);

  FrogPilotButtonsControl *selectMaps = new FrogPilotButtonsControl(tr("Select Map Data Sources"),
                                                                    tr("Select map data sources to use with 'Curve Speed Control' and 'Speed Limit Controller'."),
                                                                    "", {tr("COUNTRIES"), tr("STATES")});
  QObject::connect(selectMaps, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    mapsLayout->setCurrentIndex(id + 1);
    openMapSelection();
  });
  settingsList->addItem(selectMaps);

  downloadMapsButton = new ButtonControl(tr("Download Maps"), tr("DOWNLOAD"), tr("Downloads the selected maps to use with 'Curve Speed Control' and 'Speed Limit Controller'."));
  QObject::connect(downloadMapsButton, &ButtonControl::clicked, [this] {
    if (downloadMapsButton->text() == tr("CANCEL")) {
      if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to cancel the download?"), this)) {
        cancelDownload();
      }
    } else {
      startDownload();
    }
  });
  settingsList->addItem(downloadMapsButton);

  settingsList->addItem(downloadETA = new LabelControl(tr("Download Completion ETA")));
  settingsList->addItem(downloadStatus = new LabelControl(tr("Download Progress")));
  settingsList->addItem(downloadTimeElapsed = new LabelControl(tr("Download Time Elapsed")));
  settingsList->addItem(lastMapsDownload = new LabelControl(tr("Maps Last Updated"), params.get("LastMapsUpdate").empty() ? "Never" : QString::fromStdString(params.get("LastMapsUpdate"))));
  settingsList->addItem(mapsSize = new LabelControl(tr("Downloaded Maps Size"), calculateDirectorySize(mapsFolderPath)));

  downloadETA->setVisible(false);
  downloadStatus->setVisible(false);
  downloadTimeElapsed->setVisible(false);

  removeMapsButton = new ButtonControl(tr("Remove Maps"), tr("REMOVE"), tr("Removes downloaded maps to clear up storage space."));
  QObject::connect(removeMapsButton, &ButtonControl::clicked, [this] {
    if (FrogPilotConfirmationDialog::yesorno(tr("Are you sure you want to delete all of your downloaded maps?"), this)) {
      std::thread([this] {
        mapsSize->setText("0 MB");

        std::system("rm -rf /data/media/0/osm/offline");
      }).detach();
    }
  });
  settingsList->addItem(removeMapsButton);

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

  QObject::connect(parent, &FrogPilotSettingsWindow::closeMapSelection, [this] {
    std::string mapsSelected = params.get("MapsSelected");
    hasMapsSelected = !QJsonDocument::fromJson(QByteArray::fromStdString(mapsSelected)).object().value("nations").toArray().isEmpty();
    hasMapsSelected |= !QJsonDocument::fromJson(QByteArray::fromStdString(mapsSelected)).object().value("states").toArray().isEmpty();

    mapsLayout->setCurrentIndex(0);
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotMapsPanel::updateState);
}

void FrogPilotMapsPanel::showEvent(QShowEvent *event) {
  std::string mapsSelected = params.get("MapsSelected");
  hasMapsSelected = !QJsonDocument::fromJson(QByteArray::fromStdString(mapsSelected)).object().value("nations").toArray().isEmpty();
  hasMapsSelected |= !QJsonDocument::fromJson(QByteArray::fromStdString(mapsSelected)).object().value("states").toArray().isEmpty();

  removeMapsButton->setVisible(QDir(mapsFolderPath).exists());

  std::string osmDownloadProgress = params.get("OSMDownloadProgress");
  if (!osmDownloadProgress.empty()) {
    downloadMapsButton->setText(tr("CANCEL"));
    downloadStatus->setText("Calculating...");

    downloadStatus->setVisible(true);

    lastMapsDownload->setVisible(false);
    removeMapsButton->setVisible(false);

    updateDownloadLabels(osmDownloadProgress);
  }
}


void FrogPilotMapsPanel::updateState(const UIState &s) {
  if (!isVisible() || s.sm->frame % (UI_FREQ / 2) != 0) {
    return;
  }

  downloadMapsButton->setEnabled(!cancellingDownload && hasMapsSelected && s.scene.online);

  std::string osmDownloadProgress = params.get("OSMDownloadProgress");
  if (!osmDownloadProgress.empty() && !cancellingDownload) {
    updateDownloadLabels(osmDownloadProgress);
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
    removeMapsButton->setVisible(QDir(mapsFolderPath).exists());

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
