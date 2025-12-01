#pragma once

#include "selfdrive/ui/ui.h"

class DeveloperSidebar : public QFrame {
  Q_OBJECT

public:
  explicit DeveloperSidebar(QWidget* parent = 0);

private:
  void drawMetric(QPainter &p, const QPair<QString, QString> &label, QColor c, int y);
  void paintEvent(QPaintEvent *event) override;
  void resetVariables();
  void updateState(const UIState &s, const FrogPilotUIState &fs);
  void updateToggles();

  double lateralEngagedFrames = 0;
  double longitudinalEngagedFrames = 0;
  double maxAcceleration = 0;
  double totalFrames = 0;

  int maxSteerAngle = 0;
  int maxTorque = 0;

  QElapsedTimer torqueTimer;

  std::vector<int> metricAssignments;

  std::vector<QPair<QString, QString>> metricLabels;

  QColor metricColor;

  const QString imperialAccelerationUnit = tr(" ft/s²");
  const QString metricAccelerationUnit = tr(" m/s²");

  const QStringList metricTitles = {
    "",
    tr("ACCEL"), tr("MAX ACCEL"), tr("STEER DELAY"), tr("FRICTION"), tr("LAT ACCEL"),
    tr("STEER RATIO"), tr("STEER STIFF"), tr("LATERAL %"), tr("LONG %"),
    tr("STEER ANGLE"), tr("TORQUE %"), tr("ACT ACCEL"), tr("ACCEL JERK"),
    tr("DANGER JERK"), tr("SPEED JERK")
  };
};
