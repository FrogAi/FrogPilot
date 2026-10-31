#include "frogpilot/ui/qt/onroad/frogpilot_buttons.h"

#include "frogpilot/ui/qt/onroad/screen_recorder.h"

DrivingPersonalityButton::DrivingPersonalityButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(personality_button_width, btn_size);

  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &DrivingPersonalityButton::updateTheme);
  QObject::connect(this, &QPushButton::pressed, [] {
    frogpilotUIState()->setDistanceButtonPressed(true);
  });
  QObject::connect(this, &QPushButton::released, [] {
    frogpilotUIState()->setDistanceButtonPressed(false);
  });
}

void DrivingPersonalityButton::showEvent(QShowEvent *event) {
  updateTheme();
}

void DrivingPersonalityButton::hideEvent(QHideEvent *event) {
  setDown(false);
  frogpilotUIState()->setDistanceButtonPressed(false);
  currentGif.reset();

  QPushButton::hideEvent(event);
}

void DrivingPersonalityButton::updateTheme() {
  currentGif.clear();
  currentIcon.clear();
  currentImg = QPixmap();
}

void DrivingPersonalityButton::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible()) {
    return;
  }

  static const QString personalityIcons[] = {"aggressive", "standard", "relaxed"};
  static const QString trafficIcon = "traffic";

  const QString &icon = fs.frogpilot_scene.traffic_mode_enabled ? trafficIcon : personalityIcons[static_cast<int>(s.scene.personality)];
  if (icon == currentIcon) {
    return;
  }
  currentIcon = icon;

  loadImage("../../frogpilot/assets/active_theme/distance_icons/" + icon, currentImg, currentGif, QSize(btn_size, btn_size), this);
}

void DrivingPersonalityButton::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  QPoint center(rightHandDM ? width() / 2 - UI_BORDER_SIZE / 2 : width() / 2 + UI_BORDER_SIZE / 2, height() / 2);
  drawIcon(p, center, currentGif ? currentGif->currentPixmap() : currentImg, Qt::transparent, 1.0);
}

InstantReplayButton::InstantReplayButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size, btn_size / 3 + 10);

  QObject::connect(this, &QPushButton::clicked, &ScreenRecorder::saveReplay);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &InstantReplayButton::updateState);
}

void InstantReplayButton::updateState(const UIState &s, const FrogPilotUIState &fs) {
  ScreenRecorder::setReplayDuration(s.scene.started ? fs.frogpilot_scene.frogpilot_toggles.value(QLatin1String("instant_replay")).toInt() : 0);

  if (!isVisible()) {
    return;
  }

  bool ready = ScreenRecorder::replayReady();
  bool saving = ScreenRecorder::replaySaving();
  setEnabled(ready && !saving);

  if (saving) {
    setText(tr("SAVING..."));
  } else if (ready) {
    setText(tr("CAPTURE"));
  } else {
    setText(tr("BUFFERING..."));
  }
}

void InstantReplayButton::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  p.setBrush(QColor(0, 0, 0, 166));
  p.setPen(QPen(QColor(0, 150, 255), 8));

  QRect button_rect = rect().adjusted(10, 5, -10, -5);
  p.drawRoundedRect(button_rect, 24, 24);

  p.setFont(fitInterFont(25, QFont::DemiBold, button_rect.width() - 16, {text()}));
  p.setPen(Qt::white);
  p.drawText(button_rect, Qt::AlignCenter, text());
}

ScreenRecorderButton::ScreenRecorderButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size, btn_size / 3 + 10);

  QObject::connect(this, &QPushButton::clicked, [this] {
    if (ScreenRecorder::active()) {
      ScreenRecorder::stop();
    } else {
      ScreenRecorder::start();
    }
    update();
  });
  QObject::connect(uiState(), &UIState::offroadTransition, &ScreenRecorder::stop);
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::togglesUpdated, this, [] {
    if (!frogpilotUIState()->frogpilot_scene.frogpilot_toggles.value(QLatin1String("screen_recorder")).toBool()) {
      ScreenRecorder::stop();
    }
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, &ScreenRecorderButton::updateState);
}

void ScreenRecorderButton::updateState() {
  if (recording || ScreenRecorder::active()) {
    update();
  }
}

void ScreenRecorderButton::paintEvent(QPaintEvent *event) {
  recording = ScreenRecorder::active();

  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  if (recording) {
    qreal phase = (QDateTime::currentMSecsSinceEpoch() % 2000) / 2000.0 * 2 * M_PI;
    qreal alpha_factor = 0.5 + 0.5 * sin(phase);

    QColor glow_color(201, 34, 49);
    glow_color.setAlphaF(0.3 + 0.7 * alpha_factor);

    p.setBrush(QColor(201, 34, 49));
    p.setPen(QPen(glow_color, 8 + static_cast<int>(2 * alpha_factor)));
  } else {
    p.setBrush(QColor(0, 0, 0, 166));
    p.setPen(QPen(QColor(201, 34, 49), 8));
  }

  const int centering_offset = 10;
  QRect button_rect = rect().adjusted(centering_offset, 5, -centering_offset, -5);
  p.drawRoundedRect(button_rect, 24, 24);

  QRect text_rect = button_rect.adjusted(centering_offset, 0, -centering_offset, 0);
  QString label = recording ? tr("RECORDING") : tr("RECORD");
  p.setFont(fitInterFont(25, recording ? QFont::Bold : QFont::DemiBold, text_rect.width() - (recording ? 0 : btn_size / 5), {label}));
  p.setPen(QPen(Qt::white, 6));
  p.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, label);

  if (!recording) {
    p.setBrush(QColor(201, 34, 49, 166));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(button_rect.right() - btn_size / 10 - centering_offset, button_rect.center().y()), btn_size / 10, btn_size / 10);
  }
}
