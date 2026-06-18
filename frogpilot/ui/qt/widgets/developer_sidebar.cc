#include "frogpilot/ui/qt/widgets/developer_sidebar.h"

void DeveloperSidebar::drawMetric(QPainter &p, const QPair<QString, QString> &label, QColor c, int y) {
  const QRect rect = {12, y, 275, 126};

  p.setPen(Qt::NoPen);
  p.setBrush(QBrush(c));
  p.setClipRect(rect.x() + rect.width() - 4 - 18, rect.y(), 18, rect.height(), Qt::ClipOperation::ReplaceClip);
  p.drawRoundedRect(QRect(rect.x() + rect.width() - 4 - 100, rect.y() + 4, 100, 118), 18, 18);
  p.setClipping(false);

  QPen pen = QPen(QColor(0xff, 0xff, 0xff, 0x55));
  pen.setWidth(2);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(rect, 20, 20);

  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setFont(InterFont(35, QFont::DemiBold));
  p.drawText(rect.adjusted(0, 0, -22, 0), Qt::AlignCenter, label.first + "\n" + label.second);
}

DeveloperSidebar::DeveloperSidebar(QWidget *parent) : QFrame(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setFixedWidth(300);

  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &DeveloperSidebar::updateToggles);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &DeveloperSidebar::resetVariables);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &DeveloperSidebar::updateState);
}

void DeveloperSidebar::showEvent(QShowEvent *event) {
  updateToggles();
}

void DeveloperSidebar::updateToggles() {
  FrogPilotUIState &fs = *frogpilotUIState();
  FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;
  QJsonObject &frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  metricAssignments.clear();
  for (int i = 1; i <= 7; ++i) {
    QString key = QString("developer_sidebar_metric%1").arg(i);
    int metricId = frogpilot_toggles.value(key).toInt();
    metricAssignments.push_back(metricId);
  }

  metricColor = QColor(frogpilot_toggles.value(QLatin1String("sidebar_color1")).toString());
}

void DeveloperSidebar::resetVariables() {
  lateralEngagementTime = 0;
  longitudinalEngagementTime = 0;
  maxAcceleration = 0;
  totalEngagementTime = 0;
}

void DeveloperSidebar::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible()) {
    return;
  }

  const SubMaster &sm = *(s.sm);

  const FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;
  const SubMaster &fpsm = *(fs.sm);

  const cereal::CarControl::Reader &carControl = fpsm["carControl"].getCarControl();
  const cereal::CarState::Reader &carState = sm["carState"].getCarState();
  const cereal::FrogPilotPlan::Reader &frogpilotPlan = fpsm["frogpilotPlan"].getFrogpilotPlan();
  const cereal::LiveDelayData::Reader &liveDelay = fpsm["liveDelay"].getLiveDelay();
  const cereal::LiveParametersData::Reader &liveParameters = fpsm["liveParameters"].getLiveParameters();
  const cereal::LiveTorqueParametersData::Reader &liveTorqueParameters = fpsm["liveTorqueParameters"].getLiveTorqueParameters();

  const bool is_metric = s.scene.is_metric;
  const bool use_si = frogpilot_scene.frogpilot_toggles.value(QLatin1String("use_si_metrics")).toBool();

  const QString accelerationUnit = (is_metric || use_si) ? tr(" m/s²") : tr(" ft/s²");
  const float accelerationConversion = (is_metric || use_si) ? 1.0f : METER_TO_FOOT;

  double acceleration = carState.getAEgo() * accelerationConversion;
  if (!carState.getGasPressed()) {
    maxAcceleration = std::max(maxAcceleration, acceleration);
  }

  lateralEngagementTime += carControl.getLatActive() && !frogpilot_scene.reverse && !frogpilot_scene.standstill ? 1 : 0;
  longitudinalEngagementTime += carControl.getLongActive() && !frogpilot_scene.reverse && !frogpilot_scene.standstill ? 1 : 0;
  totalEngagementTime += !(frogpilot_scene.reverse || frogpilot_scene.standstill) || totalEngagementTime == 0 ? 1 : 0;

  static int maxSteerAngle = 0;
  int currentSteerAngle = fabs(carState.getSteeringAngleDeg());

  static int maxTorque = 0;
  int currentTorque = fabs(carControl.getActuators().getTorque() * 100);

  static QElapsedTimer torqueTimer;

  if (currentTorque >= 50) {
    maxSteerAngle = std::max(maxSteerAngle, currentSteerAngle);
    maxTorque = std::max(maxTorque, currentTorque);

    torqueTimer.start();
  } else if (torqueTimer.elapsed() >= 10000) {
    maxTorque = 0;
    maxSteerAngle = 0;

    torqueTimer.invalidate();
  }

  QString steerLabel = QString::number(currentSteerAngle) + "°";
  QString torqueLabel = QString::number(currentTorque) + "%";

  if (currentTorque >= 50 || torqueTimer.isValid()) {
    steerLabel += QString(" - (%1°)").arg(maxSteerAngle);
    torqueLabel += QString(" - (%1%)").arg(maxTorque);
  }

  accelerationStatus = ItemStatus(QPair<QString, QString>(tr("ACCEL"), QString::number(acceleration, 'f', 2) + accelerationUnit), metricColor);
  accelerationJerkStatus = ItemStatus(QPair<QString, QString>(tr("ACCEL JERK"), QString::number(frogpilotPlan.getAccelerationJerk())), metricColor);
  actuatorAccelerationStatus = ItemStatus(QPair<QString, QString>(tr("ACT ACCEL"), QString::number(carControl.getActuators().getAccel() * accelerationConversion, 'f', 2) + accelerationUnit), metricColor);
  dangerJerkStatus = ItemStatus(QPair<QString, QString>(tr("DANGER JERK"), QString::number(frogpilotPlan.getDangerJerk())), metricColor);
  delayStatus = ItemStatus(QPair<QString, QString>(tr("STEER DELAY"), QString::number(liveDelay.getLateralDelay(), 'f', 5)), metricColor);
  frictionStatus = ItemStatus(QPair<QString, QString>(tr("FRICTION"), QString::number(liveTorqueParameters.getFrictionCoefficientFiltered(), 'f', 5)), metricColor);
  latAccelStatus = ItemStatus(QPair<QString, QString>(tr("LAT ACCEL"), QString::number(liveTorqueParameters.getLatAccelFactorFiltered(), 'f', 5)), metricColor);
  lateralEngagementStatus = ItemStatus(QPair<QString, QString>(tr("LATERAL %"), QString::number((lateralEngagementTime / totalEngagementTime) * 100.0f, 'f', 2) + "%"), metricColor);
  longitudinalEngagementStatus = ItemStatus(QPair<QString, QString>(tr("LONG %"), QString::number((longitudinalEngagementTime / totalEngagementTime) * 100.0f, 'f', 2) + "%"), metricColor);
  maxAccelerationStatus = ItemStatus(QPair<QString, QString>(tr("MAX ACCEL"), QString::number(maxAcceleration, 'f', 2) + accelerationUnit), metricColor);
  speedJerkStatus = ItemStatus(QPair<QString, QString>(tr("SPEED JERK"), QString::number(frogpilotPlan.getSpeedJerk())), metricColor);
  steerAngleStatus = ItemStatus(QPair<QString, QString>(tr("STEER ANGLE"), steerLabel), metricColor);
  steerRatioStatus = ItemStatus(QPair<QString, QString>(tr("STEER RATIO"), QString::number(liveParameters.getSteerRatio(), 'f', 5)), metricColor);
  stiffnessFactorStatus = ItemStatus(QPair<QString, QString>(tr("STEER STIFF"), QString::number(liveParameters.getStiffnessFactor(), 'f', 5)), metricColor);
  torqueStatus = ItemStatus(QPair<QString, QString>(tr("TORQUE %"), torqueLabel), metricColor);

  update();
}

void DeveloperSidebar::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setPen(Qt::NoPen);
  p.setRenderHint(QPainter::Antialiasing);

  p.fillRect(rect(), QColor(57, 57, 57));

  ItemStatus *metrics[] = {
    nullptr,
    &accelerationStatus, &maxAccelerationStatus, &delayStatus, &frictionStatus, &latAccelStatus,
    &steerRatioStatus, &stiffnessFactorStatus, &lateralEngagementStatus, &longitudinalEngagementStatus,
    &steerAngleStatus, &torqueStatus, &actuatorAccelerationStatus, &accelerationJerkStatus,
    &dangerJerkStatus, &speedJerkStatus
  };
  constexpr int metricCount = std::size(metrics);

  int count = 0;
  for (size_t i = 0; i < metricAssignments.size(); ++i) {
    if (metricAssignments[i] > 0 && metricAssignments[i] < metricCount) {
      count++;
    }
  }
  if (count == 0) {
    return;
  }

  int metricHeight = 126;
  int spacing = (height() - (count * metricHeight)) / (count + 1);
  int y = spacing;

  for (size_t i = 0; i < metricAssignments.size(); ++i) {
    int metricId = metricAssignments[i];

    if (metricId <= 0 || metricId >= metricCount) {
      continue;
    }

    ItemStatus *status = metrics[metricId];
    drawMetric(p, status->first, status->second, y);
    y += metricHeight + spacing;
  }
}
