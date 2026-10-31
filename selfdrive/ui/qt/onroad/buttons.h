#pragma once

#include <QPushButton>

#include "selfdrive/ui/ui.h"

const int btn_size = 192;
const int img_size = (btn_size / 4) * 3;

class ExperimentalButton : public QPushButton {
  Q_OBJECT

public:
  explicit ExperimentalButton(QWidget *parent = 0);
  void updateState(const UIState &s, const FrogPilotUIState &fs, const QJsonObject &frogpilot_toggles);

private:
  void paintEvent(QPaintEvent *event) override;
  void changeMode();

  Params params;
  QPixmap engage_img;
  QPixmap experimental_img;
  bool experimental_mode;
  bool engageable;

  // FrogPilot variables
  QColor backgroundColor() const;
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void updateTheme();

  bool wheel_is_stock = false;

  int steering_angle_deg = 0;

  uint64_t wheel_image_update_count = 0;

  QColor background_color;

  QPixmap wheel_img;

  QSharedPointer<FrogPilotMovie> wheel_gif;
};


class MapSettingsButton : public QPushButton {
  Q_OBJECT

public:
  explicit MapSettingsButton(QWidget *parent = 0);

  // FrogPilot variables
  bool road_name_ui = false;

private:
  void paintEvent(QPaintEvent *event) override;

  QPixmap settings_img;
};

void drawIcon(QPainter &p, const QPoint &center, const QPixmap &img, const QBrush &bg, float opacity);
