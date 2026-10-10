#include "selfdrive/ui/qt/widgets/scrollview.h"

#include "frogpilot/ui/qt/offroad/telemetry_page.h"
#include "frogpilot/ui/frogpilot_ui.h"

void FrogPilotTelemetryPage::showEvent(QShowEvent *event) {
  if (layout()) {
    return;
  }

  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(45, 35, 45, 45);
  main_layout->setSpacing(0);

  QVBoxLayout *vlayout = new QVBoxLayout();
  vlayout->setContentsMargins(165, 165, 165, 45);
  main_layout->addLayout(vlayout);

  QLabel *title = new QLabel(tr("Share FrogPilot Data?"));
  title->setStyleSheet("font-size: 90px; font-weight: 500;");
  vlayout->addWidget(title, 0, Qt::AlignTop | Qt::AlignLeft);

  vlayout->addSpacing(90);

  QLabel *text = new QLabel(description());
  text->setStyleSheet("font-size: 50px; font-weight: 300; color: white; background-color: black;");
  text->setWordWrap(true);
  vlayout->addWidget(new ScrollView(text, this), 1);

  QHBoxLayout *buttons = new QHBoxLayout();
  buttons->setMargin(0);
  buttons->setSpacing(45);
  main_layout->addLayout(buttons);

  QPushButton *no_button = new QPushButton(tr("No"));
  QObject::connect(no_button, &QPushButton::clicked, [this]() {
    answer(false);
  });
  buttons->addWidget(no_button);

  QPushButton *yes_button = new QPushButton(tr("Yes"));
  yes_button->setStyleSheet(R"(
    QPushButton {
      background-color: #465BEA;
    }
    QPushButton:pressed {
      background-color: #3049F4;
    }
  )");
  QObject::connect(yes_button, &QPushButton::clicked, [this]() {
    answer(true);
  });
  buttons->addWidget(yes_button);
}

void FrogPilotTelemetryPage::answer(bool share) {
  params.putBool("FrogPilotTelemetry", share);
  params.putBool("FrogPilotTelemetryConfirmed", true);

  frogpilotUIState()->updateToggles();

  emit answered();
}

QString FrogPilotTelemetryPage::description() {
  return tr("<b>Help make FrogPilot better by sharing filtered driving logs, short speed limit sign clips, and usage stats.</b><br><br>"
            "This data is how FrogPilot gets better at driving your car. It's used to make steering smoother and more precise, tune how FrogPilot uses the gas "
            "and brakes so it speeds up, slows down, and stops more naturally, add and fine-tune support for more cars, train features like the speed limit "
            "sign reader, and find and fix problems faster. Every driver who shares makes FrogPilot drive better for everyone.<br><br>"
            "<b>Your privacy comes first.</b> Driving logs are filtered on your device before they leave, and logs and clips upload only on unmetered Wi-Fi or Ethernet "
            "while your car is off, so they never use your data plan or slow a drive.<br><br>"
            "<b>Driving logs</b> have camera footage, account details, SSH keys, and the exact GPS location and date/time removed. The last six VIN characters are masked, "
            "and the rest remains. Each drive gets its own random ID. They're stored under your FrogPilot device ID. "
            "Raw CAN data from your car is kept because it's essential for adding and improving car support, "
            "and on some cars it can still contain GPS, VIN, date/time, or who's in the seats.<br><br>"
            "<b>Speed Limit Filler</b> (if you use it) shares the speed limits it collects: a list of the roads you've driven and in which direction, linked to your device.<br><br>"
            "<b>Speed limit sign clips</b> are about 10 seconds of road camera video, saved when your dashboard shows a new speed limit. No GPS location or date and time "
            "is attached, and each clip gets an ID that doesn't reveal when it was recorded. Each is sent with that speed limit and basic details about your device, "
            "its camera, and its software, and stored under your FrogPilot device ID. The video itself shows where you were and can include license plates, people, "
            "and places, and a clip can be matched to its driving log.<br><br>"
            "<b>Usage stats</b> cover your device, software version, car, FrogPilot settings, and driving totals, with only your city, state, and country, never your "
            "exact location. They're linked to your FrogPilot device ID.<br><br>"
            "Turn this off anytime to stop all of these. Your device will still send its type and software version, check in when it starts and after each drive "
            "so we can count active devices, and send crash reports tagged with your device ID. None of these include your stats, settings, or location.");
}
