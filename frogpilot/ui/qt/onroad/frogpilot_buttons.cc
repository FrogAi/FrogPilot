#include "frogpilot/ui/qt/onroad/frogpilot_buttons.h"

#include <cmath>

#include <QDateTime>

#include "frogpilot/ui/qt/onroad/screen_recorder.h"
#include "selfdrive/ui/qt/util.h"

DistanceButton::DistanceButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size + UI_BORDER_SIZE, btn_size);

  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &DistanceButton::updateTheme);
  QObject::connect(this, &QPushButton::pressed, [this] {params_memory.putBool("OnroadDistanceButtonPressed", true);});
  QObject::connect(this, &QPushButton::released, [this] {params_memory.putBool("OnroadDistanceButtonPressed", false);});
}

void DistanceButton::showEvent(QShowEvent *event) {
  updateTheme();
}

void DistanceButton::hideEvent(QHideEvent *event) {
  setDown(false);

  params_memory.putBool("OnroadDistanceButtonPressed", false);

  clearMovie(icon_gif, this);

  QPushButton::hideEvent(event);
}

void DistanceButton::updateTheme() {
  if (!isVisible()) {
    return;
  }

  static const QStringList icon_names = {"traffic", "aggressive", "standard", "relaxed"};

  const QString icon_name = icon_names.value(traffic_mode_active ? 0 : personality);

  loadImage("../../frogpilot/assets/active_theme/distance_icons/" + icon_name, icon_img, icon_gif, QSize(btn_size, btn_size), this);
}

void DistanceButton::updateState(const UIScene &scene, const FrogPilotUIScene &frogpilot_scene) {
  bool state_changed = (traffic_mode_active != frogpilot_scene.traffic_mode_enabled) ||
                       (personality != static_cast<int>(scene.personality) + 1 && !traffic_mode_active);

  if (!state_changed) {
    return;
  }

  personality = static_cast<int>(scene.personality) + 1;
  traffic_mode_active = frogpilot_scene.traffic_mode_enabled;

  updateTheme();
}

void DistanceButton::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  drawIcon(p, rect().center() + QPoint(UI_BORDER_SIZE / 2, 0), icon_gif ? icon_gif->currentPixmap() : icon_img, Qt::transparent, 1.0);
}

ScreenRecorderButton::ScreenRecorderButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size, btn_size);

  QObject::connect(this, &QPushButton::clicked, [] {
    if (screenRecorder()->active()) {
      screenRecorder()->stop();
    } else {
      screenRecorder()->start();
    }
  });
  QObject::connect(screenRecorder(), &ScreenRecorder::stateChanged, this, [this] { update(); });
  QObject::connect(uiState(), &UIState::uiUpdate, this, [this] {
    if (screenRecorder()->active()) {
      update();
    }
  });
}

void ScreenRecorderButton::paintEvent(QPaintEvent *event) {
  bool recording = screenRecorder()->active();

  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  if (recording) {
    qreal phase = (QDateTime::currentMSecsSinceEpoch() % 2000) / 2000.0 * 2 * M_PI;
    qreal alphaFactor = 0.5 + 0.5 * sin(phase);

    QColor glowColor(201, 34, 49);
    glowColor.setAlphaF(0.3 + 0.7 * alphaFactor);

    p.setBrush(QColor(201, 34, 49));
    p.setFont(InterFont(25, QFont::Bold));
    p.setPen(QPen(glowColor, 8 + static_cast<int>(2 * alphaFactor)));
  } else {
    p.setBrush(QColor(0, 0, 0, 166));
    p.setFont(InterFont(25, QFont::DemiBold));
    p.setPen(QPen(QColor(201, 34, 49), 8));
  }

  const int centeringOffset = 10;
  QRect buttonRect(centeringOffset, btn_size / 3, btn_size - centeringOffset * 2, btn_size / 3);
  p.drawRoundedRect(buttonRect, 24, 24);

  QRect textRect = buttonRect.adjusted(centeringOffset, 0, -centeringOffset, 0);
  p.setPen(QPen(Qt::white, 6));
  p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, recording ? tr("RECORDING") : tr("RECORD"));

  if (!recording) {
    p.setBrush(QColor(201, 34, 49, 166));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(buttonRect.right() - btn_size / 10 - centeringOffset, buttonRect.center().y()), btn_size / 10, btn_size / 10);
  }
}
