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
  settingsList->addItem(downloadMapsButton);

  lastMapsDownload = new LabelControl(tr("Last Updated"));
  settingsList->addItem(lastMapsDownload);

  selectMaps = new FrogPilotButtonsControl(tr("Map Sources"),
                                           tr("<b>Pick the countries or U.S. states you drive in, so openpilot knows their speed limits.</b><br><br>Map data within about 60 miles (100 km) of where you park downloads on its own, so pick only what covers the rest of your driving."),
                                           "", {tr("COUNTRIES"), tr("STATES")});
  settingsList->addItem(selectMaps);

  downloadStatus = new LabelControl(tr("Progress"));
  settingsList->addItem(downloadStatus);

  downloadTimeElapsed = new LabelControl(tr("Time Elapsed"));
  settingsList->addItem(downloadTimeElapsed);

  downloadETA = new LabelControl(tr("Time Remaining"));
  settingsList->addItem(downloadETA);

  removeMapsButton = new ButtonControl(tr("Remove Maps"), tr("REMOVE"), tr("<b>Delete your downloaded map data and clear the places you picked under \"Map Sources\", to free up storage.</b><br><br>Only the map data around where you park comes back on its own, the next time the device is on Wi-Fi. Everywhere else, \"Speed Limit Controller\" has no map speed limits until you pick your places again and start a new download."));
  settingsList->addItem(removeMapsButton);

  lastMapsDownload->setVisible(false);
  removeMapsButton->setVisible(false);

  mapsSize = new LabelControl(tr("Storage Used"));
  settingsList->addItem(mapsSize);

  ScrollView *settingsPanel = new ScrollView(settingsList, this);
  mapsLayout->addWidget(settingsPanel);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [mapsLayout, settingsPanel, this] {
    if (forceOpenDescriptions) {
      downloadMapsButton->showDescription();
      preferredSchedule->showDescription();
      removeMapsButton->showDescription();
      selectMaps->showDescription();
    }

    mapsLayout->setCurrentWidget(settingsPanel);
  });
}

void FrogPilotMapsPanel::showEvent(QShowEvent *event) {
  if (forceOpenDescriptions) {
    downloadMapsButton->showDescription();
    preferredSchedule->showDescription();
    removeMapsButton->showDescription();
    selectMaps->showDescription();
  }
}
