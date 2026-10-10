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

  QLabel *title = new QLabel(tr("Help Improve FrogPilot?"));
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

  QPushButton *no_button = new QPushButton(tr("No Thanks"));
  QObject::connect(no_button, &QPushButton::clicked, [this]() {
    answer(false);
  });
  buttons->addWidget(no_button);

  QPushButton *yes_button = new QPushButton(tr("Yes, Share Data"));
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
            "Your everyday drives help us improve steering, acceleration, and braking, fine-tune support for more cars, train the speed limit sign reader, "
            "and find and fix problems. Keeping sharing enabled gives us more real-world examples across cars, roads, and driving conditions.<br><br>"
            "<b>Privacy protection starts on your device.</b> Driving logs are filtered, and the location included in usage stats is generalized, "
            "before they are sent.<br><br>"
            "<b>Driving logs</b> record vehicle behavior and FrogPilot's driving decisions. Before upload, we remove camera footage, audio, driver face data, "
            "account details, SSH keys, navigation routes, and map road names and IDs. Exact GPS coordinates and recording date/time fields are cleared, "
            "and <b>the VIN field is removed entirely</b>. Driver-monitoring data is limited to awareness level and whether your car is right-hand drive. "
            "We also discard CAN messages captured during startup vehicle identification. Each drive gets an ID that hides when it was recorded, "
            "and uploaded logs are linked to your FrogPilot device ID.<br><br>"
            "Raw CAN data recorded during driving is kept to help improve car support. On some cars, these vehicle messages can still contain GPS, VIN, "
            "date/time, or seat occupancy information.<br><br>"
            "<b>Usage stats</b> cover your device, software, car and calibration details, FrogPilot settings, model usage and scores, and driving totals. "
            "<b>Your location is generalized on your device.</b> It reports the nearest city in your country or US state from a list filtered to populations "
            "of <b>50,000 or more</b>. If that region has no qualifying city, it uses its capital instead. "
            "This helps protect location privacy in small communities. For location, these stats include only the resulting city, state, and country, "
            "never your exact GPS coordinates. The VIN is excluded from car details, "
            "and stats are linked to your FrogPilot device ID.<br><br>"
            "<b>Speed limit sign clips</b> are about 10 seconds of road camera video captured on supported cars when the dashboard shows a new speed limit. "
            "No GPS coordinates or recording date/time are attached, and clip IDs hide the original recording time. Each clip includes the speed limit, "
            "camera calibration, and basic device, camera, and software details, and is linked to your FrogPilot device ID. Video is uploaded as recorded "
            "and can show license plates, people, and identifiable places. Clips may be matched to driving logs.<br><br>"
            "<b>Speed Limit Filler</b>, if you use it, shares collected speed limit corrections with road IDs, travel direction, and the source of each limit. "
            "Local map lookup coordinates are excluded from the upload, but road IDs still identify roads you have driven. "
            "These entries are linked to your device.<br><br>"
            "<b>Uploads</b> of driving logs, clips, and Speed Limit Filler data happen only while your car is off, on unmetered Wi-Fi or Ethernet. "
            "Usage stats are sent at startup and after drives and may use other available internet connections.<br><br>"
            "<b>Your choice.</b> You can turn off \"Share FrogPilot Data\" anytime to stop sharing these logs, clips, speed limits, and usage stats. "
            "Basic device registration still sends device type, operating system and software build details, and a public authentication key. "
            "Startup and after-drive check-ins continue without your usage stats, settings, or location. "
            "Crash reports also continue, tagged with your device ID "
            "and software details. Python crash reports do not automatically collect local variable values.");
}
