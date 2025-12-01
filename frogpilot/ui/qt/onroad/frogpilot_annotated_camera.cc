#include <QPainterPath>

#include "frogpilot/ui/qt/onroad/frogpilot_annotated_camera.h"

FrogPilotAnnotatedCameraWidget::FrogPilotAnnotatedCameraWidget(CameraWidget *nvg, QWidget *parent) : QWidget(parent) {
  instantReplayButton = new InstantReplayButton(nvg);
  instantReplayButton->setVisible(false);

  personalityButton = new DrivingPersonalityButton(nvg);
  personalityButton->setVisible(false);

  brakePedalImg = loadPixmap("../../frogpilot/assets/other_images/brake_pedal.png", {btn_size, btn_size});
  curveSpeedIcon = loadPixmap("../../frogpilot/assets/other_images/curve_speed.png", {btn_size, btn_size});
  curveSpeedIconFlipped = curveSpeedIcon.transformed(QTransform().scale(-1, 1));
  gasPedalImg = loadPixmap("../../frogpilot/assets/other_images/gas_pedal.png", {btn_size, btn_size});
  pausedIcon = loadPixmap("../../frogpilot/assets/other_images/paused_icon.png", {widget_size, widget_size});
  speedIcon = loadPixmap("../../frogpilot/assets/other_images/speed_icon.png", {widget_size, widget_size});
  stopSignImg = loadPixmap("../../frogpilot/assets/other_images/stop_sign.png", {btn_size, btn_size});

  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &FrogPilotAnnotatedCameraWidget::updateSignals);
  QObject::connect(nvg, &CameraWidget::vipcThreadFrameReceived, frogpilotUIState(), &FrogPilotUIState::cameraFrameReceived);
  QObject::connect(uiState(), &UIState::offroadTransition, this, [this] {
    standstillTimer.invalidate();
  });
}

void FrogPilotAnnotatedCameraWidget::showEvent(QShowEvent *event) {
  if (assetsLoaded) {
    return;
  }

  updateSignals();
}

void FrogPilotAnnotatedCameraWidget::hideEvent(QHideEvent *event) {
  if (cemIcon) {
    cemIcon->stop();
  }

  QWidget::hideEvent(event);
}

void FrogPilotAnnotatedCameraWidget::updateSignals() {
  if (signalTimer.isValid()) {
    signalTimer.start();
  }

  signalAnimationLength = 0;
  signalStyle = "None";

  QVector<QPixmap>().swap(blindspotImages);
  QVector<QPixmap>().swap(signalImages);

  bool isGif = false;

  QFileInfoList files = QDir("../../frogpilot/assets/active_theme/signals/").entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
  for (const QFileInfo &fileInfo : files) {
    QString fileName = fileInfo.fileName();
    QString filePath = fileInfo.absoluteFilePath();
    bool isBlindspot = fileName.contains("blindspot", Qt::CaseInsensitive);

    QVector<QPixmap> &targetImages = isBlindspot ? blindspotImages : signalImages;

    if (fileName.endsWith(".gif", Qt::CaseInsensitive)) {
      isGif = isGif || !isBlindspot;

      QMovie movie(filePath);
      movie.setCacheMode(QMovie::CacheNone);
      movie.start();

      int frameCount = movie.frameCount();
      if (isBlindspot) {
        frameCount = std::min(frameCount, 1);
      }
      targetImages.reserve(frameCount);

      for (int i = 0; i < frameCount; ++i) {
        movie.jumpToFrame(i);

        targetImages.append(movie.currentPixmap());
      }

      movie.stop();
    } else if (fileName.endsWith(".png", Qt::CaseInsensitive)) {
      targetImages.append(QPixmap(filePath));
    } else {
      QStringList parts = fileName.split('_');
      if (parts.size() == 2) {
        bool validInterval = false;

        const int interval = parts[1].toInt(&validInterval);

        if (validInterval && interval > 0) {
          signalStyle = parts[0];
          signalAnimationLength = interval;
        }
      }
    }
  }

  if (!signalImages.isEmpty()) {
    QPixmap &firstImage = signalImages.front();
    signalHeight = firstImage.height();
    signalWidth = firstImage.width();
    totalFrames = signalImages.size();

    if (isGif && signalStyle == "traditional") {
      signalStyle = "traditional_gif";
    }
  } else {
    signalAnimationLength = 0;
    signalHeight = 0;
    signalWidth = 0;
    totalFrames = 0;

    signalStyle = "None";
  }

  assetsLoaded = true;
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

  const capnp::List<float>::Reader &positionX = modelV2.getPosition().getX();

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

  accelerationEgo = carState.getAEgo();
  blindspotLeft = carState.getLeftBlindspot();
  blindspotRight = carState.getRightBlindspot();
  blinkerLeft = carState.getLeftBlinker();
  blinkerRight = carState.getRightBlinker();
  brakeLights = frogpilotCarState.getBrakeLights();
  cameraSpeedLimit = frogpilotSignReading.getSpeedLimit();
  cscActive = frogpilotPlan.getCscActive() && !carState.getStandstill();
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
  stoppingDistance = positionX.size() != 0 ? positionX[positionX.size() - 1] : 0.0f;
  unconfirmedSpeedLimit = frogpilotPlan.getUnconfirmedSlcSpeedLimit();
  vEgo = carState.getVEgo();
  weatherDaytime = frogpilotPlan.getWeatherDaytime();
  weatherId = frogpilotPlan.getWeatherId();

  hideBottomIcons = alertHeight != 0 || (signalStyle.startsWith("traditional") && (blinkerLeft || blinkerRight));

  if (blinkerLeft || blinkerRight) {
    if (!signalTimer.isValid()) {
      signalTimer.start();
    }
  } else {
    signalTimer.invalidate();
  }

  if (cscTraining) {
    if (!glowTimer.isValid()) {
      glowTimer.start();
    }
  } else {
    glowTimer.invalidate();
  }

  if (frogpilot_scene.standstill && frogpilot_toggles.value(QLatin1String("stopped_timer")).toBool()) {
    if (!standstillTimer.isValid()) {
      standstillTimer.start();
    } else {
      standstillDuration = (sm.frame - scene.started_frame) / UI_FREQ < 60 ? 0 : standstillTimer.elapsed() / 1000;
    }
  } else {
    standstillDuration = 0;
    standstillTimer.invalidate();
    standstillTimerRect = QRect();
  }

  instantReplayButton->setVisible(frogpilot_toggles.value(QLatin1String("instant_replay")).toInt() != 0 && !standstillTimerRect.intersects(instantReplayButton->geometry()) && !(signalStyle == "static" && blinkerRight));

  personalityButton->setVisible(dmIconPosition != QPoint(0, 0) && (!hideBottomIcons || personalityButton->isDown()) && frogpilot_toggles.value(QLatin1String("onroad_distance_button")).toBool());
  personalityButton->updateState(s, fs);

  if (!isVisible()) {
    return;
  }

  updateCEMIcon();
}

void FrogPilotAnnotatedCameraWidget::mousePressEvent(QMouseEvent *mouseEvent) {
  mouseEvent->ignore();
}

bool FrogPilotAnnotatedCameraWidget::needsAdjacentPaths() const {
  return frogpilot_toggles.value(QLatin1String("adjacent_paths")).toBool() || frogpilot_toggles.value(QLatin1String("adjacent_path_metrics")).toBool() || (frogpilot_toggles.value(QLatin1String("blind_spot_path")).toBool() && (blindspotLeft || blindspotRight));
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
  QPoint longitudinalPausedPosition = lateralPausedPosition + QPoint(slotStep, 0);

  QPoint compassPosition(rightHandDM ? width() - experimentalButtonPosition.x() - widget_size : experimentalButtonPosition.x(), cemStatusPosition.y());

  instantReplayButton->move(experimentalButtonPosition.x() - UI_BORDER_SIZE - btn_size, experimentalButtonPosition.y() + screenRecorderButton->height());
  if (personalityButton->isVisible()) {
    personalityButton->move(rightHandDM ? width() - personalityButton->width() - 2 * UI_BORDER_SIZE : UI_BORDER_SIZE, dmIconPosition.y() - personalityButton->height() / 2);
  }

  if (!hideBottomIcons && frogpilot_toggles.value(QLatin1String("cem_status")).toBool()) {
    paintCEMStatus(p, cemStatusPosition);
  }

  if (!hideBottomIcons && frogpilot_toggles.value(QLatin1String("compass")).toBool()) {
    paintCompass(p, compassPosition);
  }

  if (!(signalStyle == "static" && blinkerLeft) && frogpilot_toggles.value(QLatin1String("csc_status")).toBool()) {
    if (cscTraining || (isCruiseSet && cscActive)) {
      paintCurveSpeedControl(p);
    }
  }

  if (!hideBottomIcons && (forceCoast)) {
    paintPausedIcon(p, longitudinalPausedPosition, speedIcon);
  }

  if (frogpilot_toggles.value(QLatin1String("pedals_on_ui")).toBool()) {
    paintPedalIcons(p);
  }

  if (frogpilot_toggles.value(QLatin1String("radar_tracks")).toBool()) {
    paintRadarTracks(p);
  }

  roadNameRect = QRect();
  if (alertHeight == 0 && frogpilot_toggles.value(QLatin1String("road_name_ui")).toBool()) {
    paintRoadName(p);
  }

  if (standstillDuration != 0) {
    paintStandstillTimer(p);
  }

  if (track_vertices.length() >= 1 && redLight && frogpilot_toggles.value(QLatin1String("show_stopping_point")).toBool()) {
    paintStoppingPoint(p);
  }

  if ((blinkerLeft || blinkerRight) && signalStyle != "None" && (standstillDuration == 0 || signalStyle != "static")) {
    paintTurnSignals(p);
  }
}

void FrogPilotAnnotatedCameraWidget::paintAdjacentPaths(QPainter &p) {
  std::function<void(const QPolygonF&, bool, bool, float)> paintPath = [&](const QPolygonF &path, bool isLeft, bool isBlindSpot, float laneWidth) {
    if (path.isEmpty() || laneWidth == 0.0f) {
      return;
    }

    p.save();

    float hue = 0.0f;
    if (!isBlindSpot || !frogpilot_toggles.value(QLatin1String("blind_spot_path")).toBool()) {
      const double requirement = frogpilot_toggles.value(QLatin1String("lane_detection_width")).toDouble();
      float ratio = requirement > 0 ? std::clamp(laneWidth / requirement, 0.0, 1.0) : 1.0;
      hue = (ratio * ratio) * (120.0f / 360.0f);
    }

    QLinearGradient gradient(0, height(), 0, 0);
    gradient.setColorAt(0.0f, QColor::fromHslF(hue, 0.75f, 0.5f, 0.4f));
    gradient.setColorAt(0.5f, QColor::fromHslF(hue, 0.75f, 0.5f, 0.35f));
    gradient.setColorAt(1.0f, QColor::fromHslF(hue, 0.75f, 0.5f, 0.0f));

    p.setBrush(gradient);
    p.drawPolygon(path);

    if (frogpilot_toggles.value(QLatin1String("adjacent_path_metrics")).toBool()) {
      QString text;
      if (isBlindSpot && frogpilot_toggles.value(QLatin1String("blind_spot_path")).toBool()) {
        text = tr("Vehicle in blind spot");
      } else {
        text = QString::number(laneWidth * distanceConversion, 'f', 2) + leadDistanceUnit;
      }

      int midIndex = path.size() / 2;
      QPointF anchorPoint = isLeft ? path[midIndex / 2] : path[midIndex + (path.size() - midIndex) / 2];

      p.setFont(InterFont(45, QFont::DemiBold));
      QFontMetrics metrics(p.font());

      int textWidth = metrics.horizontalAdvance(text);
      int textXPosition = isLeft ? anchorPoint.x() - textWidth : anchorPoint.x();
      int textYPosition = anchorPoint.y() - metrics.height() / 2 + metrics.ascent();

      if (QRect(textXPosition, textYPosition - metrics.ascent(), textWidth, metrics.height()).intersects(sourcesRect)) {
        textXPosition = sourcesRect.right() + UI_BORDER_SIZE;
      }
      textXPosition = std::clamp(textXPosition, 0, width() - textWidth);

      p.setPen(whiteColor());
      drawOutlinedText(p, QPointF(textXPosition, textYPosition), text);
    }

    p.restore();
  };

  paintPath(track_adjacent_vertices[0], true, blindspotLeft, laneWidthLeft);
  paintPath(track_adjacent_vertices[1], false, blindspotRight, laneWidthRight);
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

void FrogPilotAnnotatedCameraWidget::paintCurveSpeedControl(QPainter &p) {
  p.save();

  QRect curveSpeedRect(QPoint(setSpeedRect.right() + UI_BORDER_SIZE, setSpeedRect.top()), QSize(defaultSize.width() * 1.25, defaultSize.width() * 1.25));

  QPixmap &curveSpeedImage = roadCurvature < 0 ? curveSpeedIcon : curveSpeedIconFlipped;
  QSize curveSpeedSize = curveSpeedImage.size();
  QPoint curveSpeedPoint = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, curveSpeedSize, curveSpeedRect).topLeft();

  if (cscTraining) {
    qreal phase = (glowTimer.elapsed() % 2000) / 2000.0 * 2 * M_PI;
    qreal alphaFactor = 0.5 + 0.5 * sin(phase);

    QColor glowColor = blueColor();
    glowColor.setAlphaF(0.3 + 0.7 * alphaFactor);

    int glowWidth = 8 + static_cast<int>(2 * alphaFactor);

    p.setBrush(blackColor(166));
    p.setPen(QPen(glowColor, glowWidth));
    p.drawRoundedRect(curveSpeedRect, 24, 24);
    p.drawPixmap(curveSpeedPoint, curveSpeedImage);
    p.setPen(QPen(blackColor(), 10));

    QRect textRect(curveSpeedRect.topLeft() + QPoint(0, curveSpeedRect.height() + 10), QSize(curveSpeedRect.width(), 50));
    p.drawRoundedRect(textRect, 24, 24);
    QString trainingText = tr("Training...");
    p.setFont(fitInterFont(35, QFont::Bold, textRect.width() - 40, {trainingText}));
    p.setPen(QPen(whiteColor(), 6));
    p.drawText(textRect.adjusted(20, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, trainingText);
  } else {
    QRect cscRect(curveSpeedRect.topLeft() + QPoint(0, curveSpeedRect.height() + 10), QSize(curveSpeedRect.width(), 100));
    p.setBrush(blueColor(166));
    p.setFont(InterFont(45, QFont::Bold));
    p.setPen(QPen(blueColor(), 10));
    p.drawRoundedRect(cscRect, 24, 24);
    p.setPen(QPen(whiteColor(), 6));

    QString cscSpeedText = QString::number(std::nearbyint(std::min((cscSpeed - vEgo) * speedConversion + speed, speed))) + speedUnit;
    int textWidth = p.fontMetrics().horizontalAdvance(cscSpeedText);
    if (textWidth > cscRect.width() - 20) {
      p.setFont(InterFont(45 * (cscRect.width() - 20) / textWidth, QFont::Bold));
    }
    p.drawText(cscRect, Qt::AlignCenter, cscSpeedText);
    p.drawPixmap(curveSpeedPoint, curveSpeedImage);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintLeadMetrics(QPainter &p, bool adjacent, QPointF *chevron, const cereal::RadarState::LeadData::Reader &lead_data) {
  float leadDistance = lead_data.getDRel() + (adjacent ? std::abs(lead_data.getYRel()) : 0.0f);
  float leadSpeed = std::max(lead_data.getVLead(), 0.0f);

  QString distanceString = QString::number(qRound(leadDistance * distanceConversion));
  QString speedString = QString::number(qRound(leadSpeed * speedConversionMetrics));

  QVector<QString> textLines;
  textLines.reserve(3);
  if (adjacent) {
    textLines.append(distanceString + leadDistanceUnit);
    textLines.append(speedString + leadSpeedUnit);
  } else {
    if (frogpilot_toggles.value(QLatin1String("openpilot_longitudinal")).toBool()) {
      int desiredDistance = std::max(0, qRound(desiredFollowDistance * distanceConversion));
      textLines.append(QString("%1%2 (%3)").arg(distanceString, leadDistanceUnit, tr("Desired: %1").arg(desiredDistance)));
    } else {
      textLines.append(distanceString + leadDistanceUnit);
    }
    textLines.append(speedString + leadSpeedUnit);

    float timeGap = leadDistance / std::max(vEgo, 1.0f);
    textLines.append(tr("%1 seconds").arg(QString::number(timeGap, 'f', 2)));
  }

  p.save();

  p.setFont(InterFont(45, QFont::DemiBold));
  p.setPen(whiteColor());

  QFontMetrics metrics(p.font());
  int lineHeight = metrics.lineSpacing();

  int maxTextWidth = 0;
  for (QString &line : textLines) {
    maxTextWidth = std::max(maxTextWidth, metrics.horizontalAdvance(line));
  }

  int centerX = std::clamp<int>((chevron[2].x() + chevron[0].x()) / 2, maxTextWidth / 2, width() - maxTextWidth / 2);
  int startY = std::min<int>(chevron[0].y() + lineHeight + 5, height() - (textLines.size() - 1) * lineHeight - metrics.descent());

  int xMargin = maxTextWidth * 0.1;
  int yMargin = lineHeight * 0.1;

  QRect textRect(centerX - maxTextWidth / 2, startY - lineHeight, maxTextWidth, textLines.size() * lineHeight);
  textRect.adjust(-xMargin, -yMargin, xMargin, yMargin);

  if ((adjacent && textRect.intersects(adjacentLeadTextRect)) || std::any_of(leadTextRects.cbegin(), leadTextRects.cend(), [&textRect](const QRect &rect) {
    return textRect.intersects(rect);
  })) {
    p.restore();
    return;
  }

  if (adjacent) {
    adjacentLeadTextRect = textRect;
  } else {
    leadTextRects.append(textRect);

    if (vEgo < 1.0f) {
      textLines[2].clear();
    }
  }

  for (int i = 0; i < textLines.size(); ++i) {
    int lineX = centerX - metrics.horizontalAdvance(textLines[i]) / 2;
    int lineY = startY + (i * lineHeight);

    drawOutlinedText(p, QPointF(lineX, lineY), textLines[i]);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintPathEdges(QPainter &p) {
  if (!frogpilot_toggles.value(QLatin1String("model_ui")).toBool()) {
    return;
  }

  p.save();

  QLinearGradient gradient(0, height(), 0, 0);

  std::function<void(const QColor &)> setPathEdgeColors = [&gradient](const QColor &baseColor) {
    gradient.setColorAt(0.0f, QColor(baseColor.red(), baseColor.green(), baseColor.blue(), 255.0f * 0.4f));
    gradient.setColorAt(0.5f, QColor(baseColor.red(), baseColor.green(), baseColor.blue(), 255.0f * 0.35f));
    gradient.setColorAt(1.0f, QColor(baseColor.red(), baseColor.green(), baseColor.blue(), 255.0f * 0.0f));
  };

  if (frogpilot_scene.always_on_lateral_active) {
    setPathEdgeColors(bg_colors[STATUS_ALWAYS_ON_LATERAL_ACTIVE]);
  } else if (frogpilot_scene.conditional_status == 1) {
    setPathEdgeColors(bg_colors[STATUS_CEM_DISABLED]);
  } else if (experimentalMode) {
    setPathEdgeColors(bg_colors[STATUS_EXPERIMENTAL_MODE_ENABLED]);
  } else if (frogpilot_toggles.value(QLatin1String("color_scheme")).toString() != "stock") {
    setPathEdgeColors(QColor(frogpilot_toggles.value(QLatin1String("path_edges_color")).toString()));
  } else {
    gradient.setColorAt(0.0f, QColor::fromHslF(148.0f / 360.0f, 0.94f, 0.41f, 0.4f));
    gradient.setColorAt(0.5f, QColor::fromHslF(112.0f / 360.0f, 1.0f, 0.54f, 0.35f));
    gradient.setColorAt(1.0f, QColor::fromHslF(112.0f / 360.0f, 1.0f, 0.54f, 0.0f));
  }

  p.setBrush(gradient);

  QPainterPath path;
  path.addPolygon(track_vertices);
  path.addPolygon(track_edge_vertices);
  p.drawPath(path);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintPausedIcon(QPainter &p, const QPoint &position, const QPixmap &icon) {
  p.save();

  QRect pausedWidget(position, QSize(widget_size, widget_size));

  p.setBrush(blackColor(166));
  p.setPen(QPen(QColor(bg_colors[STATUS_TRAFFIC_MODE_ENABLED]), 10));
  p.drawRoundedRect(pausedWidget, 24, 24);

  p.setOpacity(0.5);
  p.drawPixmap(pausedWidget, icon);
  p.setOpacity(0.75);
  p.drawPixmap(pausedWidget, pausedIcon);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintPedalIcons(QPainter &p) {
  p.save();

  float brakeOpacity = 1.0f;
  float gasOpacity = 1.0f;

  if (frogpilot_toggles.value(QLatin1String("dynamic_pedals_on_ui")).toBool()) {
    brakeOpacity = frogpilot_scene.standstill ? 1.0f : accelerationEgo < -0.25f ? std::max(0.25f, std::abs(accelerationEgo)) : 0.25f;
    gasOpacity = std::max(0.25f, accelerationEgo);
  } else if (frogpilot_toggles.value(QLatin1String("static_pedals_on_ui")).toBool()) {
    brakeOpacity = frogpilot_scene.standstill || brakeLights || accelerationEgo < -0.25f ? 1.0f : 0.25f;
    gasOpacity = accelerationEgo > 0.25 ? 1.0f : 0.25f;
  }

  int startX = experimentalButtonPosition.x();
  int startY = experimentalButtonPosition.y() + btn_size + UI_BORDER_SIZE;

  p.setOpacity(brakeOpacity);
  p.drawPixmap(startX, startY, brakePedalImg);

  p.setOpacity(gasOpacity);
  p.drawPixmap(startX + btn_size / 2, startY, gasPedalImg);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintRadarTracks(QPainter &p) {
  if (radar_tracks.empty()) {
    return;
  }

  p.save();

  int diameter = 25;

  float radius = diameter / 2.0f;
  float track_x = p.viewport().width() - diameter;
  float track_y = p.viewport().height() - diameter;

  p.setBrush(redColor());

  for (const QPointF &track : radar_tracks) {
    float x = std::clamp(static_cast<float>(track.x()), 0.0f, track_x);
    float y = std::clamp(static_cast<float>(track.y()), 0.0f, track_y);

    p.drawEllipse(QPointF(x + radius, y + radius), radius, radius);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintRoadName(QPainter &p) {
  static const QFont font = InterFont(40, QFont::DemiBold);

  static QString cachedRoadName;
  static int cachedRoadNameWidth = 0;

  if (roadName != cachedRoadName) {
    cachedRoadName = roadName;
    cachedRoadNameWidth = QFontMetrics(font).horizontalAdvance(roadName);
  }

  int textWidth = cachedRoadNameWidth;

  QSize size(textWidth + 100, 50);
  roadNameRect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignHCenter | Qt::AlignBottom, size, rect().adjusted(0, 0, 0, -5));

  if (roadName.isEmpty()) {
    return;
  }

  p.save();

  p.setBrush(blackColor(166));
  p.setPen(QPen(blackColor(), 10));
  p.drawRoundedRect(roadNameRect, 24, 24);

  p.setFont(font);
  p.setPen(QPen(whiteColor(), 6));
  p.drawText(roadNameRect, Qt::AlignCenter, roadName);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintStandstillTimer(QPainter &p) {
  p.save();

  float transition;

  QColor startColor, endColor;
  if (standstillDuration < 150) {
    startColor = bg_colors[STATUS_ENGAGED];
    endColor = bg_colors[STATUS_CEM_DISABLED];
    transition = util::map_val<float>(standstillDuration, 60, 150, 0, 1);
  } else {
    startColor = bg_colors[STATUS_CEM_DISABLED];
    endColor = bg_colors[STATUS_TRAFFIC_MODE_ENABLED];
    transition = util::map_val<float>(standstillDuration, 150, 300, 0, 1);
  }

  QColor blendedColor(
    startColor.red() + transition * (endColor.red() - startColor.red()),
    startColor.green() + transition * (endColor.green() - startColor.green()),
    startColor.blue() + transition * (endColor.blue() - startColor.blue())
  );

  std::function<QRect(const QString &, int, const QFont &, const QColor &)> drawText = [&](const QString &text, int y, const QFont &font, const QColor &color) {
    p.setFont(font);
    p.setPen(color);

    QRect standstillRect = p.fontMetrics().boundingRect(text);
    standstillRect.moveCenter({rect().center().x(), y - standstillRect.height() / 2});
    p.drawText(standstillRect.x(), standstillRect.bottom(), text);

    return standstillRect;
  };

  int minutes = standstillDuration / 60;
  QString minuteStr = minutes == 1 ? tr("1 minute") : tr("%1 minutes").arg(minutes);
  standstillTimerRect = drawText(minuteStr, 210, InterFont(176, QFont::Bold), blendedColor);

  int seconds = standstillDuration % 60;
  QString secondStr = seconds == 1 ? tr("1 second") : tr("%1 seconds").arg(seconds);
  drawText(secondStr, 290, InterFont(66), whiteColor());

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintStoppingPoint(QPainter &p) {
  p.save();

  QPointF centerPoint = (track_vertices.first() + track_vertices.last()) / 2.0f;
  QPointF stopSignPosition = centerPoint - QPointF(stopSignImg.width() / 2.0f, stopSignImg.height());
  p.drawPixmap(stopSignPosition, stopSignImg);

  if (frogpilot_toggles.value(QLatin1String("show_stopping_point_metrics")).toBool()) {
    float distance = stoppingDistance * distanceConversion;
    QString distanceText = QString::number(std::nearbyint(distance)) + leadDistanceUnit;

    QFont font = InterFont(45, QFont::DemiBold);
    QFontMetrics fm(font);

    QPointF textPosition(centerPoint.x() - fm.horizontalAdvance(distanceText) / 2.0f, centerPoint.y() - stopSignImg.height() - fm.ascent());

    p.setFont(font);
    p.setPen(whiteColor());
    drawOutlinedText(p, textPosition, distanceText);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintTurnSignals(QPainter &p) {
  int frameIndex = (signalTimer.elapsed() / signalAnimationLength) % totalFrames;

  bool blindspotActive = blinkerLeft ? blindspotLeft : blindspotRight;
  bool showBlindspot = blindspotActive && !blindspotImages.isEmpty();

  int signalXPosition = 0;
  int signalYPosition = 0;

  if (signalStyle == "static") {
    signalXPosition = blinkerLeft ? (rect().center().x() * 0.75) - signalWidth : rect().center().x() * 1.25;
    signalYPosition = signalHeight / 2;
  } else {
    if (signalStyle == "traditional_gif") {
      int signalMovement = (width() + signalWidth * 2) / totalFrames;
      signalXPosition = blinkerLeft ? width() - (frameIndex * signalMovement) + signalWidth : (frameIndex * signalMovement) - signalWidth;
    } else {
      signalXPosition = std::clamp(blinkerLeft ? width() - ((frameIndex + 1) * signalWidth) : frameIndex * signalWidth, 0, width() - signalWidth);
    }
    if (showBlindspot) {
      signalXPosition = blinkerLeft ? width() - signalWidth : 0;
    }
    signalYPosition = (roadNameRect.isNull() ? height() - alertHeight : roadNameRect.top() - 5) - signalHeight;
  }

  const QPixmap &signalImage = showBlindspot ? blindspotImages.at(0) : signalImages.at(frameIndex);
  if (blinkerLeft) {
    p.drawPixmap(signalXPosition, signalYPosition, signalWidth, signalHeight, signalImage);
  } else {
    p.save();
    p.translate(signalXPosition + signalWidth, signalYPosition);
    p.scale(-1, 1);
    p.drawPixmap(0, 0, signalWidth, signalHeight, signalImage);
    p.restore();
  }
}
