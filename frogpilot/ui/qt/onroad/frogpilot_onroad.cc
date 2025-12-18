#include "frogpilot/ui/qt/onroad/frogpilot_onroad.h"

FrogPilotOnroadWindow::FrogPilotOnroadWindow(QWidget *parent) : QWidget(parent) {
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::cameraFrameReceived, this, [this] {
    update();
  });
  QObject::connect(uiState(), &UIState::offroadTransition, this, &FrogPilotOnroadWindow::resetFPSStats);
}

void FrogPilotOnroadWindow::resetFPSStats() {
  avgFPS = 0.0f;
  maxFPS = 0.0f;
  minFPS = 99.9f;

  smoothedSteer = 0.0f;
}

void FrogPilotOnroadWindow::resizeEvent(QResizeEvent *event) {
  rect = QWidget::rect();
  marginRegion = QRegion(rect) - QRegion(rect.marginsRemoved(QMargins(UI_BORDER_SIZE, UI_BORDER_SIZE, UI_BORDER_SIZE, UI_BORDER_SIZE)));
}

void FrogPilotOnroadWindow::updateState(const UIState &s, const FrogPilotUIState &fs) {
  const QJsonObject &frogpilot_toggles = fs.frogpilot_scene.frogpilot_toggles;

  const SubMaster &sm = *(s.sm);
  const SubMaster &fpsm = *(fs.sm);

  const cereal::CarState::Reader &carState = sm["carState"].getCarState();
  const cereal::CarControl::Reader &carControl = fpsm["carControl"].getCarControl();

  bool blindSpotLeft = carState.getLeftBlindspot();
  bool blindSpotRight = carState.getRightBlindspot();
  bool turnSignalLeft = carState.getLeftBlinker();
  bool turnSignalRight = carState.getRightBlinker();

  torque = -carControl.getActuators().getTorque();

  showBlindspot = (blindSpotLeft || blindSpotRight) && frogpilot_toggles.value(QLatin1String("blind_spot_metrics")).toBool();
  showFPS = frogpilot_toggles.value(QLatin1String("show_fps")).toBool();
  showSignal = (turnSignalLeft || turnSignalRight) && frogpilot_toggles.value(QLatin1String("signal_metrics")).toBool();
  showSteering = frogpilot_toggles.value(QLatin1String("steering_metrics")).toBool();

  if (showSteering) {
    float absTorque = std::abs(torque);
    smoothedSteer = 0.25f * absTorque + 0.75f * smoothedSteer;
    if (std::abs(smoothedSteer - absTorque) < 0.01f) {
      smoothedSteer = absTorque;
    }
  }

  if (showBlindspot || showSignal) {
    if (!flickerTimer.isValid()) {
      flickerTimer.start();
    }

    int interval = showBlindspot ? 250 : 500;
    bool flickerActive = (flickerTimer.elapsed() / interval) % 2 == 1;

    std::function<QColor(bool, bool)> getBorderColor = [&](bool blindSpot, bool turnSignal) {
      if (turnSignal && showSignal) {
        if (blindSpot) {
          return flickerActive ? bg_colors[STATUS_TRAFFIC_MODE_ENABLED] : bg_colors[STATUS_CEM_DISABLED];
        } else {
          return flickerActive ? bg_colors[STATUS_CEM_DISABLED] : bg;
        }
      } else if (blindSpot && showBlindspot) {
        return bg_colors[STATUS_TRAFFIC_MODE_ENABLED];
      } else {
        return bg;
      }
    };

    leftBorderColor = getBorderColor(blindSpotLeft, turnSignalLeft);
    rightBorderColor = getBorderColor(blindSpotRight, turnSignalRight);
  } else {
    flickerTimer.invalidate();
  }

  if (showFPS && fps > 0.0f) {
    if (avgFPS == 0.0f) {
      avgFPS = fps;
    }

    constexpr float alpha = 1.0f / (UI_FREQ * 60.0f);
    avgFPS = alpha * fps + (1.0f - alpha) * avgFPS;

    minFPS = std::min(minFPS, fps);
    maxFPS = std::max(maxFPS, fps);

    fpsDisplayString = tr("FPS: %1 | Min: %2 | Max: %3 | Avg: %4")
                          .arg(qRound(fps))
                          .arg(qRound(minFPS))
                          .arg(qRound(maxFPS))
                          .arg(qRound(avgFPS));
  }
}

void FrogPilotOnroadWindow::paintEvent(QPaintEvent *event) {
  if (!showSteering && !showBlindspot && !showSignal && !showFPS) {
    return;
  }

  QPainter p(this);

  p.setClipRegion(marginRegion);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  if (showSteering) {
    paintSteeringTorqueBorder(p);
  }

  if (showBlindspot || showSignal) {
    paintTurnSignalBorder(p);
  }

  if (showFPS) {
    paintFPS(p);
  }
}

void FrogPilotOnroadWindow::paintFPS(QPainter &p) {
  p.save();

  p.setFont(InterFont(28, QFont::DemiBold));
  p.setPen(Qt::white);

  int xPos = (rect.width() - p.fontMetrics().horizontalAdvance(fpsDisplayString)) / 2;
  int yPos = rect.bottom() - 5;

  p.drawText(xPos, yPos, fpsDisplayString);

  p.restore();
}

void FrogPilotOnroadWindow::paintSteeringTorqueBorder(QPainter &p) {
  p.save();

  QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
  gradient.setColorAt(0.0, bg_colors[STATUS_TRAFFIC_MODE_ENABLED]);
  gradient.setColorAt(0.25, bg_colors[STATUS_EXPERIMENTAL_MODE_ENABLED]);
  gradient.setColorAt(0.5, bg_colors[STATUS_CEM_DISABLED]);
  gradient.setColorAt(0.75, bg_colors[STATUS_ENGAGED]);

  int visibleHeight = rect.height() * smoothedSteer;
  int xPos = (torque < 0) ? rect.x() : (rect.x() + rect.width() - UI_BORDER_SIZE);
  int yPos = rect.y() + rect.height() - visibleHeight;

  p.fillRect(QRect(xPos, yPos, UI_BORDER_SIZE, visibleHeight), gradient);

  p.restore();
}

void FrogPilotOnroadWindow::paintTurnSignalBorder(QPainter &p) {
  p.save();

  p.fillRect(rect.x(), rect.y(), rect.width() / 2, rect.height(), leftBorderColor);
  p.fillRect(rect.x() + rect.width() / 2, rect.y(), rect.width() / 2, rect.height(), rightBorderColor);

  p.restore();
}
