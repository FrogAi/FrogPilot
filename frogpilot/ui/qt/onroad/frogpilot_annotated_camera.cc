#include <QPainterPath>

#include "frogpilot/ui/qt/onroad/frogpilot_annotated_camera.h"

FrogPilotAnnotatedCameraWidget::FrogPilotAnnotatedCameraWidget(CameraWidget *nvg, QWidget *parent) : QWidget(parent) {
  QObject::connect(nvg, &CameraWidget::vipcThreadFrameReceived, frogpilotUIState(), &FrogPilotUIState::cameraFrameReceived);
}

void FrogPilotAnnotatedCameraWidget::hideEvent(QHideEvent *event) {
  QWidget::hideEvent(event);
}

void FrogPilotAnnotatedCameraWidget::updateState(const UIState &s, const FrogPilotUIState &fs) {
  frogpilot_scene = fs.frogpilot_scene;

  const UIScene &scene = s.scene;

  bool useSIMetrics = frogpilot_toggles.value(QLatin1String("use_si_metrics")).toBool();

  speedUnit = scene.is_metric ? tr("km/h") : tr("mph");
  speedConversion = scene.is_metric ? MS_TO_KPH : MS_TO_MPH;

  if (scene.is_metric || useSIMetrics) {
    leadDistanceUnit = tr(" meters");
    leadSpeedUnit = useSIMetrics ? tr(" m/s") : tr(" km/h");

    distanceConversion = 1.0f;
    speedConversionMetrics = useSIMetrics ? 1.0f : MS_TO_KPH;
  } else {
    leadDistanceUnit = tr(" feet");
    leadSpeedUnit = tr(" mph");

    distanceConversion = METER_TO_FOOT;
    speedConversionMetrics = MS_TO_MPH;
  }

  hideBottomIcons = alertHeight != 0;

  if (!isVisible()) {
    return;
  }
}

void FrogPilotAnnotatedCameraWidget::mousePressEvent(QMouseEvent *mouseEvent) {
  mouseEvent->ignore();
}

void FrogPilotAnnotatedCameraWidget::drawOutlinedText(QPainter &p, const QPointF &position, const QString &text) {
  QPainterPath path;
  path.addText(position, p.font(), text);
  p.strokePath(path, QPen(Qt::black, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

  p.drawText(position, text);
}

void FrogPilotAnnotatedCameraWidget::paintFrogPilotWidgets(QPainter &p) {
  int slotStep = rightHandDM ? -widget_size - 2 * UI_BORDER_SIZE : widget_size + 2 * UI_BORDER_SIZE;
}

void FrogPilotAnnotatedCameraWidget::paintWorldWidgets(QPainter &p) {
}

