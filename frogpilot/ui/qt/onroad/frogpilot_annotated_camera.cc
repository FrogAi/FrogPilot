#include <QPainterPath>
#include <QStyle>

#include "frogpilot/ui/qt/onroad/frogpilot_annotated_camera.h"

FrogPilotAnnotatedCameraWidget::FrogPilotAnnotatedCameraWidget(QWidget *parent) : QWidget(parent) {
  instantReplayButton = new InstantReplayButton(parent);
  instantReplayButton->setVisible(false);

  personalityButton = new DrivingPersonalityButton(parent);
  personalityButton->setVisible(false);

  screenRecorderButton = new ScreenRecorderButton(parent);
  screenRecorderButton->setVisible(false);

  QSize iconSize(img_size / 4, img_size / 4);

  brakePedalImg = loadPixmap("../../frogpilot/assets/other_images/brake_pedal.svg", {btn_size, btn_size});
  cameraIcon = loadPixmap("../../frogpilot/assets/other_images/camera_icon.svg", iconSize);
  curveSpeedIcon = loadPixmap("../../frogpilot/assets/other_images/curve_speed.svg", {btn_size, btn_size});
  curveSpeedIconFlipped = curveSpeedIcon.transformed(QTransform().scale(-1, 1));
  dashboardIcon = loadPixmap("../../frogpilot/assets/other_images/dashboard_icon.png", iconSize);
  gasPedalImg = loadPixmap("../../frogpilot/assets/other_images/gas_pedal.svg", {btn_size, btn_size});
  mapDataIcon = loadPixmap("../../frogpilot/assets/other_images/offline_maps_icon.png", iconSize);
  navigationIcon = loadPixmap("../../frogpilot/assets/other_images/navigation_icon.png", iconSize);
  nextMapsIcon = loadPixmap("../../frogpilot/assets/other_images/next_maps_icon.png", iconSize);
  pausedIcon = loadPixmap("../../frogpilot/assets/other_images/paused_icon.svg", {widget_size, widget_size});
  speedIcon = loadPixmap("../../frogpilot/assets/other_images/speed_icon.svg", {widget_size, widget_size});
  stopSignImg = loadPixmap("../../frogpilot/assets/other_images/stop_sign.svg", {btn_size, btn_size});
  turnIcon = loadPixmap("../../frogpilot/assets/other_images/turn_icon.svg", {widget_size, widget_size});

  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &FrogPilotAnnotatedCameraWidget::updateSignals);
  QObject::connect(uiState(), &UIState::offroadTransition, this, [this] {
    standstillTimer.invalidate();
  });

  updateSignals();
}

void FrogPilotAnnotatedCameraWidget::hideEvent(QHideEvent *event) {
  if (cemIcon) {
    cemIcon->stop();
  }
  if (weatherIcon) {
    weatherIcon->stop();
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
  static const QMap<int, QString> cemIcons = {{1, "chill_mode_icon.svg"}, {3, "curve_icon.svg"}, {4, "lead_icon.svg"}, {6, "conditional_speed_icon.svg"}, {7, "conditional_speed_icon.svg"}, {8, "light_icon.svg"}, {9, "turn_icon.gif"}, {10, "turn_icon.gif"}};

  QString cemPath;
  if (frogpilot_toggles.value(QLatin1String("cem_status")).toBool()) {
    QString icon = experimentalMode ? cemIcons.value(frogpilot_scene.conditional_status, "experimental_mode_icon.svg") : "chill_mode_icon.svg";
    if (experimentalMode && frogpilot_scene.conditional_status == 5) {
      icon = blinkerLeft ? "conditional_turn_left_icon.svg" : "conditional_turn_right_icon.svg";
    }
    cemPath = QString("../../frogpilot/assets/other_images/%1").arg(icon);
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

void FrogPilotAnnotatedCameraWidget::updateWeatherIcon() {
  QString weatherPath;
  if (weatherId != 0) {
    QString icon = weatherDaytime ? "weather_clear_day" : "weather_clear_night";
    if ((weatherId >= 200 && weatherId <= 232) || (weatherId >= 300 && weatherId <= 321) || (weatherId >= 500 && weatherId <= 531) || weatherId == 771 || weatherId == 781) {
      icon = "weather_rain";
    } else if (weatherId >= 600 && weatherId <= 622) {
      icon = "weather_snow";
    } else if (weatherId >= 701 && weatherId <= 762) {
      icon = "weather_low_visibility";
    } else if (weatherId >= 801 && weatherId <= 804) {
      icon = weatherDaytime ? "weather_cloudy_day" : "weather_cloudy_night";
    }
    weatherPath = QString("../../frogpilot/assets/other_images/%1.svg").arg(icon);
  }
  updateIcon(weatherPath, weatherIcon, weatherIconPath);
}

void FrogPilotAnnotatedCameraWidget::updateState(const UIState &s, const FrogPilotUIState &fs) {
  frogpilot_scene = fs.frogpilot_scene;
  frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  const UIScene &scene = s.scene;

  const SubMaster &sm = *(s.sm);
  const SubMaster &fpsm = *(fs.sm);

  const cereal::CarState::Reader &carState = sm["carState"].getCarState();
  const cereal::ControlsState::Reader &controlsState = sm["controlsState"].getControlsState();
  const cereal::FrogPilotCarState::Reader &frogpilotCarState = fpsm["frogpilotCarState"].getFrogpilotCarState();
  const cereal::FrogPilotNavigation::Reader &frogpilotNavigation = fpsm["frogpilotNavigation"].getFrogpilotNavigation();
  const cereal::FrogPilotPlan::Reader &frogpilotPlan = fpsm["frogpilotPlan"].getFrogpilotPlan();
  const cereal::FrogPilotSignReading::Reader &frogpilotSignReading = fpsm["frogpilotSignReading"].getFrogpilotSignReading();
  const cereal::ModelDataV2::Reader &modelV2 = sm["modelV2"].getModelV2();
  const cereal::NavInstruction::Reader &navInstruction = sm["navInstruction"].getNavInstruction();

  const capnp::List<float>::Reader &positionX = modelV2.getPosition().getX();
  const capnp::List<float>::Reader &velocityX = modelV2.getVelocity().getX();

  bool useSIMetrics = frogpilot_toggles.value(QLatin1String("use_si_metrics")).toBool();

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
  cameraSpeedLimit = fpsm.alive("frogpilotSignReading") ? frogpilotSignReading.getSpeedLimit() : 0;
  cscActive = frogpilotPlan.getCscControllingSpeed() && !carState.getStandstill();
  cscSpeed = frogpilotPlan.getCscSpeed();
  cscTraining = frogpilotPlan.getCscTraining();
  dashboardSpeedLimit = frogpilotCarState.getDashboardSpeedLimit();
  desiredFollowDistance = frogpilotPlan.getDesiredFollowDistance();
  experimentalMode = controlsState.getExperimentalMode();
  forceCoast = frogpilotCarState.getForceCoast();
  gpsBearing = frogpilotPlan.getGpsBearing();
  laneWidthLeft = frogpilotPlan.getLaneWidthLeft();
  laneWidthRight = frogpilotPlan.getLaneWidthRight();
  lateralPaused = frogpilotCarState.getPauseLateral();
  longitudinalPaused = frogpilotCarState.getPauseLongitudinal();
  maneuverDistance = navInstruction.getManeuverDistance();
  mapSpeedLimit = frogpilotPlan.getSlcMapSpeedLimit();
  mapboxSpeedLimit = frogpilotPlan.getSlcMapboxSpeedLimit();
  navigationSpeedLimit = frogpilotNavigation.getNavigationSpeedLimit();
  nextSpeedLimit = frogpilotPlan.getSlcNextSpeedLimit();
  redLight = frogpilotPlan.getRedLight();
  roadCurvature = frogpilotPlan.getRoadCurvature();
  speedLimitChanged = frogpilotPlan.getSpeedLimitChanged();
  speedLimitSource = frogpilotPlan.getSlcSpeedLimitSource();
  unconfirmedSpeedLimit = frogpilotPlan.getUnconfirmedSlcSpeedLimit();
  vEgo = carState.getVEgo();
  weatherDaytime = frogpilotPlan.getWeatherDaytime();
  weatherId = frogpilotPlan.getWeatherId();

  stoppingDistance = 0.0f;
  for (int i = 0; i < velocityX.size(); ++i) {
    if (velocityX[i] < 1.0f) {
      stoppingDistance = positionX[i];
      break;
    }
  }

  float speedLimitOffset = frogpilotPlan.getSlcSpeedLimitOffset() * speedConversion;
  if (speedLimitOffset == 0) {
    speedLimitOffsetStr = "–";
  } else {
    speedLimitOffsetStr = (speedLimitOffset > 0 ? "+" : "") + QString::number(speedLimitOffset, 'f', 0);
  }

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

  if (speedLimitChanged) {
    if (!pendingLimitTimer.isValid()) {
      pendingLimitTimer.start();
    }
  } else {
    pendingLimitTimer.invalidate();
  }

  if (frogpilot_scene.standstill && frogpilot_toggles.value(QLatin1String("stopped_timer")).toBool()) {
    if (!standstillTimer.isValid()) {
      standstillTimer.start();
    }

    standstillDuration = frogpilot_scene.map_open || (sm.frame - scene.started_frame) / UI_FREQ < 60 ? 0 : standstillTimer.elapsed() / 1000;
  } else {
    standstillDuration = 0;
    standstillTimer.invalidate();
    standstillTimerRect = QRect();
  }

  staticSignalRect = QRect();
  if (!bigMapOpen && (blinkerLeft || blinkerRight) && signalStyle == "static" && (standstillDuration == 0 || !standstillTimerRect.isNull())) {
    int signalXPosition = blinkerLeft ? (rect().center().x() * 0.75) - signalWidth : rect().center().x() * 1.25;
    if (standstillDuration != 0) {
      signalXPosition = blinkerLeft ? standstillTimerRect.left() - UI_BORDER_SIZE - signalWidth : standstillTimerRect.right() + UI_BORDER_SIZE;
    }
    staticSignalRect = QRect(signalXPosition, signalHeight / 2, signalWidth, signalHeight);
  }

  screenRecorderButton->move(experimentalButtonPosition.x() - UI_BORDER_SIZE - btn_size + 10, experimentalButtonPosition.y() + (btn_size - screenRecorderButton->height()) / 2);
  instantReplayButton->move(screenRecorderButton->x(), screenRecorderButton->y() + screenRecorderButton->height() + UI_BORDER_SIZE / 2 - 10);
  personalityButton->rightHandDM = rightHandDM;
  personalityButton->move(rightHandDM ? width() - personalityButton->width() - UI_BORDER_SIZE : UI_BORDER_SIZE, dmIconPosition.y() - personalityButton->height() / 2);

  instantReplayButton->setVisible(!frogpilot_scene.map_open && !standstillTimerRect.intersects(instantReplayButton->geometry()) && !staticSignalRect.intersects(instantReplayButton->geometry()) && frogpilot_toggles.value(QLatin1String("instant_replay")).toInt() != 0);

  personalityButton->setVisible(!bigMapOpen && dmIconPosition != QPoint(0, 0) && (!hideBottomIcons || personalityButton->isDown()) && frogpilot_toggles.value(QLatin1String("onroad_distance_button")).toBool());
  personalityButton->updateState(s, fs);

  screenRecorderButton->setVisible(!frogpilot_scene.map_open && !standstillTimerRect.intersects(screenRecorderButton->geometry()) && !staticSignalRect.intersects(screenRecorderButton->geometry()) && frogpilot_toggles.value(QLatin1String("screen_recorder")).toBool());

  if (!isVisible()) {
    return;
  }

  if (frogpilot_toggles.value(QLatin1String("road_name_ui")).toBool()) {
    roadName = QString::fromStdString(params_memory.get("RoadName"));
  }

  if (vEgo > 0 && frogpilot_toggles.value(QLatin1String("rainbow_path")).toBool()) {
    hueOffset = fmodf(hueOffset + sqrtf(vEgo) / sqrtf(145.0f / MS_TO_KPH), 360.0f);
  }

  updateCEMIcon();
  updateWeatherIcon();
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

  QPoint lateralPausedPosition = cemStatusPosition;
  if (frogpilot_toggles.value(QLatin1String("openpilot_longitudinal")).toBool() || frogpilot_toggles.value(QLatin1String("cem_status")).toBool()) {
    lateralPausedPosition += QPoint(slotStep, 0);
  }

  QPoint longitudinalPausedPosition = lateralPausedPosition + QPoint(slotStep, 0);

  QPoint compassPosition(rightHandDM ? width() - experimentalButtonPosition.x() - btn_size : experimentalButtonPosition.x() + btn_size - widget_size, cemStatusPosition.y());
  if (mapButtonEnabled || frogpilot_toggles.value(QLatin1String("hide_map")).toBool()) {
    compassPosition.rx() += rightHandDM ? btn_size + 2 * UI_BORDER_SIZE : -btn_size - 2 * UI_BORDER_SIZE;
  }
  QPoint weatherPosition = compassPosition - QPoint(slotStep, 0);

  bool showBottomIcons = dmIconPosition != QPoint(0, 0) && !hideBottomIcons;

  if (showBottomIcons && !bigMapOpen && frogpilot_toggles.value(QLatin1String("cem_status")).toBool()) {
    paintCEMStatus(p, cemStatusPosition);
  }

  if (showBottomIcons && !frogpilot_scene.map_open && frogpilot_toggles.value(QLatin1String("compass")).toBool()) {
    paintCompass(p, compassPosition);
  }

  if (!frogpilot_scene.map_open && !speedLimitChanged && frogpilot_toggles.value(QLatin1String("csc_status")).toBool()) {
    if (cscTraining || (isCruiseSet && cscActive)) {
      paintCurveSpeedControl(p);
    }
  }

  if (showBottomIcons && !frogpilot_scene.map_open && lateralPaused) {
    paintPausedIcon(p, lateralPausedPosition, turnIcon);
  }

  if (showBottomIcons && !frogpilot_scene.map_open && (forceCoast || longitudinalPaused)) {
    paintPausedIcon(p, longitudinalPausedPosition, speedIcon);
  }

  if (!bigMapOpen && frogpilot_toggles.value(QLatin1String("pedals_on_ui")).toBool()) {
    paintPedalIcons(p);
  }

  sourcesRect = QRect();

  if (speedLimitChanged) {
    paintPendingSpeedLimit(p);
  }

  if (!bigMapOpen && dmIconPosition != QPoint(0, 0) && (mutcdSpeedLimit || viennaSpeedLimit) && frogpilot_toggles.value(QLatin1String("speed_limit_sources")).toBool()) {
    paintSpeedLimitSources(p);
  }

  if (standstillDuration != 0) {
    paintStandstillTimer(p);
  }

  if (!staticSignalRect.isNull()) {
    paintTurnSignals(p);
  }

  if (showBottomIcons && !frogpilot_scene.map_open) {
    paintWeather(p, weatherPosition);
  }
}

void FrogPilotAnnotatedCameraWidget::paintAdjacentPaths(QPainter &p) {
  const QPolygonF *track_adjacent_vertices = frogpilotUIState()->frogpilot_scene.track_adjacent_vertices;

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
  if (!blindspotLeft && !blindspotRight) {
    return;
  }

  const QPolygonF *track_adjacent_vertices = frogpilotUIState()->frogpilot_scene.track_adjacent_vertices;

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

void FrogPilotAnnotatedCameraWidget::paintLeadMetrics(QPainter &p, bool adjacent, QPointF *chevron, float leadDistance, float vLead) {
  if (!frogpilot_toggles.value(QLatin1String("lead_info")).toBool()) {
    return;
  }

  float leadSpeed = std::max(vLead, 0.0f);

  QString distanceString = QString::number(qRound(leadDistance * distanceConversion));
  QString speedString = QString::number(qRound(leadSpeed * speedConversionMetrics));

  QStringList textLines;
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

  p.setFont(fitInterFont(45, QFont::DemiBold, width() - 2 * UI_BORDER_SIZE, textLines));
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

  const QPolygonF &track_edge_vertices = frogpilotUIState()->frogpilot_scene.track_edge_vertices;
  const QPolygonF &track_vertices = uiState()->scene.track_vertices;

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
  } else if (frogpilot_scene.traffic_mode_enabled) {
    setPathEdgeColors(bg_colors[STATUS_TRAFFIC_MODE_ENABLED]);
  } else if (frogpilot_scene.model_length > maneuverDistance && maneuverDistance >= 1) {
    setPathEdgeColors(bg_colors[STATUS_NAVIGATION_ACTIVE]);
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

  paintWidgetFrame(p, pausedWidget, bg_colors[STATUS_TRAFFIC_MODE_ENABLED]);

  p.setOpacity(0.65);
  p.drawPixmap(pausedWidget, icon);
  p.setOpacity(1.0);
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

void FrogPilotAnnotatedCameraWidget::paintPendingSpeedLimit(QPainter &p) {
  p.save();

  QString newSpeedLimitStr = (unconfirmedSpeedLimit > 1) ? QString::number(std::nearbyint(unconfirmedSpeedLimit * speedConversion)) : "–";

  newSpeedLimitRect = speedLimitRect.translated(speedLimitRect.width() + UI_BORDER_SIZE, 0);
  if (!viennaSpeedLimit) {
    newSpeedLimitRect.setWidth(newSpeedLimitStr.size() >= 3 ? 200 : 175);
  }

  paintSignFrame(p, newSpeedLimitRect, pendingLimitTimer.elapsed() % 1000 < 500 ? QPen(blackColor(), 6) : QPen(redColor(), 6), viennaSpeedLimit);

  if (!viennaSpeedLimit) {
    QString pendingWord = tr("PENDING");
    QString limitWord = tr("LIMIT");
    p.setFont(fitInterFont(28, QFont::DemiBold, newSpeedLimitRect.width() - 30, {pendingWord, limitWord}));
    p.drawText(newSpeedLimitRect.adjusted(0, 22, 0, 0), Qt::AlignTop | Qt::AlignHCenter, pendingWord);
    p.drawText(newSpeedLimitRect.adjusted(0, 51, 0, 0), Qt::AlignTop | Qt::AlignHCenter, limitWord);
    p.setFont(InterFont(70, QFont::Bold));
    p.drawText(newSpeedLimitRect.adjusted(0, 85, 0, 0), Qt::AlignTop | Qt::AlignHCenter, newSpeedLimitStr);
  } else {
    QString pendingWord = tr("PENDING");
    p.setFont(fitInterFont(28, QFont::DemiBold, 90, {pendingWord}));
    p.drawText(newSpeedLimitRect.adjusted(0, 42, 0, 0), Qt::AlignTop | Qt::AlignHCenter, pendingWord);
    p.setFont(InterFont((newSpeedLimitStr.size() >= 3) ? 60 : 70, QFont::Bold));
    p.drawText(newSpeedLimitRect.adjusted(0, 30, 0, 0), Qt::AlignCenter, newSpeedLimitStr);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintRainbowPath(QLinearGradient &bg, float lin_grad_point) {
  float alpha = util::map_val(lin_grad_point, 0.0f, 1.0f, 0.5f, 0.1f);
  float pathHue = fmodf(lin_grad_point * 120.0f + hueOffset, 360.0f);

  bg.setColorAt(lin_grad_point, QColor::fromHslF(pathHue / 360.0f, 1.0f, 0.5f, alpha));
}

void FrogPilotAnnotatedCameraWidget::paintRadarTracks(QPainter &p) {
  UIState &s = *uiState();
  SubMaster &sm = *(s.sm);

  capnp::List<cereal::LiveTracks>::Reader liveTracks = (*frogpilotUIState()->sm)["liveTracks"].getLiveTracks();
  update_radar_tracks(liveTracks, sm["modelV2"].getModelV2().getPosition(), s, sm);

  const std::vector<QPointF> &radar_tracks = frogpilotUIState()->frogpilot_scene.live_radar_tracks;
  if (radar_tracks.empty()) {
    return;
  }

  p.save();

  int diameter = 25;

  float radius = diameter / 2.0f;

  p.setBrush(redColor());

  for (const QPointF &track : radar_tracks) {
    if (!rect().contains(track.toPoint())) {
      continue;
    }

    p.drawEllipse(track, radius, radius);
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintRoadName(QPainter &p) {
  static const QFont font = InterFont(40, QFont::DemiBold);

  static QString cachedRoadName;
  static QString elidedRoadName;
  static int cachedRoadNameWidth = 0;
  static int cachedViewWidth = 0;

  if (roadName != cachedRoadName || width() != cachedViewWidth) {
    cachedRoadName = roadName;
    cachedViewWidth = width();

    QFontMetrics metrics(font);
    elidedRoadName = metrics.elidedText(roadName, Qt::ElideRight, width() - 2 * UI_BORDER_SIZE - 100);
    cachedRoadNameWidth = metrics.horizontalAdvance(elidedRoadName);
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
  p.drawText(roadNameRect, Qt::AlignCenter, elidedRoadName);

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintSignFrame(QPainter &p, const QRect &rect, const QPen &pen, bool vienna) {
  p.setBrush(whiteColor());
  p.setPen(Qt::NoPen);
  if (!vienna) {
    p.drawRoundedRect(rect, 24, 24);
    p.setPen(pen);
    p.drawRoundedRect(rect.adjusted(9, 9, -9, -9), 16, 16);
  } else {
    p.drawEllipse(rect);
    p.setPen(QPen(Qt::red, 20));
    p.drawEllipse(rect.adjusted(16, 16, -16, -16));
    p.setPen(pen);
  }
}

void FrogPilotAnnotatedCameraWidget::paintSpeedLimitSources(QPainter &p) {
  p.save();

  int borderWidth = 8;
  int rowHeight = 45;
  int rowSpacing = UI_BORDER_SIZE / 2;
  int setSpeedBorderWidth = 6;

  std::function<void(QRect&, QPixmap&, const char*, double)> drawSource = [&](QRect &rect, QPixmap &icon, const char *title, double speedLimitValue) {
    if (speedLimitSource == "Mapbox" && QLatin1String(title) == QLatin1String("Navigation")) {
      speedLimitValue = mapboxSpeedLimit * speedConversion;

      title = QT_TR_NOOP("Mapbox");
    }

    bool isActive = speedLimitSource == title && speedLimitValue != 0;

    if (isActive) {
      p.setBrush(redColor(166));
      p.setPen(QPen(redColor(), borderWidth));
    } else {
      p.setBrush(blackColor(166));
      p.setPen(QPen(blackColor(), borderWidth));
    }

    QRect iconRect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignLeft | Qt::AlignVCenter, icon.size(), rect.adjusted(20, 0, 0, 0));

    QString speedText;
    if (speedLimitValue != 0) {
      speedText = QString::number(std::nearbyint(speedLimitValue)) + speedUnit;
    } else {
      speedText = tr("N/A");
    }

    QString fullText = tr(title) + " - " + speedText;

    p.drawRoundedRect(rect, 24, 24);
    p.drawPixmap(iconRect, icon);

    p.setPen(QPen(whiteColor(), 6));
    QRect textRect(iconRect.right() + 10, rect.y(), rect.width() - iconRect.width() - 30, rect.height());
    p.setFont(fitInterFont(35, isActive ? QFont::Bold : QFont::DemiBold, textRect.width() - 4, {fullText}));

    if (isActive) {
      QFontMetrics fm(p.font());
      int textYPosition = textRect.y() + (textRect.height() - fm.height()) / 2 + fm.ascent();

      drawOutlinedText(p, QPointF(textRect.x(), textYPosition), fullText);
    } else {
      p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, fullText);
    }

    rect.translate(0, rowHeight + rowSpacing);
  };

  int sourcesHeight = 5 * rowHeight + 4 * rowSpacing;

  int sourcesTop = setSpeedRect.y() + setSpeedRect.height() + setSpeedBorderWidth / 2;
  int sourcesBottom = dmIconPosition.y() - btn_size / 2;

  QRect rowRect(setSpeedRect.x() + (borderWidth - setSpeedBorderWidth) / 2, sourcesTop + (sourcesBottom - sourcesTop - sourcesHeight) / 2, 450, rowHeight);
  sourcesRect = QRect(rowRect.topLeft(), QSize(rowRect.width(), sourcesHeight)).adjusted(-borderWidth / 2, -borderWidth / 2, borderWidth / 2, borderWidth / 2);

  drawSource(rowRect, cameraIcon, QT_TR_NOOP("Camera"), cameraSpeedLimit * speedConversion);
  drawSource(rowRect, dashboardIcon, QT_TR_NOOP("Dashboard"), dashboardSpeedLimit * speedConversion);
  drawSource(rowRect, mapDataIcon, QT_TR_NOOP("Map Data"), mapSpeedLimit * speedConversion);
  drawSource(rowRect, navigationIcon, QT_TR_NOOP("Navigation"), navigationSpeedLimit * speedConversion);
  drawSource(rowRect, nextMapsIcon, QT_TR_NOOP("Upcoming"), nextSpeedLimit * speedConversion);

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

void FrogPilotAnnotatedCameraWidget::paintStoppingPoint(QPainter &p) {
  const QPolygonF &track_vertices = uiState()->scene.track_vertices;

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

void FrogPilotAnnotatedCameraWidget::paintWeather(QPainter &p, const QPoint &position) {
  if (weatherId == 0) {
    return;
  }

  p.save();

  QRect weatherRect(position, QSize(widget_size, widget_size));

  paintWidgetFrame(p, weatherRect, blackColor());

  if (weatherIcon) {
    p.drawPixmap(weatherRect, weatherIcon->currentPixmap());
  }

  p.restore();
}

void FrogPilotAnnotatedCameraWidget::paintWidgetFrame(QPainter &p, const QRect &rect, const QColor &borderColor) {
  p.setBrush(blackColor(166));
  p.setPen(QPen(borderColor, 10));
  p.drawRoundedRect(rect, 24, 24);
}

void FrogPilotAnnotatedCameraWidget::paintWorldWidgets(QPainter &p) {
  if (!driverView && uiState()->scene.track_vertices.length() >= 1 && redLight && stoppingDistance > 0 && frogpilot_toggles.value(QLatin1String("show_stopping_point")).toBool()) {
    paintStoppingPoint(p);
  }

  if (!driverView && frogpilot_toggles.value(QLatin1String("radar_tracks")).toBool()) {
    paintRadarTracks(p);
  }

  roadNameRect = QRect();
  if (alertHeight == 0 && frogpilot_toggles.value(QLatin1String("road_name_ui")).toBool()) {
    paintRoadName(p);
  }

  if (!bigMapOpen && (blinkerLeft || blinkerRight) && signalStyle != "None" && signalStyle != "static") {
    paintTurnSignals(p);
  }
}

