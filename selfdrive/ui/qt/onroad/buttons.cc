#include "selfdrive/ui/qt/onroad/buttons.h"

#include <QMovie>
#include <QPainter>

#include "selfdrive/ui/qt/util.h"

void drawIcon(QPainter &p, const QPoint &center, const QPixmap &img, const QBrush &bg, float opacity, const int &angle) {
  p.setRenderHint(QPainter::Antialiasing);
  p.setOpacity(1.0);  // bg dictates opacity of ellipse
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(center, btn_size / 2, btn_size / 2);
  p.save();
  p.translate(center);
  p.rotate(angle);
  p.setOpacity(opacity);
  p.drawPixmap(-QPoint(img.width() / 2, img.height() / 2), img);
  p.restore();
  p.setOpacity(1.0);
}

// ExperimentalButton
ExperimentalButton::ExperimentalButton(QWidget *parent) : experimental_mode(false), engageable(false), QPushButton(parent) {
  setFixedSize(btn_size, btn_size);

  engage_img = loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size});
  experimental_img = loadPixmap("../assets/img_experimental.svg", {img_size, img_size});
  QObject::connect(this, &QPushButton::clicked, this, &ExperimentalButton::changeMode);

  // FrogPilot variables
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &ExperimentalButton::updateTheme);
}

void ExperimentalButton::showEvent(QShowEvent *event) {
  updateTheme();
}

void ExperimentalButton::updateTheme() {
  loadImage("../../frogpilot/assets/active_theme/steering_wheel/wheel", wheel_img, wheel_gif, QSize(img_size, img_size), this);
}

void ExperimentalButton::changeMode() {
  const auto cp = (*uiState()->sm)["carParams"].getCarParams();
  bool can_change = hasLongitudinalControl(cp) && params.getBool("ExperimentalModeConfirmed");
  if (can_change) {
    if (frogpilotUIState()->frogpilot_toggles.value("conditional_experimental_mode").toBool()) {
      int conditional_status = frogpilotUIState()->frogpilot_scene.conditional_status;
      int override_value = (conditional_status == 1 || conditional_status == 2) ? 0 : experimental_mode ? 1 : 2;
      params_memory.putInt("CEStatus", override_value);
    } else {
      params.putBool("ExperimentalMode", !experimental_mode);
    }
  }
}

void ExperimentalButton::updateState(const UIState &s, const FrogPilotUIState &fs, const QJsonObject &frogpilot_toggles) {
  const auto cs = (*s.sm)["controlsState"].getControlsState();
  bool eng = cs.getEngageable() || cs.getEnabled() || fs.frogpilot_scene.always_on_lateral_active;
  if ((cs.getExperimentalMode() != experimental_mode) || (eng != engageable)) {
    engageable = eng;
    experimental_mode = cs.getExperimentalMode();
    update();
  }

  // FrogPilot variables
  SubMaster &fpsm = *(fs.sm);

  use_stock_wheel = frogpilot_toggles.value("wheel_image").toString() == "stock";

  if (frogpilot_toggles.value("rotating_wheel").toBool() && steering_angle != -fpsm["carState"].getCarState().getSteeringAngleDeg()) {
    steering_angle = -fpsm["carState"].getCarState().getSteeringAngleDeg();

    update();
  } else if (!frogpilot_toggles.value("rotating_wheel").toBool()) {
    steering_angle = 0;
  }

  if (params_memory.getBool("UpdateWheelImage")) {
    loadImage("../../frogpilot/assets/active_theme/steering_wheel/wheel", wheel_img, wheel_gif, QSize(img_size, img_size), this);

    params_memory.remove("UpdateWheelImage");
  }
}

void ExperimentalButton::updateBackgroundColor() {
  UIState &s = *uiState();
  UIScene &scene = s.scene;
  FrogPilotUIState &fs = *frogpilotUIState();
  FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;

  static const QMap<QString, QColor> status_color_map {
    {"default", QColor(0, 0, 0, 166)},
    {"always_on_lateral_active", bg_colors[STATUS_ALWAYS_ON_LATERAL_ACTIVE]},
    {"conditional_overridden", bg_colors[STATUS_CONDITIONAL_OVERRIDDEN]},
    {"experimental_mode_enabled", bg_colors[STATUS_EXPERIMENTAL_MODE_ENABLED]},
    {"navigation_active", bg_colors[STATUS_NAVIGATION_ACTIVE]},
    {"traffic_mode_enabled", bg_colors[STATUS_TRAFFIC_MODE_ENABLED]}
  };

  if (isDown() || !engageable) {
    background_color = status_color_map["default"];
    return;
  }

  if (frogpilot_scene.always_on_lateral_active) {
    background_color = status_color_map["always_on_lateral_active"];
  } else if (frogpilot_scene.conditional_status == 1) {
    background_color = status_color_map["conditional_overridden"];
  } else if (experimental_mode) {
    background_color = status_color_map["experimental_mode_enabled"];
  } else if (scene.navigate_on_openpilot) {
    background_color = status_color_map["navigation_active"];
  } else if (frogpilot_scene.traffic_mode_enabled) {
    background_color = status_color_map["traffic_mode_enabled"];
  } else {
    background_color = status_color_map["default"];
  }
}

void ExperimentalButton::paintEvent(QPaintEvent *event) {
  updateBackgroundColor();

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  QPainterPath clip_path;
  clip_path.addEllipse(QPoint(btn_size / 2, btn_size / 2), btn_size / 2, btn_size / 2);
  p.setClipPath(clip_path);

  if (use_stock_wheel) {
    QPixmap img = experimental_mode ? experimental_img : engage_img;
    drawIcon(p, QPoint(btn_size / 2, btn_size / 2), img, background_color, (isDown() || !engageable) ? 0.6 : 1.0, steering_angle);
  } else if (wheel_gif) {
    drawIcon(p, QPoint(btn_size / 2, btn_size / 2), wheel_gif->currentPixmap(), background_color, (isDown() || !engageable) ? 0.6 : 1.0, steering_angle);
  } else if (!wheel_img.isNull()) {
    drawIcon(p, QPoint(btn_size / 2, btn_size / 2), wheel_img, background_color, (isDown() || !engageable) ? 0.6 : 1.0, steering_angle);
  }

  p.setClipping(false);
}

// MapSettingsButton
MapSettingsButton::MapSettingsButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size, btn_size + UI_BORDER_SIZE);
  settings_img = loadPixmap("../assets/navigation/icon_directions_outlined.svg", {img_size, img_size});

  // hidden by default, made visible if map is created (has prime or mapbox token)
  setVisible(false);
  setEnabled(false);
}

void MapSettingsButton::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  drawIcon(p, QPoint(btn_size / 2, btn_size / 2 + (road_name_ui ? 0 : UI_BORDER_SIZE)), settings_img, QColor(0, 0, 0, 166), isDown() ? 0.6 : 1.0);
}
