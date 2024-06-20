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

  if (s.scene.map_on_left) {
    split->setDirection(QBoxLayout::LeftToRight);
  } else {
    split->setDirection(QBoxLayout::RightToLeft);
  }

  alerts->updateState(s);
  nvg->updateState(s);

  bool shouldUpdate = false;

  QColor bgColor = bg_colors[s.status];
  if (bg != bgColor) {
    // repaint border
    bg = bgColor;
    shouldUpdate = true;
  }

  // FrogPilot variables
  const UIScene &scene = s.scene;

  blindSpotLeft = scene.blind_spot_left;
  blindSpotRight = scene.blind_spot_right;
  showBlindspot = scene.show_blind_spot && (blindSpotLeft || blindSpotRight);
  showSteering = scene.show_steering;
  steer = scene.steer;
  steeringAngleDeg = scene.steering_angle_deg;

  if (showBlindspot || showSteering) {
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
    QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
    gradient.setColorAt(0.0, bg_colors[STATUS_TRAFFIC_MODE_ACTIVE]);
    gradient.setColorAt(0.15, bg_colors[STATUS_EXPERIMENTAL_MODE_ACTIVE]);
    gradient.setColorAt(0.5, bg_colors[STATUS_CONDITIONAL_OVERRIDDEN]);
    gradient.setColorAt(0.85, bg_colors[STATUS_ENGAGED]);
    gradient.setColorAt(1.0, bg_colors[STATUS_ENGAGED]);

    QBrush brush(gradient);
    int fillWidth = UI_BORDER_SIZE;

    steer = 0.10 * std::abs(steer) + 0.90 * steer;
    int visibleHeight = rect.height() * steer;

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
}
