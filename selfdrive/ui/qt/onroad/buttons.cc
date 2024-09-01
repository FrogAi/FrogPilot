#include "selfdrive/ui/qt/onroad/buttons.h"

#include <QPainter>

#include "selfdrive/ui/qt/util.h"

void drawIcon(QPainter &p, const QPoint &center, const QPixmap &img, const QBrush &bg, float opacity, const int angle) {
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
  p.setOpacity(1.0);
  p.restore();
}

// ExperimentalButton
ExperimentalButton::ExperimentalButton(QWidget *parent) : experimental_mode(false), engageable(false), QPushButton(parent) {
  setFixedSize(btn_size, btn_size + 10);

  engage_img = loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size});
  experimental_img = loadPixmap("../assets/img_experimental.svg", {img_size, img_size});
  QObject::connect(this, &QPushButton::clicked, this, &ExperimentalButton::changeMode);

  // FrogPilot variables
  wheel_gif_path = "../frogpilot/assets/active_theme/steering_wheel/wheel.gif";
  wheel_png_path = "../frogpilot/assets/active_theme/steering_wheel/wheel.png";

  gif_label = new QLabel(this);
  gif_label->setScaledContents(true);
}

ExperimentalButton::~ExperimentalButton() {
  if (gif != nullptr) {
    gif->stop();
    delete gif;
    gif = nullptr;
    gif_label->hide();
  }
}

void ExperimentalButton::changeMode() {
  const auto cp = (*uiState()->sm)["carParams"].getCarParams();
  bool can_change = hasLongitudinalControl(cp) && params.getBool("ExperimentalModeConfirmed");
  if (can_change) {
    if (conditional_experimental) {
      int override_value = (conditional_status >= 1 && conditional_status <= 6) ? 0 : conditional_status >= 7 ? 5 : 6;
      params_memory.putInt("CEStatus", override_value);
    } else {
      params.putBool("ExperimentalMode", !experimental_mode);
    }
  }
}

void ExperimentalButton::updateState(const UIState &s, bool lead_info) {
  const auto cs = (*s.sm)["controlsState"].getControlsState();
  bool eng = cs.getEngageable() || cs.getEnabled() || always_on_lateral_active;
  if ((cs.getExperimentalMode() != experimental_mode) || (eng != engageable)) {
    engageable = eng;
    experimental_mode = cs.getExperimentalMode();
    update();
  }

  // FrogPilot variables
  const UIScene &scene = s.scene;

  always_on_lateral_active = scene.always_on_lateral_active;
  big_map = scene.big_map;
  conditional_experimental = scene.conditional_experimental;
  conditional_status = scene.conditional_status;
  map_open = scene.map_open;
  navigate_on_openpilot = scene.navigate_on_openpilot;
  rotating_wheel = scene.rotating_wheel;
  traffic_mode_active = scene.traffic_mode_active;
  use_stock_wheel = scene.use_stock_wheel;
  y_offset = lead_info ? 10 : 0;

  if (rotating_wheel && steering_angle_deg != scene.steering_angle_deg) {
    steering_angle_deg = scene.steering_angle_deg;
    update();
  } else if (!rotating_wheel) {
    steering_angle_deg = 0;
  }

  if (params_memory.getBool("UpdateWheelImage")) {
    updateIcon();
    params_memory.remove("UpdateWheelImage");
  }
}

void ExperimentalButton::updateBackgroundColor() {
  static const QMap<QString, QColor> status_color_map {
    {"default", QColor(0, 0, 0, 166)},
    {"always_on_lateral_active", bg_colors[STATUS_ALWAYS_ON_LATERAL_ACTIVE]},
    {"conditional_overridden", bg_colors[STATUS_CONDITIONAL_OVERRIDDEN]},
    {"experimental_mode_active", bg_colors[STATUS_EXPERIMENTAL_MODE_ACTIVE]},
    {"navigation_active", bg_colors[STATUS_NAVIGATION_ACTIVE]},
    {"traffic_mode_active", bg_colors[STATUS_TRAFFIC_MODE_ACTIVE]}
  };

  if (isDown() || !engageable || use_stock_wheel) {
    background_color = status_color_map["default"];
    return;
  }

  if (always_on_lateral_active) {
    background_color = status_color_map["always_on_lateral_active"];
  } else if (conditional_status == 1 || conditional_status == 3 || conditional_status == 5) {
    background_color = status_color_map["conditional_overridden"];
  } else if (experimental_mode) {
    background_color = status_color_map["experimental_mode_active"];
  } else if (navigate_on_openpilot) {
    background_color = status_color_map["navigation_active"];
  } else if (traffic_mode_active) {
    background_color = status_color_map["traffic_mode_active"];
  } else {
    background_color = status_color_map["default"];
  }
}

void ExperimentalButton::updateIcon() {
  if (gif != nullptr) {
    gif->stop();
    delete gif;
    gif = nullptr;
    gif_label->hide();
  }

  if (QFile::exists(wheel_gif_path)) {
    gif = new QMovie(wheel_gif_path);

    gif_label->setMovie(gif);
    gif_label->resize(img_size, img_size);
    gif_label->move((btn_size - img_size) / 2, (btn_size - img_size) / 2 + y_offset);
    gif_label->show();

    gif->start();

    use_gif = true;
    image_empty = false;
  } else if (QFile::exists(wheel_png_path)) {
    img = loadPixmap(wheel_png_path, QSize(img_size, img_size));
    image_empty = false;
    use_gif = false;
  } else {
    image_empty = true;
    use_gif = false;
  }

  update();
}

void ExperimentalButton::paintEvent(QPaintEvent *event) {
  if ((big_map && map_open) || image_empty || use_gif) {
    return;
  }

  QPainter p(this);
  if (use_stock_wheel) {
    img = experimental_mode ? experimental_img : engage_img;
  }
  updateBackgroundColor();
  drawIcon(p, QPoint(btn_size / 2, btn_size / 2 + y_offset), img, background_color, (isDown() || !engageable) ? 0.6 : 1.0, steering_angle_deg);
}

// MapSettingsButton
MapSettingsButton::MapSettingsButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size, btn_size + 20);
  settings_img = loadPixmap("../assets/navigation/icon_directions_outlined.svg", {img_size, img_size});

  // hidden by default, made visible if map is created (has prime or mapbox token)
  setVisible(false);
  setEnabled(false);
}

void MapSettingsButton::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  drawIcon(p, QPoint(btn_size / 2, btn_size / 2), settings_img, QColor(0, 0, 0, 166), isDown() ? 0.6 : 1.0);
}

// FrogPilot buttons

// DistanceButton
DistanceButton::DistanceButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size * 1.5, btn_size * 1.5);

  gif_label = new QLabel(this);
  gif_label->setScaledContents(true);

  connect(this, &QPushButton::pressed, [this] {params_memory.putBool("OnroadDistanceButtonPressed", true);});
  connect(this, &QPushButton::released, [this] {params_memory.putBool("OnroadDistanceButtonPressed", false);});
}

DistanceButton::~DistanceButton() {
  qDeleteAll(profile_data_gif);
  profile_data_gif.clear();
  profile_data_png.clear();
}

void DistanceButton::updateState(const UIScene &scene) {
  bool state_changed = (traffic_mode_active != scene.traffic_mode_active) ||
                       (personality != static_cast<int>(scene.personality) + 1 && !traffic_mode_active);

  if (!state_changed) {
    return;
  }

  personality = static_cast<int>(scene.personality) + 1;
  traffic_mode_active = scene.traffic_mode_active;

  int profile_index = traffic_mode_active ? 0 : personality;

  if (QMovie *gif = profile_data_gif.value(profile_index)) {
    gif_label->setMovie(gif);
    gif_label->resize(btn_size, btn_size);
    gif_label->move(UI_BORDER_SIZE, btn_size / 2.5);
    gif_label->show();

    gif->start();

    use_gif = true;
  } else {
    gif_label->hide();

    profile_image = profile_data_png.value(profile_index);

    use_gif = false;
  }

  update();
}

void DistanceButton::updateIcon() {
  qDeleteAll(profile_data_gif);
  profile_data_gif.clear();
  profile_data_png.clear();

  static const QVector<QString> file_names = {
    "../frogpilot/assets/active_theme/distance_icons/traffic",
    "../frogpilot/assets/active_theme/distance_icons/aggressive",
    "../frogpilot/assets/active_theme/distance_icons/standard",
    "../frogpilot/assets/active_theme/distance_icons/relaxed"
  };

  for (int i = 0; i < file_names.size(); ++i) {
    const QString &file_name = file_names[i];
    QString gif_file = file_name + ".gif";
    QString png_file = file_name + ".png";
    QString fallback_file = QString("../frogpilot/assets/other_images/%1.png").arg(QFileInfo(file_name).baseName().toLower());

    if (QFile::exists(gif_file)) {
      QMovie *movie = new QMovie(gif_file);
      profile_data_gif.push_back(movie);
      profile_data_png.push_back(QPixmap());
    } else {
      int pixmap_size = btn_size * 1.25;
      QPixmap pixmap = loadPixmap(QFile::exists(png_file) ? png_file : fallback_file, QSize(pixmap_size, pixmap_size));
      profile_data_gif.push_back(nullptr);
      profile_data_png.push_back(pixmap);
    }
  }

  personality = 0;
}

void DistanceButton::paintEvent(QPaintEvent *event) {
  if (use_gif) {
    return;
  }

  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  drawIcon(p, QPoint((btn_size / 2) + (UI_BORDER_SIZE * 0.5), btn_size - (UI_BORDER_SIZE * 1.5)), profile_image, Qt::transparent, 1.0);
}
