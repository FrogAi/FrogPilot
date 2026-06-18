#include "selfdrive/ui/qt/util.h"

#include <cmath>

#include <QDateTime>

#include "frogpilot/ui/qt/onroad/frogpilot_buttons.h"
#include "frogpilot/ui/qt/onroad/screen_recorder.h"

DrivingPersonalityButton::DrivingPersonalityButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size + UI_BORDER_SIZE, btn_size);

  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &DrivingPersonalityButton::updateTheme);
  QObject::connect(this, &QPushButton::pressed, [this] {params_memory.putBool("OnroadDistanceButtonPressed", true);});
  QObject::connect(this, &QPushButton::released, [this] {params_memory.putBool("OnroadDistanceButtonPressed", false);});
}

void DrivingPersonalityButton::showEvent(QShowEvent *event) {
  updateTheme();
}

void DrivingPersonalityButton::updateTheme() {
  currentGif.clear();
  currentImg = QPixmap();

  theme_updated = true;
}

void DrivingPersonalityButton::updateState(const UIState &s, const FrogPilotUIState &fs) {
  const UIScene &scene = s.scene;

  const SubMaster &fpsm = *(fs.sm);

  const cereal::FrogPilotCarState::Reader &frogpilotCarState = fpsm["frogpilotCarState"].getFrogpilotCarState();

  bool new_traffic_mode_active = frogpilotCarState.getTrafficModeEnabled();

  int new_personality = static_cast<int>(scene.personality) + 1;

  bool state_changed = (traffic_mode_active != new_traffic_mode_active) ||
                       (personality != new_personality && !new_traffic_mode_active);

  if (!state_changed && !theme_updated) {
    return;
  }

  traffic_mode_active = new_traffic_mode_active;

  personality = new_personality;

  theme_updated = false;

  QString icon;
  if (traffic_mode_active) {
    icon = "traffic";
  } else if (personality == 1) {
    icon = "aggressive";
  } else if (personality == 2) {
    icon = "standard";
  } else if (personality == 3) {
    icon = "relaxed";
  }

  loadImage("../../frogpilot/assets/active_theme/distance_icons/" + icon, currentImg, currentGif, QSize(btn_size, btn_size), this);
}

void DrivingPersonalityButton::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  drawIcon(p, rect().center() + QPoint(UI_BORDER_SIZE / 2, 0), currentGif ? currentGif->currentPixmap() : currentImg, Qt::transparent, 1.0);
}

ScreenRecorderButton::ScreenRecorderButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size, btn_size);

  QObject::connect(this, &QPushButton::clicked, [this] {
    if (ScreenRecorder::active()) {
      ScreenRecorder::stop();
    } else {
      ScreenRecorder::start();
    }
    update();
  });
  QObject::connect(uiState(), &UIState::offroadTransition, this, [](bool offroad) {
    if (offroad) {
      ScreenRecorder::stop();
    }
  });
  QObject::connect(uiState(), &UIState::uiUpdate, this, [this] {
    if (ScreenRecorder::active()) {
      update();
    }
  });
}

void ScreenRecorderButton::paintEvent(QPaintEvent *event) {
  bool recording = ScreenRecorder::active();

  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  if (recording) {
    qreal phase = (QDateTime::currentMSecsSinceEpoch() % 2000) / 2000.0 * 2 * M_PI;
    qreal alpha_factor = 0.5 + 0.5 * sin(phase);

    QColor glow_color(201, 34, 49);
    glow_color.setAlphaF(0.3 + 0.7 * alpha_factor);

    p.setBrush(QColor(201, 34, 49));
    p.setFont(InterFont(25, QFont::Bold));
    p.setPen(QPen(glow_color, 8 + static_cast<int>(2 * alpha_factor)));
  } else {
    p.setBrush(QColor(0, 0, 0, 166));
    p.setFont(InterFont(25, QFont::DemiBold));
    p.setPen(QPen(QColor(201, 34, 49), 8));
  }

  const int centering_offset = 10;
  QRect button_rect(centering_offset, btn_size / 3, btn_size - centering_offset * 2, btn_size / 3);
  p.drawRoundedRect(button_rect, 24, 24);

  QRect text_rect = button_rect.adjusted(centering_offset, 0, -centering_offset, 0);
  p.setPen(QPen(Qt::white, 6));
  p.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, recording ? tr("RECORDING") : tr("RECORD"));

  if (!recording) {
    p.setBrush(QColor(201, 34, 49, 166));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(button_rect.right() - btn_size / 10 - centering_offset, button_rect.center().y()), btn_size / 10, btn_size / 10);
  }
}
