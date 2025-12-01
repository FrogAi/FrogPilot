#pragma once

#include "selfdrive/ui/qt/onroad/alerts.h"
#include "selfdrive/ui/qt/onroad/annotated_camera.h"

#include "frogpilot/ui/qt/onroad/frogpilot_onroad.h"

class OnroadWindow : public QWidget {
  Q_OBJECT

public:
  OnroadWindow(QWidget* parent = 0);

private:
  void paintEvent(QPaintEvent *event);
  OnroadAlerts *alerts;
  AnnotatedCameraWidget *nvg;
  QColor bg = bg_colors[STATUS_DISENGAGED];
  QHBoxLayout* split;

  // FrogPilot variables
  void mousePressEvent(QMouseEvent* mouseEvent);

  FrogPilotAnnotatedCameraWidget *frogpilot_nvg;
  FrogPilotOnroadWindow *frogpilot_onroad;

private slots:
  void offroadTransition(bool offroad);
  void updateState(const UIState &s);
};
