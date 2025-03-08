#pragma once

#include <QLabel>
#include <QMovie>
#include <QPushButton>

#include "selfdrive/ui/ui.h"

const int btn_size = 192;
const int img_size = (btn_size / 4) * 3;

class ExperimentalButton : public QPushButton {
  Q_OBJECT

public:
  explicit ExperimentalButton(QWidget *parent = 0);
  void updateState(const UIState &s);

  // FrogPilot widgets
  ~ExperimentalButton();
  void updateIcon();

private:
  void paintEvent(QPaintEvent *event) override;
  void changeMode();

  Params params;
  QPixmap engage_img;
  QPixmap experimental_img;
  bool experimental_mode;
  bool engageable;

  // FrogPilot widgets
  void updateBackgroundColor();

  // FrogPilot variables
  Params params_memory{"/dev/shm/params"};

  QColor background_color;

  QLabel *gif_label;

  QMovie *gif;

  QPixmap img;

  QString wheel_gif_path;
  QString wheel_png_path;

  bool always_on_lateral_active;
  bool big_map;
  bool conditional_experimental;
  bool image_empty;
  bool map_open;
  bool navigate_on_openpilot;
  bool rotating_wheel;
  bool traffic_mode_active;
  bool use_gif;
  bool use_stock_wheel;

  int conditional_status;
  int steering_angle_deg;
};


class MapSettingsButton : public QPushButton {
  Q_OBJECT

public:
  explicit MapSettingsButton(QWidget *parent = 0);

private:
  void paintEvent(QPaintEvent *event) override;

  QPixmap settings_img;
};

void drawIcon(QPainter &p, const QPoint &center, const QPixmap &img, const QBrush &bg, float opacity, const int angle = 0);

// FrogPilot buttons
class DistanceButton : public QPushButton {
  Q_OBJECT

public:
  explicit DistanceButton(QWidget *parent = 0);
  ~DistanceButton();
  void updateIcon();
  void updateState(const UIScene &scene);

private:
  void paintEvent(QPaintEvent *event) override;

  Params params_memory{"/dev/shm/params"};

  QLabel *gif_label;
  QPixmap profile_image;

  QVector<QPixmap> profile_data_png;
  QVector<QMovie*> profile_data_gif;

  bool traffic_mode_active;
  bool use_gif;

  int personality;
};
