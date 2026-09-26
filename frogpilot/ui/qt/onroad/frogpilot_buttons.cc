#include "frogpilot/ui/qt/onroad/frogpilot_buttons.h"

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
