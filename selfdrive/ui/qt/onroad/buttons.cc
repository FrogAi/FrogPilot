#include "selfdrive/ui/qt/onroad/buttons.h"

#include <QPainter>

#include "selfdrive/ui/qt/util.h"

void drawIcon(QPainter &p, const QPoint &center, const QPixmap &img, const QBrush &bg, float opacity) {
  p.setRenderHint(QPainter::Antialiasing);
  p.setOpacity(1.0);  // bg dictates opacity of ellipse
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(center, btn_size / 2, btn_size / 2);
  p.setOpacity(opacity);
  p.drawPixmap(center - QPoint(img.width() / 2, img.height() / 2), img);
  p.setOpacity(1.0);
}

// ExperimentalButton
ExperimentalButton::ExperimentalButton(QWidget *parent) : experimental_mode(false), engageable(false), QPushButton(parent) {
  setFixedSize(btn_size, btn_size);

  engage_img = loadPixmap("../assets/icons/chffr_wheel.png", {img_size, img_size});
  experimental_img = loadPixmap("../assets/icons/experimental.svg", {img_size, img_size});
  QObject::connect(this, &QPushButton::clicked, this, &ExperimentalButton::changeMode);

  // FrogPilot variables
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &ExperimentalButton::updateTheme);
}

void ExperimentalButton::changeMode() {
  const auto cp = (*uiState()->sm)["carParams"].getCarParams();
  bool can_change = hasLongitudinalControl(cp) && params.getBool("ExperimentalModeConfirmed");
  if (can_change) {
    // FrogPilot variables
    if (frogpilotUIState()->frogpilot_scene.frogpilot_toggles.value(QLatin1String("conditional_experimental_mode")).toBool()) {
      frogpilotUIState()->experimentalModePressed();
    } else {
      params.putBool("ExperimentalMode", !experimental_mode);
    }
  }
}

void ExperimentalButton::updateState(const UIState &s) {
  const auto cs = (*s.sm)["selfdriveState"].getSelfdriveState();
  bool eng = cs.getEngageable() || cs.getEnabled() || frogpilotUIState()->frogpilot_scene.always_on_lateral_active;
  if ((cs.getExperimentalMode() != experimental_mode) || (eng != engageable)) {
    engageable = eng;
    experimental_mode = cs.getExperimentalMode();
    update();
  }

  // FrogPilot variables
  const FrogPilotUIState &fs = *frogpilotUIState();
  const cereal::CarState::Reader &carState = (*s.sm)["carState"].getCarState();

  QColor current_background_color = backgroundColor();
  if (current_background_color != background_color) {
    background_color = current_background_color;
    update();
  }

  int current_steering_angle_deg = 0;
  if (fs.frogpilot_scene.frogpilot_toggles.value(QLatin1String("rotating_wheel")).toBool()) {
    current_steering_angle_deg = -carState.getSteeringAngleDeg();
  }

  if (current_steering_angle_deg != steering_angle_deg) {
    steering_angle_deg = current_steering_angle_deg;
    update();
  }

  uint64_t current_wheel_image_update_count = (*fs.sm)["frogpilotPlan"].getFrogpilotPlan().getWheelImageUpdateCount();
  if (current_wheel_image_update_count != wheel_image_update_count) {
    wheel_image_update_count = current_wheel_image_update_count;
    updateTheme();
  }
}

void ExperimentalButton::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  QPixmap img = experimental_mode ? experimental_img : engage_img;

  // FrogPilot variables
  if (!wheel_is_stock) {
    img = wheel_gif ? wheel_gif->currentPixmap() : wheel_img;
  }
  if (img.isNull()) {
    return;
  }

  p.setClipRegion(QRegion(QRect(0, 0, btn_size, btn_size), QRegion::Ellipse));
  p.translate(btn_size / 2, btn_size / 2);
  p.rotate(steering_angle_deg);
  p.translate(-btn_size / 2, -btn_size / 2);

  drawIcon(p, QPoint(btn_size / 2, btn_size / 2), img, backgroundColor(), (isDown() || !engageable) ? 0.6 : 1.0);
}

// FrogPilot variables
void ExperimentalButton::showEvent(QShowEvent *event) {
  updateTheme();
}

void ExperimentalButton::hideEvent(QHideEvent *event) {
  wheel_gif.reset();

  QPushButton::hideEvent(event);
}

QColor ExperimentalButton::backgroundColor() const {
  const FrogPilotUIScene &frogpilot_scene = frogpilotUIState()->frogpilot_scene;

  if (!isDown() && engageable) {
    if (frogpilot_scene.always_on_lateral_active) {
      return bg_colors[STATUS_ALWAYS_ON_LATERAL_ACTIVE];
    } else if (frogpilot_scene.conditional_status == 1) {
      return bg_colors[STATUS_CEM_DISABLED];
    } else if (experimental_mode) {
      return bg_colors[STATUS_EXPERIMENTAL_MODE_ENABLED];
    } else if (frogpilot_scene.traffic_mode_enabled) {
      return bg_colors[STATUS_TRAFFIC_MODE_ENABLED];
    }
  }
  return blackColor(166);
}

void ExperimentalButton::updateTheme() {
  if (isVisible()) {
    loadImage("../../frogpilot/assets/active_theme/steering_wheel/wheel", wheel_img, wheel_gif, QSize(img_size, img_size), this, false);
  } else {
    wheel_gif.reset();
  }

  const QString wheel_source = QFileInfo("../../frogpilot/assets/active_theme/steering_wheel/wheel.svg").canonicalFilePath();
  const QString stock_source = QFileInfo("../../frogpilot/assets/stock_theme/steering_wheel/wheel.svg").canonicalFilePath();
  wheel_is_stock = wheel_source == stock_source;
}
