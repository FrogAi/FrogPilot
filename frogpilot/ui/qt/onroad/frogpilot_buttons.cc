#include "frogpilot/ui/qt/onroad/frogpilot_buttons.h"

DrivingPersonalityButton::DrivingPersonalityButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size + UI_BORDER_SIZE, btn_size);

  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &DrivingPersonalityButton::updateTheme);
  QObject::connect(this, &QPushButton::pressed, [] {frogpilotUIState()->setDistanceButtonPressed(true);});
  QObject::connect(this, &QPushButton::released, [] {frogpilotUIState()->setDistanceButtonPressed(false);});
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

  drawIcon(p, rect().center() + QPoint(UI_BORDER_SIZE / 2, 0), currentGif ? currentGif->currentPixmap() : currentImg, Qt::transparent, 1.0);
}

InstantReplayButton::InstantReplayButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size, btn_size / 3);

  QObject::connect(this, &QPushButton::clicked, &ScreenRecorder::saveReplay);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &InstantReplayButton::updateState);
}

void InstantReplayButton::updateState(const UIState &s, const FrogPilotUIState &fs) {
  ScreenRecorder::setReplayDuration(s.scene.started ? fs.frogpilot_scene.frogpilot_toggles.value(QLatin1String("instant_replay")).toInt() : 0);

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
  p.setPen(QPen(QColor(0, 150, 255), 6));

  QRect button_rect = rect().adjusted(10, 4, -10, -4);
  p.drawRoundedRect(button_rect, 20, 20);

  p.setFont(InterFont(25, QFont::DemiBold));
  int text_width = p.fontMetrics().horizontalAdvance(text());
  if (text_width > button_rect.width() - 16) {
    p.setFont(InterFont(25 * (button_rect.width() - 16) / text_width, QFont::DemiBold));
  }

  p.setPen(Qt::white);
  p.drawText(button_rect, Qt::AlignCenter, text());
}
