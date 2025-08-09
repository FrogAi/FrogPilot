#pragma once

#include "selfdrive/ui/qt/onroad/annotated_camera.h"

class FrogPilotOnroadWindow : public QWidget {
  Q_OBJECT

public:
  FrogPilotOnroadWindow(QWidget* parent = 0);

  void updateState(const UIState &s, const FrogPilotUIState &fs);

  double fps;

  QColor bg;

private:
  void paintEvent(QPaintEvent *event);
  void paintFPS(QPainter &p, const QRect &rect);
  void paintSteeringTorqueBorder(QPainter &p, const QRect &rect);
  void paintTurnSignalBorder(QPainter &p, const QRect &rect);

  bool blindSpotLeft;
  bool blindSpotRight;
  bool flickerActive;
  bool showBlindspot;
  bool showFPS;
  bool showSignal;
  bool showSteering;
  bool turnSignalLeft;
  bool turnSignalRight;

  float steer;

  QTimer *signalTimer;
};
