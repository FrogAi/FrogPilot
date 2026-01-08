#include <QPainterPath>

#include "frogpilot/ui/qt/onroad/frogpilot_annotated_camera.h"

FrogPilotAnnotatedCameraWidget::FrogPilotAnnotatedCameraWidget(CameraWidget *nvg, QWidget *parent) : QWidget(parent) {
  instantReplayButton = new InstantReplayButton(nvg);
  instantReplayButton->setVisible(false);

  QObject::connect(nvg, &CameraWidget::vipcThreadFrameReceived, frogpilotUIState(), &FrogPilotUIState::cameraFrameReceived);
}

void FrogPilotAnnotatedCameraWidget::showEvent(QShowEvent *event) {
}

void FrogPilotAnnotatedCameraWidget::hideEvent(QHideEvent *event) {
  if (cemIcon) {
    cemIcon->stop();
  }

  QWidget::hideEvent(event);
}

void FrogPilotAnnotatedCameraWidget::updateCEMIcon() {
  static const QMap<int, QString> cemIcons = {{1, "chill_mode_icon"}, {3, "curve_icon"}, {4, "lead_icon"}, {5, "turn_icon"}, {6, "speed_icon"}, {7, "speed_icon"}, {8, "light_icon"}};

  QString cemPath;
  if (frogpilot_toggles.value(QLatin1String("cem_status")).toBool()) {
    QString icon = experimentalMode ? cemIcons.value(frogpilot_scene.conditional_status, "experimental_mode_icon") : "chill_mode_icon";
    cemPath = QString("../../frogpilot/assets/other_images/%1.gif").arg(icon);
  }
  updateIcon(cemPath, cemIcon, cemIconPath);
}

void FrogPilotAnnotatedCameraWidget::updateIcon(const QString &path, QSharedPointer<QMovie> &icon, QString &iconPath) {
  if (hideBottomIcons && !path.isEmpty()) {
    if (icon) {
      icon->stop();
    }
  } else if (path != iconPath || (!path.isEmpty() && (!icon || icon->state() != QMovie::Running))) {
    loadGif(path, icon, QSize(widget_size, widget_size), this, false);
    iconPath = icon && icon->state() == QMovie::Running ? path : QString();
  }
}

void FrogPilotAnnotatedCameraWidget::updateState(const UIState &s, const FrogPilotUIState &fs) {
  frogpilot_scene = fs.frogpilot_scene;
  frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  const UIScene &scene = s.scene;

  const SubMaster &sm = *(s.sm);
  const SubMaster &fpsm = *(fs.sm);

  const cereal::CarState::Reader &carState = sm["carState"].getCarState();
  const cereal::FrogPilotCarState::Reader &frogpilotCarState = fpsm["frogpilotCarState"].getFrogpilotCarState();
  const cereal::FrogPilotPlan::Reader &frogpilotPlan = fpsm["frogpilotPlan"].getFrogpilotPlan();
  const cereal::FrogPilotSignReading::Reader &frogpilotSignReading = fpsm["frogpilotSignReading"].getFrogpilotSignReading();
  const cereal::MapdOut::Reader &mapdOut = fpsm["mapdOut"].getMapdOut();
  const cereal::ModelDataV2::Reader &modelV2 = sm["modelV2"].getModelV2();
  const cereal::SelfdriveState::Reader &selfdriveState = sm["selfdriveState"].getSelfdriveState();

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

  blindspotLeft = carState.getLeftBlindspot();
  blindspotRight = carState.getRightBlindspot();
  brakeLights = frogpilotCarState.getBrakeLights();
  cameraSpeedLimit = frogpilotSignReading.getSpeedLimit();
  cscSpeed = frogpilotPlan.getCscSpeed();
  cscTraining = frogpilotPlan.getCscTraining();
  dashboardSpeedLimit = frogpilotCarState.getDashboardSpeedLimit();
  desiredFollowDistance = frogpilotPlan.getDesiredFollowDistance();
  experimentalMode = selfdriveState.getExperimentalMode();
  forceCoast = frogpilotCarState.getForceCoast();
  gpsBearing = frogpilotPlan.getGpsBearing();
  laneWidthLeft = frogpilotPlan.getLaneWidthLeft();
  laneWidthRight = frogpilotPlan.getLaneWidthRight();
  lateralPaused = frogpilotCarState.getPauseLateral();
  longitudinalPaused = frogpilotCarState.getPauseLongitudinal();
  mapSpeedLimit = frogpilotPlan.getSlcMapSpeedLimit();
  nextSpeedLimit = frogpilotPlan.getSlcNextSpeedLimit();
  redLight = frogpilotPlan.getRedLight();
  roadCurvature = frogpilotPlan.getRoadCurvature();
  roadName = QString::fromStdString(mapdOut.getRoadName());
  speedLimitChanged = frogpilotPlan.getSpeedLimitChanged();
  speedLimitSource = frogpilotPlan.getSlcSpeedLimitSource();
  unconfirmedSpeedLimit = frogpilotPlan.getUnconfirmedSlcSpeedLimit();
  weatherDaytime = frogpilotPlan.getWeatherDaytime();
  weatherId = frogpilotPlan.getWeatherId();

  hideBottomIcons = alertHeight != 0;

  instantReplayButton->setVisible(frogpilot_toggles.value(QLatin1String("instant_replay")).toInt() != 0 && standstillDuration == 0 && !(signalStyle == "static" && blinkerRight));

  if (!isVisible()) {
    return;
  }

  updateCEMIcon();
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

  QPoint cemStatusPosition(dmIconPosition.x() + (rightHandDM ? -btn_size / 2 - 2 * UI_BORDER_SIZE - widget_size : btn_size / 2 + 2 * UI_BORDER_SIZE), dmIconPosition.y() - widget_size / 2);

  QPoint compassPosition(rightHandDM ? width() - experimentalButtonPosition.x() - widget_size : experimentalButtonPosition.x(), cemStatusPosition.y());

  instantReplayButton->move(experimentalButtonPosition.x() - UI_BORDER_SIZE - btn_size, experimentalButtonPosition.y() + screenRecorderButton->height());

  if (!hideBottomIcons && frogpilot_toggles.value(QLatin1String("cem_status")).toBool()) {
    paintCEMStatus(p, cemStatusPosition);
  }

  if (!hideBottomIcons && frogpilot_toggles.value(QLatin1String("compass")).toBool()) {
    paintCompass(p, compassPosition);
  }
}

void FrogPilotAnnotatedCameraWidget::paintBlindSpotPath(QPainter &p) {
  p.save();

  QLinearGradient bs(0, height(), 0, 0);
  bs.setColorAt(0.0f, QColor::fromHslF(0.0f, 0.75f, 0.5f, 0.4f));
  bs.setColorAt(0.5f, QColor::fromHslF(0.0f, 0.75f, 0.5f, 0.35f));
  bs.setColorAt(1.0f, QColor::fromHslF(0.0f, 0.75f, 0.5f, 0.0f));
  p.setBrush(bs);

  if (track_adjacent_vertices[0].boundingRect().width() > 0 && blindspotLeft) {
    p.drawPolygon(track_adjacent_vertices[0]);
  }
  if (track_adjacent_vertices[1].boundingRect().width() > 0 && blindspotRight) {
    p.drawPolygon(track_adjacent_vertices[1]);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintCEMStatus(QPainter &p, const QPoint &position) {
  p.save();

  QRect cemWidget(position, QSize(widget_size, widget_size));

  p.setBrush(blackColor(166));
  if (frogpilot_scene.conditional_status == 1) {
    p.setPen(QPen(QColor(bg_colors[STATUS_CEM_DISABLED]), 10));
  } else if (experimentalMode) {
    p.setPen(QPen(QColor(bg_colors[STATUS_EXPERIMENTAL_MODE_ENABLED]), 10));
  } else {
    p.setPen(QPen(blackColor(), 10));
  }
  p.drawRoundedRect(cemWidget, 24, 24);

  if (cemIcon) {
    p.drawPixmap(cemWidget, cemIcon->currentPixmap());
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintCompass(QPainter &p, const QPoint &position) {
  p.save();

  constexpr double PIXELS_PER_DEGREE = 2.5;

  constexpr int BASE_RIBBON_WIDTH = static_cast<int>(360 * PIXELS_PER_DEGREE);
  constexpr int BORDER_WIDTH = 10;
  constexpr int MARGIN = 5;
  constexpr int TRIANGLE_SIZE = 40;

  static QPixmap compassRibbon = [&]() {
    QPixmap ribbon(BASE_RIBBON_WIDTH * 2, widget_size);
    ribbon.fill(Qt::transparent);

    QPainter ribbonPainter(&ribbon);
    ribbonPainter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    QFont font = InterFont(65, QFont::Bold);
    ribbonPainter.setFont(font);
    QFontMetrics fm(font);

    QMap<int, QString> directionLabels = {{0, tr("N")}, {45, tr("NE")}, {90, tr("E")}, {135, tr("SE")}, {180, tr("S")}, {225, tr("SW")}, {270, tr("W")}, {315, tr("NW")}};

    for (int cycle = 0; cycle < 2; ++cycle) {
      int xOffset = cycle * 360;

      for (int degree = 0; degree < 360; ++degree) {
        int x = qRound((xOffset + degree) * PIXELS_PER_DEGREE);

        if (directionLabels.contains(degree)) {
          QString label = directionLabels[degree];
          ribbonPainter.setPen(whiteColor());
          ribbonPainter.drawText(x - fm.horizontalAdvance(label) / 2, fm.ascent(), label);
        }

        int notchHeight = (degree % 45 == 0) ? 35 : (degree % 15 == 0) ? 25 : 15;
        int notchWidth = (degree % 45 == 0) ? 5 : (degree % 15 == 0) ? 4 : 3;

        ribbonPainter.setPen(QPen(whiteColor(), notchWidth));
        ribbonPainter.drawLine(x, widget_size - notchHeight - MARGIN, x, widget_size);
      }
    }

    return ribbon;
  }();

  QRect compassWidget(position, QSize(widget_size, widget_size));

  p.setBrush(blackColor(166));
  p.setPen(QPen(blackColor(), BORDER_WIDTH));
  p.drawRoundedRect(compassWidget, 24, 24);

  QPainterPath clipPath;
  clipPath.addRoundedRect(compassWidget.adjusted(MARGIN, MARGIN, -MARGIN, -MARGIN), 24, 24);
  p.setClipPath(clipPath);

  int bearing = qRound(fmod(gpsBearing + 360.0, 360.0));
  int offset = qRound(bearing * PIXELS_PER_DEGREE) % BASE_RIBBON_WIDTH;
  int drawX = compassWidget.center().x() - offset;

  p.drawPixmap(drawX - BASE_RIBBON_WIDTH, compassWidget.top() + MARGIN, compassRibbon);
  p.drawPixmap(drawX, compassWidget.top() + MARGIN, compassRibbon);

  int triangleX = compassWidget.center().x();
  int triangleY = compassWidget.bottom() - TRIANGLE_SIZE;
  QPolygon triangle({
    QPoint(triangleX, triangleY - TRIANGLE_SIZE),
    QPoint(triangleX - TRIANGLE_SIZE / 1.5, triangleY),
    QPoint(triangleX + TRIANGLE_SIZE / 1.5, triangleY)
  });

  p.setBrush(whiteColor());
  p.setPen(Qt::NoPen);
  p.drawPolygon(triangle);

  p.restore();
}
