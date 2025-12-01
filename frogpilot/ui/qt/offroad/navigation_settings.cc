#include "frogpilot/ui/qt/offroad/navigation_settings.h"

FrogPilotNavigationPanel::FrogPilotNavigationPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  navigationLayout = new QStackedLayout();
  addItem(navigationLayout);

  FrogPilotListWidget *settingsList = new FrogPilotListWidget(this);
  ipLabel = new LabelControl(tr("Manage Your Settings At"), tr("Offline..."), tr("<b>Open this address in a browser on the same Wi-Fi to reach \"The Pond\", where you manage your drives, recordings, themes, and more from a phone or computer.</b>"));
  settingsList->addItem(ipLabel);

  pairingToggle = new ParamControl("ThePondPairing", tr("Pairing Code"), tr("<b>Ask for a code before a browser can use \"The Pond\".</b><br><br>Without a code, any device on the same Wi-Fi or Tailscale network can open your drives, camera footage, logs, location, and saved places, and can change your settings, unlock your doors, reboot the device, switch branches, run any command on the device from its shell, restore a backup that replaces its software, or sign the device into a Tailscale account. The code shows next to the address above. Turning this off signs every browser out, and turning it back on makes a new code."), "");
  settingsList->addItem(pairingToggle);

  publicMapboxKeyControl = new FrogPilotButtonsControl(tr("Public Mapbox Key"), tr("<b>Lets openpilot get speed limits from Mapbox.</b><br><br>You make this key yourself on Mapbox's website. \"Mapbox Setup Instructions\" walks you through it."), "", {tr("ADD"), tr("TEST")});
  settingsList->addItem(publicMapboxKeyControl);

  secretMapboxKeyControl = new FrogPilotButtonsControl(tr("Secret Mapbox Key"), tr("<b>Draws the map itself on your driving screen, alongside what the \"Public Mapbox Key\" does for searching.</b><br><br>Keep this one to yourself, since it can be used to run up charges on your Mapbox account. After you add it you are asked whether to reboot now or later, and it does not take effect until the device has actually rebooted."), "", {tr("ADD"), tr("TEST")});
  settingsList->addItem(secretMapboxKeyControl);

  setupButton = new ButtonControl(tr("Mapbox Setup Instructions"), tr("VIEW"), tr("<b>Walks you through getting your own free Mapbox keys.</b><br><br>The guide only shows the steps for where you are in setup, so it changes as you add each key. Tap the instructions to come back here."), this);
  settingsList->addItem(setupButton);

  speedLimitFillerToggle = new ParamControl("SpeedLimitFiller", tr("Speed Limit Filler"), tr("<b>Learns speed limits from the signs the camera reads on roads where your downloaded maps have no limit or the wrong one.</b><br><br>Each learned limit is saved for that road and direction and used in place of the map's limit the next time you drive there, and it is forgotten once the map matches the signs. It also keeps a cropped photo of the sign each learned limit came from. Roads with conditional limits, such as ones that change with the time of day or the weather, are skipped. Needs downloaded maps. With \"Share FrogPilot Data\" on, the learned limits, their sign photos, and the roads where the signs matched your map are sent to FrogPilot while parked on unmetered Wi-Fi or Ethernet and used to improve speed limits for everyone using FrogPilot."), "");
  settingsList->addItem(speedLimitFillerToggle);

  ScrollView *settingsPanel = new ScrollView(settingsList, this);
  navigationLayout->addWidget(settingsPanel);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [this]() {
    if (forceOpenDescriptions) {
      ipLabel->showDescription();
      pairingToggle->showDescription();
      publicMapboxKeyControl->showDescription();
      secretMapboxKeyControl->showDescription();
      setupButton->showDescription();
      speedLimitFillerToggle->showDescription();
    }
  });
}

void FrogPilotNavigationPanel::showEvent(QShowEvent *event) {
  if (forceOpenDescriptions) {
    ipLabel->showDescription();
    pairingToggle->showDescription();
    publicMapboxKeyControl->showDescription();
    secretMapboxKeyControl->showDescription();
    setupButton->showDescription();
    speedLimitFillerToggle->showDescription();
  }

  pairingToggle->setVisible(parent->tuningLevel >= parent->frogpilotToggleLevels.value("ThePondPairing").toDouble());
  speedLimitFillerToggle->setVisible(parent->tuningLevel >= parent->frogpilotToggleLevels.value("SpeedLimitFiller").toDouble());
}
