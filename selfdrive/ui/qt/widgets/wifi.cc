#include "selfdrive/ui/qt/widgets/wifi.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>

#include "selfdrive/ui/ui.h"

WiFiPromptWidget::WiFiPromptWidget(QWidget *parent) : QFrame(parent) {
  // Setup Firehose Mode
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(56, 40, 56, 40);
  main_layout->setSpacing(42);  
  
  QLabel *title = new QLabel(tr("<span style='font-family: \"Noto Color Emoji\";'>🔥</span> Firehose Mode <span style='font-family: Noto Color Emoji;'>🔥</span>"));
  title->setStyleSheet("font-size: 64px; font-weight: 500;");
  main_layout->addWidget(title);

  QLabel *desc = new QLabel(tr("Maximize your training data uploads to improve openpilot's driving models."));
  desc->setStyleSheet("font-size: 40px; font-weight: 400;");
  desc->setWordWrap(true);
  main_layout->addWidget(desc);

  QPushButton *settings_btn = new QPushButton(tr("Open"));
  connect(settings_btn, &QPushButton::clicked, [=]() { emit openSettings(1, "FirehosePanel"); });
  settings_btn->setStyleSheet(R"(
    QPushButton {
      font-size: 48px;
      font-weight: 500;
      border-radius: 10px;
      background-color: #465BEA;
      padding: 32px;
    }
    QPushButton:pressed {
      background-color: #3049F4;
    }
  )");
  main_layout->addWidget(settings_btn);

  // FrogPilot variables
  QWidget *uploading_disabled = new QWidget;
  QVBoxLayout *uploading_disabled_layout = new QVBoxLayout(uploading_disabled);
  uploading_disabled_layout->setContentsMargins(0, 0, 0, 0);
  uploading_disabled_layout->setSpacing(42);

  QHBoxLayout *uploading_disabled_title_layout = new QHBoxLayout;
  QLabel *uploading_disabled_title = new QLabel(tr("Uploading disabled"));
  uploading_disabled_title->setStyleSheet("font-size: 64px; font-weight: 500;");
  uploading_disabled_title->setWordWrap(true);
  uploading_disabled_title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  uploading_disabled_title_layout->addWidget(uploading_disabled_title);
  uploading_disabled_title_layout->addStretch();

  QLabel *uploading_disabled_icon = new QLabel;
  QPixmap uploading_disabled_pixmap("../../frogpilot/assets/other_images/icon_wifi_uploading_disabled.svg");
  uploading_disabled_icon->setPixmap(uploading_disabled_pixmap.scaledToWidth(120, Qt::SmoothTransformation));
  uploading_disabled_title_layout->addWidget(uploading_disabled_icon);
  uploading_disabled_layout->addLayout(uploading_disabled_title_layout);

  QLabel *uploading_disabled_desc = new QLabel(tr("Turn off \"Disable Uploads\" to upload your drives again."));
  uploading_disabled_desc->setStyleSheet("font-size: 40px; font-weight: 400;");
  uploading_disabled_desc->setWordWrap(true);
  uploading_disabled_layout->addWidget(uploading_disabled_desc);

  uploading_disabled->setVisible(false);
  main_layout->addWidget(uploading_disabled);

  QObject::connect(frogpilotUIState(), &FrogPilotUIState::togglesUpdated, this, [=]() {
    const QJsonObject &frogpilot_toggles = frogpilotUIState()->frogpilot_scene.frogpilot_toggles;
    bool uploads_disabled = frogpilot_toggles.value("uploads_disabled").toBool();
    title->setVisible(!uploads_disabled);
    desc->setVisible(!uploads_disabled);
    settings_btn->setVisible(!uploads_disabled);
    uploading_disabled->setVisible(uploads_disabled);
  });

  setStyleSheet(R"(
    WiFiPromptWidget {
      background-color: #333333;
      border-radius: 10px;
    }
  )");
}