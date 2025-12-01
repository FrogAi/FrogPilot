#pragma once

#include <QPushButton>

#include "selfdrive/ui/ui.h"

const int btn_size = 192;
const int img_size = (btn_size / 4) * 3;

class ExperimentalButton : public QPushButton {
  Q_OBJECT

public:
  explicit ExperimentalButton(QWidget *parent = 0);
  void updateState(const UIState &s);

private:
  void paintEvent(QPaintEvent *event) override;
  void changeMode();

  Params params;
  QPixmap engage_img;
  QPixmap experimental_img;
  bool experimental_mode;
  bool engageable;

  // FrogPilot variables
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void updateBackgroundColor(const FrogPilotUIScene &frogpilot_scene);
  void updateTheme();

  bool wheel_is_stock = false;

  QColor background_color;

  QPixmap wheel_img;

  QSharedPointer<QMovie> wheel_gif;
};

void drawIcon(QPainter &p, const QPoint &center, const QPixmap &img, const QBrush &bg, float opacity);
