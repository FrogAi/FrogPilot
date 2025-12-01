#include "frogpilot/ui/qt/offroad/model_settings.h"

FrogPilotModelPanel::FrogPilotModelPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  QStackedLayout *modelLayout = new QStackedLayout();
  addItem(modelLayout);

  FrogPilotListWidget *modelList = new FrogPilotListWidget(this);

  ScrollView *modelPanel = new ScrollView(modelList, this);

  modelLayout->addWidget(modelPanel);

  const std::vector<std::tuple<QString, QString, QString, QString>> modelToggles {
    {"AutomaticallyDownloadModels", tr("Automatically Download New Models"), tr("<b>Download new driving models on their own as they are released, so they are ready when you want to try one.</b><br><br>This runs whenever the device is on Wi-Fi or another unmetered connection, including while you are driving. It also grabs every model that is not already on the device, not just newly released ones, so anything you removed with \"Delete Driving Models\" comes back."), ""},
    {"DeleteModel", tr("Delete Driving Models"), tr("<b>Remove driving models you have downloaded to free up storage.</b><br><br>\"DELETE\" lets you pick which ones, \"DELETE ALL\" removes the rest. The model you are currently using and the one FrogPilot ships with are always kept. Turn \"Automatically Download New Models\" off first, or anything you delete is downloaded again within the hour whenever the device is on Wi-Fi or another unmetered connection."), ""},
    {"DownloadModel", tr("Download Driving Models"), tr("<b>Download driving models onto the device so you can switch to them.</b><br><br>\"DOWNLOAD\" lets you pick which ones, \"DOWNLOAD ALL\" fetches everything. Your car has to be parked and online, and models are large, so this can take a while."), ""},
    {"ManageBlacklistedModels", tr("Manage Model Blacklist"), tr("<b>Stop the \"Model Randomizer\" from picking driving models you did not like.</b><br><br>Blocking a model here has no effect on choosing it yourself under \"Select Driving Model\"."), ""},
    {"ManageScores", tr("Manage Model Ratings"), tr("<b>See how you rated each driving model and how many drives you gave it, or wipe those ratings and start fresh.</b><br><br>These are for your own comparison. The \"Model Randomizer\" picks at random and does not favor your higher-rated models."), ""},
    {"ModelRandomizer", tr("Model Randomizer"), tr("<b>Picks a different driving model for you at the start of every drive, then asks how it went when you park, so you can work out which one you like best.</b><br><br>It only chooses from models you have downloaded and have not blacklisted, and it only asks for a rating after drives longer than 15 minutes. Your ratings are saved under \"Manage Model Ratings\" for you to compare."), ""},
    {"SelectModel", tr("Select Driving Model"), tr("<b>Choose which driving model does the driving.</b><br><br>The model is the part of openpilot that decides how to steer, speed up, and slow down, so switching it changes how the car feels. Only models you have downloaded are listed, and changing it while driving asks you to reboot."), ""}
  };

  for (const auto &[param, title, desc, icon] : modelToggles) {
    AbstractControl *modelToggle;

    if (param == "DeleteModel") {
      modelToggle = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DELETE ALL")});
    } else if (param == "DownloadModel") {
      modelToggle = new FrogPilotButtonsControl(title, desc, icon, {tr("DOWNLOAD"), tr("DOWNLOAD ALL")});
    } else if (param == "ManageBlacklistedModels") {
      modelToggle = new FrogPilotButtonsControl(title, desc, icon, {tr("ADD"), tr("REMOVE"), tr("REMOVE ALL")});
    } else if (param == "ManageScores") {
      modelToggle = new FrogPilotButtonsControl(title, desc, icon, {tr("RESET"), tr("VIEW")});
    } else if (param == "SelectModel") {
      modelToggle = new ButtonControl(title, tr("SELECT"), desc);
    } else {
      modelToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = modelToggle;

    modelList->addItem(modelToggle);
  }

  openDescriptions(forceOpenDescriptions, toggles);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [modelLayout, modelPanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    modelLayout->setCurrentWidget(modelPanel);
  });
}

void FrogPilotModelPanel::showEvent(QShowEvent *event) {
  updateToggles();
}

void FrogPilotModelPanel::updateToggles() {
  for (auto &[key, toggle] : toggles) {
    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels.value(key).toDouble();

    if (key == "ManageBlacklistedModels" || key == "ManageScores") {
      setVisible &= (parent->tuningLevel >= parent->frogpilotToggleLevels.value("ModelRandomizer").toDouble() && params.getBool("ModelRandomizer"));
    }

    else if (key == "SelectModel") {
      setVisible &= !(parent->tuningLevel >= parent->frogpilotToggleLevels.value("ModelRandomizer").toDouble() && params.getBool("ModelRandomizer"));
    }

    toggle->setVisible(setVisible);
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
