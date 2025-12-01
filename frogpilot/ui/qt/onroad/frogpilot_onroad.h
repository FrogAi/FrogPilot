#pragma once

#include "selfdrive/ui/qt/onroad/annotated_camera.h"

class FrogPilotOnroadWindow : public QWidget {
  Q_OBJECT

public:
  explicit FrogPilotOnroadWindow(QWidget *parent = 0);

  void updateState(const UIState &s, const FrogPilotUIState &fs);

  float fps = 0.0f;

  QColor bg;

private:
  void paintEvent(QPaintEvent *event) override;
  void paintFPS(QPainter &p);
  void paintSteeringTorqueBorder(QPainter &p);
  void resetFPSStats();
  void resizeEvent(QResizeEvent *event) override;

  bool showFPS = false;
  bool showSteering = false;

  float avgFPS = 0.0f;
  float maxFPS = 0.0f;
  float minFPS = 99.9f;
  float smoothedSteer = 0.0f;
  float torque = 0.0f;

  QRect rect;

  QRegion marginRegion;

  QString fpsDisplayString;
};
