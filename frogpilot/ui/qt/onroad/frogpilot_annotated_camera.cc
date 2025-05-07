#include <QMovie>

#include "frogpilot/ui/qt/onroad/frogpilot_annotated_camera.h"

FrogPilotAnnotatedCameraWidget::FrogPilotAnnotatedCameraWidget(QWidget *parent) : QWidget(parent) {
  animationTimer = new QTimer(this);

  brakePedalImg = loadPixmap("../../frogpilot/assets/other_images/brake_pedal.png", {btn_size, btn_size});
  chillModeIcon = loadPixmap("../../frogpilot/assets/other_images/chill_mode_icon.png", {img_size / 2, img_size / 2});
  curveIcon = loadPixmap("../../frogpilot/assets/other_images/curve_icon.png", {img_size / 2, img_size / 2});
  curveSpeedLeftIcon = loadPixmap("../../frogpilot/assets/other_images/curve_speed_left.png", {img_size, img_size});
  curveSpeedRightIcon = loadPixmap("../../frogpilot/assets/other_images/curve_speed_right.png", {img_size, img_size});
  dashboardIcon = loadPixmap("../../frogpilot/assets/other_images/dashboard_icon.png", {img_size / 2, img_size / 2});
  experimentalModeIcon = loadPixmap("../assets/img_experimental.svg", {img_size / 2, img_size / 2});
  gasPedalImg = loadPixmap("../../frogpilot/assets/other_images/gas_pedal.png", {btn_size, btn_size});
  leadIcon = loadPixmap("../../frogpilot/assets/other_images/lead_icon.png", {img_size / 2, img_size / 2});
  lightIcon = loadPixmap("../../frogpilot/assets/other_images/light_icon.png", {img_size / 2, img_size / 2});
  mapDataIcon = loadPixmap("../../frogpilot/assets/other_images/offline_maps_icon.png", {img_size / 2, img_size / 2});
  navigationIcon = loadPixmap("../../frogpilot/assets/other_images/navigation_icon.png", {img_size / 2, img_size / 2});
  nextMapsIcon = loadPixmap("../../frogpilot/assets/other_images/next_maps_icon.png", {img_size / 2, img_size / 2});
  pausedIcon = loadPixmap("../../frogpilot/assets/other_images/paused_icon.png", {img_size / 2, img_size / 2});
  speedIcon = loadPixmap("../../frogpilot/assets/other_images/speed_icon.png", {img_size / 2, img_size / 2});
  stopSignImg = loadPixmap("../../frogpilot/assets/other_images/stop_sign.png", {img_size, img_size});
  turnIcon = loadPixmap("../../frogpilot/assets/other_images/turn_icon.png", {img_size / 2, img_size / 2});

  QObject::connect(animationTimer, &QTimer::timeout, [this] {
    animationFrameIndex = (animationFrameIndex + 1) % totalFrames;
  });
}

void FrogPilotAnnotatedCameraWidget::showEvent(QShowEvent *event) {
  update_theme(frogpilotUIState());

  FrogPilotUIState &fs = *frogpilotUIState();
  QJsonObject &frogpilot_toggles = fs.frogpilot_toggles;
  UIState &s = *uiState();
  UIScene &scene = s.scene;

  if (scene.is_metric || frogpilot_toggles.value("use_si_metrics").toBool()) {
    accelerationUnit = tr("m/s²");
    leadDistanceUnit = tr("meters");
    leadSpeedUnit = frogpilot_toggles.value("use_si_metrics").toBool() ? tr("m/s") : tr("km/h");

    distanceConversion = 1.0f;
    speedConversion = scene.is_metric ? MS_TO_KPH : MS_TO_MPH;
    speedConversionMetrics = frogpilot_toggles.value("use_si_metrics").toBool() ? 1.0f : MS_TO_KPH;
  } else {
    accelerationUnit = tr("ft/s²");
    leadDistanceUnit = tr("feet");
    leadSpeedUnit = tr("mph");

    distanceConversion = METER_TO_FOOT;
    speedConversion = MS_TO_MPH;
    speedConversionMetrics = MS_TO_MPH;
  }

  updateSignals();
}

void FrogPilotAnnotatedCameraWidget::updateSignals() {
  blindspotImages.clear();
  signalImages.clear();

  bool isGif = false;

  QFileInfoList files = QDir("../../frogpilot/assets/active_theme/signals/").entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
  for (QFileInfo &fileInfo : files) {
    QString fileName = fileInfo.fileName();
    QString filePath = fileInfo.absoluteFilePath();

    if (fileName.endsWith(".gif", Qt::CaseInsensitive)) {
      isGif = true;

      QMovie movie(filePath);
      movie.start();

      int frameCount = movie.frameCount();
      for (int i = 0; i < frameCount; ++i) {
        movie.jumpToFrame(i);

        QPixmap frame = movie.currentPixmap();
        signalImages.append(frame);
        signalImages.append(frame.transformed(QTransform().scale(-1, 1)));
      }

      movie.stop();
    } else if (fileName.endsWith(".png", Qt::CaseInsensitive)) {
      QVector<QPixmap> &targetList = fileName.contains("blindspot", Qt::CaseInsensitive) ? blindspotImages : signalImages;

      QPixmap pixmap(filePath);
      targetList.append(pixmap);
      targetList.append(pixmap.transformed(QTransform().scale(-1, 1)));
    } else {
      QStringList parts = fileName.split('_');
      if (parts.size() == 2) {
        signalStyle = parts[0];
        signalAnimationLength = parts[1].toInt();
      }
    }
  }

  if (!signalImages.isEmpty()) {
    QPixmap &firstImage = signalImages.front();
    signalHeight = firstImage.height();
    signalWidth = firstImage.width();

    totalFrames = signalImages.size() / 2;

    if (isGif && signalStyle == "traditional") {
      signalMovement = (width() + (signalWidth * 2)) / totalFrames;
      signalStyle = "traditional_gif";
    } else {
      signalMovement = 0;
    }
  } else {
    signalStyle = "None";

    signalHeight = 0;
    signalWidth = 0;
    totalFrames = 0;
  }
}

void FrogPilotAnnotatedCameraWidget::updateState(const FrogPilotUIState &fs) {
  const FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;
  const SubMaster &fpsm = *(fs.sm);

  const cereal::FrogPilotPlan::Reader &frogpilotPlan = fpsm["frogpilotPlan"].getFrogpilotPlan();

  float speedLimitOffset = frogpilotPlan.getSlcSpeedLimitOffset() * speedConversion;

  mtscSpeedStr = (frogpilotPlan.getMtscSpeed() != 0) ? QString::number(std::nearbyint(fmin(speed, frogpilotPlan.getMtscSpeed()))) + speedUnit : "–";
  speedLimitOffsetStr = (speedLimitOffset != 0) ? QString::number(speedLimitOffset, 'f', 0).prepend((speedLimitOffset > 0) ? "+" : "-") : "–";
  vtscSpeedStr = (frogpilotPlan.getVtscSpeed() != 0) ? QString::number(std::nearbyint(fmin(speed, frogpilotPlan.getVtscSpeed()))) + speedUnit : "–";

  static QElapsedTimer standstillTimer;
  if (frogpilot_scene.standstill && frogpilot_scene.started_timer / UI_FREQ >= 60) {
    if (!standstillTimer.isValid()) {
      standstillTimer.start();
    }
    standstillDuration = frogpilot_scene.map_open ? 0 : standstillTimer.elapsed() / 1000;
  } else {
    standstillTimer.invalidate();

    standstillDuration = 0;
  }

  update();
}

void FrogPilotAnnotatedCameraWidget::paintFrogPilotWidgets(QPainter &p, UIState &s, FrogPilotUIState &fs, SubMaster &sm, SubMaster &fpsm, QJsonObject &frogpilot_toggles) {
  FrogPilotUIScene &frogpilot_scene = fs.frogpilot_scene;
  UIScene &scene = s.scene;

  const cereal::CarState::Reader &carState = fpsm["carState"].getCarState();
  const cereal::FrogPilotCarState::Reader &frogpilotCarState = fpsm["frogpilotCarState"].getFrogpilotCarState();
  const cereal::FrogPilotNavigation::Reader &frogpilotNavigation = fpsm["frogpilotNavigation"].getFrogpilotNavigation();
  const cereal::FrogPilotPlan::Reader &frogpilotPlan = fpsm["frogpilotPlan"].getFrogpilotPlan();
  const cereal::ModelDataV2::Reader &model = sm["modelV2"].getModelV2();

  if (frogpilot_toggles.value("adjacent_path_metrics").toBool() || frogpilot_toggles.value("adjacent_paths").toBool()) {
    paintAdjacentPaths(p, carState, model, frogpilot_scene, frogpilot_toggles);
  }

  if ((carState.getLeftBlindspot() || carState.getRightBlindspot()) && frogpilot_toggles.value("blind_spot_path").toBool()) {
    paintBlindSpotPath(p, carState, frogpilot_scene);
  }

  if (!hideBottomIcons && frogpilot_toggles.value("cem_status").toBool()) {
    paintCEMStatus(p, frogpilot_scene, sm);
  } else {
    cemStatusPosition.setX(0);
    cemStatusPosition.setY(0);
  }

  if (!frogpilot_scene.map_open && !frogpilotPlan.getSpeedLimitChanged() && isCruiseSet && frogpilot_toggles.value("csc_status").toBool()) {
    paintCurveSpeedControl(p, frogpilotPlan, frogpilot_toggles);
  }

  if (!frogpilot_scene.map_open && frogpilotCarState.getPauseLateral() && !hideBottomIcons) {
    paintLateralPaused(p, frogpilot_scene);
  } else {
    lateralPausedPosition.setX(0);
    lateralPausedPosition.setY(0);
  }

  if (!frogpilot_scene.map_open && (frogpilotCarState.getForceCoast() || frogpilotCarState.getPauseLongitudinal()) && !hideBottomIcons) {
    paintLongitudinalPaused(p, frogpilot_scene);
  }

  if (!bigMapOpen && frogpilot_toggles.value("pedals_on_ui").toBool()) {
    paintPedalIcons(p, carState, frogpilotCarState, frogpilot_scene, frogpilot_toggles);
  }

  if (frogpilotPlan.getSpeedLimitChanged()) {
    paintPendingSpeedLimit(p, frogpilotPlan);
  } else {
    pendingLimitTimer.invalidate();
  }

  if (frogpilot_toggles.value("radar_tracks").toBool()) {
    paintRadarTracks(p, model, s, fs, frogpilot_scene, sm, fpsm);
  }

  if (frogpilot_toggles.value("road_name_ui").toBool()) {
    paintRoadName(p);
  }

  if ((mutcdSpeedLimit || viennaSpeedLimit) && frogpilot_toggles.value("speed_limit_sources").toBool()) {
    paintSpeedLimitSources(p, frogpilotCarState, frogpilotNavigation, frogpilotPlan);
  }

  if (!frogpilot_scene.map_open && standstillDuration != 0) {
    paintStandstillTimer(p);
  }

  if (scene.track_vertices.length() >= 1 && frogpilotPlan.getRedLight() && frogpilot_toggles.value("show_stopping_point").toBool()) {
    paintStoppingPoint(p, scene, frogpilot_scene, frogpilot_toggles);
  }

  if (!bigMapOpen && (carState.getLeftBlinker() || carState.getRightBlinker())) {
    if (!animationTimer->isActive()) {
      animationTimer->start(signalAnimationLength);
    }
    paintTurnSignals(p, carState);
  } else if (animationTimer->isActive()) {
    animationTimer->stop();
  }
}

void FrogPilotAnnotatedCameraWidget::paintAdjacentPaths(QPainter &p, const cereal::CarState::Reader &carState, const cereal::ModelDataV2::Reader &model, FrogPilotUIScene &frogpilot_scene, QJsonObject &frogpilot_toggles) {
  std::function<void(float, float, QPolygonF &)> drawAdjacentPath = [this, &p](float width, float requirement, QPolygonF &polygon) {
    float ratio = std::clamp(width / requirement, 0.0f, 1.0f);
    float hue = ratio * (120.0f / 360.0f);

    QLinearGradient gradient(0, height(), 0, 0);
    gradient.setColorAt(0.0f, QColor::fromHslF(hue, 0.75f, 0.5f, 0.6f));
    gradient.setColorAt(0.5f, QColor::fromHslF(hue, 0.75f, 0.5f, 0.4f));
    gradient.setColorAt(1.0f, QColor::fromHslF(hue, 0.75f, 0.5f, 0.2f));

    p.setBrush(gradient);
    p.drawPolygon(polygon);
  };

  std::function<void(bool, float, QPolygonF &)> drawAdjacentPathMetric = [&](bool isBlindSpot, float width, QPolygonF &polygon) {
    QString text = isBlindSpot ? tr("Vehicle in blind spot") : QString::number(width * distanceConversion, 'f', 2) + leadDistanceUnit;

    p.setFont(InterFont(30, QFont::DemiBold));
    p.setPen(Qt::white);
    p.drawText(polygon.boundingRect(), Qt::AlignCenter, text);
  };

  if (frogpilot_scene.lane_width_left != 0 && model.getRoadEdgeStds()[0] < 0.75f) {
    p.save();

    drawAdjacentPath(frogpilot_scene.lane_width_left, frogpilot_toggles.value("lane_detection_width").toDouble(), frogpilot_scene.track_adjacent_vertices[0]);

    if (frogpilot_toggles.value("adjacent_path_metrics").toBool()) {
      drawAdjacentPathMetric(carState.getLeftBlindspot(), frogpilot_scene.lane_width_left, frogpilot_scene.track_adjacent_vertices[0]);
    }

    p.restore();
  }

  if (frogpilot_scene.lane_width_right != 0 && model.getRoadEdgeStds()[1] < 0.75f) {
    p.save();

    drawAdjacentPath(frogpilot_scene.lane_width_right, frogpilot_toggles.value("lane_detection_width").toDouble(), frogpilot_scene.track_adjacent_vertices[1]);

    if (frogpilot_toggles.value("adjacent_path_metrics").toBool()) {
      drawAdjacentPathMetric(carState.getRightBlindspot(), frogpilot_scene.lane_width_right, frogpilot_scene.track_adjacent_vertices[1]);
    }

    p.restore();
  }
}

void FrogPilotAnnotatedCameraWidget::paintBlindSpotPath(QPainter &p, const cereal::CarState::Reader &carState, FrogPilotUIScene &frogpilot_scene) {
  p.save();

  QLinearGradient bs(0, height(), 0, 0);
  bs.setColorAt(0.0f, QColor::fromHslF(0 / 360.0f, 0.75f, 0.5f, 0.6f));
  bs.setColorAt(0.5f, QColor::fromHslF(0 / 360.0f, 0.75f, 0.5f, 0.4f));
  bs.setColorAt(1.0f, QColor::fromHslF(0 / 360.0f, 0.75f, 0.5f, 0.2f));

  p.setBrush(bs);
  if (frogpilot_scene.lane_width_left != 0 && carState.getLeftBlindspot()) {
    p.drawPolygon(frogpilot_scene.track_adjacent_vertices[0]);
  }
  if (frogpilot_scene.lane_width_right != 0 && carState.getRightBlindspot()) {
    p.drawPolygon(frogpilot_scene.track_adjacent_vertices[1]);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintCEMStatus(QPainter &p, FrogPilotUIScene &frogpilot_scene, SubMaster &sm) {
  if (dmIconPosition == QPoint(0, 0)) {
    return;
  }

  p.save();

  cemStatusPosition.rx() = dmIconPosition.x();
  cemStatusPosition.ry() = dmIconPosition.y() - img_size / 2;
  cemStatusPosition.rx() += (rightHandDM ? -img_size : img_size) / (frogpilot_scene.map_open ? 1.25 : 1);

  QRect cemWidget(cemStatusPosition.x(), cemStatusPosition.y(), img_size, img_size);

  if (frogpilot_scene.conditional_status == 1) {
    p.setPen(QPen(QColor(bg_colors[STATUS_CONDITIONAL_OVERRIDDEN]), 10));
  } else if (frogpilot_scene.enabled && sm["controlsState"].getControlsState().getExperimentalMode()) {
    p.setPen(QPen(QColor(bg_colors[STATUS_EXPERIMENTAL_MODE_ENABLED]), 10));
  } else {
    p.setPen(QPen(blackColor(), 10));
  }
  p.setBrush(blackColor(166));
  p.drawRoundedRect(cemWidget, 24, 24);

  QPixmap iconToDraw;
  if (frogpilot_scene.enabled && sm["controlsState"].getControlsState().getExperimentalMode()) {
    if (frogpilot_scene.conditional_status == 1) {
      iconToDraw = chillModeIcon;
    } else if (frogpilot_scene.conditional_status == 2) {
      iconToDraw = experimentalModeIcon;
    } else if (frogpilot_scene.conditional_status == 3 || frogpilot_scene.conditional_status == 4) {
      iconToDraw = speedIcon;
    } else if (frogpilot_scene.conditional_status == 5 || frogpilot_scene.conditional_status == 7) {
      iconToDraw = turnIcon;
    } else if (frogpilot_scene.conditional_status == 6 || frogpilot_scene.conditional_status == 11 || frogpilot_scene.conditional_status == 12) {
      iconToDraw = lightIcon;
    } else if (frogpilot_scene.conditional_status == 8) {
      iconToDraw = curveIcon;
    } else if (frogpilot_scene.conditional_status == 9 || frogpilot_scene.conditional_status == 10) {
      iconToDraw = leadIcon;
    } else {
      iconToDraw = experimentalModeIcon;
    }
  } else {
    iconToDraw = chillModeIcon;
  }
  p.drawPixmap(cemWidget, iconToDraw);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintCurveSpeedControl(QPainter &p, const cereal::FrogPilotPlan::Reader &frogpilotPlan, QJsonObject &frogpilot_toggles) {
  p.save();

  std::function<void(QRect&, const QString&, bool)> drawCurveSpeedControl = [&](QRect &rect, const QString &speedStr, bool isMtsc) {
    if (isMtsc && !frogpilotPlan.getVtscControllingCurve()) {
      p.setPen(QPen(greenColor(), 10));
      p.setBrush(greenColor(166));
      p.setFont(InterFont(45, QFont::Bold));
    } else if (!isMtsc && frogpilotPlan.getVtscControllingCurve()) {
      p.setPen(QPen(redColor(), 10));
      p.setBrush(redColor(166));
      p.setFont(InterFont(45, QFont::Bold));
    } else {
      p.setPen(QPen(blackColor(), 10));
      p.setBrush(blackColor(166));
      p.setFont(InterFont(35, QFont::DemiBold));
    }

    p.drawRoundedRect(rect, 24, 24);

    p.setPen(QPen(whiteColor(), 6));
    p.drawText(rect.adjusted(20, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, speedStr);
  };

  QRect curveSpeedRect(QPoint(setSpeedRect.right() + UI_BORDER_SIZE, setSpeedRect.top()), QSize(defaultSize.width() * 1.25, defaultSize.width() * 1.25));
  QPixmap scaledCurveSpeedIcon = (frogpilotPlan.getRoadCurvature() < 0 ? curveSpeedLeftIcon : curveSpeedRightIcon).scaled(curveSpeedRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

  p.setOpacity(1.0);

  if ((setSpeed - frogpilotPlan.getMtscSpeed() > 1) && frogpilot_toggles.value("map_turn_speed_controller").toBool()) {
    QRect mtscRect(curveSpeedRect.topLeft() + QPoint(0, curveSpeedRect.height() + 10), QSize(curveSpeedRect.width(), frogpilotPlan.getVtscControllingCurve() ? 50 : 100));
    drawCurveSpeedControl(mtscRect, mtscSpeedStr, true);

    if ((setSpeed - frogpilotPlan.getVtscSpeed() > 1) && frogpilot_toggles.value("vision_turn_speed_controller").toBool()) {
      QRect vtscRect(mtscRect.topLeft() + QPoint(0, mtscRect.height() + 20), QSize(mtscRect.width(), frogpilotPlan.getVtscControllingCurve() ? 100 : 50));
      drawCurveSpeedControl(vtscRect, vtscSpeedStr, false);
    }

    p.drawPixmap(curveSpeedRect, scaledCurveSpeedIcon);
  } else if ((setSpeed - frogpilotPlan.getVtscSpeed() > 1) && frogpilot_toggles.value("vision_turn_speed_controller").toBool()) {
    QRect vtscRect(curveSpeedRect.topLeft() + QPoint(0, curveSpeedRect.height() + 10), QSize(curveSpeedRect.width(), 150));
    drawCurveSpeedControl(vtscRect, vtscSpeedStr, false);

    p.drawPixmap(curveSpeedRect, scaledCurveSpeedIcon);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintLateralPaused(QPainter &p, FrogPilotUIScene &frogpilot_scene) {
  if (dmIconPosition == QPoint(0, 0)) {
    return;
  }

  p.save();

  if (cemStatusPosition != QPoint(0, 0)) {
    lateralPausedPosition = cemStatusPosition;
  } else {
    lateralPausedPosition.rx() = dmIconPosition.x();
    lateralPausedPosition.ry() = dmIconPosition.y() - img_size / 2;
  }
  lateralPausedPosition.rx() += ((rightHandDM ? -img_size : img_size) * 1.5) / (frogpilot_scene.map_open ? 1.25 : 1);

  QRect lateralWidget(lateralPausedPosition.x(), lateralPausedPosition.y(), img_size, img_size);

  p.setBrush(blackColor(166));
  p.setPen(QPen(QColor(bg_colors[STATUS_TRAFFIC_MODE_ENABLED]), 10));
  p.drawRoundedRect(lateralWidget, 24, 24);

  p.setOpacity(0.5);
  p.drawPixmap(lateralWidget, turnIcon);
  p.setOpacity(0.75);
  p.drawPixmap(lateralWidget, pausedIcon);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintLeadMetrics(QPainter &p, bool adjacent, QPointF *chevron, const cereal::FrogPilotPlan::Reader &frogpilotPlan, const cereal::RadarState::LeadData::Reader &lead_data) {
  float lead_distance = lead_data.getDRel() + (adjacent ? fabs(lead_data.getYRel()) : 0);
  float lead_speed = std::max(lead_data.getVLead(), 0.0f);

  p.setFont(InterFont(35, QFont::Bold));
  p.setPen(Qt::white);

  QString text;
  if (adjacent) {
    text = QString("%1 %2 | %3 %4")
              .arg(qRound(lead_distance * distanceConversion))
              .arg(leadDistanceUnit)
              .arg(qRound(lead_speed * speedConversionMetrics))
              .arg(leadSpeedUnit);
  } else {
    text = QString("%1 %2 (%3) | %4 %5 | %6%7")
              .arg(qRound(lead_distance * distanceConversion))
              .arg(leadDistanceUnit)
              .arg(QString("Desired: %1").arg(frogpilotPlan.getDesiredFollowDistance() * distanceConversion))
              .arg(qRound(lead_speed * speedConversionMetrics))
              .arg(leadSpeedUnit)
              .arg(QString::number(std::max(lead_distance / std::max(speed / speedConversion, 1.0f), 1.0f), 'f', 2))
              .arg("s");
  }

  QFontMetrics metrics(p.font());
  int textHeight = metrics.height();
  int textWidth = metrics.horizontalAdvance(text);

  int text_x = ((chevron[2].x() + chevron[0].x()) / 2) - textWidth / 2;
  int text_y = chevron[0].y() + textHeight + 5;

  if (!adjacent) {
    int xMargin = textWidth * 0.25;
    int yMargin = textHeight * 0.25;

    leadTextRect = QRect(text_x, text_y - textHeight, textWidth, textHeight).adjusted(-xMargin, -yMargin, xMargin, yMargin);
    p.drawText(text_x, text_y, text);
  } else {
    QRect adjacentTextRect(text_x, text_y - textHeight, textWidth, textHeight);
    if (!adjacentTextRect.intersects(leadTextRect)) {
      p.drawText(text_x, text_y, text);
    }
  }
}

void FrogPilotAnnotatedCameraWidget::paintLongitudinalPaused(QPainter &p, FrogPilotUIScene &frogpilot_scene) {
  if (dmIconPosition == QPoint(0, 0)) {
    return;
  }

  p.save();

  QPoint longitudinalIconPosition;
  if (lateralPausedPosition != QPoint(0, 0)) {
    longitudinalIconPosition = lateralPausedPosition;
  } else if (cemStatusPosition != QPoint(0, 0)) {
    longitudinalIconPosition = cemStatusPosition;
  } else {
    longitudinalIconPosition.rx() = dmIconPosition.x();
    longitudinalIconPosition.ry() = dmIconPosition.y() - img_size / 2;
  }
  longitudinalIconPosition.rx() += ((rightHandDM ? -img_size : img_size) * 1.5) / (frogpilot_scene.map_open ? 1.25 : 1);

  QRect longitudinalWidget(longitudinalIconPosition.x(), longitudinalIconPosition.y(), img_size, img_size);

  p.setBrush(blackColor(166));
  p.setPen(QPen(QColor(bg_colors[STATUS_TRAFFIC_MODE_ENABLED]), 10));
  p.drawRoundedRect(longitudinalWidget, 24, 24);

  p.setOpacity(0.5);
  p.drawPixmap(longitudinalWidget, speedIcon);
  p.setOpacity(0.75);
  p.drawPixmap(longitudinalWidget, pausedIcon);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintPathEdges(QPainter &p, const cereal::NavInstruction::Reader &navInstruction, const UIScene &scene, const FrogPilotUIScene &frogpilot_scene, SubMaster &sm) {
  p.save();

  std::function<void(QLinearGradient&, const QColor&)> setPathEdgeColors = [&](QLinearGradient &gradient, QColor baseColor) {
    baseColor.setAlphaF(1.0f); gradient.setColorAt(0.0f, baseColor);
    baseColor.setAlphaF(0.5f); gradient.setColorAt(0.5f, baseColor);
    baseColor.setAlphaF(0.1f); gradient.setColorAt(1.0f, baseColor);
  };

  QLinearGradient pe(0, height(), 0, 0);
  if (frogpilot_scene.always_on_lateral_active) {
    setPathEdgeColors(pe, bg_colors[STATUS_ALWAYS_ON_LATERAL_ACTIVE]);
  } else if (frogpilot_scene.conditional_status == 1) {
    setPathEdgeColors(pe, bg_colors[STATUS_CONDITIONAL_OVERRIDDEN]);
  } else if (sm["controlsState"].getControlsState().getExperimentalMode()) {
    setPathEdgeColors(pe, bg_colors[STATUS_EXPERIMENTAL_MODE_ENABLED]);
  } else if (frogpilot_scene.traffic_mode_enabled) {
    setPathEdgeColors(pe, bg_colors[STATUS_TRAFFIC_MODE_ENABLED]);
  } else if (frogpilot_scene.model_length > navInstruction.getManeuverDistance() && navInstruction.getManeuverDistance() >= 1) {
    setPathEdgeColors(pe, bg_colors[STATUS_NAVIGATION_ACTIVE]);
  } else if (!frogpilot_scene.use_stock_colors) {
    setPathEdgeColors(pe, frogpilot_scene.path_edges_color);
  } else {
    pe.setColorAt(0.0f, QColor::fromHslF(148 / 360.0f, 0.94f, 0.51f, 1.0f));
    pe.setColorAt(0.5f, QColor::fromHslF(112 / 360.0f, 1.00f, 0.68f, 0.5f));
    pe.setColorAt(1.0f, QColor::fromHslF(112 / 360.0f, 1.00f, 0.68f, 0.1f));
  }

  QPainterPath path;
  path.addPolygon(scene.track_vertices);
  path.addPolygon(frogpilot_scene.track_edge_vertices);
  p.setBrush(pe);
  p.drawPath(path);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintPedalIcons(QPainter &p, const cereal::CarState::Reader &carState, const cereal::FrogPilotCarState::Reader &frogpilotCarState, FrogPilotUIScene &frogpilot_scene, QJsonObject &frogpilot_toggles) {
  p.save();

  float brakeOpacity = 1.0f;
  float gasOpacity = 1.0f;

  if (frogpilot_toggles.value("dynamic_pedals_on_ui").toBool()) {
    brakeOpacity = frogpilot_scene.standstill ? 1.0f : carState.getAEgo() < -0.25f ? std::max(0.25f, std::abs(carState.getAEgo())) : 0.25f;
    gasOpacity = std::max(0.25f, carState.getAEgo());
  } else if (frogpilot_toggles.value("static_pedals_on_ui").toBool()) {
    brakeOpacity = frogpilot_scene.standstill || frogpilotCarState.getBrakeLights() || carState.getAEgo() < -0.25f ? 1.0f : 0.25f;
    gasOpacity = carState.getAEgo() > 0.25 ? 1.0f : 0.25f;
  }

  int startX = experimentalButtonPosition.x();
  int startY = experimentalButtonPosition.y() + btn_size + UI_BORDER_SIZE;

  p.setOpacity(brakeOpacity);
  p.drawPixmap(startX, startY, brakePedalImg);

  p.setOpacity(gasOpacity);
  p.drawPixmap(startX + btn_size / 2, startY, gasPedalImg);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintPendingSpeedLimit(QPainter &p, const cereal::FrogPilotPlan::Reader &frogpilotPlan) {
  p.save();

  if (!pendingLimitTimer.isValid()) {
    pendingLimitTimer.start();
  }

  QString newSpeedLimitStr = (frogpilotPlan.getUnconfirmedSlcSpeedLimit() > 1) ? QString::number(std::nearbyint(frogpilotPlan.getUnconfirmedSlcSpeedLimit() * speedConversion)) : "–";
  newSpeedLimitRect = speedLimitRect.translated(speedLimitRect.width() + UI_BORDER_SIZE, 0);

  if (!viennaSpeedLimit) {
    newSpeedLimitRect.setWidth(newSpeedLimitStr.size() >= 3 ? 200 : 175);

    p.setBrush(whiteColor());
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(newSpeedLimitRect, 24, 24);
    p.setPen(pendingLimitTimer.elapsed() % 1000 < 500 ? QPen(blackColor(), 6) : QPen(redColor(), 6));
    p.drawRoundedRect(newSpeedLimitRect.adjusted(9, 9, -9, -9), 16, 16);

    p.setFont(InterFont(28, QFont::DemiBold));
    p.drawText(newSpeedLimitRect.adjusted(0, 22, 0, 0), Qt::AlignTop | Qt::AlignHCenter, tr("PENDING"));
    p.drawText(newSpeedLimitRect.adjusted(0, 51, 0, 0), Qt::AlignTop | Qt::AlignHCenter, tr("LIMIT"));
    p.setFont(InterFont(70, QFont::Bold));
    p.drawText(newSpeedLimitRect.adjusted(0, 85, 0, 0), Qt::AlignTop | Qt::AlignHCenter, newSpeedLimitStr);
  } else {
    p.setBrush(whiteColor());
    p.setPen(Qt::NoPen);
    p.drawEllipse(newSpeedLimitRect);
    p.setPen(QPen(Qt::red, 20));
    p.drawEllipse(newSpeedLimitRect.adjusted(16, 16, -16, -16));

    p.setPen(pendingLimitTimer.elapsed() % 1000 < 500 ? QPen(blackColor(), 6) : QPen(redColor(), 6));
    p.setFont(InterFont((newSpeedLimitStr.size() >= 3) ? 60 : 70, QFont::Bold));
    p.drawText(newSpeedLimitRect, Qt::AlignCenter, newSpeedLimitStr);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintRadarTracks(QPainter &p, const cereal::ModelDataV2::Reader &model, UIState &s, FrogPilotUIState &fs, FrogPilotUIScene &frogpilot_scene, SubMaster &sm, SubMaster &fpsm) {
  p.save();

  capnp::List<cereal::LiveTracks>::Reader liveTracks = fpsm["liveTracks"].getLiveTracks();
  update_radar_tracks(liveTracks, model.getPosition(), s, sm);

  int diameter = 25;

  QRect viewport = p.viewport();

  for (std::size_t i = 0; i < frogpilot_scene.live_radar_tracks.size(); ++i) {
    const RadarTrackData &track = frogpilot_scene.live_radar_tracks[i];

    float x = std::clamp(static_cast<float>(track.calibrated_point.x()), 0.0f, float(viewport.width() - diameter));
    float y = std::clamp(static_cast<float>(track.calibrated_point.y()), 0.0f, float(viewport.height() - diameter));

    p.setBrush(redColor());
    p.drawEllipse(QPointF(x + diameter / 2.0f, y + diameter / 2.0f), diameter / 2.0f, diameter / 2.0f);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintRainbowPath(QPainter &p, QLinearGradient &bg, float lin_grad_point, SubMaster &sm) {
  p.save();

  static float hue_offset = 0.0;
  if (sm["carState"].getCarState().getVEgo() > 0) {
    hue_offset += powf(sm["carState"].getCarState().getVEgo(), 0.5f) / sqrtf(145.0f / MS_TO_KPH);
  }

  float alpha = util::map_val(lin_grad_point, 0.0f, 1.0f, 0.5f, 0.1f);
  float path_hue = fmodf((lin_grad_point * 360.0f) + hue_offset, 360.0f);

  bg.setColorAt(lin_grad_point, QColor::fromHslF(path_hue / 360.0f, 1.0f, 0.5f, alpha));
  bg.setSpread(QGradient::RepeatSpread);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintRoadName(QPainter &p) {
  QString roadName = QString::fromStdString(params_memory.get("RoadName"));
  if (roadName.isEmpty()) {
    return;
  }

  p.save();

  QFont font = InterFont(40, QFont::DemiBold);

  int textWidth = QFontMetrics(font).horizontalAdvance(roadName);

  QRect roadNameRect((width() - (textWidth + 100)) / 2, rect().bottom() - 55 + 1, textWidth + 100, 50);

  p.setBrush(blackColor(166));
  p.setOpacity(1.0);
  p.setPen(QPen(blackColor(), 10));
  p.drawRoundedRect(roadNameRect, 24, 24);

  p.setFont(font);
  p.setPen(QPen(Qt::white, 6));
  p.drawText(roadNameRect, Qt::AlignCenter, roadName);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintSpeedLimitSources(QPainter &p, const cereal::FrogPilotCarState::Reader &frogpilotCarState, const cereal::FrogPilotNavigation::Reader &frogpilotNavigation, const cereal::FrogPilotPlan::Reader &frogpilotPlan) {
  p.save();

  std::function<void(QRect&, QPixmap&, const QString&, const double)> drawSource = [&](QRect &rect, QPixmap &icon, QString title, double speedLimitValue) {
    if (QString::fromUtf8(frogpilotPlan.getSlcSpeedLimitSource().cStr()) == "Mapbox" && title == "Navigation") {
      speedLimitValue = frogpilotPlan.getSlcMapboxSpeedLimit() * speedConversion;

      title = "Mapbox";
    }

    if (QString::fromUtf8(frogpilotPlan.getSlcSpeedLimitSource().cStr()) == title && speedLimitValue != 0) {
      p.setBrush(redColor(166));
      p.setFont(InterFont(35, QFont::Bold));
      p.setPen(QPen(redColor(), 10));
    } else {
      p.setBrush(blackColor(166));
      p.setFont(InterFont(35, QFont::DemiBold));
      p.setPen(QPen(blackColor(), 10));
    }

    QRect iconRect(rect.x() + 20, rect.y() + (rect.height() - img_size / 4) / 2, img_size / 4, img_size / 4);
    QPixmap scaledIcon = icon.scaled(iconRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QString speedText;
    if (speedLimitValue != 0) {
      speedText = QString::number(std::nearbyint(speedLimitValue)) + " " + speedUnit;
    } else {
      speedText = "N/A";
    }

    QString fullText = tr(title.toUtf8().constData()) + " - " + speedText;

    p.setOpacity(1.0);
    p.drawRoundedRect(rect, 24, 24);
    p.drawPixmap(iconRect, scaledIcon);

    p.setPen(QPen(whiteColor(), 6));
    QRect textRect(iconRect.right() + 10, rect.y(), rect.width() - iconRect.width() - 30, rect.height());
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, fullText);
  };

  QRect dashboardRect(speedLimitRect.x() - signMargin, speedLimitRect.y() + speedLimitRect.height() + UI_BORDER_SIZE, 450, 60);
  QRect mapDataRect(dashboardRect.x(), dashboardRect.y() + dashboardRect.height() + UI_BORDER_SIZE / 2, 450, 60);
  QRect navigationRect(mapDataRect.x(), mapDataRect.y() + mapDataRect.height() + UI_BORDER_SIZE / 2, 450, 60);
  QRect nextLimitRect(navigationRect.x(), navigationRect.y() + navigationRect.height() + UI_BORDER_SIZE / 2, 450, 60);

  drawSource(dashboardRect, dashboardIcon, "Dashboard", frogpilotCarState.getDashboardSpeedLimit() * speedConversion);
  drawSource(mapDataRect, mapDataIcon, "Map Data", frogpilotPlan.getSlcMapSpeedLimit() * speedConversion);
  drawSource(navigationRect, navigationIcon, "Navigation", frogpilotNavigation.getNavigationSpeedLimit() * speedConversion);
  drawSource(nextLimitRect, nextMapsIcon, "Upcoming", frogpilotPlan.getSlcNextSpeedLimit() * speedConversion);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintStandstillTimer(QPainter &p) {
  p.save();

  float transition = 0.0f;

  QColor startColor, endColor;
  if (standstillDuration < 60) {
    startColor = endColor = bg_colors[STATUS_ENGAGED];
  } else if (standstillDuration < 150) {
    startColor = bg_colors[STATUS_ENGAGED];
    endColor = bg_colors[STATUS_CONDITIONAL_OVERRIDDEN];

    transition = (standstillDuration - 60) / 30.0f;
  } else if (standstillDuration < 300) {
    startColor = bg_colors[STATUS_CONDITIONAL_OVERRIDDEN];
    endColor = bg_colors[STATUS_TRAFFIC_MODE_ENABLED];

    transition = (standstillDuration - 150) / 30.0f;
  } else {
    startColor = endColor = bg_colors[STATUS_TRAFFIC_MODE_ENABLED];
  }

  QColor blendedColor = QColor::fromRgbF(
    startColor.redF() + transition * (endColor.redF() - startColor.redF()),
    startColor.greenF() + transition * (endColor.greenF() - startColor.greenF()),
    startColor.blueF() + transition * (endColor.blueF() - startColor.blueF())
  );

  int minutes = standstillDuration / 60;
  QString minuteText = minutes == 1 ? "1 minute" : QString("%1 minutes").arg(minutes);

  p.setFont(InterFont(176, QFont::Bold));
  QRect minuteRect = p.fontMetrics().boundingRect(minuteText);
  minuteRect.moveCenter(QPoint(rect().center().x(), 210 - minuteRect.height() / 2));

  p.setPen(QPen(blendedColor));
  p.drawText(minuteRect, Qt::AlignBottom | Qt::AlignHCenter, minuteText);

  int seconds = standstillDuration % 60;
  QString secondsText = QString("%1 seconds").arg(seconds);

  p.setFont(InterFont(66));
  QRect secondsRect = p.fontMetrics().boundingRect(secondsText);
  secondsRect.moveCenter(QPoint(rect().center().x(), 290 - secondsRect.height() / 2));

  p.setPen(QColor(255, 255, 255));
  p.drawText(secondsRect, Qt::AlignBottom | Qt::AlignHCenter, secondsText);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintStoppingPoint(QPainter &p, UIScene &scene, FrogPilotUIScene &frogpilot_scene, QJsonObject &frogpilot_toggles) {
  p.save();

  QPointF center_point = (scene.track_vertices.first() + scene.track_vertices.last()) / 2.0;
  QPointF adjusted_point = center_point - QPointF(stopSignImg.width() / 2, stopSignImg.height());
  p.drawPixmap(adjusted_point, stopSignImg);

  if (frogpilot_toggles.value("show_stopping_point_metrics").toBool()) {
    QFont font = InterFont(35, QFont::DemiBold);
    QString text = QString::number(std::nearbyint(frogpilot_scene.model_length * distanceConversion)) + leadDistanceUnit;
    QPointF text_position = center_point - QPointF(QFontMetrics(font).horizontalAdvance(text) / 2, stopSignImg.height() + 35);

    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(text_position, text);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintTurnSignals(QPainter &p, const cereal::CarState::Reader &carState) {
  p.save();

  bool blindspotActive = carState.getLeftBlinker() ? carState.getLeftBlindspot() : carState.getRightBlindspot();

  if (signalStyle == "static") {
    int signalXPosition = carState.getLeftBlinker() ? (rect().center().x() * 0.75) - signalWidth : rect().center().x() * 1.25;
    int signalYPosition = signalHeight / 2;

    if (blindspotActive && !blindspotImages.empty()) {
      p.drawPixmap(signalXPosition, signalYPosition, signalWidth, signalHeight, blindspotImages[carState.getLeftBlinker() ? 0 : 1]);
    } else {
      p.drawPixmap(signalXPosition, signalYPosition, signalWidth, signalHeight, signalImages[2 * animationFrameIndex + (carState.getLeftBlinker() ? 0 : 1)]);
    }
  } else {
    int signalXPosition;
    if (signalStyle == "traditional_gif") {
      signalXPosition = carState.getLeftBlinker() ? width() - (animationFrameIndex * signalMovement) + signalWidth : (animationFrameIndex * signalMovement) - signalWidth;
    } else {
      signalXPosition = carState.getLeftBlinker() ? width() - ((animationFrameIndex + 1) * signalWidth) : animationFrameIndex * signalWidth;
    }

    int signalYPosition = height() - signalHeight;
    signalYPosition -= alertHeight;

    if (blindspotActive && !blindspotImages.empty()) {
      p.drawPixmap(carState.getLeftBlinker() ? width() - signalWidth : 0, signalYPosition, signalWidth, signalHeight, blindspotImages[carState.getLeftBlinker() ? 0 : 1]);
    } else {
      p.drawPixmap(signalXPosition, signalYPosition, signalWidth, signalHeight, signalImages[2 * animationFrameIndex + (carState.getLeftBlinker() ? 0 : 1)]);
    }
  }

  p.restore();
}
