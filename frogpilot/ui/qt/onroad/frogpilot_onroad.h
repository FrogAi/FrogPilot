#pragma once

#include "selfdrive/ui/qt/onroad/annotated_camera.h"

class FrogPilotOnroadWindow : public QWidget {
  Q_OBJECT

public:
  explicit FrogPilotOnroadWindow(QWidget *parent = 0);

  void updateState(const UIState &s, const FrogPilotUIState &fs);

  QColor bg;

private:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

  QRect rect;

  QRegion marginRegion;
};
