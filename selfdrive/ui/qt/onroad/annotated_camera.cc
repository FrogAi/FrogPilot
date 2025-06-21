
#include "selfdrive/ui/qt/onroad/annotated_camera.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

#include "common/swaglog.h"
#include "selfdrive/ui/qt/onroad/buttons.h"
#include "selfdrive/ui/qt/util.h"

// Window that shows camera view and variety of info drawn on top
AnnotatedCameraWidget::AnnotatedCameraWidget(VisionStreamType type, QWidget* parent) : fps_filter(UI_FREQ, 3, 1. / UI_FREQ), CameraWidget("camerad", type, true, parent) {
  pm = std::make_unique<PubMaster, const std::initializer_list<const char *>>({"uiDebug"});

  main_layout = new QVBoxLayout(this);
  main_layout->setMargin(UI_BORDER_SIZE);
  main_layout->setSpacing(0);

  experimental_btn = new ExperimentalButton(this);
  main_layout->addWidget(experimental_btn, 0, Qt::AlignTop | Qt::AlignRight);

  map_settings_btn = new MapSettingsButton(this);
  main_layout->addWidget(map_settings_btn, 0, Qt::AlignBottom | Qt::AlignRight);

  dm_img = loadPixmap("../assets/img_driver_face.png", {img_size + 5, img_size + 5});

  // FrogPilot variables
  frogpilot_nvg = new FrogPilotAnnotatedCameraWidget(this);
  frogpilot_nvg->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  distance_btn = new DistanceButton(this);
  screen_recorder = new ScreenRecorder(this);

  distance_btn->setVisible(false);
}

void AnnotatedCameraWidget::resizeEvent(QResizeEvent *event) {
  CameraWidget::resizeEvent(event);

  frogpilot_nvg->setGeometry(rect());

  screen_recorder->move(experimental_btn->x() - UI_BORDER_SIZE - btn_size, experimental_btn->y());
}

void AnnotatedCameraWidget::updateState(const UIState &s, const FrogPilotUIState &fs) {
  const int SET_SPEED_NA = 255;
  const SubMaster &sm = *(s.sm);
  const SubMaster &fpsm = *(fs.sm);
  const QJsonObject &frogpilot_toggles = fs.frogpilot_toggles;

  const bool cs_alive = sm.alive("controlsState");
  const bool nav_alive = sm.alive("navInstruction") && sm["navInstruction"].getValid();
  const auto cs = sm["controlsState"].getControlsState();
  const auto car_state = sm["carState"].getCarState();
  const auto nav_instruction = sm["navInstruction"].getNavInstruction();

  const cereal::FrogPilotPlan::Reader &frogpilotPlan = fpsm["frogpilotPlan"].getFrogpilotPlan();

  // Handle older routes where vCruiseCluster is not set
  float v_cruise = cs.getVCruiseCluster() == 0.0 ? cs.getVCruise() : cs.getVCruiseCluster();
  setSpeed = cs_alive ? v_cruise : SET_SPEED_NA;
  is_cruise_set = setSpeed > 0 && (int)setSpeed != SET_SPEED_NA;
  if (is_cruise_set && !s.scene.is_metric) {
    setSpeed *= KM_TO_MILE;
  }

  // Handle older routes where vEgoCluster is not set
  v_ego_cluster_seen = v_ego_cluster_seen || car_state.getVEgoCluster() != 0.0;
  float v_ego = v_ego_cluster_seen && !frogpilot_toggles.value("use_wheel_speed").toBool() ? car_state.getVEgoCluster() : car_state.getVEgo();
  speed = cs_alive ? std::max<float>(0.0, v_ego) : 0.0;
  speed *= s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH;

  auto speed_limit_sign = nav_instruction.getSpeedLimitSign();
  if (frogpilot_toggles.value("show_speed_limits").toBool() || frogpilot_toggles.value("speed_limit_controller").toBool()) {
    speedLimit = frogpilotPlan.getSlcOverriddenSpeed() != 0 ? frogpilotPlan.getSlcOverriddenSpeed() : frogpilotPlan.getSlcSpeedLimit();
    if (frogpilotPlan.getSlcOverriddenSpeed() == 0 && !frogpilot_toggles.value("show_speed_limit_offset").toBool()) {
      speedLimit += frogpilotPlan.getSlcSpeedLimitOffset();
    }
  } else {
    speedLimit = nav_alive ? nav_instruction.getSpeedLimit() : 0.0;
  }
  speedLimit *= (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH);

  has_us_speed_limit = (nav_alive && speed_limit_sign == cereal::NavInstruction::SpeedLimitSign::MUTCD);
  has_us_speed_limit |= frogpilot_toggles.value("show_speed_limits").toBool() || frogpilot_toggles.value("speed_limit_controller").toBool();
  has_us_speed_limit &= !frogpilot_toggles.value("speed_limit_vienna").toBool();
  has_us_speed_limit &= !frogpilot_toggles.value("hide_speed_limit").toBool();
  has_eu_speed_limit = (nav_alive && speed_limit_sign == cereal::NavInstruction::SpeedLimitSign::VIENNA);
  has_eu_speed_limit |= (frogpilot_toggles.value("show_speed_limits").toBool() || frogpilot_toggles.value("speed_limit_controller").toBool()) && frogpilot_toggles.value("speed_limit_vienna").toBool();
  has_eu_speed_limit &= !frogpilot_toggles.value("hide_speed_limit").toBool();
  is_metric = s.scene.is_metric;
  speedUnit =  s.scene.is_metric ? tr("km/h") : tr("mph");
  hideBottomIcons = (cs.getAlertSize() != cereal::ControlsState::AlertSize::NONE);
  hideBottomIcons |= (frogpilot_nvg->signalStyle == "traditional" || frogpilot_nvg->signalStyle == "traditional_gif") && (car_state.getLeftBlinker() || car_state.getRightBlinker());
  status = s.status;

  // update engageability/experimental mode button
  experimental_btn->updateState(s, fs, frogpilot_toggles);

  // update DM icon
  auto dm_state = sm["driverMonitoringState"].getDriverMonitoringState();
  dmActive = dm_state.getIsActiveMode();
  rightHandDM = dm_state.getIsRHD();
  // DM icon transition
  dm_fade_state = std::clamp(dm_fade_state+0.2*(0.5-dmActive), 0.0, 1.0);

  // hide map settings button for alerts and flip for right hand DM
  map_settings_btn->road_name_ui = frogpilot_toggles.value("road_name_ui").toBool();
  if (map_settings_btn->isEnabled()) {
    map_settings_btn->setVisible(!hideBottomIcons && !frogpilot_toggles.value("hide_map_icon").toBool());
    main_layout->setAlignment(map_settings_btn, (rightHandDM ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignBottom);
  }

  // FrogPilot variables
  distance_btn->setEnabled(frogpilot_nvg->dmIconPosition != QPoint(0, 0) && !hideBottomIcons && frogpilot_toggles.value("onroad_distance_button").toBool());
  distance_btn->setVisible(distance_btn->isEnabled());
  if (distance_btn->isEnabled()) {
    distance_btn->move(rightHandDM ? width() - UI_BORDER_SIZE - distance_btn->width() - (UI_BORDER_SIZE / 2) : UI_BORDER_SIZE, frogpilot_nvg->dmIconPosition.y() - distance_btn->height() / 2);
    distance_btn->updateState(s.scene, fs.frogpilot_scene);
  }
  screen_recorder->setVisible(frogpilot_nvg->standstillDuration == 0 && !fs.frogpilot_scene.map_open && !(frogpilot_nvg->signalStyle == "static" && car_state.getRightBlinker()) && frogpilot_toggles.value("screen_recorder").toBool());

  frogpilot_nvg->updateState(fs, frogpilot_toggles);
}

void AnnotatedCameraWidget::drawHud(QPainter &p, const cereal::FrogPilotPlan::Reader &frogpilotPlan, const FrogPilotUIState &fs, const QJsonObject &frogpilot_toggles) {
  p.save();

  // Header gradient
  QLinearGradient bg(0, UI_HEADER_HEIGHT - (UI_HEADER_HEIGHT / 2.5), 0, UI_HEADER_HEIGHT);
  bg.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.45));
  bg.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));
  p.fillRect(0, 0, width(), UI_HEADER_HEIGHT, bg);

  QString speedLimitStr = (speedLimit > 1) ? QString::number(std::nearbyint(speedLimit)) : "–";
  QString speedStr = QString::number(std::nearbyint(speed));
  QString setSpeedStr = is_cruise_set ? QString::number(std::nearbyint(setSpeed)) : "–";

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
  if (!frogpilot_toggles.value("hide_max_speed").toBool()) {
    if (fs.frogpilot_scene.traffic_mode_enabled) {
      p.setPen(QPen(redColor(), 10));
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
  }

  const QRect sign_rect = set_speed_rect.adjusted(sign_margin, default_size.height(), -sign_margin, -sign_margin);
  // US/Canada (MUTCD style) sign
  if (has_us_speed_limit) {
    p.setPen(Qt::NoPen);
    p.setBrush(whiteColor());
    p.drawRoundedRect(sign_rect, 24, 24);
    p.setPen(QPen(blackColor(), 6));
    p.drawRoundedRect(sign_rect.adjusted(9, 9, -9, -9), 16, 16);

    p.save();
    p.setOpacity(frogpilotPlan.getSlcOverriddenSpeed() == 0 ? 1.0 : 0.25);
    if (frogpilotPlan.getSlcOverriddenSpeed() == 0 && frogpilot_toggles.value("show_speed_limit_offset").toBool()) {
      p.setFont(InterFont(28, QFont::DemiBold));
      p.drawText(sign_rect.adjusted(0, 22, 0, 0), Qt::AlignTop | Qt::AlignHCenter, tr("LIMIT"));
      p.setFont(InterFont(70, QFont::Bold));
      p.drawText(sign_rect.adjusted(0, 51, 0, 0), Qt::AlignTop | Qt::AlignHCenter, speedLimitStr);
      p.setFont(InterFont(50, QFont::DemiBold));
      p.drawText(sign_rect.adjusted(0, 120, 0, 0), Qt::AlignTop | Qt::AlignHCenter, frogpilot_nvg->speedLimitOffsetStr);
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
    p.setOpacity(frogpilotPlan.getSlcOverriddenSpeed() == 0 ? 1.0 : 0.25);
    p.setPen(blackColor());
    if (frogpilot_toggles.value("show_speed_limit_offset").toBool()) {
      p.setFont(InterFont((speedLimitStr.size() >= 3) ? 60 : 70, QFont::Bold));
      p.drawText(sign_rect.adjusted(0, -25, 0, 0), Qt::AlignCenter, speedLimitStr);
      p.setFont(InterFont(40, QFont::DemiBold));
      p.drawText(sign_rect.adjusted(0, 100, 0, 0), Qt::AlignTop | Qt::AlignHCenter, frogpilot_nvg->speedLimitOffsetStr);
    } else {
      p.setFont(InterFont((speedLimitStr.size() >= 3) ? 60 : 70, QFont::Bold));
      p.drawText(sign_rect, Qt::AlignCenter, speedLimitStr);
    }
    p.restore();
  }

  // current speed
  if (!frogpilot_nvg->bigMapOpen && frogpilot_nvg->standstillDuration == 0 && !frogpilot_toggles.value("hide_speed").toBool()) {
    p.setFont(InterFont(176, QFont::Bold));
    drawText(p, rect().center().x(), 210, speedStr);
    p.setFont(InterFont(66));
    drawText(p, rect().center().x(), 290, speedUnit, 200);
  }

  p.restore();

  // FrogPilot variables
  frogpilot_nvg->defaultSize = default_size;
  frogpilot_nvg->experimentalButtonPosition = QPoint(experimental_btn->x(), experimental_btn->y());
  frogpilot_nvg->hideBottomIcons = hideBottomIcons;
  frogpilot_nvg->isCruiseSet = is_cruise_set;
  frogpilot_nvg->mapButtonVisible = map_settings_btn->isVisible();
  frogpilot_nvg->mutcdSpeedLimit = has_us_speed_limit;
  frogpilot_nvg->rightHandDM = rightHandDM;
  frogpilot_nvg->setSpeed = setSpeed / (is_metric ? MS_TO_KPH : MS_TO_MPH);
  frogpilot_nvg->setSpeedRect = set_speed_rect;
  frogpilot_nvg->signMargin = sign_margin;
  frogpilot_nvg->speed = speed;
  frogpilot_nvg->speedLimitRect = sign_rect;
  frogpilot_nvg->speedUnit = speedUnit;
  frogpilot_nvg->viennaSpeedLimit = has_eu_speed_limit;
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

void AnnotatedCameraWidget::drawLaneLines(QPainter &painter, const UIState *s, const FrogPilotUIState *fs) {
  painter.save();

  const UIScene &scene = s->scene;
  const FrogPilotUIScene &frogpilot_scene = fs->frogpilot_scene;
  const QJsonObject &frogpilot_toggles = fs->frogpilot_toggles;
  SubMaster &sm = *(s->sm);
  SubMaster &fpsm = *(fs->sm);

  // lanelines
  for (int i = 0; i < std::size(scene.lane_line_vertices); ++i) {
    if (frogpilot_scene.use_stock_colors) {
      painter.setBrush(QColor::fromRgbF(1.0, 1.0, 1.0, std::clamp<float>(scene.lane_line_probs[i], 0.0, 0.7)));
    } else {
      QColor lane_color = QColor(frogpilot_scene.lane_lines_color);
      lane_color.setAlphaF(lane_color.alphaF() * std::clamp(scene.lane_line_probs[i], 0.0f, 1.0f));
      painter.setBrush(lane_color);
    }
    painter.drawPolygon(scene.lane_line_vertices[i]);
  }

  // road edges
  for (int i = 0; i < std::size(scene.road_edge_vertices); ++i) {
    painter.setBrush(QColor::fromRgbF(1.0, 0, 0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0)));
    painter.drawPolygon(scene.road_edge_vertices[i]);
  }

  // paint path
  QLinearGradient bg(0, height(), 0, 0);
  if (sm["controlsState"].getControlsState().getExperimentalMode() || frogpilot_toggles.value("acceleration_path").toBool() || frogpilot_toggles.value("rainbow_path").toBool()) {
    // The first half of track_vertices are the points for the right side of the path
    // and the indices match the positions of accel from uiPlan
    const auto &acceleration = sm["uiPlan"].getUiPlan().getAccel();
    const int max_len = std::min<int>(scene.track_vertices.length() / 2, acceleration.size());

    for (int i = 0; i < max_len; ++i) {
      // Some points are out of frame
      int track_idx = max_len - i - 1;  // flip idx to start from bottom right
      if (scene.track_vertices[track_idx].y() < 0 || scene.track_vertices[track_idx].y() > height()) continue;

      // Flip so 0 is bottom of frame
      float lin_grad_point = (height() - scene.track_vertices[track_idx].y()) / height();

      if ((fabs(acceleration[i]) < 0.25 || !frogpilot_toggles.value("acceleration_path").toBool()) && frogpilot_toggles.value("rainbow_path").toBool()) {
        frogpilot_nvg->paintRainbowPath(painter, bg, lin_grad_point, sm);
      } else if (fabs(acceleration[i]) < 0.25 && !frogpilot_scene.use_stock_colors) {
        QColor color = frogpilot_scene.path_color;
        color.setAlphaF(util::map_val(lin_grad_point, 0.0f, 1.0f, 1.0f, 0.1f));
        bg.setColorAt(lin_grad_point, color);
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

  } else if (!frogpilot_scene.use_stock_colors) {
    QColor color = frogpilot_scene.path_color;
    bg.setColorAt(0.0f, color);
    color.setAlphaF(1.0f);
    bg.setColorAt(0.5f, color);
    color.setAlphaF(0.1f);
    bg.setColorAt(1.0f, color);

  } else {
    bg.setColorAt(0.0, QColor::fromHslF(148 / 360., 0.94, 0.51, 0.4));
    bg.setColorAt(0.5, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.35));
    bg.setColorAt(1.0, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.0));
  }

  painter.setBrush(bg);
  painter.drawPolygon(scene.track_vertices);

  // paint path edges
  if (frogpilot_toggles.value("adjacent_path_metrics").toBool() || frogpilot_toggles.value("adjacent_paths").toBool()) {
    frogpilot_nvg->paintAdjacentPaths(painter, sm["carState"].getCarState(), frogpilot_scene, frogpilot_toggles);
  } else if ((sm["carState"].getCarState().getLeftBlindspot() || sm["carState"].getCarState().getRightBlindspot()) && frogpilot_toggles.value("blind_spot_path").toBool()) {
    frogpilot_nvg->paintBlindSpotPath(painter, sm["carState"].getCarState(), frogpilot_scene);
  }
  frogpilot_nvg->paintPathEdges(painter, fpsm["navInstruction"].getNavInstruction(), scene, frogpilot_scene, sm);

  painter.restore();
}

void AnnotatedCameraWidget::drawDriverState(QPainter &painter, const UIState *s, const QJsonObject &frogpilot_toggles) {
  const UIScene &scene = s->scene;

  painter.save();

  // base icon
  int offset = UI_BORDER_SIZE + btn_size / 2;
  int x = rightHandDM ? width() - offset : offset;
  if (distance_btn->isEnabled()) {
    if (rightHandDM) {
      x -= UI_BORDER_SIZE + distance_btn->width() + UI_BORDER_SIZE;
    } else {
      x += UI_BORDER_SIZE + distance_btn->width() + UI_BORDER_SIZE;
    }
  }
  if (frogpilot_toggles.value("road_name_ui").toBool()) {
    offset += UI_BORDER_SIZE;
  }
  int y = height() - offset;
  frogpilot_nvg->dmIconPosition.setX(x);
  frogpilot_nvg->dmIconPosition.setY(y);
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

void AnnotatedCameraWidget::drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const cereal::FrogPilotPlan::Reader &frogpilotPlan, const QPointF &vd, const QColor &marker_color, const FrogPilotUIState *fs, bool adjacent) {
  painter.save();

  const float speedBuff = 10.;
  const float leadBuff = std::max((speed / frogpilot_nvg->speedConversion) * 10.0f, 40.0f);
  const float d_rel = lead_data.getDRel() + (adjacent ? fabs(lead_data.getYRel()) : 0);
  const float v_rel = lead_data.getVRel();

  float fillAlpha = 0;
  if (frogpilotPlan.getTrackingLead() || adjacent) {
    fillAlpha = 255 * (1.0 - (d_rel / leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255 * (-1 * (v_rel / speedBuff));
    }
    fillAlpha = std::clamp(fillAlpha, 0.f, 255.f);
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp((float)vd.x(), 0.f, width() - sz / 2);
  float y = std::fmin(height() - sz * .6, (float)vd.y());

  float g_xo = sz / 5;
  float g_yo = sz / 10;

  QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_yo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};
  if (lead_data.getFarLead()) {
    painter.setBrush(QColor(0, 255, 255, 255));
  } else {
    painter.setBrush(QColor(218, 202, 37, 255));
  }
  painter.drawPolygon(glow, std::size(glow));

  // chevron
  QPointF chevron[] = {{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz}};
  if (!adjacent && fs->frogpilot_scene.use_stock_colors) {
    painter.setBrush(redColor(fillAlpha));
  } else {
    painter.setBrush(QColor(marker_color.red(), marker_color.green(), marker_color.blue(), fillAlpha));
  }
  painter.drawPolygon(chevron, std::size(chevron));

  if (fs->frogpilot_toggles.value("lead_metrics").toBool()) {
    frogpilot_nvg->paintLeadMetrics(painter, adjacent, chevron, frogpilotPlan, lead_data);
  }

  painter.restore();
}

void AnnotatedCameraWidget::paintGL() {
}

void AnnotatedCameraWidget::paintEvent(QPaintEvent *event) {
  UIState *s = uiState();
  FrogPilotUIState *fs = frogpilotUIState();
  QJsonObject &frogpilot_toggles = fs->frogpilot_toggles;
  QPainter painter(this);
  SubMaster &sm = *(s->sm);
  SubMaster &fpsm = *(fs->sm);
  const double start_draw_t = millis_since_boot();
  const cereal::ModelDataV2::Reader &model = sm["modelV2"].getModelV2();
  const cereal::FrogPilotPlan::Reader &frogpilotPlan = fpsm["frogpilotPlan"].getFrogpilotPlan();

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
    if (has_wide_cam && frogpilot_toggles.value("camera_view").toInt() == 0) {
      float v_ego = sm["carState"].getCarState().getVEgo();
      if ((v_ego < 10) || available_streams.size() == 1) {
        wide_cam_requested = true;
      } else if (v_ego > 15) {
        wide_cam_requested = false;
      }
      wide_cam_requested = wide_cam_requested && sm["controlsState"].getControlsState().getExperimentalMode();
      // for replay of old routes, never go to widecam
      wide_cam_requested = wide_cam_requested && s->scene.calibration_wide_valid;
    }
    CameraWidget::setStreamType(frogpilot_toggles.value("camera_view").toInt() == 1 ? VISION_STREAM_DRIVER :
                                frogpilot_toggles.value("camera_view").toInt() == 3 || wide_cam_requested ? VISION_STREAM_WIDE_ROAD :
                                VISION_STREAM_ROAD);

    s->scene.wide_cam = CameraWidget::getStreamType() == VISION_STREAM_WIDE_ROAD;
    if (s->scene.calibration_valid) {
      auto calib = s->scene.wide_cam ? s->scene.view_from_wide_calib : s->scene.view_from_calib;
      CameraWidget::updateCalibration(calib);
    } else {
      CameraWidget::updateCalibration(DEFAULT_CALIBRATION);
    }
    painter.beginNativePainting();
    CameraWidget::setFrameId(model.getFrameId());
    CameraWidget::paintGL();
    painter.endNativePainting();
  }

  painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
  painter.setPen(Qt::NoPen);

  if (s->scene.world_objects_visible) {
    update_model(s, fs, model, sm["uiPlan"].getUiPlan(), frogpilot_toggles);
    drawLaneLines(painter, s, fs);

    if (s->scene.longitudinal_control && sm.rcv_frame("radarState") > s->scene.started_frame && !frogpilot_toggles.value("hide_lead_marker").toBool()) {
      auto radar_state = sm["radarState"].getRadarState();
      update_leads(s, radar_state, model.getPosition());
      auto lead_one = radar_state.getLeadOne();
      auto lead_two = radar_state.getLeadTwo();
      auto lead_left = radar_state.getLeadLeft();
      auto lead_right = radar_state.getLeadRight();
      if (lead_left.getStatus()) {
        drawLead(painter, lead_left, frogpilotPlan, s->scene.lead_vertices[2], frogpilot_nvg->blueColor(), fs, true);
      }
      if (lead_right.getStatus()) {
        drawLead(painter, lead_right, frogpilotPlan, s->scene.lead_vertices[3], frogpilot_nvg->purpleColor(), fs, true);
      }
      if (lead_one.getStatus()) {
        drawLead(painter, lead_one, frogpilotPlan, s->scene.lead_vertices[0], fs->frogpilot_scene.lead_marker_color, fs);
      } else {
        frogpilot_nvg->leadTextRect = QRect();
      }
      if (lead_two.getStatus() && (std::abs(lead_one.getDRel() - lead_two.getDRel()) > 3.0)) {
        drawLead(painter, lead_two, frogpilotPlan, s->scene.lead_vertices[1], fs->frogpilot_scene.lead_marker_color, fs);
      }
    }
  }

  // DMoji
  if (!hideBottomIcons && (sm.rcv_frame("driverStateV2") > s->scene.started_frame)) {
    update_dmonitoring(s, sm["driverStateV2"].getDriverStateV2(), dm_fade_state, rightHandDM);
    drawDriverState(painter, s, frogpilot_toggles);
  }

  drawHud(painter, frogpilotPlan, *fs, frogpilot_toggles);

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

  // FrogPilot variables
  if (s->scene.world_objects_visible) {
    frogpilot_nvg->paintFrogPilotWidgets(painter, *s, *fs, sm, fpsm, frogpilot_toggles);
  }
}

void AnnotatedCameraWidget::showEvent(QShowEvent *event) {
  CameraWidget::showEvent(event);

  ui_update_params(uiState());
  prev_draw_t = millis_since_boot();
}
