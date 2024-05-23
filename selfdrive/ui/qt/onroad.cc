#include "selfdrive/ui/qt/onroad.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>

#include <QApplication>
#include <QDebug>
#include <QMouseEvent>

#include "common/swaglog.h"
#include "common/timing.h"
#include "selfdrive/ui/qt/util.h"
#ifdef ENABLE_MAPS
#include "selfdrive/ui/qt/maps/map_helpers.h"
#include "selfdrive/ui/qt/maps/map_panel.h"
#endif

static void drawIcon(QPainter &p, const QPoint &center, const QPixmap &img, const QBrush &bg, float opacity, const int angle = 0) {
  p.setRenderHint(QPainter::Antialiasing);
  p.setOpacity(1.0);  // bg dictates opacity of ellipse
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(center, btn_size / 2, btn_size / 2);
  p.save();
  p.translate(center);
  p.rotate(angle);
  p.setOpacity(opacity);
  p.drawPixmap(-QPoint(img.width() / 2, img.height() / 2), img);
  p.setOpacity(1.0);
  p.restore();
}

static void drawIconGif(QPainter &p, const QPoint &center, const QMovie &img, const QBrush &bg, float opacity) {
  p.setRenderHint(QPainter::Antialiasing);
  p.setOpacity(1.0);  // bg dictates opacity of ellipse
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(center.x() - btn_size / 2, center.y() - btn_size / 2, btn_size, btn_size);
  p.setOpacity(opacity);
  QPixmap currentFrame = img.currentPixmap();
  p.drawPixmap(center - QPoint(currentFrame.width() / 2, currentFrame.height() / 2), currentFrame);
  p.setOpacity(1.0);
}

OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent), scene(uiState()->scene) {
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

  QColor bgColor = bg_colors[s.status];
  Alert alert = Alert::get(*(s.sm), s.scene.started_frame);
  alerts->updateAlert(alert);

  if (s.scene.map_on_left) {
    split->setDirection(QBoxLayout::LeftToRight);
  } else {
    split->setDirection(QBoxLayout::RightToLeft);
  }

  nvg->updateState(s);

  if (bg != bgColor) {
    // repaint border
    bg = bgColor;
    update();
  }
}

void OnroadWindow::mousePressEvent(QMouseEvent* e) {
  // FrogPilot clickable widgets
  QSize size = this->size();
  QRect leftRect(0, 0, size.width() / 2, size.height());
  QRect rightRect = leftRect.translated(size.width() / 2, 0);
  bool isLeftSideClicked = leftRect.contains(e->pos()) && scene.speed_limit_changed;
  bool isRightSideClicked = rightRect.contains(e->pos()) && scene.speed_limit_changed;

  QRect maxSpeedRect(7, 25, 225, 225);
  bool isMaxSpeedClicked = maxSpeedRect.contains(e->pos()) && scene.reverse_cruise_ui;

  QRect speedLimitRect(7, 250, 225, 225);
  bool isSpeedLimitClicked = speedLimitRect.contains(e->pos()) && scene.show_slc_offset_ui;

  if (isLeftSideClicked || isRightSideClicked) {
    bool slcConfirmed = isLeftSideClicked && !scene.right_hand_drive || isRightSideClicked && scene.right_hand_drive;
    paramsMemory.putBoolNonBlocking("SLCConfirmed", slcConfirmed);
    paramsMemory.putBoolNonBlocking("SLCConfirmedPressed", true);
    return;
  }

  if (isMaxSpeedClicked) {
    bool currentReverseCruise = scene.reverse_cruise;
    uiState()->scene.reverse_cruise = !currentReverseCruise;
    params.putBoolNonBlocking("ReverseCruise", !currentReverseCruise);
    updateFrogPilotToggles();
    return;
  }

  if (isSpeedLimitClicked) {
    bool currentShowSLCOffset = scene.show_slc_offset;
    scene.show_slc_offset = !currentShowSLCOffset;
    params.putBoolNonBlocking("ShowSLCOffset", !currentShowSLCOffset);
    return;
  }

  if (scene.experimental_mode_via_screen && e->pos() != timeoutPoint) {
    if (clickTimer.isActive()) {
      clickTimer.stop();

      if (scene.conditional_experimental) {
        int override_value = (scene.conditional_status >= 1 && scene.conditional_status <= 6) ? 0 : (scene.conditional_status >= 7 ? 5 : 6);
        paramsMemory.putIntNonBlocking("CEStatus", override_value);
      } else {
        bool experimentalMode = params.getBool("ExperimentalMode");
        params.putBoolNonBlocking("ExperimentalMode", !experimentalMode);
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
    bool show_map = uiState()->scene.navigate_on_openpilot ? sidebarVisible : !sidebarVisible;
    map->setVisible(show_map && !map->isVisible());
  }
#endif
  // propagation event to parent(HomeWindow)
  QWidget::mousePressEvent(e);
}

void OnroadWindow::offroadTransition(bool offroad) {
#ifdef ENABLE_MAPS
  if (!offroad) {
    if (map == nullptr && (uiState()->hasPrime() || !MAPBOX_TOKEN.isEmpty())) {
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
    }
  }
#endif

  alerts->updateAlert({});
}

void OnroadWindow::primeChanged(bool prime) {
#ifdef ENABLE_MAPS
  if (map && (!prime && MAPBOX_TOKEN.isEmpty())) {
    nvg->map_settings_btn->setEnabled(false);
    nvg->map_settings_btn->setVisible(false);
    map->deleteLater();
    map = nullptr;
  }
#endif
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  UIState *s = uiState();
  SubMaster &sm = *(s->sm);

  QPainter p(this);
  QRect rect = this->rect();
  p.fillRect(rect, QColor(bg.red(), bg.green(), bg.blue(), 255));

  if (scene.show_steering) {
    QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
    gradient.setColorAt(0.0, bg_colors[STATUS_TRAFFIC_MODE_ACTIVE]);
    gradient.setColorAt(0.15, bg_colors[STATUS_EXPERIMENTAL_MODE_ACTIVE]);
    gradient.setColorAt(0.5, bg_colors[STATUS_CONDITIONAL_OVERRIDDEN]);
    gradient.setColorAt(0.85, bg_colors[STATUS_ENGAGED]);
    gradient.setColorAt(1.0, bg_colors[STATUS_ENGAGED]);

    QBrush brush(gradient);
    int fillWidth = UI_BORDER_SIZE;

    steer = 0.10 * abs(scene.steer) + 0.90 * steer;
    int visibleHeight = rect.height() * steer;

    if (scene.steering_angle_deg < 0) {
      QRect leftRect(rect.x(), rect.y() + rect.height() - visibleHeight, fillWidth, visibleHeight);
      p.fillRect(leftRect, brush);
      QRect leftRectHidden(rect.x(), rect.y(), fillWidth, rect.height() - visibleHeight);
      p.fillRect(leftRectHidden, QColor(bg.red(), bg.green(), bg.blue(), 255));
    } else if (scene.steering_angle_deg > 0 ) {
      QRect rightRect(rect.x() + rect.width() - fillWidth, rect.y() + rect.height() - visibleHeight, fillWidth, visibleHeight);
      p.fillRect(rightRect, brush);
      QRect rightRectHidden(rect.x() + rect.width() - fillWidth, rect.y(), fillWidth, rect.height() - visibleHeight);
      p.fillRect(rightRectHidden, QColor(bg.red(), bg.green(), bg.blue(), 255));
    }
  }

  if (scene.show_signal) {
    static int signal_frames = 0;
    QColor signal_border_color;

    if (scene.turn_signal_left || scene.turn_signal_right) {
      if (sm.frame % 20 == 0) {
        signal_border_color = bg_colors[STATUS_CONDITIONAL_OVERRIDDEN];
        signal_frames = 15;
      } else if (signal_frames > 0) {
        signal_border_color = bg_colors[STATUS_CONDITIONAL_OVERRIDDEN];
        signal_frames--;
      } else {
        signal_border_color = bg;
      }

      if (scene.turn_signal_left) {
        QRect leftHalf(rect.x(), rect.y(), rect.width() / 2, rect.height());
        p.fillRect(leftHalf, QColor(signal_border_color.red(), signal_border_color.green(), signal_border_color.blue(), 255));
      } else if (scene.turn_signal_right) {
        QRect rightHalf(rect.x() + rect.width() / 2, rect.y(), rect.width() / 2, rect.height());
        p.fillRect(rightHalf, QColor(signal_border_color.red(), signal_border_color.green(), signal_border_color.blue(), 255));
      }
    } else {
      signal_frames = 0;
    }
  }

  if (scene.show_blind_spot) {
    auto getBlindspotColor = [&](bool turn_signal, int &frames) {
      if (turn_signal) {
        if (sm.frame % 10 == 0) {
          frames = 5;
        }
        return (frames-- > 0) ? bg_colors[STATUS_TRAFFIC_MODE_ACTIVE] : bg;
      }
      return bg_colors[STATUS_TRAFFIC_MODE_ACTIVE];
    };

    static int blindspot_frames_left = 0;
    static int blindspot_frames_right = 0;

    if (scene.blind_spot_left || scene.blind_spot_right) {
      if (scene.blind_spot_left) {
        QRect leftHalf(rect.x(), rect.y(), rect.width() / 2, rect.height());
        QColor color = getBlindspotColor(scene.turn_signal_left, blindspot_frames_left);
        p.fillRect(leftHalf, QColor(color.red(), color.green(), color.blue(), 255));
      }
      if (scene.blind_spot_right) {
        QRect rightHalf(rect.x() + rect.width() / 2, rect.y(), rect.width() / 2, rect.height());
        QColor color = getBlindspotColor(scene.turn_signal_right, blindspot_frames_right);
        p.fillRect(rightHalf, QColor(color.red(), color.green(), color.blue(), 255));
      }
    } else {
      blindspot_frames_left = blindspot_frames_right = 0;
    }
  }

  if (scene.fps_counter) {
    qint64 currentMillis = QDateTime::currentMSecsSinceEpoch();
    auto fpsQueue = std::queue<std::pair<qint64, double>>();

    static double avgFPS = 0.0;
    static double maxFPS = 0.0;
    static double minFPS = 99.9;

    minFPS = qMin(minFPS, fps);
    maxFPS = qMax(maxFPS, fps);

    fpsQueue.push({currentMillis, fps});

    while (!fpsQueue.empty() && currentMillis - fpsQueue.front().first > 60000) {
      fpsQueue.pop();
    }

    if (!fpsQueue.empty()) {
      double totalFPS = 0;
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

    update();
  }

  QString logicsDisplayString = QString();
  if (scene.show_jerk) {
    logicsDisplayString += QString("Acceleration Jerk: %1")
      .arg(scene.acceleration_jerk, 0, 'f', 3);
    if (scene.acceleration_jerk_difference != 0) {
      logicsDisplayString += QString(" (%1%2)")
        .arg(scene.acceleration_jerk_difference > 0 ? "-" : "", 0)
        .arg(scene.acceleration_jerk_difference, 0, 'f', 3);
    }
    logicsDisplayString += QString(" | Speed Jerk: %1")
      .arg(scene.speed_jerk, 0, 'f', 3);
    if (scene.speed_jerk_difference != 0) {
      logicsDisplayString += QString(" (%1%2)")
        .arg(scene.speed_jerk_difference > 0 ? "-" : "", 0)
        .arg(scene.speed_jerk_difference, 0, 'f', 3);
    }
    logicsDisplayString += " | ";
  }
  if (scene.show_tuning) {
    if (!scene.live_valid) {
      logicsDisplayString += "Friction: Calculating... | Lateral Acceleration: Calculating...";
    } else {
      logicsDisplayString += QString("Friction: %1 | Lateral Acceleration: %2")
        .arg(scene.friction, 0, 'f', 3)
        .arg(scene.lat_accel, 0, 'f', 3);
    }
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

        if (text.endsWith(")") && (subParts[i - 1].contains("Acceleration") || subParts[i - 1].contains("Speed"))) {
          p.drawText(currentX, logicsY, subParts[i - 1] + " (");
          currentX += p.fontMetrics().horizontalAdvance(subParts[i - 1] + " (");
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
    update();
  }
}

// ***** onroad widgets *****

// OnroadAlerts
void OnroadAlerts::updateAlert(const Alert &a) {
  if (!alert.equal(a)) {
    alert = a;
    update();
  }
}

void OnroadAlerts::paintEvent(QPaintEvent *event) {
  if (alert.size == cereal::ControlsState::AlertSize::NONE) {
    return;
  }
  static std::map<cereal::ControlsState::AlertSize, const int> alert_heights = {
    {cereal::ControlsState::AlertSize::SMALL, 271},
    {cereal::ControlsState::AlertSize::MID, 420},
    {cereal::ControlsState::AlertSize::FULL, height()},
  };
  int h = alert_heights[alert.size];

  int margin = 40;
  int radius = 30;
  int offset = scene.show_aol_status_bar || scene.show_cem_status_bar || scene.road_name_ui ? 25 : 0;
  if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    margin = 0;
    radius = 0;
    offset = 0;
  }
  QRect r = QRect(0 + margin, height() - h + margin - offset, width() - margin*2, h - margin*2);

  QPainter p(this);

  // draw background + gradient
  p.setPen(Qt::NoPen);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  p.setBrush(QBrush(alert_colors[alert.status]));
  p.drawRoundedRect(r, radius, radius);

  QLinearGradient g(0, r.y(), 0, r.bottom());
  g.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.05));
  g.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0.35));

  p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
  p.setBrush(QBrush(g));
  p.drawRoundedRect(r, radius, radius);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  // text
  const QPoint c = r.center();
  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setRenderHint(QPainter::TextAntialiasing);
  if (alert.size == cereal::ControlsState::AlertSize::SMALL) {
    p.setFont(InterFont(74, QFont::DemiBold));
    p.drawText(r, Qt::AlignCenter, alert.text1);
  } else if (alert.size == cereal::ControlsState::AlertSize::MID) {
    p.setFont(InterFont(88, QFont::Bold));
    p.drawText(QRect(0, c.y() - 125, width(), 150), Qt::AlignHCenter | Qt::AlignTop, alert.text1);
    p.setFont(InterFont(66));
    p.drawText(QRect(0, c.y() + 21, width(), 90), Qt::AlignHCenter, alert.text2);
  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    bool l = alert.text1.length() > 15;
    p.setFont(InterFont(l ? 132 : 177, QFont::Bold));
    p.drawText(QRect(0, r.y() + (l ? 240 : 270), width(), 600), Qt::AlignHCenter | Qt::TextWordWrap, alert.text1);
    p.setFont(InterFont(88));
    p.drawText(QRect(0, r.height() - (l ? 361 : 420), width(), 300), Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);
  }
}

// ExperimentalButton
ExperimentalButton::ExperimentalButton(QWidget *parent) : experimental_mode(false), engageable(false), QPushButton(parent), scene(uiState()->scene) {
  setFixedSize(btn_size, btn_size + 10);

  engage_img = loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size});
  experimental_img = loadPixmap("../assets/img_experimental.svg", {img_size, img_size});
  QObject::connect(this, &QPushButton::clicked, this, &ExperimentalButton::changeMode);

  wheelImages = {
    {0, loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size})},
    {1, loadPixmap("../frogpilot/assets/wheel_images/lexus.png", {img_size, img_size})},
    {2, loadPixmap("../frogpilot/assets/wheel_images/toyota.png", {img_size, img_size})},
    {3, loadPixmap("../frogpilot/assets/wheel_images/frog.png", {img_size, img_size})},
    {4, loadPixmap("../frogpilot/assets/wheel_images/rocket.png", {img_size, img_size})},
    {5, loadPixmap("../frogpilot/assets/wheel_images/hyundai.png", {img_size, img_size})},
    {6, loadPixmap("../frogpilot/assets/wheel_images/stalin.png", {img_size, img_size})},
    {7, loadPixmap("../frogpilot/assets/random_events/images/firefox.png", {img_size, img_size})},
  };

  wheelImagesGif[1] = new QMovie("../frogpilot/assets/random_events/images/weeb_wheel.gif", QByteArray(), this);
  wheelImagesGif[2] = new QMovie("../frogpilot/assets/random_events/images/tree_fiddy.gif", QByteArray(), this);
  wheelImagesGif[3] = new QMovie("../frogpilot/assets/random_events/images/great_scott.gif", QByteArray(), this);
}

void ExperimentalButton::changeMode() {
  const auto cp = (*uiState()->sm)["carParams"].getCarParams();
  bool can_change = hasLongitudinalControl(cp) && params.getBool("ExperimentalModeConfirmed");
  if (can_change) {
    if (scene.conditional_experimental) {
      int override_value = (scene.conditional_status >= 1 && scene.conditional_status <= 6) ? 0 : scene.conditional_status >= 7 ? 5 : 6;
      paramsMemory.putIntNonBlocking("CEStatus", override_value);
    } else {
      params.putBool("ExperimentalMode", !experimental_mode);
    }
  }
}

void ExperimentalButton::updateState(const UIState &s, bool leadInfo) {
  const auto cs = (*s.sm)["controlsState"].getControlsState();
  bool eng = cs.getEngageable() || cs.getEnabled() || scene.always_on_lateral_active;
  if ((cs.getExperimentalMode() != experimental_mode) || (eng != engageable)) {
    engageable = eng;
    experimental_mode = cs.getExperimentalMode();
    update();
  }

  // FrogPilot variables
  int randomEvent = scene.current_random_event;

  rotatingWheel = scene.rotating_wheel;
  wheelIcon = scene.wheel_icon;
  wheelIconGif = 0;

  y_offset = leadInfo ? 10 : 0;

  if (randomEvent == 0 && gifLabel) {
    delete gifLabel;
    gifLabel = nullptr;
  } else if (randomEvent == 1) {
    static int rotationDegree = 0;
    rotationDegree = (rotationDegree + 36) % 360;
    steeringAngleDeg = rotationDegree;
    wheelIcon = 7;
    update();

  } else if (randomEvent == 2 || randomEvent == 3 || randomEvent == 4) {
    if (!gifLabel) {
      gifLabel = new QLabel(this);
      QMovie *movie = wheelImagesGif[randomEvent - 1];
      if (movie) {
        gifLabel->setMovie(movie);
        gifLabel->setFixedSize(img_size, img_size);
        gifLabel->move((width() - gifLabel->width()) / 2, (height() - gifLabel->height()) / 2 + y_offset);
        gifLabel->movie()->start();
      }
    }
    gifLabel->show();
    wheelIconGif = randomEvent - 1;
    update();

  } else if (rotatingWheel && steeringAngleDeg != scene.steering_angle_deg) {
    steeringAngleDeg = scene.steering_angle_deg;
    update();
    steeringAngleDeg = scene.steering_angle_deg;
  } else if (!rotatingWheel) {
    steeringAngleDeg = 0;
  }
}

void ExperimentalButton::paintEvent(QPaintEvent *event) {
  if (wheelIcon < 0) {
    return;
  }

  QPainter p(this);
  engage_img = wheelImages[wheelIcon];
  QPixmap img = wheelIcon != 0 ? engage_img : (experimental_mode ? experimental_img : engage_img);
  QMovie *gif = wheelImagesGif[wheelIconGif];

  QColor background_color = wheelIcon != 0 && !isDown() && engageable ?
    (scene.always_on_lateral_active ? bg_colors[STATUS_ALWAYS_ON_LATERAL_ACTIVE] :
    (scene.conditional_status == 1 || scene.conditional_status == 3 || scene.conditional_status == 5 ? bg_colors[STATUS_CONDITIONAL_OVERRIDDEN] :
    (experimental_mode ? bg_colors[STATUS_EXPERIMENTAL_MODE_ACTIVE] :
    (scene.traffic_mode_active ? bg_colors[STATUS_TRAFFIC_MODE_ACTIVE] :
    (scene.navigate_on_openpilot ? bg_colors[STATUS_NAVIGATION_ACTIVE] : QColor(0, 0, 0, 166)))))) :
    QColor(0, 0, 0, 166);

  if (wheelIconGif != 0) {
    drawIconGif(p, QPoint(btn_size / 2, btn_size / 2 + y_offset), *gif, background_color, 1.0);
  } else {
    drawIcon(p, QPoint(btn_size / 2, btn_size / 2 + y_offset), img, background_color, (isDown() || !engageable) ? 0.6 : 1.0, steeringAngleDeg);
  }
}


// MapSettingsButton
MapSettingsButton::MapSettingsButton(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size, btn_size + 20);
  settings_img = loadPixmap("../assets/navigation/icon_directions_outlined.svg", {img_size, img_size});

  // hidden by default, made visible if map is created (has prime or mapbox token)
  setVisible(false);
  setEnabled(false);
}

void MapSettingsButton::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  drawIcon(p, QPoint(btn_size / 2, btn_size / 2), settings_img, QColor(0, 0, 0, 166), isDown() ? 0.6 : 1.0);
}


// Window that shows camera view and variety of info drawn on top
AnnotatedCameraWidget::AnnotatedCameraWidget(VisionStreamType type, QWidget* parent) : fps_filter(UI_FREQ, 3, 1. / UI_FREQ), CameraWidget("camerad", type, true, parent), scene(uiState()->scene) {
  pm = std::make_unique<PubMaster, const std::initializer_list<const char *>>({"uiDebug"});

  main_layout = new QVBoxLayout(this);
  main_layout->setMargin(UI_BORDER_SIZE);
  main_layout->setSpacing(0);

  QHBoxLayout *buttons_layout = new QHBoxLayout();
  buttons_layout->setSpacing(0);

  experimental_btn = new ExperimentalButton(this);
  buttons_layout->addWidget(experimental_btn);

  QVBoxLayout *top_right_layout = new QVBoxLayout();
  top_right_layout->setSpacing(0);
  top_right_layout->addLayout(buttons_layout);

  pedal_icons = new PedalIcons(this);
  top_right_layout->addWidget(pedal_icons, 0, Qt::AlignRight);

  main_layout->addLayout(top_right_layout, 0);
  main_layout->setAlignment(top_right_layout, Qt::AlignTop | Qt::AlignRight);

  map_settings_btn = new MapSettingsButton(this);
  main_layout->addWidget(map_settings_btn, 0, Qt::AlignBottom | Qt::AlignRight);

  dm_img = loadPixmap("../assets/img_driver_face.png", {img_size + 5, img_size + 5});

  // Initialize FrogPilot widgets
  initializeFrogPilotWidgets();
}

void AnnotatedCameraWidget::updateState(const UIState &s) {
  const int SET_SPEED_NA = 255;
  const SubMaster &sm = *(s.sm);

  const bool cs_alive = sm.alive("controlsState");
  const bool nav_alive = sm.alive("navInstruction") && sm["navInstruction"].getValid();
  const auto cs = sm["controlsState"].getControlsState();
  const auto car_state = sm["carState"].getCarState();
  const auto nav_instruction = sm["navInstruction"].getNavInstruction();

  // Handle older routes where vCruiseCluster is not set
  float v_cruise = cs.getVCruiseCluster() == 0.0 ? cs.getVCruise() : cs.getVCruiseCluster();
  setSpeed = cs_alive ? v_cruise : SET_SPEED_NA;
  is_cruise_set = setSpeed > 0 && (int)setSpeed != SET_SPEED_NA;
  if (is_cruise_set && !s.scene.is_metric) {
    setSpeed *= KM_TO_MILE;
  }

  // Handle older routes where vEgoCluster is not set
  v_ego_cluster_seen = v_ego_cluster_seen || car_state.getVEgoCluster() != 0.0;
  float v_ego = v_ego_cluster_seen ? car_state.getVEgoCluster() : car_state.getVEgo();
  speed = cs_alive ? std::max<float>(0.0, v_ego) : 0.0;
  speed *= s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH;

  auto speed_limit_sign = nav_instruction.getSpeedLimitSign();
  speedLimit = slcOverridden ? scene.speed_limit_overridden_speed : speedLimitController ? scene.speed_limit : nav_alive ? nav_instruction.getSpeedLimit() : 0.0;
  speedLimit *= (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH);
  if (speedLimitController && !slcOverridden) {
    speedLimit = speedLimit - (showSLCOffset ? slcSpeedLimitOffset : 0);
  }

  has_us_speed_limit = (nav_alive && speed_limit_sign == cereal::NavInstruction::SpeedLimitSign::MUTCD) || (speedLimitController && !useViennaSLCSign);
  has_eu_speed_limit = (nav_alive && speed_limit_sign == cereal::NavInstruction::SpeedLimitSign::VIENNA) && !(speedLimitController && !useViennaSLCSign) || (speedLimitController && useViennaSLCSign);
  is_metric = s.scene.is_metric;
  speedUnit =  s.scene.is_metric ? tr("km/h") : tr("mph");
  hideBottomIcons = (cs.getAlertSize() != cereal::ControlsState::AlertSize::NONE || customSignals != 0 && (turnSignalLeft || turnSignalRight));
  status = s.status;

  // update engageability/experimental mode button
  experimental_btn->updateState(s, leadInfo);

  // update DM icon
  auto dm_state = sm["driverMonitoringState"].getDriverMonitoringState();
  dmActive = dm_state.getIsActiveMode();
  rightHandDM = dm_state.getIsRHD();
  // DM icon transition
  dm_fade_state = std::clamp(dm_fade_state+0.2*(0.5-dmActive), 0.0, 1.0);

  // hide map settings button for alerts and flip for right hand DM
  if (map_settings_btn->isEnabled()) {
    map_settings_btn->setVisible(!hideBottomIcons && compass);
    main_layout->setAlignment(map_settings_btn, (rightHandDM ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignTop);
  }

  // Update FrogPilot widgets
  updateFrogPilotWidgets();
}

void AnnotatedCameraWidget::drawHud(QPainter &p) {
  p.save();

  // Header gradient
  QLinearGradient bg(0, UI_HEADER_HEIGHT - (UI_HEADER_HEIGHT / 2.5), 0, UI_HEADER_HEIGHT);
  bg.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.45));
  bg.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));
  p.fillRect(0, 0, width(), UI_HEADER_HEIGHT, bg);

  QString speedLimitStr = (speedLimit > 1) ? QString::number(std::nearbyint(speedLimit)) : "–";
  QString speedLimitOffsetStr = slcSpeedLimitOffset == 0 ? "–" : QString::number(slcSpeedLimitOffset, 'f', 0).prepend(slcSpeedLimitOffset > 0 ? "+" : "");
  QString speedStr = QString::number(std::nearbyint(speed));
  QString setSpeedStr = is_cruise_set ? QString::number(std::nearbyint(setSpeed - cruiseAdjustment)) : "–";

  // Draw outer box + border to contain set speed and speed limit
  const int sign_margin = 12;
  const int us_sign_height = 186;
  const int eu_sign_size = 176;

  const QSize default_size = {172, 204};
  QSize set_speed_size = default_size;
  if (is_metric || has_eu_speed_limit) set_speed_size.rwidth() = 200;
  if (has_us_speed_limit && speedLimitStr.size() >= 3) set_speed_size.rwidth() = 223;

  if (has_us_speed_limit) set_speed_size.rheight() += us_sign_height + sign_margin;
  else if (has_eu_speed_limit) set_speed_size.rheight() += eu_sign_size + sign_margin;

  int top_radius = 32;
  int bottom_radius = has_eu_speed_limit ? 100 : 32;

  QRect set_speed_rect(QPoint(60 + (default_size.width() - set_speed_size.width()) / 2, 45), set_speed_size);
  if (is_cruise_set && cruiseAdjustment != 0) {
    float transition = qBound(0.0f, 4.0f * (cruiseAdjustment / setSpeed), 1.0f);
    QColor min = whiteColor(75);
    QColor max = vtscControllingCurve ? redColor() : greenColor();

    p.setPen(QPen(QColor::fromRgbF(
      min.redF()   + transition * (max.redF()   - min.redF()),
      min.greenF() + transition * (max.greenF() - min.greenF()),
      min.blueF()  + transition * (max.blueF()  - min.blueF())
    ), 10));
  } else if (trafficModeActive) {
    p.setPen(QPen(redColor(), 10));
  } else if (scene.reverse_cruise) {
    p.setPen(QPen(blueColor(), 6));
  } else {
    p.setPen(QPen(whiteColor(75), 6));
  }
  p.setBrush(blackColor(166));
  drawRoundedRect(p, set_speed_rect, top_radius, top_radius, bottom_radius, bottom_radius);

  // Draw MAX
  QColor max_color = QColor(0x80, 0xd8, 0xa6, 0xff);
  QColor set_speed_color = whiteColor();
  if (is_cruise_set) {
    if (status == STATUS_DISENGAGED) {
      max_color = whiteColor();
    } else if (status == STATUS_OVERRIDE) {
      max_color = QColor(0x91, 0x9b, 0x95, 0xff);
    } else if (speedLimit > 0) {
      auto interp_color = [=](QColor c1, QColor c2, QColor c3) {
        return speedLimit > 0 ? interpColor(setSpeed, {speedLimit + 5, speedLimit + 15, speedLimit + 25}, {c1, c2, c3}) : c1;
      };
      max_color = interp_color(max_color, QColor(0xff, 0xe4, 0xbf), QColor(0xff, 0xbf, 0xbf));
      set_speed_color = interp_color(set_speed_color, QColor(0xff, 0x95, 0x00), QColor(0xff, 0x00, 0x00));
    }
  } else {
    max_color = QColor(0xa6, 0xa6, 0xa6, 0xff);
    set_speed_color = QColor(0x72, 0x72, 0x72, 0xff);
  }
  p.setFont(InterFont(40, QFont::DemiBold));
  p.setPen(max_color);
  p.drawText(set_speed_rect.adjusted(0, 27, 0, 0), Qt::AlignTop | Qt::AlignHCenter, tr("MAX"));
  p.setFont(InterFont(90, QFont::Bold));
  p.setPen(set_speed_color);
  p.drawText(set_speed_rect.adjusted(0, 77, 0, 0), Qt::AlignTop | Qt::AlignHCenter, setSpeedStr);

  const QRect sign_rect = set_speed_rect.adjusted(sign_margin, default_size.height(), -sign_margin, -sign_margin);
  // US/Canada (MUTCD style) sign
  if (has_us_speed_limit) {
    p.setPen(Qt::NoPen);
    p.setBrush(whiteColor());
    p.drawRoundedRect(sign_rect, 24, 24);
    p.setPen(QPen(blackColor(), 6));
    p.drawRoundedRect(sign_rect.adjusted(9, 9, -9, -9), 16, 16);

    p.save();
    p.setOpacity(slcOverridden ? 0.25 : 1.0);
    if (speedLimitController && showSLCOffset && !slcOverridden) {
      p.setFont(InterFont(28, QFont::DemiBold));
      p.drawText(sign_rect.adjusted(0, 22, 0, 0), Qt::AlignTop | Qt::AlignHCenter, tr("LIMIT"));
      p.setFont(InterFont(70, QFont::Bold));
      p.drawText(sign_rect.adjusted(0, 51, 0, 0), Qt::AlignTop | Qt::AlignHCenter, speedLimitStr);
      p.setFont(InterFont(50, QFont::DemiBold));
      p.drawText(sign_rect.adjusted(0, 120, 0, 0), Qt::AlignTop | Qt::AlignHCenter, speedLimitOffsetStr);
    } else {
      p.setFont(InterFont(28, QFont::DemiBold));
      p.drawText(sign_rect.adjusted(0, 22, 0, 0), Qt::AlignTop | Qt::AlignHCenter, tr("SPEED"));
      p.drawText(sign_rect.adjusted(0, 51, 0, 0), Qt::AlignTop | Qt::AlignHCenter, tr("LIMIT"));
      p.setFont(InterFont(70, QFont::Bold));
      p.drawText(sign_rect.adjusted(0, 85, 0, 0), Qt::AlignTop | Qt::AlignHCenter, speedLimitStr);
    }
    p.restore();
  }

  // EU (Vienna style) sign
  if (has_eu_speed_limit) {
    p.setPen(Qt::NoPen);
    p.setBrush(whiteColor());
    p.drawEllipse(sign_rect);
    p.setPen(QPen(Qt::red, 20));
    p.drawEllipse(sign_rect.adjusted(16, 16, -16, -16));

    p.save();
    p.setOpacity(slcOverridden ? 0.25 : 1.0);
    p.setPen(blackColor());
    if (showSLCOffset) {
      p.setFont(InterFont((speedLimitStr.size() >= 3) ? 60 : 70, QFont::Bold));
      p.drawText(sign_rect.adjusted(0, -25, 0, 0), Qt::AlignCenter, speedLimitStr);
      p.setFont(InterFont(40, QFont::DemiBold));
      p.drawText(sign_rect.adjusted(0, 100, 0, 0), Qt::AlignTop | Qt::AlignHCenter, speedLimitOffsetStr);
    } else {
      p.setFont(InterFont((speedLimitStr.size() >= 3) ? 60 : 70, QFont::Bold));
      p.drawText(sign_rect, Qt::AlignCenter, speedLimitStr);
    }
    p.restore();
  }

  // current speed
  p.setFont(InterFont(176, QFont::Bold));
  drawText(p, rect().center().x(), 210, speedStr);
  p.setFont(InterFont(66));
  drawText(p, rect().center().x(), 290, speedUnit, 200);

  p.restore();

  // Draw FrogPilot widgets
  paintFrogPilotWidgets(p);
}

void AnnotatedCameraWidget::drawText(QPainter &p, int x, int y, const QString &text, int alpha) {
  QRect real_rect = p.fontMetrics().boundingRect(text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void AnnotatedCameraWidget::initializeGL() {
  CameraWidget::initializeGL();
  qInfo() << "OpenGL version:" << QString((const char*)glGetString(GL_VERSION));
  qInfo() << "OpenGL vendor:" << QString((const char*)glGetString(GL_VENDOR));
  qInfo() << "OpenGL renderer:" << QString((const char*)glGetString(GL_RENDERER));
  qInfo() << "OpenGL language version:" << QString((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

  prev_draw_t = millis_since_boot();
  setBackgroundColor(bg_colors[STATUS_DISENGAGED]);
}

void AnnotatedCameraWidget::updateFrameMat() {
  CameraWidget::updateFrameMat();
  UIState *s = uiState();
  int w = width(), h = height();

  s->fb_w = w;
  s->fb_h = h;

  // Apply transformation such that video pixel coordinates match video
  // 1) Put (0, 0) in the middle of the video
  // 2) Apply same scaling as video
  // 3) Put (0, 0) in top left corner of video
  s->car_space_transform.reset();
  s->car_space_transform.translate(w / 2 - x_offset, h / 2 - y_offset)
      .scale(zoom, zoom)
      .translate(-intrinsic_matrix.v[2], -intrinsic_matrix.v[5]);
}

void AnnotatedCameraWidget::drawLaneLines(QPainter &painter, const UIState *s) {
  painter.save();

  SubMaster &sm = *(s->sm);

  // lanelines
  for (int i = 0; i < std::size(scene.lane_line_vertices); ++i) {
    if (currentHolidayTheme != 0) {
      painter.setBrush(std::get<2>(holidayThemeConfiguration[currentHolidayTheme]).begin()->second);
    } else if (customColors != 0) {
      painter.setBrush(std::get<2>(themeConfiguration[customColors]).begin()->second);
    } else {
      painter.setBrush(QColor::fromRgbF(1.0, 1.0, 1.0, std::clamp<float>(scene.lane_line_probs[i], 0.0, 0.7)));
    }
    painter.drawPolygon(scene.lane_line_vertices[i]);
  }

  // road edges
  for (int i = 0; i < std::size(scene.road_edge_vertices); ++i) {
    if (currentHolidayTheme != 0) {
      painter.setBrush(std::get<2>(holidayThemeConfiguration[currentHolidayTheme]).begin()->second);
    } else if (customColors != 0) {
      painter.setBrush(std::get<2>(themeConfiguration[customColors]).begin()->second);
    } else {
      painter.setBrush(QColor::fromRgbF(1.0, 0, 0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0)));
    }
    painter.drawPolygon(scene.road_edge_vertices[i]);
  }

  // paint path
  QLinearGradient bg(0, height(), 0, 0);
  if (experimentalMode || scene.acceleration_path) {
    // The first half of track_vertices are the points for the right side of the path
    // and the indices match the positions of accel from uiPlan
    const auto &acceleration_const = sm["uiPlan"].getUiPlan().getAccel();
    const int max_len = std::min<int>(scene.track_vertices.length() / 2, acceleration_const.size());

    // Copy of the acceleration vector
    std::vector<float> acceleration;
    for (int i = 0; i < acceleration_const.size(); i++) {
      acceleration.push_back(acceleration_const[i]);
    }

    for (int i = 0; i < max_len; ++i) {
      // Some points are out of frame
      if (scene.track_vertices[i].y() < 0 || scene.track_vertices[i].y() > height()) continue;

      // Flip so 0 is bottom of frame
      float lin_grad_point = (height() - scene.track_vertices[i].y()) / height();

      // If acceleration is between -0.25 and 0.25, resort to the theme color
      if (std::abs(acceleration[i]) < 0.25 && (currentHolidayTheme != 0)) {
        const std::map<double, QBrush> &colorMap = std::get<2>(holidayThemeConfiguration[currentHolidayTheme]);
        for (const std::pair<double, QBrush> &entry : colorMap) {
          bg.setColorAt(entry.first, entry.second.color());
        }
      } else if (std::abs(acceleration[i]) < 0.25 && (customColors != 0)) {
        const std::map<double, QBrush> &colorMap = std::get<2>(themeConfiguration[customColors]);
        for (const std::pair<double, QBrush> &entry : colorMap) {
          bg.setColorAt(entry.first, entry.second.color());
        }
      } else {
        // speed up: 120, slow down: 0
        float path_hue = fmax(fmin(60 + acceleration[i] * 35, 120), 0);
        // FIXME: painter.drawPolygon can be slow if hue is not rounded
        path_hue = int(path_hue * 100 + 0.5) / 100;

        float saturation = fmin(fabs(acceleration[i] * 1.5), 1);
        float lightness = util::map_val(saturation, 0.0f, 1.0f, 0.95f, 0.62f);  // lighter when grey
        float alpha = util::map_val(lin_grad_point, 0.75f / 2.f, 0.75f, 0.4f, 0.0f);  // matches previous alpha fade
        bg.setColorAt(lin_grad_point, QColor::fromHslF(path_hue / 360., saturation, lightness, alpha));

        // Skip a point, unless next is last
        i += (i + 2) < max_len ? 1 : 0;
      }
    }
  } else if (currentHolidayTheme != 0) {
    const std::map<double, QBrush> &colorMap = std::get<2>(holidayThemeConfiguration[currentHolidayTheme]);
    for (const std::pair<double, QBrush> &entry : colorMap) {
      bg.setColorAt(entry.first, entry.second.color());
    }
  } else if (customColors != 0) {
    const std::map<double, QBrush> &colorMap = std::get<2>(themeConfiguration[customColors]);
    for (const std::pair<double, QBrush> &entry : colorMap) {
      bg.setColorAt(entry.first, entry.second.color());
    }
  } else {
    bg.setColorAt(0.0, QColor::fromHslF(148 / 360., 0.94, 0.51, 0.4));
    bg.setColorAt(0.5, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.35));
    bg.setColorAt(1.0, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.0));
  }

  painter.setBrush(bg);
  painter.drawPolygon(scene.track_vertices);

  // Paint blindspot path
  if (scene.blind_spot_path) {
    QLinearGradient bs(0, height(), 0, 0);

    bs.setColorAt(0.0, QColor::fromHslF(0 / 360., 0.75, 0.50, 0.6));
    bs.setColorAt(0.5, QColor::fromHslF(0 / 360., 0.75, 0.50, 0.4));
    bs.setColorAt(1.0, QColor::fromHslF(0 / 360., 0.75, 0.50, 0.2));

    painter.setBrush(bs);
    if (blindSpotLeft) {
      painter.drawPolygon(scene.track_adjacent_vertices[4]);
    }
    if (blindSpotRight) {
      painter.drawPolygon(scene.track_adjacent_vertices[5]);
    }
  }

  // Paint adjacent lane paths
  if (scene.adjacent_path && (laneWidthLeft != 0 || laneWidthRight != 0)) {
    float minLaneWidth = laneDetectionWidth * 0.5;
    float maxLaneWidth = laneDetectionWidth * 1.5;

    auto paintLane = [&](const QPolygonF &lane, float laneWidth, bool blindspot) {
      QLinearGradient al(0, height(), 0, 0);

      bool redPath = laneWidth < minLaneWidth || laneWidth > maxLaneWidth || blindspot;
      float hue = redPath ? 0.0 : 120.0 * (laneWidth - minLaneWidth) / (maxLaneWidth - minLaneWidth);

      al.setColorAt(0.0, QColor::fromHslF(hue / 360.0, 0.75, 0.50, 0.6));
      al.setColorAt(0.5, QColor::fromHslF(hue / 360.0, 0.75, 0.50, 0.4));
      al.setColorAt(1.0, QColor::fromHslF(hue / 360.0, 0.75, 0.50, 0.2));

      painter.setBrush(al);
      painter.drawPolygon(lane);

      if (scene.adjacent_path_metrics) {
        painter.setFont(InterFont(30, QFont::DemiBold));
        painter.setPen(Qt::white);

        QRectF boundingRect = lane.boundingRect();
        QString text = blindspot ? tr("Vehicle in blind spot") : QString("%1%2").arg(laneWidth * distanceConversion, 0, 'f', 2).arg(leadDistanceUnit);
        painter.drawText(boundingRect, Qt::AlignCenter, text);

        painter.setPen(Qt::NoPen);
      }
    };

    paintLane(scene.track_adjacent_vertices[4], laneWidthLeft, blindSpotLeft);
    paintLane(scene.track_adjacent_vertices[5], laneWidthRight, blindSpotRight);
  }

  painter.restore();
}

void AnnotatedCameraWidget::drawDriverState(QPainter &painter, const UIState *s) {
  painter.save();

  // base icon
  int offset = UI_BORDER_SIZE + btn_size / 2;
  int x = rightHandDM ? width() - offset : offset;
  x += onroadDistanceButton ? 250 : 0;
  offset += showAlwaysOnLateralStatusBar || showConditionalExperimentalStatusBar || roadNameUI ? 25 : 0;
  int y = height() - offset;
  float opacity = dmActive ? 0.65 : 0.2;
  drawIcon(painter, QPoint(x, y), dm_img, blackColor(70), opacity);

  // face
  QPointF face_kpts_draw[std::size(default_face_kpts_3d)];
  float kp;
  for (int i = 0; i < std::size(default_face_kpts_3d); ++i) {
    kp = (scene.face_kpts_draw[i].v[2] - 8) / 120 + 1.0;
    face_kpts_draw[i] = QPointF(scene.face_kpts_draw[i].v[0] * kp + x, scene.face_kpts_draw[i].v[1] * kp + y);
  }

  painter.setPen(QPen(QColor::fromRgbF(1.0, 1.0, 1.0, opacity), 5.2, Qt::SolidLine, Qt::RoundCap));
  painter.drawPolyline(face_kpts_draw, std::size(default_face_kpts_3d));

  // tracking arcs
  const int arc_l = 133;
  const float arc_t_default = 6.7;
  const float arc_t_extend = 12.0;
  QColor arc_color = QColor::fromRgbF(0.545 - 0.445 * s->engaged(),
                                      0.545 + 0.4 * s->engaged(),
                                      0.545 - 0.285 * s->engaged(),
                                      0.4 * (1.0 - dm_fade_state));
  float delta_x = -scene.driver_pose_sins[1] * arc_l / 2;
  float delta_y = -scene.driver_pose_sins[0] * arc_l / 2;
  painter.setPen(QPen(arc_color, arc_t_default+arc_t_extend*fmin(1.0, scene.driver_pose_diff[1] * 5.0), Qt::SolidLine, Qt::RoundCap));
  painter.drawArc(QRectF(std::fmin(x + delta_x, x), y - arc_l / 2, fabs(delta_x), arc_l), (scene.driver_pose_sins[1]>0 ? 90 : -90) * 16, 180 * 16);
  painter.setPen(QPen(arc_color, arc_t_default+arc_t_extend*fmin(1.0, scene.driver_pose_diff[0] * 5.0), Qt::SolidLine, Qt::RoundCap));
  painter.drawArc(QRectF(x - arc_l / 2, std::fmin(y + delta_y, y), arc_l, fabs(delta_y)), (scene.driver_pose_sins[0]>0 ? 0 : 180) * 16, 180 * 16);

  painter.restore();
}

void AnnotatedCameraWidget::drawLead(QPainter &painter, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data, const QPointF &vd, const float v_ego) {
  painter.save();

  const float speedBuff = currentHolidayTheme != 0 || customColors != 0 ? 25. : 10.;  // Make the center of the chevron appear sooner if a theme is active
  const float leadBuff = currentHolidayTheme != 0 || customColors != 0 ? 100. : 40.;  // Make the center of the chevron appear sooner if a theme is active
  const float d_rel = lead_data.getX()[0];
  const float v_rel = lead_data.getV()[0] - v_ego;

  float fillAlpha = 0;
  if (d_rel < leadBuff) {
    fillAlpha = 255 * (1.0 - (d_rel / leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255 * (-1 * (v_rel / speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp((float)vd.x(), 0.f, width() - sz / 2);
  float y = std::fmin(height() - sz * .6, (float)vd.y());

  float g_xo = sz / 5;
  float g_yo = sz / 10;

  QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_yo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};
  painter.setBrush(QColor(218, 202, 37, 255));
  painter.drawPolygon(glow, std::size(glow));

  // chevron
  QPointF chevron[] = {{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz}};
  if (currentHolidayTheme != 0) {
    painter.setBrush(std::get<2>(holidayThemeConfiguration[currentHolidayTheme]).begin()->second);
  } else if (customColors != 0) {
    painter.setBrush(std::get<2>(themeConfiguration[customColors]).begin()->second);
  } else {
    painter.setBrush(redColor(fillAlpha));
  }
  painter.drawPolygon(chevron, std::size(chevron));

  if (leadInfo) {
    float lead_speed = std::max(v_rel + v_ego, 0.0f);

    painter.setPen(Qt::white);
    painter.setFont(InterFont(35, QFont::Bold));

    QString text = QString("%1 %2 | %3 %4")
                      .arg(qRound(d_rel * distanceConversion))
                      .arg(leadDistanceUnit)
                      .arg(qRound(lead_speed * speedConversion))
                      .arg(leadSpeedUnit);

    QFontMetrics metrics(painter.font());
    int middle_x = (chevron[2].x() + chevron[0].x()) / 2;
    int textWidth = metrics.horizontalAdvance(text);
    painter.drawText(middle_x - textWidth / 2, chevron[0].y() + metrics.height() + 5, text);
  }

  painter.restore();
}

void AnnotatedCameraWidget::paintGL() {
  UIState *s = uiState();
  SubMaster &sm = *(s->sm);
  const double start_draw_t = millis_since_boot();
  const cereal::ModelDataV2::Reader &model = sm["modelV2"].getModelV2();
  const float v_ego = sm["carState"].getCarState().getVEgo();

  // draw camera frame
  {
    std::lock_guard lk(frame_lock);

    if (frames.empty()) {
      if (skip_frame_count > 0) {
        skip_frame_count--;
        qDebug() << "skipping frame, not ready";
        return;
      }
    } else {
      // skip drawing up to this many frames if we're
      // missing camera frames. this smooths out the
      // transitions from the narrow and wide cameras
      skip_frame_count = 5;
    }

    // Wide or narrow cam dependent on speed
    bool has_wide_cam = available_streams.count(VISION_STREAM_WIDE_ROAD);
    if (has_wide_cam) {
      if ((v_ego < 10) || available_streams.size() == 1) {
        wide_cam_requested = true;
      } else if (v_ego > 15) {
        wide_cam_requested = false;
      }
      wide_cam_requested = wide_cam_requested && experimentalMode;
      // for replay of old routes, never go to widecam
      wide_cam_requested = wide_cam_requested && s->scene.calibration_wide_valid;
    }
    CameraWidget::setStreamType(wide_cam_requested ? VISION_STREAM_WIDE_ROAD : VISION_STREAM_ROAD);

    s->scene.wide_cam = CameraWidget::getStreamType() == VISION_STREAM_WIDE_ROAD;
    if (s->scene.calibration_valid) {
      auto calib = s->scene.wide_cam ? s->scene.view_from_wide_calib : s->scene.view_from_calib;
      CameraWidget::updateCalibration(calib);
    } else {
      CameraWidget::updateCalibration(DEFAULT_CALIBRATION);
    }
    CameraWidget::setFrameId(model.getFrameId());
    CameraWidget::paintGL();
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);

  if (s->scene.world_objects_visible) {
    update_model(s, model, sm["uiPlan"].getUiPlan());
    drawLaneLines(painter, s);

    if (s->scene.longitudinal_control && sm.rcv_frame("modelV2") > s->scene.started_frame) {
      update_leads(s, model);
      float prev_drel = -1;
      for (int i = 0; i < model.getLeadsV3().size() && i < 2; i++) {
        const auto &lead = model.getLeadsV3()[i];
        auto lead_drel = lead.getX()[0];
        if (lead.getProb() > scene.lead_detection_threshold && (prev_drel < 0 || std::abs(lead_drel - prev_drel) > 3.0)) {
          drawLead(painter, lead, s->scene.lead_vertices[i], v_ego);
        }
        prev_drel = lead_drel;
      }
    }
  }

  // DMoji
  if (!hideBottomIcons && (sm.rcv_frame("driverStateV2") > s->scene.started_frame)) {
    update_dmonitoring(s, sm["driverStateV2"].getDriverStateV2(), dm_fade_state, rightHandDM);
    drawDriverState(painter, s);
  }

  drawHud(painter);

  double cur_draw_t = millis_since_boot();
  double dt = cur_draw_t - prev_draw_t;
  fps = fps_filter.update(1. / dt * 1000);
  if (fps < 15) {
    LOGW("slow frame rate: %.2f fps", fps);
  }
  prev_draw_t = cur_draw_t;

  // publish debug msg
  MessageBuilder msg;
  auto m = msg.initEvent().initUiDebug();
  m.setDrawTimeMillis(cur_draw_t - start_draw_t);
  pm->send("uiDebug", msg);
}

void AnnotatedCameraWidget::showEvent(QShowEvent *event) {
  CameraWidget::showEvent(event);

  ui_update_params(uiState());
  prev_draw_t = millis_since_boot();
}

// FrogPilot widgets
void AnnotatedCameraWidget::initializeFrogPilotWidgets() {
  bottom_layout = new QHBoxLayout();

  distance_btn = new DistanceButton(this);
  bottom_layout->addWidget(distance_btn);

  QSpacerItem *spacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
  bottom_layout->addItem(spacer);

  compass_img = new Compass(this);
  bottom_layout->addWidget(compass_img);

  map_settings_btn_bottom = new MapSettingsButton(this);
  bottom_layout->addWidget(map_settings_btn_bottom);

  main_layout->addLayout(bottom_layout);

  themeConfiguration = {
    {1, {"frog_theme", QColor(23, 134, 68, 242), {{0.0, QBrush(QColor::fromHslF(144 / 360., 0.71, 0.31, 0.9))},
                                                  {0.5, QBrush(QColor::fromHslF(144 / 360., 0.71, 0.31, 0.5))},
                                                  {1.0, QBrush(QColor::fromHslF(144 / 360., 0.71, 0.31, 0.1))}}}},
    {2, {"tesla_theme", QColor(0, 72, 255, 255), {{0.0, QBrush(QColor::fromHslF(223 / 360., 1.0, 0.5, 0.9))},
                                                  {0.5, QBrush(QColor::fromHslF(223 / 360., 1.0, 0.5, 0.5))},
                                                  {1.0, QBrush(QColor::fromHslF(223 / 360., 1.0, 0.5, 0.1))}}}},
    {3, {"stalin_theme", QColor(255, 0, 0, 255), {{0.0, QBrush(QColor::fromHslF(0 / 360., 1.0, 0.5, 0.9))},
                                                  {0.5, QBrush(QColor::fromHslF(0 / 360., 1.0, 0.5, 0.5))},
                                                  {1.0, QBrush(QColor::fromHslF(0 / 360., 1.0, 0.5, 0.1))}}}},
  };

  holidayThemeConfiguration = {
    {1, {"april_fools", QColor(255, 165, 0, 255), {{0.0, QBrush(QColor::fromHslF(39 / 360., 1.0, 0.5, 0.9))},
                                                   {0.5, QBrush(QColor::fromHslF(39 / 360., 1.0, 0.5, 0.5))},
                                                   {1.0, QBrush(QColor::fromHslF(39 / 360., 1.0, 0.5, 0.1))}}}},
    {2, {"christmas", QColor(0, 72, 255, 255), {{0.0, QBrush(QColor::fromHslF(223 / 360., 1.0, 0.5, 0.9))},
                                                {0.5, QBrush(QColor::fromHslF(223 / 360., 1.0, 0.5, 0.5))},
                                                {1.0, QBrush(QColor::fromHslF(223 / 360., 1.0, 0.5, 0.1))}}}},
    {3, {"cinco_de_mayo", QColor(0, 104, 71, 255), {{0.0, QBrush(QColor::fromHslF(161 / 360., 1.0, 0.2, 0.9))},
                                                    {0.5, QBrush(QColor::fromHslF(161 / 360., 1.0, 0.2, 0.5))},
                                                    {1.0, QBrush(QColor::fromHslF(161 / 360., 1.0, 0.2, 0.1))}}}},
    {4, {"easter", QColor(200, 150, 200, 255), {{0.0, QBrush(QColor::fromHslF(300 / 360., 0.31, 0.69, 0.9))},
                                                {0.5, QBrush(QColor::fromHslF(300 / 360., 0.31, 0.69, 0.5))},
                                                {1.0, QBrush(QColor::fromHslF(300 / 360., 0.31, 0.69, 0.1))}}}},
    {5, {"fourth_of_july", QColor(0, 72, 255, 255), {{0.0, QBrush(QColor::fromHslF(223 / 360., 1.0, 0.5, 0.9))},
                                                     {0.5, QBrush(QColor::fromHslF(223 / 360., 1.0, 0.5, 0.5))},
                                                     {1.0, QBrush(QColor::fromHslF(223 / 360., 1.0, 0.5, 0.1))}}}},
    {6, {"halloween", QColor(255, 0, 0, 255), {{0.0, QBrush(QColor::fromHslF(0 / 360., 1.0, 0.5, 0.9))},
                                               {0.5, QBrush(QColor::fromHslF(0 / 360., 1.0, 0.5, 0.5))},
                                               {1.0, QBrush(QColor::fromHslF(0 / 360., 1.0, 0.5, 0.1))}}}},
    {7, {"new_years_day", QColor(23, 134, 68, 242), {{0.0, QBrush(QColor::fromHslF(144 / 360., 0.71, 0.31, 0.9))},
                                                     {0.5, QBrush(QColor::fromHslF(144 / 360., 0.71, 0.31, 0.5))},
                                                     {1.0, QBrush(QColor::fromHslF(144 / 360., 0.71, 0.31, 0.1))}}}},
    {8, {"st_patricks_day", QColor(0, 128, 0, 255), {{0.0, QBrush(QColor::fromHslF(120 / 360., 1.0, 0.25, 0.9))},
                                                     {0.5, QBrush(QColor::fromHslF(120 / 360., 1.0, 0.25, 0.5))},
                                                     {1.0, QBrush(QColor::fromHslF(120 / 360., 1.0, 0.25, 0.1))}}}},
    {9, {"thanksgiving", QColor(255, 0, 0, 255), {{0.0, QBrush(QColor::fromHslF(0 / 360., 1.0, 0.5, 0.9))},
                                                  {0.5, QBrush(QColor::fromHslF(0 / 360., 1.0, 0.5, 0.5))},
                                                  {1.0, QBrush(QColor::fromHslF(0 / 360., 1.0, 0.5, 0.1))}}}},
    {10, {"valentines_day", QColor(23, 134, 68, 242), {{0.0, QBrush(QColor::fromHslF(144 / 360., 0.71, 0.31, 0.9))},
                                                       {0.5, QBrush(QColor::fromHslF(144 / 360., 0.71, 0.31, 0.5))},
                                                       {1.0, QBrush(QColor::fromHslF(144 / 360., 0.71, 0.31, 0.1))}}}},
    {11, {"world_frog_day", QColor(23, 134, 68, 242), {{0.0, QBrush(QColor::fromHslF(144 / 360., 0.71, 0.31, 0.9))},
                                                       {0.5, QBrush(QColor::fromHslF(144 / 360., 0.71, 0.31, 0.5))},
                                                       {1.0, QBrush(QColor::fromHslF(144 / 360., 0.71, 0.31, 0.1))}}}},
  };

  animationTimer = new QTimer(this);
  connect(animationTimer, &QTimer::timeout, this, [this] {
    animationFrameIndex = (animationFrameIndex + 1) % totalFrames;
  });
}

void AnnotatedCameraWidget::updateFrogPilotWidgets() {
  if (is_metric) {
    accelerationUnit = tr(" m/s²");
    leadDistanceUnit = tr(mapOpen ? "m" : "meters");
    leadSpeedUnit = tr("kph");

    accelerationConversion = 1.0f;
    distanceConversion = 1.0f;
    speedConversion = MS_TO_KPH;
  } else {
    accelerationUnit = tr(" ft/s²");
    leadDistanceUnit = tr(mapOpen ? "ft" : "feet");
    leadSpeedUnit = tr("mph");

    accelerationConversion = METER_TO_FOOT;
    distanceConversion = METER_TO_FOOT;
    speedConversion = MS_TO_MPH;
  }

  alertSize = scene.alert_size;

  alwaysOnLateralActive = scene.always_on_lateral_active;
  showAlwaysOnLateralStatusBar = scene.show_aol_status_bar;

  blindSpotLeft = scene.blind_spot_left;
  blindSpotRight = scene.blind_spot_right;

  compass = scene.compass;

  conditionalStatus = scene.conditional_status;
  showConditionalExperimentalStatusBar = scene.show_cem_status_bar;

  bool disableSmoothing = vtscControllingCurve ? scene.disable_smoothing_vtsc : scene.disable_smoothing_mtsc;
  cruiseAdjustment = disableSmoothing || !is_cruise_set ? fmax(setSpeed - scene.adjusted_cruise, 0) : fmax(0.25 * (setSpeed - scene.adjusted_cruise) + 0.75 * cruiseAdjustment - 1, 0);
  vtscControllingCurve = scene.vtsc_controlling_curve;

  customColors = scene.custom_colors;

  experimentalMode = scene.experimental_mode;

  laneDetectionWidth = scene.lane_detection_width;
  laneWidthLeft = scene.lane_width_left;
  laneWidthRight = scene.lane_width_right;

  leadInfo = scene.lead_info;
  obstacleDistance = scene.obstacle_distance;
  obstacleDistanceStock = scene.obstacle_distance_stock;

  mapOpen = scene.map_open;

  onroadDistanceButton = scene.onroad_distance_button;

  roadNameUI = scene.road_name_ui;

  speedLimitController = scene.speed_limit_controller;
  showSLCOffset = speedLimitController && scene.show_slc_offset;
  slcOverridden = speedLimitController && scene.speed_limit_overridden;
  slcSpeedLimitOffset = scene.speed_limit_offset * (is_metric ? MS_TO_KPH : MS_TO_MPH);
  useViennaSLCSign = scene.use_vienna_slc_sign;

  trafficModeActive = scene.traffic_mode_active;

  turnSignalLeft = scene.turn_signal_left;
  turnSignalRight = scene.turn_signal_right;

  if (currentHolidayTheme != scene.current_holiday_theme || customSignals != scene.custom_signals) {
    currentHolidayTheme = scene.current_holiday_theme;
    customSignals = scene.custom_signals;

    QString themePath;

    if (currentHolidayTheme != 0) {
      themePath = QString("../frogpilot/assets/holiday_themes/%1/images").arg(
        holidayThemeConfiguration.find(currentHolidayTheme) != holidayThemeConfiguration.end() ?
        std::get<0>(holidayThemeConfiguration[currentHolidayTheme]) : "");
    } else {
      themePath = QString("../frogpilot/assets/custom_themes/%1/images").arg(
        themeConfiguration.find(customSignals) != themeConfiguration.end() ?
        std::get<0>(themeConfiguration[customSignals]) : "");
    }

    const QStringList imagePaths = {
      themePath + "/turn_signal_1.png",
      themePath + "/turn_signal_2.png",
      themePath + "/turn_signal_3.png",
      themePath + "/turn_signal_4.png"
    };

    signalImgVector.clear();
    signalImgVector.reserve(2 * imagePaths.size() + 2);

    for (const QString &imagePath : imagePaths) {
      QPixmap pixmap(imagePath);
      signalImgVector.push_back(pixmap);
      signalImgVector.push_back(pixmap.transformed(QTransform().scale(-1, 1)));
    }

    const QPixmap blindSpotPixmap(themePath + "/turn_signal_1_red.png");
    signalImgVector.push_back(blindSpotPixmap);
    signalImgVector.push_back(blindSpotPixmap.transformed(QTransform().scale(-1, 1)));
  }
}

void AnnotatedCameraWidget::paintFrogPilotWidgets(QPainter &p) {
  if (showAlwaysOnLateralStatusBar || showConditionalExperimentalStatusBar || roadNameUI) {
    drawStatusBar(p);
  }

  if (customSignals != 0 && (turnSignalLeft || turnSignalRight)) {
    if (!animationTimer->isActive()) {
      animationTimer->start(totalFrames * 11);  // 440 milliseconds per loop; syncs up perfectly with my 2019 Lexus ES 350 turn signal clicks
    }
    drawTurnSignals(p);
  } else if (animationTimer->isActive()) {
    animationTimer->stop();
  }

  if (leadInfo) {
    drawLeadInfo(p);
  }

  if (scene.speed_limit_changed) {
    drawSLCConfirmation(p);
  }

  bool enableCompass = compass && !hideBottomIcons;
  compass_img->setVisible(enableCompass);
  if (enableCompass) {
    compass_img->updateState();
    bottom_layout->setAlignment(compass_img, (rightHandDM ? Qt::AlignLeft : Qt::AlignRight));
  }

  bool enableDistanceButton = onroadDistanceButton && !hideBottomIcons;
  distance_btn->setVisible(enableDistanceButton);
  if (enableDistanceButton) {
    distance_btn->updateState();
    bottom_layout->setAlignment(distance_btn, (rightHandDM ? Qt::AlignRight : Qt::AlignLeft));
  }

  bool enablePedalIcons = scene.pedals_on_ui && !bigMapOpen;
  pedal_icons->setVisible(enablePedalIcons);
  if (enablePedalIcons) {
    pedal_icons->updateState();
  }

  map_settings_btn_bottom->setEnabled(map_settings_btn->isEnabled());
  if (map_settings_btn_bottom->isEnabled()) {
    map_settings_btn_bottom->setVisible(!hideBottomIcons && !compass);
    bottom_layout->setAlignment(map_settings_btn_bottom, rightHandDM ? Qt::AlignLeft : Qt::AlignRight);
  }
}

Compass::Compass(QWidget *parent) : QWidget(parent), scene(uiState()->scene) {
  setFixedSize(btn_size * 1.5, btn_size * 1.5);

  compassSize = btn_size;
  circleOffset = compassSize / 2;
  degreeLabelOffset = circleOffset + 25;
  innerCompass = compassSize / 2;

  x = (btn_size * 1.5) / 2 + 20;
  y = (btn_size * 1.5) / 2;

  compassInnerImg = loadPixmap("../frogpilot/assets/other_images/compass_inner.png", QSize(compassSize / 1.75, compassSize / 1.75));

  staticElements = QPixmap(size());
  staticElements.fill(Qt::transparent);
  QPainter p(&staticElements);

  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  QPen whitePen(Qt::white, 2);
  p.setPen(whitePen);

  p.setOpacity(1.0);
  p.setBrush(QColor(0, 0, 0, 100));
  p.drawEllipse(x - circleOffset, y - circleOffset, circleOffset * 2, circleOffset * 2);

  p.setBrush(Qt::NoBrush);
  p.drawEllipse(x - (innerCompass + 5), y - (innerCompass + 5), (innerCompass + 5) * 2, (innerCompass + 5) * 2);
  p.drawEllipse(x - degreeLabelOffset, y - degreeLabelOffset, degreeLabelOffset * 2, degreeLabelOffset * 2);

  QPainterPath outerCircle, innerCircle;
  outerCircle.addEllipse(x - degreeLabelOffset, y - degreeLabelOffset, degreeLabelOffset * 2, degreeLabelOffset * 2);
  innerCircle.addEllipse(x - circleOffset, y - circleOffset, compassSize, compassSize);
  p.fillPath(outerCircle.subtracted(innerCircle), Qt::black);
}

void Compass::updateState() {
  if (bearingDeg != scene.bearing_deg) {
    update();
    bearingDeg = scene.bearing_deg;
  }
}

void Compass::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  bearingDeg = fmod(bearingDeg, 360);
  if (bearingDeg < 0) {
    bearingDeg += 360;
  }

  p.drawPixmap(0, 0, staticElements);

  p.translate(x, y);
  p.rotate(bearingDeg);
  p.drawPixmap(-compassInnerImg.width() / 2, -compassInnerImg.height() / 2, compassInnerImg);
  p.rotate(-bearingDeg);
  p.translate(-x, -y);

  QFont font = InterFont(10, QFont::Normal);
  for (int i = 0; i < 360; i += 15) {
    bool isBold = abs(i - bearingDeg) <= 7;
    font.setWeight(isBold ? QFont::Bold : QFont::Normal);
    p.setFont(font);
    p.setPen(QPen(Qt::white, i % 90 == 0 ? 2 : 1));

    p.save();
    p.translate(x, y);
    p.rotate(i);
    p.drawLine(0, -(compassSize / 2 - (i % 90 == 0 ? 12 : 8)), 0, -(compassSize / 2));
    p.translate(0, -(compassSize / 2 + 12));
    p.rotate(-i);
    p.drawText(QRect(-20, -10, 40, 20), Qt::AlignCenter, QString::number(i));
    p.restore();
  }

  p.setFont(InterFont(20, QFont::Bold));
  std::map<QString, std::tuple<QPair<float, float>, int, QColor>> directionInfo = {
    {"N", {{292.5, 67.5}, Qt::AlignTop | Qt::AlignHCenter, Qt::white}},
    {"E", {{22.5, 157.5}, Qt::AlignRight | Qt::AlignVCenter, Qt::white}},
    {"S", {{112.5, 247.5}, Qt::AlignBottom | Qt::AlignHCenter, Qt::white}},
    {"W", {{202.5, 337.5}, Qt::AlignLeft | Qt::AlignVCenter, Qt::white}}
  };
  int directionOffset = 20;

  for (auto &item : directionInfo) {
    QString direction = item.first;
    auto &[range, alignmentFlag, color] = item.second;
    auto &[minRange, maxRange] = range;

    QRect textRect(x - innerCompass + directionOffset, y - innerCompass + directionOffset, innerCompass * 2 - 2 * directionOffset, innerCompass * 2 - 2 * directionOffset);

    bool isInRange = false;
    if (minRange > maxRange) {
      isInRange = bearingDeg >= minRange || bearingDeg <= maxRange;
    } else {
      isInRange = bearingDeg >= minRange && bearingDeg <= maxRange;
    }

    p.setOpacity(isInRange ? 1.0 : 0.2);
    p.setPen(QPen(color));
    p.drawText(textRect, alignmentFlag, direction);
  }
}

DistanceButton::DistanceButton(QWidget *parent) : QPushButton(parent), scene(uiState()->scene) {
  setFixedSize(btn_size * 1.5, btn_size * 1.5);

  profile_data = {
    {QPixmap("../frogpilot/assets/other_images/traffic.png"), "Traffic"},
    {QPixmap("../frogpilot/assets/other_images/aggressive.png"), "Aggressive"},
    {QPixmap("../frogpilot/assets/other_images/standard.png"), "Standard"},
    {QPixmap("../frogpilot/assets/other_images/relaxed.png"), "Relaxed"}
  };

  profile_data_kaofui = {
    {QPixmap("../frogpilot/assets/other_images/traffic_kaofui.png"), "Traffic"},
    {QPixmap("../frogpilot/assets/other_images/aggressive_kaofui.png"), "Aggressive"},
    {QPixmap("../frogpilot/assets/other_images/standard_kaofui.png"), "Standard"},
    {QPixmap("../frogpilot/assets/other_images/relaxed_kaofui.png"), "Relaxed"}
  };

  transitionTimer.start();

  connect(this, &QPushButton::pressed, this, &DistanceButton::buttonPressed);
  connect(this, &QPushButton::released, this, &DistanceButton::buttonReleased);
}

void DistanceButton::buttonPressed() {
  paramsMemory.putBool("OnroadDistanceButtonPressed", true);
}

void DistanceButton::buttonReleased() {
  paramsMemory.putBool("OnroadDistanceButtonPressed", false);
}

void DistanceButton::updateState() {
  bool stateChanged = (trafficModeActive != scene.traffic_mode_active) ||
                      (personality != static_cast<int>(scene.personality) && !trafficModeActive);

  if (stateChanged) {
    transitionTimer.restart();
  }

  personality = static_cast<int>(scene.personality);
  trafficModeActive = scene.traffic_mode_active;
}

void DistanceButton::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  constexpr qreal fadeDuration = 1000.0;
  constexpr qreal textDuration = 3000.0;
  int elapsed = transitionTimer.elapsed();

  qreal textOpacity = qBound(0.0, 1.0 - ((elapsed - textDuration) / fadeDuration), 1.0);
  qreal imageOpacity = qBound(0.0, (elapsed - textDuration) / fadeDuration, 1.0);

  int profile = trafficModeActive ? 0 : personality + 1;
  auto &[profileImage, profileText] = scene.use_kaofui_icons ? profile_data_kaofui[profile] : profile_data[profile];

  if (textOpacity > 0.0) {
    p.setOpacity(textOpacity);
    p.setFont(InterFont(40, QFont::Bold));
    p.setPen(Qt::white);
    QRect textRect(-25, 0, width(), height() + 95);
    p.drawText(textRect, Qt::AlignCenter, profileText);
  }

  if (imageOpacity > 0.0) {
    drawIcon(p, QPoint((btn_size / 2) * 1.25, btn_size / 2 + 95), profileImage, Qt::transparent, imageOpacity);
  }
}

void AnnotatedCameraWidget::drawLeadInfo(QPainter &p) {
  static QElapsedTimer timer;
  static bool isFiveSecondsPassed = false;
  static double maxAcceleration = 0.0;
  constexpr int maxAccelDuration = 5000;

  double acceleration = std::round(scene.acceleration * 100) / 100;
  int randomEvent = scene.current_random_event;

  if (acceleration > maxAcceleration && (status == STATUS_ENGAGED || status == STATUS_TRAFFIC_MODE_ACTIVE)) {
    maxAcceleration = acceleration;
    isFiveSecondsPassed = false;
    timer.start();
  } else if (randomEvent == 2 && maxAcceleration < 3.0) {
    maxAcceleration = 3.0;
    isFiveSecondsPassed = false;
    timer.start();
  } else if (randomEvent == 3 && maxAcceleration < 3.5) {
    maxAcceleration = 3.5;
    isFiveSecondsPassed = false;
    timer.start();
  } else if (randomEvent == 4 && maxAcceleration < 4.0) {
    maxAcceleration = 4.0;
    isFiveSecondsPassed = false;
    timer.start();
  } else {
    isFiveSecondsPassed = timer.hasExpired(maxAccelDuration);
  }

  auto createText = [&](const QString &title, const double data) {
    return title + QString::number(std::round(data * distanceConversion)) + " " + leadDistanceUnit;
  };

  QString accelText = QString(tr("Accel: %1%2"))
                      .arg(acceleration * accelerationConversion, 0, 'f', 2)
                      .arg(accelerationUnit);

  QString maxAccSuffix;
  if (!mapOpen) {
    maxAccSuffix = tr(" - Max: %1%2")
                      .arg(maxAcceleration * accelerationConversion, 0, 'f', 2)
                      .arg(accelerationUnit);
  }

  QString obstacleText = createText(mapOpen ? tr(" | Obstacle: ") : tr("  |  Obstacle Factor: "), obstacleDistance);
  QString stopText = createText(mapOpen ? tr(" - Stop: ") : tr("  -  Stop Factor: "), scene.stopped_equivalence);
  QString followText = " = " + createText(mapOpen ? tr("Follow: ") : tr("Follow Distance: "), scene.desired_follow);

  auto createDiffText = [&](const double data, const double stockData) {
    double difference = std::round((data - stockData) * distanceConversion);
    return difference != 0 ? QString(" (%1%2)").arg(difference > 0 ? "+" : "").arg(difference) : QString();
  };

  p.save();

  QRect insightsRect(rect().left() - 1, rect().top() - 60, rect().width() + 2, 100);
  p.setBrush(QColor(0, 0, 0, 150));
  p.drawRoundedRect(insightsRect, 30, 30);
  p.setFont(InterFont(28, QFont::Bold));
  p.setRenderHint(QPainter::TextAntialiasing);

  QRect adjustedRect = insightsRect.adjusted(0, 27, 0, 27);
  int textBaseLine = adjustedRect.y() + (adjustedRect.height() + p.fontMetrics().height()) / 2 - p.fontMetrics().descent();

  int totalTextWidth = p.fontMetrics().horizontalAdvance(accelText)
                     + p.fontMetrics().horizontalAdvance(maxAccSuffix)
                     + p.fontMetrics().horizontalAdvance(obstacleText)
                     + p.fontMetrics().horizontalAdvance(createDiffText(obstacleDistance, obstacleDistanceStock))
                     + p.fontMetrics().horizontalAdvance(stopText)
                     + p.fontMetrics().horizontalAdvance(followText);

  int textStartPos = adjustedRect.x() + (adjustedRect.width() - totalTextWidth) / 2;

  auto drawText = [&](const QString &text, const QColor &color) {
    p.setPen(color);
    p.drawText(textStartPos, textBaseLine, text);
    textStartPos += p.fontMetrics().horizontalAdvance(text);
  };

  drawText(accelText, Qt::white);
  if (!maxAccSuffix.isEmpty()) {
    drawText(maxAccSuffix, isFiveSecondsPassed ? Qt::white : redColor());
  }
  drawText(obstacleText, Qt::white);
  drawText(createDiffText(obstacleDistance, obstacleDistanceStock), (obstacleDistance - obstacleDistanceStock) > 0 ? Qt::green : Qt::red);
  drawText(stopText, Qt::white);
  drawText(followText, Qt::white);

  p.restore();
}

PedalIcons::PedalIcons(QWidget *parent) : QWidget(parent), scene(uiState()->scene) {
  setFixedSize(btn_size, btn_size);

  brake_pedal_img = loadPixmap("../frogpilot/assets/other_images/brake_pedal.png", QSize(img_size, img_size));
  gas_pedal_img = loadPixmap("../frogpilot/assets/other_images/gas_pedal.png", QSize(img_size, img_size));
}

void PedalIcons::updateState() {
  acceleration = scene.acceleration;

  accelerating = acceleration > 0.25;
  decelerating = acceleration < -0.25;

  if (accelerating || decelerating) {
    update();
  }
}

void PedalIcons::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  int totalWidth = 2 * img_size;
  int startX = (width() - totalWidth) / 2;

  int brakeX = startX + img_size / 2;
  int gasX = startX + img_size;

  float brakeOpacity = 1.0f;
  float gasOpacity = 1.0f;

  if (scene.dynamic_pedals_on_ui) {
    brakeOpacity = scene.standstill ? 1.0f : decelerating ? std::max(0.25f, std::abs(acceleration)) : 0.25f;
    gasOpacity = accelerating ? std::max(0.25f, acceleration) : 0.25f;
  } else if (scene.static_pedals_on_ui) {
    brakeOpacity = scene.standstill || scene.brake_lights_on || acceleration < -0.5 ? 1.0f : 0.25f;
    gasOpacity = !scene.standstill && acceleration > 0 ? 1.0f : 0.25f;
  }

  p.setOpacity(brakeOpacity);
  p.drawPixmap(brakeX, (height() - img_size) / 2, brake_pedal_img);

  p.setOpacity(gasOpacity);
  p.drawPixmap(gasX, (height() - img_size) / 2, gas_pedal_img);
}

void AnnotatedCameraWidget::drawSLCConfirmation(QPainter &p) {
  p.save();

  QSize size = this->size();

  QRect leftRect(0, 0, size.width() / 2, size.height());
  QRect rightRect = leftRect.translated(size.width() / 2, 0);

  p.setOpacity(0.5);
  p.fillRect(leftRect, rightHandDM ? redColor() : greenColor());
  p.fillRect(rightRect, rightHandDM ? greenColor() : redColor());
  p.setOpacity(1.0);

  p.setFont(InterFont(75, QFont::Bold));
  p.setPen(Qt::white);

  QString unitText = is_metric ? tr("kph") : tr("mph");
  QString speedText = QString::number(std::nearbyint(scene.unconfirmed_speed_limit * (is_metric ? MS_TO_KPH : MS_TO_MPH))) + " " + unitText;
  QString confirmText = tr("Confirm speed limit\n") + speedText;
  QString ignoreText = tr("Ignore speed limit\n") + speedText;

  QRect textLeftRect = QRect(leftRect.left(), leftRect.top() + leftRect.height() / 2 - 225, leftRect.width(), leftRect.height() / 2);
  QRect textRightRect = QRect(rightRect.left(), rightRect.top() + rightRect.height() / 2 - 225, rightRect.width(), rightRect.height() / 2);

  p.drawText(textLeftRect, Qt::AlignCenter, rightHandDM ? ignoreText : confirmText);
  p.drawText(textRightRect, Qt::AlignCenter, rightHandDM ? confirmText : ignoreText);

  p.restore();
}

void AnnotatedCameraWidget::drawStatusBar(QPainter &p) {
  p.save();

  static bool displayStatusText = false;

  constexpr qreal fadeDuration = 1500.0;
  constexpr qreal textDuration = 5000.0;

  static QElapsedTimer timer;
  static QString lastShownStatus;

  QString newStatus;

  QRect currentRect = rect();
  QRect statusBarRect(currentRect.left() - 1, currentRect.bottom() - 50, currentRect.width() + 2, 100);

  p.setBrush(QColor(0, 0, 0, 150));
  p.setOpacity(1.0);
  p.drawRoundedRect(statusBarRect, 30, 30);

  std::map<int, QString> conditionalStatusMap = {
    {0, tr("Conditional Experimental Mode ready")},
    {1, tr("Conditional Experimental overridden")},
    {2, tr("Experimental Mode manually activated")},
    {3, tr("Conditional Experimental overridden")},
    {4, tr("Experimental Mode manually activated")},
    {5, tr("Conditional Experimental overridden")},
    {6, tr("Experimental Mode manually activated")},
    {7, tr("Experimental Mode activated for") + (mapOpen ? tr(" intersection") : tr(" upcoming intersection"))},
    {8, tr("Experimental Mode activated for") + (mapOpen ? tr(" turn") : tr(" upcoming turn"))},
    {9, tr("Experimental Mode activated due to") + (mapOpen ? tr(" SLC") : tr(" no speed limit set"))},
    {10, tr("Experimental Mode activated due to") + (mapOpen ? tr(" speed") : tr(" speed being less than ") + QString::number(scene.conditional_speed_lead) + (is_metric ? tr(" kph") : tr(" mph")))},
    {11, tr("Experimental Mode activated due to") + (mapOpen ? tr(" speed") : tr(" speed being less than ") + QString::number(scene.conditional_speed) + (is_metric ? tr(" kph") : tr(" mph")))},
    {12, tr("Experimental Mode activated for stopped lead")},
    {13, tr("Experimental Mode activated for slower lead")},
    {14, tr("Experimental Mode activated for turn") + (mapOpen ? "" : tr(" / lane change"))},
    {15, tr("Experimental Mode activated for curve")},
    {16, tr("Experimental Mode activated for stop") + (mapOpen ? "" : tr(" sign / stop light"))},
  };

  QString roadName = roadNameUI ? QString::fromStdString(paramsMemory.get("RoadName")) : QString();

  if (alwaysOnLateralActive && showAlwaysOnLateralStatusBar) {
    newStatus = tr("Always On Lateral active") + (mapOpen ? "" : tr(". Press the \"Cruise Control\" button to disable"));
  } else if (showConditionalExperimentalStatusBar) {
    newStatus = conditionalStatusMap[status != STATUS_DISENGAGED ? conditionalStatus : 0];
  }

  QString distanceSuffix = tr(". Long press the \"distance\" button to revert");
  QString lkasSuffix = tr(". Double press the \"LKAS\" button to revert");
  QString screenSuffix = tr(". Double tap the screen to revert");

  if (!alwaysOnLateralActive && !mapOpen && status != STATUS_DISENGAGED && !newStatus.isEmpty()) {
    if (conditionalStatus == 1 || conditionalStatus == 2) {
      newStatus += distanceSuffix;
    } else if (conditionalStatus == 3 || conditionalStatus == 4) {
      newStatus += lkasSuffix;
    } else if (conditionalStatus == 5 || conditionalStatus == 6) {
      newStatus += screenSuffix;
    }
  }

  if (newStatus != lastShownStatus || roadName.isEmpty()) {
    displayStatusText = true;
    lastShownStatus = newStatus;
    timer.restart();
  } else if (displayStatusText && timer.hasExpired(textDuration + fadeDuration)) {
    displayStatusText = false;
  }

  p.setFont(InterFont(40, QFont::Bold));
  p.setPen(Qt::white);
  p.setRenderHint(QPainter::TextAntialiasing);

  static qreal roadNameOpacity;
  static qreal statusTextOpacity;
  int elapsed = timer.elapsed();
  if (displayStatusText) {
    statusTextOpacity = qBound(0.0, 1.0 - (elapsed - textDuration) / fadeDuration, 1.0);
    roadNameOpacity = 1.0 - statusTextOpacity;
  } else {
    roadNameOpacity = qBound(0.0, elapsed / fadeDuration, 1.0);
    statusTextOpacity = 0.0;
  }

  p.setOpacity(statusTextOpacity);
  QRect textRect = p.fontMetrics().boundingRect(statusBarRect, Qt::AlignCenter | Qt::TextWordWrap, newStatus);
  textRect.moveBottom(statusBarRect.bottom() - 50);
  p.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, newStatus);

  if (!roadName.isEmpty()) {
    p.setOpacity(roadNameOpacity);
    textRect = p.fontMetrics().boundingRect(statusBarRect, Qt::AlignCenter | Qt::TextWordWrap, roadName);
    textRect.moveBottom(statusBarRect.bottom() - 50);
    p.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, roadName);
  }

  p.restore();
}

void AnnotatedCameraWidget::drawTurnSignals(QPainter &p) {
  constexpr int signalHeight = 480;
  constexpr int signalWidth = 360;

  p.setRenderHint(QPainter::Antialiasing);

  int baseYPosition = (height() - signalHeight) / 2 + (showAlwaysOnLateralStatusBar || showConditionalExperimentalStatusBar || roadNameUI ? 225 : 300) - alertSize;
  int leftSignalXPosition = 75 + width() - signalWidth - 300 * (blindSpotLeft ? 0 : animationFrameIndex);
  int rightSignalXPosition = -75 + 300 * (blindSpotRight ? 0 : animationFrameIndex);

  if (animationFrameIndex < signalImgVector.size()) {
    auto drawSignal = [&](bool signalActivated, int xPosition, bool flip, bool blindspot) {
      if (signalActivated) {
        int uniqueImages = signalImgVector.size() / 4;
        int index = (blindspot ? 2 * uniqueImages : 2 * animationFrameIndex % totalFrames) + (flip ? 1 : 0);
        QPixmap &signal = signalImgVector[index];
        p.drawPixmap(xPosition, baseYPosition, signalWidth, signalHeight, signal);
      }
    };

    drawSignal(turnSignalLeft, leftSignalXPosition, false, blindSpotLeft);
    drawSignal(turnSignalRight, rightSignalXPosition, true, blindSpotRight);
  }
}
