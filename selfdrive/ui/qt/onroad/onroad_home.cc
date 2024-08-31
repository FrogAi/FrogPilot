#include "selfdrive/ui/qt/onroad/onroad_home.h"

#include <QApplication>
#include <QPainter>
#include <QStackedLayout>

#ifdef ENABLE_MAPS
#include "selfdrive/ui/qt/maps/map_helpers.h"
#include "selfdrive/ui/qt/maps/map_panel.h"
#endif

#include "selfdrive/ui/qt/util.h"

OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout  = new QVBoxLayout(this);
  main_layout->setMargin(UI_BORDER_SIZE);
  QStackedLayout *stacked_layout = new QStackedLayout;
  stacked_layout->setStackingMode(QStackedLayout::StackAll);
  main_layout->addLayout(stacked_layout);

  nvg = new AnnotatedCameraWidget(VISION_STREAM_ROAD, this);

  QWidget * split_wrapper = new QWidget;
  split = new QHBoxLayout(split_wrapper);
  split->setContentsMargins(0, 0, 0, 0);
  split->setSpacing(0);
  split->addWidget(nvg);

  if (getenv("DUAL_CAMERA_VIEW")) {
    CameraWidget *arCam = new CameraWidget("camerad", VISION_STREAM_ROAD, true, this);
    split->insertWidget(0, arCam);
  }

  if (getenv("MAP_RENDER_VIEW")) {
    CameraWidget *map_render = new CameraWidget("navd", VISION_STREAM_MAP, false, this);
    split->insertWidget(0, map_render);
  }

  stacked_layout->addWidget(split_wrapper);

  alerts = new OnroadAlerts(this);
  alerts->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  stacked_layout->addWidget(alerts);

  // setup stacking order
  alerts->raise();

  setAttribute(Qt::WA_OpaquePaintEvent);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &OnroadWindow::updateState);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &OnroadWindow::offroadTransition);
  QObject::connect(uiState(), &UIState::primeChanged, this, &OnroadWindow::primeChanged);

  // FrogPilot variables
  QObject::connect(&clickTimer, &QTimer::timeout, this, [this]() {
    clickTimer.stop();
    QMouseEvent *event = new QMouseEvent(QEvent::MouseButtonPress, timeoutPoint, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::postEvent(this, event);
  });
}

void OnroadWindow::updateState(const UIState &s) {
  if (!s.scene.started) {
    return;
  }

  if (s.scene.map_on_left || s.scene.full_map) {
    split->setDirection(QBoxLayout::LeftToRight);
  } else {
    split->setDirection(QBoxLayout::RightToLeft);
  }

  alerts->updateState(s);
  nvg->updateState(alerts->alert_height, s);

  bool shouldUpdate = false;

  QColor bgColor = bg_colors[s.status];
  if (bg != bgColor) {
    // repaint border
    bg = bgColor;
    shouldUpdate = true;
  }

  // FrogPilot variables
  const UIScene &scene = s.scene;

  accelerationJerk = scene.acceleration_jerk;
  accelerationJerkDifference = scene.acceleration_jerk_difference;
  blindSpotLeft = scene.blind_spot_left;
  blindSpotRight = scene.blind_spot_right;
  fps = scene.fps;
  friction = scene.friction;
  hasLead = scene.has_lead;
  latAccel = scene.lat_accel;
  liveValid = scene.live_valid;
  showBlindspot = scene.show_blind_spot && (blindSpotLeft || blindSpotRight);
  showFPS = scene.show_fps;
  showJerk = scene.show_jerk;
  showSignal = scene.show_signal && (turnSignalLeft || turnSignalRight);
  showSteering = scene.show_steering;
  showTuning = scene.show_tuning;
  speedJerk = scene.speed_jerk;
  speedJerkDifference = scene.speed_jerk_difference;
  steer = scene.steer;
  steeringAngleDeg = scene.steering_angle_deg;
  turnSignalLeft = scene.turn_signal_left;
  turnSignalRight = scene.turn_signal_right;

  if (showBlindspot || showFPS || (showJerk && hasLead) || showSignal || showSteering || showTuning) {
    shouldUpdate = true;
  }

  if (shouldUpdate) {
    update();
  }
}

void OnroadWindow::mousePressEvent(QMouseEvent* e) {
  // FrogPilot variables
  UIState *s = uiState();
  UIScene &scene = s->scene;

  // FrogPilot clickable widgets
  QPoint pos = e->pos();

  QSize size = this->size();
  QRect leftRect(0, 0, size.width() / 2, size.height());
  QRect rightRect(size.width() / 2, 0, size.width() / 2, size.height());

  QRect hideSpeedRect(rect().center().x() - 175, 50, 350, 350);
  QRect maxSpeedRect(7, 25, 225, 225);
  QRect speedLimitRect(7, 250, 225, 225);

  if (scene.speed_limit_changed && (leftRect.contains(pos) || rightRect.contains(pos))) {
    bool slcConfirmed = leftRect.contains(pos) ? !scene.right_hand_drive : scene.right_hand_drive;
    paramsMemory.putBoolNonBlocking("SLCConfirmed", slcConfirmed);
    paramsMemory.putBoolNonBlocking("SLCConfirmedPressed", true);
    return;
  }

  if (maxSpeedRect.contains(pos) && scene.reverse_cruise_ui) {
    scene.reverse_cruise = !scene.reverse_cruise;
    params.putBoolNonBlocking("ReverseCruise", scene.reverse_cruise);
    updateFrogPilotToggles();
    return;
  }

  if (hideSpeedRect.contains(pos) && scene.hide_speed_ui) {
    scene.hide_speed = !scene.hide_speed;
    params.putBoolNonBlocking("HideSpeed", scene.hide_speed);
    return;
  }

  if (speedLimitRect.contains(pos) && scene.show_slc_offset_ui) {
    scene.show_slc_offset = !scene.show_slc_offset;
    params.putBoolNonBlocking("ShowSLCOffset", scene.show_slc_offset);
    return;
  }

  if (scene.experimental_mode_via_screen && pos != timeoutPoint) {
    if (clickTimer.isActive()) {
      clickTimer.stop();

      if (scene.conditional_experimental) {
        int override_value = (scene.conditional_status >= 1 && scene.conditional_status <= 6) ? 0 : (scene.conditional_status >= 7 ? 5 : 6);
        paramsMemory.putIntNonBlocking("CEStatus", override_value);
      } else {
        params.putBoolNonBlocking("ExperimentalMode", !params.getBool("ExperimentalMode"));
      }

    } else {
      clickTimer.start(500);
    }
    return;
  }

#ifdef ENABLE_MAPS
  if (map != nullptr) {
    // Switch between map and sidebar when using navigate on openpilot
    bool sidebarVisible = geometry().x() > 0;
    bool show_map = scene.navigate_on_openpilot ? sidebarVisible : !sidebarVisible;
    map->setVisible(show_map && !map->isVisible());
    if (scene.big_map) {
      map->setFixedWidth(width());
    } else {
      map->setFixedWidth(topWidget(this)->width() / 2 - UI_BORDER_SIZE);
    }
  }
#endif
  // propagation event to parent(HomeWindow)
  QWidget::mousePressEvent(e);
}

void OnroadWindow::createMapWidget() {
#ifdef ENABLE_MAPS
  auto m = new MapPanel(get_mapbox_settings());
  map = m;
  QObject::connect(m, &MapPanel::mapPanelRequested, this, &OnroadWindow::mapPanelRequested);
  QObject::connect(nvg->map_settings_btn, &MapSettingsButton::clicked, m, &MapPanel::toggleMapSettings);
  QObject::connect(nvg->map_settings_btn_bottom, &MapSettingsButton::clicked, m, &MapPanel::toggleMapSettings);
  nvg->map_settings_btn->setEnabled(true);

  m->setFixedWidth(topWidget(this)->width() / 2 - UI_BORDER_SIZE);
  split->insertWidget(0, m);
  // hidden by default, made visible when navRoute is published
  m->setVisible(false);
#endif
}

void OnroadWindow::offroadTransition(bool offroad) {
#ifdef ENABLE_MAPS
  if (!offroad) {
    if (map == nullptr && (uiState()->hasPrime() || !MAPBOX_TOKEN.isEmpty())) {
      createMapWidget();
    }
  }
#endif
  alerts->clear();
}

void OnroadWindow::primeChanged(bool prime) {
#ifdef ENABLE_MAPS
  if (map && (!prime && MAPBOX_TOKEN.isEmpty())) {
    nvg->map_settings_btn->setEnabled(false);
    nvg->map_settings_btn->setVisible(false);
    map->deleteLater();
    map = nullptr;
  } else if (!map && (prime || !MAPBOX_TOKEN.isEmpty())) {
    createMapWidget();
  }
#endif
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);

  // FrogPilot variables
  UIState *s = uiState();
  SubMaster &sm = *(s->sm);

  QRect rect = this->rect();
  QColor bgColor(bg.red(), bg.green(), bg.blue(), 255);
  p.fillRect(rect, bgColor);

  if (showSteering) {
    static float smoothedSteer = 0.0;

    smoothedSteer = 0.1 * std::abs(steer) + 0.9 * smoothedSteer;

    if (std::abs(smoothedSteer - steer) < 0.01) {
      smoothedSteer = steer;
    }

    int visibleHeight = rect.height() * smoothedSteer;

    QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
    gradient.setColorAt(0.0, bg_colors[STATUS_TRAFFIC_MODE_ACTIVE]);
    gradient.setColorAt(0.15, bg_colors[STATUS_EXPERIMENTAL_MODE_ACTIVE]);
    gradient.setColorAt(0.5, bg_colors[STATUS_CONDITIONAL_OVERRIDDEN]);
    gradient.setColorAt(0.85, bg_colors[STATUS_ENGAGED]);
    gradient.setColorAt(1.0, bg_colors[STATUS_ENGAGED]);

    QBrush brush(gradient);
    int fillWidth = UI_BORDER_SIZE;

    if (steeringAngleDeg != 0) {
      QRect rectToFill, rectToHide;
      if (steeringAngleDeg < 0) {
        rectToFill = QRect(rect.x(), rect.y() + rect.height() - visibleHeight, fillWidth, visibleHeight);
        rectToHide = QRect(rect.x(), rect.y(), fillWidth, rect.height() - visibleHeight);
      } else {
        rectToFill = QRect(rect.x() + rect.width() - fillWidth, rect.y() + rect.height() - visibleHeight, fillWidth, visibleHeight);
        rectToHide = QRect(rect.x() + rect.width() - fillWidth, rect.y(), fillWidth, rect.height() - visibleHeight);
      }
      p.fillRect(rectToFill, brush);
      p.fillRect(rectToHide, bgColor);
    }
  }

  if (showBlindspot) {
    QColor blindspotColorLeft = bgColor;
    QColor blindspotColorRight = bgColor;

    if (blindSpotLeft) {
      blindspotColorLeft = bg_colors[STATUS_TRAFFIC_MODE_ACTIVE];
    }

    if (blindSpotRight) {
      blindspotColorRight = bg_colors[STATUS_TRAFFIC_MODE_ACTIVE];
    }

    int xLeft = rect.x();
    int xRight = rect.x() + rect.width() / 2;
    QRect blindspotRectLeft(xLeft, rect.y(), rect.width() / 2, rect.height());
    QRect blindspotRectRight(xRight, rect.y(), rect.width() / 2, rect.height());

    p.fillRect(blindspotRectLeft, blindspotColorLeft);
    p.fillRect(blindspotRectRight, blindspotColorRight);
  }

  if (showSignal) {
    static int signalFramesLeft = 0;
    static int signalFramesRight = 0;

    bool blindSpotActive = (blindSpotLeft && turnSignalLeft) || (blindSpotRight && turnSignalRight);
    bool turnSignalActive = (turnSignalLeft && signalFramesLeft > 0) || (turnSignalRight && signalFramesRight > 0);

    QColor signalBorderColorLeft = bg;
    QColor signalBorderColorRight = bg;

    if (blindSpotLeft) {
      signalBorderColorLeft = bg_colors[STATUS_TRAFFIC_MODE_ACTIVE];
    }

    if (blindSpotRight) {
      signalBorderColorRight = bg_colors[STATUS_TRAFFIC_MODE_ACTIVE];
    }

    if (sm.frame % 20 == 0 || blindSpotActive || turnSignalActive) {
      QColor activeColor = bg_colors[STATUS_CONDITIONAL_OVERRIDDEN];

      if (turnSignalLeft) {
        signalFramesLeft = sm.frame % 10 == 0 && blindSpotActive ? 5 : sm.frame % 20 == 0 ? 10 : signalFramesLeft - 1;
        if (signalFramesLeft > 0) {
          signalBorderColorLeft = activeColor;
        }
      }

      if (turnSignalRight) {
        signalFramesRight = sm.frame % 10 == 0 && blindSpotActive ? 5 : sm.frame % 20 == 0 ? 10 : signalFramesRight - 1;
        if (signalFramesRight > 0) {
          signalBorderColorRight = activeColor;
        }
      }
    }

    int xLeft = rect.x();
    int xRight = rect.x() + rect.width() / 2;
    QRect signalRectLeft(xLeft, rect.y(), rect.width() / 2, rect.height());
    QRect signalRectRight(xRight, rect.y(), rect.width() / 2, rect.height());

    if (turnSignalLeft) {
      p.fillRect(signalRectLeft, signalBorderColorLeft);
    }

    if (turnSignalRight) {
      p.fillRect(signalRectRight, signalBorderColorRight);
    }
  }

  QString logicsDisplayString;
  auto appendJerkInfo = [&](const QString &label, float value, float difference) {
    logicsDisplayString += QString("%1: %2").arg(label).arg(value, 0, 'f', 3);
    if (difference != 0) {
      logicsDisplayString += QString(" (%1%2)").arg(difference > 0 ? "-" : "").arg(difference, 0, 'f', 3);
    }
    logicsDisplayString += " | ";
  };

  if (showJerk) {
    appendJerkInfo("Acceleration Jerk", accelerationJerk, accelerationJerkDifference);
    appendJerkInfo("Speed Jerk", speedJerk, speedJerkDifference);
  }

  if (showTuning) {
    logicsDisplayString += liveValid
        ? QString("Friction: %1 | Lateral Acceleration: %2").arg(friction, 0, 'f', 3).arg(latAccel, 0, 'f', 3)
        : "Friction: Calculating... | Lateral Acceleration: Calculating...";
  }

  if (logicsDisplayString.endsWith(" | ")) {
    logicsDisplayString.chop(3);
  }

  if (!logicsDisplayString.isEmpty()) {
    p.setFont(InterFont(28, QFont::DemiBold));
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setPen(Qt::white);

    int logicsWidth = p.fontMetrics().horizontalAdvance(logicsDisplayString);
    int logicsX = (rect.width() - logicsWidth) / 2;
    int logicsY = rect.top() + 27;

    QStringList parts = logicsDisplayString.split(" | ");
    int currentX = logicsX;

    for (const QString &part : parts) {
      QStringList subParts = part.split(" ");
      for (int i = 0; i < subParts.size(); ++i) {
        QString text = subParts[i];

        if (text.endsWith(")") && i > 0 && (subParts[i - 1].contains("Acceleration") || subParts[i - 1].contains("Speed"))) {
          QString prefix = subParts[i - 1] + " (";
          p.drawText(currentX, logicsY, prefix);
          currentX += p.fontMetrics().horizontalAdvance(prefix);
          text.chop(1);
          p.setPen(text.contains("-") ? redColor() : Qt::white);
        } else if (text.startsWith("(") && i > 0) {
          p.drawText(currentX, logicsY, " (");
          currentX += p.fontMetrics().horizontalAdvance(" (");
          text = text.mid(1);
          p.setPen(text.contains("-") ? redColor() : Qt::white);
        } else {
          p.setPen(Qt::white);
        }

        p.drawText(currentX, logicsY, text);
        currentX += p.fontMetrics().horizontalAdvance(text + " ");
      }
    }
  }

  if (showFPS) {
    qint64 currentMillis = QDateTime::currentMSecsSinceEpoch();
    static std::queue<std::pair<qint64, float>> fpsQueue;

    static float avgFPS = 0.0;
    static float maxFPS = 0.0;
    static float minFPS = 99.9;

    minFPS = std::min(minFPS, fps);
    maxFPS = std::max(maxFPS, fps);

    fpsQueue.push({currentMillis, fps});

    while (!fpsQueue.empty() && currentMillis - fpsQueue.front().first > 60000) {
      fpsQueue.pop();
    }

    if (!fpsQueue.empty()) {
      float totalFPS = 0.0;
      for (auto tempQueue = fpsQueue; !tempQueue.empty(); tempQueue.pop()) {
        totalFPS += tempQueue.front().second;
      }
      avgFPS = totalFPS / fpsQueue.size();
    }

    QString fpsDisplayString = QString("FPS: %1 (%2) | Min: %3 | Max: %4 | Avg: %5")
        .arg(qRound(fps))
        .arg(paramsMemory.getInt("CameraFPS"))
        .arg(qRound(minFPS))
        .arg(qRound(maxFPS))
        .arg(qRound(avgFPS));

    p.setFont(InterFont(28, QFont::DemiBold));
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setPen(Qt::white);

    int textWidth = p.fontMetrics().horizontalAdvance(fpsDisplayString);
    int xPos = (rect.width() - textWidth) / 2;
    int yPos = rect.bottom() - 5;

    p.drawText(xPos, yPos, fpsDisplayString);
  }
}
