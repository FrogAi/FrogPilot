#pragma once

#include "selfdrive/ui/qt/onroad/annotated_camera.h"

class FrogPilotOnroadWindow : public QWidget {
  Q_OBJECT

public:
  FrogPilotOnroadWindow(QWidget* parent = 0);

  void updateState(const UIState &s, const FrogPilotUIState &fs);

  float fps = 0.0f;

  FrogPilotUIScene frogpilot_scene = {};

  QColor bg;

  QJsonObject frogpilot_toggles;

private:
  void paintEvent(QPaintEvent *event);
  void paintFPS(QPainter &p);
  void paintSteeringTorqueBorder(QPainter &p);
  void paintTurnSignalBorder(QPainter &p);
  void resetFPSStats();
  void resizeEvent(QResizeEvent *event);

  bool blindSpotLeft = false;
  bool blindSpotRight = false;
  bool flickerActive = false;
  bool showBlindspot = false;
  bool showFPS = false;
  bool showSignal = false;
  bool showSteering = false;
  bool turnSignalLeft = false;
  bool turnSignalRight = false;

  float avgFPS = 0.0f;
  float maxFPS = 0.0f;
  float minFPS = 99.9f;
  float smoothedSteer = 0.0f;
  float torque = 0.0f;

  QColor leftBorderColor;
  QColor rightBorderColor;

  QRect rect;

  QRegion marginRegion;

  QString fpsDisplayString;

  QTimer *signalTimer;
};
