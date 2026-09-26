#include "frogpilot/ui/qt/onroad/frogpilot_buttons.h"

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
