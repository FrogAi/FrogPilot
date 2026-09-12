#pragma once

#include "selfdrive/ui/qt/onroad/annotated_camera.h"

class FrogPilotOnroadWindow : public QWidget {
  Q_OBJECT

public:
  FrogPilotOnroadWindow(QWidget* parent = 0);

  void updateState(const UIState &s, const FrogPilotUIState &fs);

  double fps = 0.0;

  QColor bg;

private:
  void paintEvent(QPaintEvent *event);
  void paintFPS(QPainter &p, const QRect &rect);
  void paintSteeringTorqueBorder(QPainter &p, const QRect &rect);
  void paintTurnSignalBorder(QPainter &p, const QRect &rect);
  void resetFPSStats();

  bool blindSpotLeft = false;
  bool blindSpotRight = false;
  bool flickerActive = false;
  bool showBlindspot = false;
  bool showFPS = false;
  bool showSignal = false;
  bool showSteering = false;
  bool turnSignalLeft = false;
  bool turnSignalRight = false;

  double maxFPS = 0.0;
  double minFPS = 99.9;
  double totalFPS = 0.0;

  float smoothedSteer = 0.0f;
  float steer = 0.0f;

  QList<QPair<qint64, double>> fpsHistory;

  QTimer *signalTimer;
};
