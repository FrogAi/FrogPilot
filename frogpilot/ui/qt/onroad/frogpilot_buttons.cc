#include "frogpilot/ui/qt/onroad/frogpilot_buttons.h"

DistanceButton::DistanceButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size + UI_BORDER_SIZE, btn_size);

  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &DistanceButton::updateTheme);
  QObject::connect(this, &QPushButton::pressed, [this] {params_memory.putBool("OnroadDistanceButtonPressed", true);});
  QObject::connect(this, &QPushButton::released, [this] {params_memory.putBool("OnroadDistanceButtonPressed", false);});
}

void DistanceButton::showEvent(QShowEvent *event) {
  updateTheme();
}

void DistanceButton::updateTheme() {
  for (QMap<int, QPair<QPixmap, QSharedPointer<QMovie>>>::iterator it = icon_map.begin(); it != icon_map.end(); ++it) {
    QSharedPointer<QMovie> movie = it.value().second;
    if (!movie.isNull()) {
      QObject::disconnect(movie.data(), nullptr, this, nullptr);
      movie->stop();
    }
  }

  icon_map.clear();

  QPixmap traffic_img, aggressive_img, standard_img, relaxed_img;
  QSharedPointer<QMovie> traffic_gif, aggressive_gif, standard_gif, relaxed_gif;

  loadImage("../../frogpilot/assets/active_theme/distance_icons/traffic", traffic_img, traffic_gif, QSize(btn_size, btn_size), this);
  loadImage("../../frogpilot/assets/active_theme/distance_icons/aggressive", aggressive_img, aggressive_gif, QSize(btn_size, btn_size), this);
  loadImage("../../frogpilot/assets/active_theme/distance_icons/standard", standard_img, standard_gif, QSize(btn_size, btn_size), this);
  loadImage("../../frogpilot/assets/active_theme/distance_icons/relaxed", relaxed_img, relaxed_gif, QSize(btn_size, btn_size), this);

  icon_map.insert(0, qMakePair(traffic_img, traffic_gif));
  icon_map.insert(1, qMakePair(aggressive_img, aggressive_gif));
  icon_map.insert(2, qMakePair(standard_img, standard_gif));
  icon_map.insert(3, qMakePair(relaxed_img, relaxed_gif));
}

void DistanceButton::updateState(const UIScene &scene, const FrogPilotUIScene &frogpilot_scene) {
  bool state_changed = (traffic_mode_active != frogpilot_scene.traffic_mode_enabled) ||
                       (personality != static_cast<int>(scene.personality) + 1 && !traffic_mode_active);

  if (!state_changed) {
    return;
  }

  personality = static_cast<int>(scene.personality) + 1;
  traffic_mode_active = frogpilot_scene.traffic_mode_enabled;

  update();
}

void DistanceButton::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  QPair<QPixmap, QSharedPointer<QMovie>> icon = icon_map.value(traffic_mode_active ? 0 : personality);
  QPixmap img = icon.first;
  QMovie *gif = icon.second.data();

  drawIcon(p, rect().center() + QPoint(UI_BORDER_SIZE / 2, 0), gif ? gif->currentPixmap() : img, Qt::transparent, 1.0);
}
