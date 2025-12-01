#include "frogpilot/ui/qt/onroad/frogpilot_buttons.h"

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

  const QString &icon = personalityIcons[static_cast<int>(s.scene.personality)];
  if (icon == currentIcon) {
    return;
  }
  currentIcon = icon;

  loadImage("../../frogpilot/assets/active_theme/distance_icons/" + icon, currentImg, currentGif, QSize(btn_size, btn_size), this, false);
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

  p.setBrush(blackColor(166));
  p.setPen(QPen(QColor(0, 150, 255), 8));

  QRect button_rect = rect().adjusted(10, 5, -10, -5);
  p.drawRoundedRect(button_rect, 24, 24);

  p.setFont(fitInterFont(25, QFont::DemiBold, button_rect.width() - 16, {text()}));
  p.setPen(Qt::white);
  p.drawText(button_rect, Qt::AlignCenter, text());
}
