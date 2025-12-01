#include <QPainterPath>

#include "frogpilot/ui/qt/onroad/frogpilot_annotated_camera.h"

FrogPilotAnnotatedCameraWidget::FrogPilotAnnotatedCameraWidget(CameraWidget *nvg, QWidget *parent) : QWidget(parent) {
  instantReplayButton = new InstantReplayButton(nvg);
  instantReplayButton->setVisible(false);

  curveSpeedIcon = loadPixmap("../../frogpilot/assets/other_images/curve_speed.svg", {btn_size, btn_size});
  curveSpeedIconFlipped = curveSpeedIcon.transformed(QTransform().scale(-1, 1));

  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &FrogPilotAnnotatedCameraWidget::updateSignals);
  QObject::connect(nvg, &CameraWidget::vipcThreadFrameReceived, frogpilotUIState(), &FrogPilotUIState::cameraFrameReceived);
  QObject::connect(uiState(), &UIState::offroadTransition, this, [this] {
    standstillTimer.invalidate();
  });

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

  signalIsGif = false;

  signalAnimationLength = 0;
  signalHeight = 0;
  signalWidth = 0;

  signalStyle = "None";

  QVector<QPixmap>().swap(blindspotImages);
  QVector<QPixmap>().swap(signalImages);

  QFileInfoList files = QDir("../../frogpilot/assets/active_theme/signals/").entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
  for (const QFileInfo &fileInfo : files) {
    QString fileName = fileInfo.fileName();
    QString filePath = fileInfo.absoluteFilePath();
    bool isBlindspot = fileName.contains("blindspot", Qt::CaseInsensitive);

    QVector<QPixmap> &targetImages = isBlindspot ? blindspotImages : signalImages;

    if (fileName.endsWith(".gif", Qt::CaseInsensitive)) {
      signalIsGif = signalIsGif || !isBlindspot;

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
    } else if (fileName.endsWith(".png", Qt::CaseInsensitive) || fileName.endsWith(".svg", Qt::CaseInsensitive)) {
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

  if (signalImages.isEmpty()) {
    signalAnimationLength = 0;

    signalStyle = "None";
  } else {
    QPixmap &firstImage = signalImages.front();
    signalHeight = firstImage.height();
    signalWidth = firstImage.width();
  }
}

void FrogPilotAnnotatedCameraWidget::updateCEMIcon() {
  static const QMap<int, QString> cemIcons = {{1, "chill_mode_icon"}, {3, "curve_icon"}, {4, "lead_icon"}, {6, "conditional_speed_icon"}, {7, "conditional_speed_icon"}, {8, "light_icon"}};

  QString cemPath;
  if (frogpilot_toggles.value(QLatin1String("cem_status")).toBool()) {
    QString icon = experimentalMode ? cemIcons.value(frogpilot_scene.conditional_status, "experimental_mode_icon") : "chill_mode_icon";
    if (experimentalMode && frogpilot_scene.conditional_status == 5) {
      icon = blinkerLeft ? "conditional_turn_left_icon" : "conditional_turn_right_icon";
    }
    cemPath = QString("../../frogpilot/assets/other_images/%1.svg").arg(icon);
  }
  updateIcon(cemPath, cemIcon, cemIconPath);
}

void FrogPilotAnnotatedCameraWidget::updateIcon(const QString &path, QSharedPointer<FrogPilotMovie> &icon, QString &iconPath) {
  if (hideBottomIcons && !path.isEmpty()) {
    if (icon) {
      icon->stop();
    }
  } else if (path != iconPath || (!path.isEmpty() && (!icon || !icon->isRunning()))) {
    loadMovie(path, icon, QSize(widget_size, widget_size), this, false);
    iconPath = icon && icon->isRunning() ? path : QString();
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
  const capnp::List<float>::Reader &velocityX = modelV2.getVelocity().getX();

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
  cscActive = frogpilotPlan.getCscActive() && !carState.getStandstill();
  cscSpeed = frogpilotPlan.getCscSpeed();
  cscTraining = frogpilotPlan.getCscTraining();
  experimentalMode = selfdriveState.getExperimentalMode();
  gpsBearing = frogpilotPlan.getGpsBearing();
  redLight = frogpilotPlan.getRedLight();
  roadCurvature = frogpilotPlan.getRoadCurvature();
  roadName = QString::fromStdString(mapdOut.getRoadName());
  stoppingDistance = positionX.size() != 0 ? positionX[positionX.size() - 1] : 0.0f;
  vEgo = carState.getVEgo();

  hideBottomIcons = alertHeight != 0;

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
    }

    standstillDuration = (sm.frame - scene.started_frame) / UI_FREQ < 60 ? 0 : standstillTimer.elapsed() / 1000;
  } else {
    standstillDuration = 0;
    standstillTimer.invalidate();
    standstillTimerRect = QRect();
  }

  staticSignalRect = QRect();
  if ((blinkerLeft || blinkerRight) && signalStyle == "static" && (standstillDuration == 0 || !standstillTimerRect.isNull())) {
    int signalXPosition = blinkerLeft ? (rect().center().x() * 0.75) - signalWidth : rect().center().x() * 1.25;
    if (standstillDuration != 0) {
      signalXPosition = blinkerLeft ? standstillTimerRect.left() - UI_BORDER_SIZE - signalWidth : standstillTimerRect.right() + UI_BORDER_SIZE;
    }
    staticSignalRect = QRect(signalXPosition, signalHeight / 2, signalWidth, signalHeight);
  }

  instantReplayButton->setVisible(!standstillTimerRect.intersects(instantReplayButton->geometry()) && !staticSignalRect.intersects(instantReplayButton->geometry()) && frogpilot_toggles.value(QLatin1String("instant_replay")).toInt() != 0);

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

QLinearGradient FrogPilotAnnotatedCameraWidget::adjacentPathGradient(float hue) {
  QLinearGradient gradient(0, height(), 0, 0);
  gradient.setColorAt(0.0f, QColor::fromHslF(hue, 0.75f, 0.5f, 0.4f));
  gradient.setColorAt(0.5f, QColor::fromHslF(hue, 0.75f, 0.5f, 0.35f));
  gradient.setColorAt(1.0f, QColor::fromHslF(hue, 0.75f, 0.5f, 0.0f));
  return gradient;
}

void FrogPilotAnnotatedCameraWidget::drawOutlinedText(QPainter &p, const QPointF &position, const QString &text) {
  QPainterPath path;
  path.addText(position, p.font(), text);
  p.strokePath(path, QPen(Qt::black, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

  p.drawText(position, text);
}

void FrogPilotAnnotatedCameraWidget::paintFrogPilotWidgets(QPainter &p) {
  if (driverView) {
    paintWorldWidgets(p);
  }

  int slotStep = rightHandDM ? -widget_size - 2 * UI_BORDER_SIZE : widget_size + 2 * UI_BORDER_SIZE;

  QPoint cemStatusPosition(dmIconPosition.x() + (rightHandDM ? -btn_size / 2 - 2 * UI_BORDER_SIZE - widget_size : btn_size / 2 + 2 * UI_BORDER_SIZE), dmIconPosition.y() - widget_size / 2);

  QPoint compassPosition(rightHandDM ? width() - experimentalButtonPosition.x() - btn_size : experimentalButtonPosition.x() + btn_size - widget_size, cemStatusPosition.y());

  instantReplayButton->move(experimentalButtonPosition.x() - UI_BORDER_SIZE - btn_size, experimentalButtonPosition.y());

  if (!hideBottomIcons && frogpilot_toggles.value(QLatin1String("cem_status")).toBool()) {
    paintCEMStatus(p, cemStatusPosition);
  }

  if (!hideBottomIcons && frogpilot_toggles.value(QLatin1String("compass")).toBool()) {
    paintCompass(p, compassPosition);
  }

  if (frogpilot_toggles.value(QLatin1String("csc_status")).toBool()) {
    if (cscTraining || (isCruiseSet && cscActive)) {
      paintCurveSpeedControl(p);
    }
  }

  if (standstillDuration != 0) {
    paintStandstillTimer(p);
  }

  if (!staticSignalRect.isNull()) {
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

    p.setBrush(adjacentPathGradient(hue));
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
  if (!blindspotLeft && !blindspotRight) {
    return;
  }

  p.save();

  p.setBrush(adjacentPathGradient(0.0f));

  if (blindspotLeft && !track_adjacent_vertices[0].isEmpty()) {
    p.drawPolygon(track_adjacent_vertices[0]);
  }
  if (blindspotRight && !track_adjacent_vertices[1].isEmpty()) {
    p.drawPolygon(track_adjacent_vertices[1]);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintCEMStatus(QPainter &p, const QPoint &position) {
  p.save();

  QRect cemWidget(position, QSize(widget_size, widget_size));

  if (frogpilot_scene.conditional_status == 1) {
    paintWidgetFrame(p, cemWidget, bg_colors[STATUS_CEM_DISABLED]);
  } else if (experimentalMode) {
    paintWidgetFrame(p, cemWidget, bg_colors[STATUS_EXPERIMENTAL_MODE_ENABLED]);
  } else {
    paintWidgetFrame(p, cemWidget, blackColor());
  }

  if (cemIcon) {
    p.drawPixmap(cemWidget, cemIcon->currentPixmap());
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintCompass(QPainter &p, const QPoint &position) {
  p.save();

  constexpr double PIXELS_PER_DEGREE = 2.5;

  constexpr int BASE_RIBBON_WIDTH = static_cast<int>(360 * PIXELS_PER_DEGREE);
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

  paintWidgetFrame(p, compassWidget, blackColor());

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
  QRect curveSpeedRect(QPoint(setSpeedRect.x() + setSpeedRect.width() + UI_BORDER_SIZE, setSpeedRect.top()), QSize(defaultSize.width() * 1.25, defaultSize.width() * 1.25));
  QRect cscRect(curveSpeedRect.topLeft() + QPoint(0, curveSpeedRect.height() + UI_BORDER_SIZE / 2), QSize(curveSpeedRect.width(), cscTraining ? 50 : 100));

  p.save();

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

    p.drawRoundedRect(cscRect, 24, 24);
    QString trainingText = tr("Training...");
    p.setFont(fitInterFont(35, QFont::Bold, cscRect.width() - 40, {trainingText}));
    p.setPen(QPen(whiteColor(), 6));
    p.drawText(cscRect.adjusted(20, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, trainingText);
  } else {
    p.setBrush(blueColor(166));
    p.setPen(QPen(blueColor(), 10));
    p.drawRoundedRect(cscRect, 24, 24);
    p.setPen(QPen(whiteColor(), 6));

    QString cscSpeedText = QString::number(std::nearbyint(std::min((cscSpeed - vEgo) * speedConversion + speed, speed))) + speedUnit;
    p.setFont(fitInterFont(45, QFont::Bold, cscRect.width() - 20, {cscSpeedText}));
    p.drawText(cscRect, Qt::AlignCenter, cscSpeedText);
    p.drawPixmap(curveSpeedPoint, curveSpeedImage);
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

  QSize size(cachedRoadNameWidth + 100, 50);
  roadNameRect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignHCenter | Qt::AlignBottom, size, rect().adjusted(0, 0, 0, -10));

  if (roadName.isEmpty()) {
    return;
  }

  p.save();

  paintWidgetFrame(p, roadNameRect, blackColor());

  p.setFont(font);
  p.setPen(QPen(whiteColor(), 6));
  p.drawText(roadNameRect, Qt::AlignCenter, roadName);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintStandstillTimer(QPainter &p) {
  p.save();

  float transition;

  QColor endColor;
  QColor startColor;
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
  QString minuteStr = tr("%n minute(s)", "", minutes);
  standstillTimerRect = drawText(minuteStr, 210, InterFont(176, QFont::Bold), blendedColor);

  int seconds = standstillDuration % 60;
  QString secondStr = tr("%n second(s)", "", seconds);
  drawText(secondStr, 290, InterFont(66), whiteColor());

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintTurnSignals(QPainter &p) {
  int frameIndex = (signalTimer.elapsed() / signalAnimationLength) % signalImages.size();

  bool blindspotActive = blinkerLeft ? blindspotLeft : blindspotRight;
  bool showBlindspot = blindspotActive && !blindspotImages.isEmpty();

  int signalXPosition = 0;
  int signalYPosition = 0;

  if (signalStyle == "static") {
    signalXPosition = staticSignalRect.x();
    signalYPosition = staticSignalRect.y();
  } else {
    if (signalIsGif && signalStyle == "traditional") {
      int signalMovement = (width() + signalWidth * 2) / signalImages.size();
      signalXPosition = blinkerLeft ? width() - (frameIndex * signalMovement) + signalWidth : (frameIndex * signalMovement) - signalWidth;
    } else {
      signalXPosition = blinkerLeft ? width() - ((frameIndex + 1) * signalWidth) : frameIndex * signalWidth;
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

void FrogPilotAnnotatedCameraWidget::paintWidgetFrame(QPainter &p, const QRect &rect, const QColor &borderColor) {
  p.setBrush(blackColor(166));
  p.setPen(QPen(borderColor, 10));
  p.drawRoundedRect(rect, 24, 24);
}

void FrogPilotAnnotatedCameraWidget::paintWorldWidgets(QPainter &p) {
  roadNameRect = QRect();
  if (alertHeight == 0 && frogpilot_toggles.value(QLatin1String("road_name_ui")).toBool()) {
    paintRoadName(p);
  }

  if ((blinkerLeft || blinkerRight) && signalStyle != "None" && signalStyle != "static") {
    paintTurnSignals(p);
  }
}

