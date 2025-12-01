#include "frogpilot/ui/qt/offroad/data_settings.h"

FrogPilotDataPanel::FrogPilotDataPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent) {
  QStackedLayout *dataLayout = new QStackedLayout();
  addItem(dataLayout);

  FrogPilotListWidget *dataMainList = new FrogPilotListWidget(this);
  ScrollView *dataMainPanel = new ScrollView(dataMainList, this);
  dataLayout->addWidget(dataMainPanel);

  accountButton = new FrogPilotButtonsControl(tr("FrogPilot.com Account"), tr("<b>Link this device to your FrogPilot.com account, or delete the data FrogPilot keeps about it.</b><br><br>A linked device's stats and reports show in your account. Both show a code to scan with your phone."), "", {tr("LINK"), tr("DELETE"), tr("UNLINK")});
  accountButton->setVisible(false);
  dataMainList->addItem(accountButton);

  QSizePolicy accountButtonPolicy = accountButton->sizePolicy();
  accountButtonPolicy.setRetainSizeWhenHidden(true);
  accountButton->setSizePolicy(accountButtonPolicy);
}
