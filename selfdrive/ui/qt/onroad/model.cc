#include "selfdrive/ui/qt/onroad/model.h"

constexpr int CLIP_MARGIN = 500;

static int get_path_length_idx(const cereal::XYZTData::Reader &line, const float path_height) {
  const auto &line_x = line.getX();
  int max_idx = 0;
  for (int i = 1; i < line_x.size() && line_x[i] <= path_height; ++i) {
    max_idx = i;
  }
  return max_idx;
}

void ModelRenderer::draw(QPainter &painter, const QRect &surface_rect) {
  auto *s = uiState();
  auto &sm = *(s->sm);
  // Check if data is up-to-date
  if (sm.rcv_frame("liveCalibration") < s->scene.started_frame ||
      sm.rcv_frame("modelV2") < s->scene.started_frame) {
    return;
  }

  clip_region = surface_rect.adjusted(-CLIP_MARGIN, -CLIP_MARGIN, CLIP_MARGIN, CLIP_MARGIN);
  experimental_mode = sm["selfdriveState"].getSelfdriveState().getExperimentalMode();
  longitudinal_control = sm["carParams"].getCarParams().getOpenpilotLongitudinalControl();
  path_offset_z = sm["liveCalibration"].getLiveCalibration().getHeight()[0];

  painter.save();

  const auto &model = sm["modelV2"].getModelV2();
  const auto &radar_state = sm["radarState"].getRadarState();
  const auto &lead_one = radar_state.getLeadOne();

  update_model(model, lead_one, surface_rect.height());
  drawLaneLines(painter);
  drawPath(painter, model, surface_rect.height());

  if ((longitudinal_control || frogpilot_toggles.value("lead_info").toBool()) && sm.alive("radarState") && !frogpilot_toggles.value("hide_lead_marker").toBool()) {
    update_leads(radar_state, model.getPosition());
    const auto &lead_two = radar_state.getLeadTwo();
    frogpilot_nvg->leadTextRects.clear();
    if (lead_one.getStatus()) {
      if (lead_one.getModelProb() >= frogpilot_toggles.value("lead_detection_probability").toDouble()) {
        drawLead(painter, lead_one, lead_vertices[0], surface_rect, QColor(frogpilot_toggles.value("lead_marker_color").toString()));
      } else {
        drawLead(painter, lead_one, lead_vertices[0], surface_rect, frogpilot_nvg->whiteColor());
      }
    }
    if (lead_two.getStatus() && (std::abs(lead_one.getDRel() - lead_two.getDRel()) > 3.0)) {
      drawLead(painter, lead_two, lead_vertices[1], surface_rect, QColor(frogpilot_toggles.value("lead_marker_color").toString()));
    }

    // FrogPilot variables
    SubMaster &fpsm = *(frogpilotUIState()->sm);
    const cereal::FrogPilotRadarState::Reader &frogpilot_radar_state = fpsm["frogpilotRadarState"].getFrogpilotRadarState();

    const cereal::FrogPilotRadarState::LeadData::Reader &lead_left = frogpilot_radar_state.getLeadLeft();
    const cereal::FrogPilotRadarState::LeadData::Reader &lead_right = frogpilot_radar_state.getLeadRight();

    updateAdjacentLeads(frogpilot_radar_state, model.getPosition());

    frogpilot_nvg->adjacentLeadTextRect = QRect();

    if (lead_left.getStatus() && lead_right.getStatus() && (lead_left.getDRel() < lead_right.getDRel())) {
      drawLead(painter, reinterpret_cast<const cereal::RadarState::LeadData::Reader&>(lead_left), adjacent_lead_vertices[0], surface_rect, frogpilot_nvg->blueColor(), true);
      drawLead(painter, reinterpret_cast<const cereal::RadarState::LeadData::Reader&>(lead_right), adjacent_lead_vertices[1], surface_rect, frogpilot_nvg->purpleColor(), true);
    } else {
      if (lead_left.getStatus()) {
        drawLead(painter, reinterpret_cast<const cereal::RadarState::LeadData::Reader&>(lead_left), adjacent_lead_vertices[0], surface_rect, frogpilot_nvg->blueColor(), true);
      }
      if (lead_right.getStatus()) {
        drawLead(painter, reinterpret_cast<const cereal::RadarState::LeadData::Reader&>(lead_right), adjacent_lead_vertices[1], surface_rect, frogpilot_nvg->purpleColor(), true);
      }
    }
  }

  // FrogPilot variables
  if (frogpilot_toggles.value("radar_tracks").toBool()) {
    updateRadarTracks(model.getPosition());
  }

  painter.restore();
}

void ModelRenderer::update_leads(const cereal::RadarState::Reader &radar_state, const cereal::XYZTData::Reader &line) {
  for (int i = 0; i < 2; ++i) {
    const auto &lead_data = (i == 0) ? radar_state.getLeadOne() : radar_state.getLeadTwo();
    if (lead_data.getStatus()) {
      float z = line.getZ()[get_path_length_idx(line, lead_data.getDRel())];
      mapToScreen(lead_data.getDRel(), -lead_data.getYRel(), z + path_offset_z, &lead_vertices[i]);
    }
  }
}

void ModelRenderer::update_model(const cereal::ModelDataV2::Reader &model, const cereal::RadarState::LeadData::Reader &lead, float height) {
  const auto &model_position = model.getPosition();
  float max_distance = *(model_position.getX().end() - 1);

  // update lane lines
  const auto &lane_lines = model.getLaneLines();
  const auto &line_probs = model.getLaneLineProbs();
  int max_idx = get_path_length_idx(lane_lines[0], max_distance);
  for (int i = 0; i < std::size(lane_line_vertices); i++) {
    lane_line_probs[i] = line_probs[i];
    mapLineToPolygon(lane_lines[i], (frogpilot_toggles.value("model_ui").toBool() ? frogpilot_toggles.value("lane_line_width").toDouble() : 0.025) * lane_line_probs[i], 0, &lane_line_vertices[i], max_idx);
  }

  // update road edges
  const auto &road_edges = model.getRoadEdges();
  const auto &edge_stds = model.getRoadEdgeStds();
  for (int i = 0; i < std::size(road_edge_vertices); i++) {
    road_edge_stds[i] = edge_stds[i];
    mapLineToPolygon(road_edges[i], frogpilot_toggles.value("model_ui").toBool() ? frogpilot_toggles.value("road_edge_width").toDouble() : 0.025, 0, &road_edge_vertices[i], max_idx);
  }

  // update path
  if (lead.getStatus()) {
    const float lead_d = lead.getDRel() * 2.;
    max_distance = std::clamp((float)(lead_d - fmin(lead_d * 0.35, 10.)), 0.0f, max_distance);
  }
  max_idx = get_path_length_idx(model_position, max_distance);
  // FrogPilot variables
  float path_width = frogpilot_toggles.value("path_width").toDouble();
  if (frogpilot_toggles.value("dynamic_path_width").toBool()) {
    UIState *s = uiState();
    path_width *= s->status == STATUS_ENGAGED ? 1.0f : s->status == STATUS_ALWAYS_ON_LATERAL_ACTIVE ? 0.75f : 0.50f;
  }
  mapLineToPolygon(model_position, frogpilot_toggles.value("model_ui").toBool() ? path_width * (1 - (frogpilot_toggles.value("path_edge_width").toDouble() / 100.0f)) : 0.9, path_offset_z, &track_vertices, max_idx, false);

  // FrogPilot variables
  FrogPilotUIState *fs = frogpilotUIState();
  SubMaster &fpsm = *(fs->sm);

  const cereal::FrogPilotPlan::Reader &frogpilotPlan = fpsm["frogpilotPlan"].getFrogpilotPlan();

  frogpilot_nvg->track_vertices = track_vertices;

  mapLineToPolygon(model_position, frogpilot_toggles.value("model_ui").toBool() ? path_width : 0, path_offset_z, &frogpilot_nvg->track_edge_vertices, max_idx, false);

  mapAveragedLineToPolygon(lane_lines[0], lane_lines[1], frogpilotPlan.getLaneWidthLeft() / 2.0f, 0, &frogpilot_nvg->track_adjacent_vertices[0], max_idx, height, false);
  mapAveragedLineToPolygon(lane_lines[2], lane_lines[3], frogpilotPlan.getLaneWidthRight() / 2.0f, 0, &frogpilot_nvg->track_adjacent_vertices[1], max_idx, height, false);
}

void ModelRenderer::drawLaneLines(QPainter &painter) {
  // lanelines
  for (int i = 0; i < std::size(lane_line_vertices); ++i) {
    if (frogpilot_toggles.value("color_scheme").toString() != "stock") {
      painter.setBrush(QColor::fromRgbF(1.0, 1.0, 1.0, std::clamp<float>(lane_line_probs[i], 0.0, 0.7)));
    } else {
      QColor lane_color = QColor(frogpilot_toggles.value("lane_lines_color").toString());
      lane_color.setAlphaF(lane_color.alphaF() * std::clamp<float>(lane_line_probs[i], 0.0, 0.7));
      painter.setBrush(lane_color);
    }
    painter.drawPolygon(lane_line_vertices[i]);
  }

  // road edges
  for (int i = 0; i < std::size(road_edge_vertices); ++i) {
    painter.setBrush(QColor::fromRgbF(1.0, 0, 0, std::clamp<float>(1.0 - road_edge_stds[i], 0.0, 1.0)));
    painter.drawPolygon(road_edge_vertices[i]);
  }
}

void ModelRenderer::drawPath(QPainter &painter, const cereal::ModelDataV2::Reader &model, int height) {
  QLinearGradient bg(0, height, 0, 0);
  if (experimental_mode || frogpilot_toggles.value("acceleration_path").toBool() || frogpilot_toggles.value("rainbow_path").toBool()) {
    // The first half of track_vertices are the points for the right side of the path
    const auto &acceleration = model.getAcceleration().getX();
    const int max_len = std::min<int>(track_vertices.length() / 2, acceleration.size());

    for (int i = 0; i < max_len; ++i) {
      // Some points are out of frame
      int track_idx = max_len - i - 1;  // flip idx to start from bottom right
      if (track_vertices[track_idx].y() < 0 || track_vertices[track_idx].y() > height) continue;

      // Flip so 0 is bottom of frame
      float lin_grad_point = (height - track_vertices[track_idx].y()) / height;

      if ((fabs(acceleration[i]) < 0.25 || !frogpilot_toggles.value("acceleration_path").toBool()) && frogpilot_toggles.value("rainbow_path").toBool()) {
        frogpilot_nvg->paintRainbowPath(painter, bg, lin_grad_point);
      } else if (fabs(acceleration[i]) < 0.25 && frogpilot_toggles.value("color_scheme").toString() != "stock") {
        QColor color = QColor(frogpilot_toggles.value("path_color").toString());
        color.setAlphaF(util::map_val(lin_grad_point, 0.0f, 1.0f, 1.0f, 0.1f));
        bg.setColorAt(lin_grad_point, color);
      } else {
        // speed up: 120, slow down: 0
        float path_hue = fmax(fmin(60 + acceleration[i] * 35, 120), 0);
        // FIXME: painter.drawPolygon can be slow if hue is not rounded
        path_hue = int(path_hue * 100 + 0.5) / 100;

        float saturation = fmin(fabs(acceleration[i] * 1.5), 1);
        float lightness = util::map_val(saturation, 0.0f, 1.0f, 0.95f, 0.62f);        // lighter when grey
        float alpha = util::map_val(lin_grad_point, 0.75f / 2.f, 0.75f, 0.4f, 0.0f);  // matches previous alpha fade
        bg.setColorAt(lin_grad_point, QColor::fromHslF(path_hue / 360., saturation, lightness, alpha));

        // Skip a point, unless next is last
        i += (i + 2) < max_len ? 1 : 0;
      }
    }

  } else {
    updatePathGradient(bg);
  }

  painter.setBrush(bg);
  painter.drawPolygon(track_vertices);

  // FrogPilot variables
  if (frogpilot_toggles.value("adjacent_paths").toBool() || frogpilot_toggles.value("adjacent_path_metrics").toBool()) {
    frogpilot_nvg->paintAdjacentPaths(painter);
  } else if (frogpilot_toggles.value("blind_spot_path").toBool()) {
    frogpilot_nvg->paintBlindSpotPath(painter);
  }

  frogpilot_nvg->paintPathEdges(painter, height);
}

void ModelRenderer::updatePathGradient(QLinearGradient &bg) {
  static const QColor throttle_colors[] = {
      QColor::fromHslF(148. / 360., 0.94, 0.51, 0.4),
      QColor::fromHslF(112. / 360., 1.0, 0.68, 0.35),
      QColor::fromHslF(112. / 360., 1.0, 0.68, 0.0)};

  static const QColor no_throttle_colors[] = {
      QColor::fromHslF(148. / 360., 0.0, 0.95, 0.4),
      QColor::fromHslF(112. / 360., 0.0, 0.95, 0.35),
      QColor::fromHslF(112. / 360., 0.0, 0.95, 0.0),
  };

  // Transition speed; 0.1 corresponds to 0.5 seconds at UI_FREQ
  constexpr float transition_speed = 0.1f;

  // Start transition if throttle state changes
  bool allow_throttle = (*uiState()->sm)["longitudinalPlan"].getLongitudinalPlan().getAllowThrottle() || !longitudinal_control;
  if (allow_throttle != prev_allow_throttle) {
    prev_allow_throttle = allow_throttle;
    // Invert blend factor for a smooth transition when the state changes mid-animation
    blend_factor = std::max(1.0f - blend_factor, 0.0f);
  }

  const QColor *begin_colors = allow_throttle ? no_throttle_colors : throttle_colors;
  const QColor *end_colors = allow_throttle ? throttle_colors : no_throttle_colors;
  if (blend_factor < 1.0f) {
    blend_factor = std::min(blend_factor + transition_speed, 1.0f);
  }

  // Set gradient colors by blending the start and end colors
  bg.setColorAt(0.0f, blendColors(begin_colors[0], end_colors[0], blend_factor));
  bg.setColorAt(0.5f, blendColors(begin_colors[1], end_colors[1], blend_factor));
  bg.setColorAt(1.0f, blendColors(begin_colors[2], end_colors[2], blend_factor));
}

QColor ModelRenderer::blendColors(const QColor &start, const QColor &end, float t) {
  if (t == 1.0f) return end;
  return QColor::fromRgbF(
      (1 - t) * start.redF() + t * end.redF(),
      (1 - t) * start.greenF() + t * end.greenF(),
      (1 - t) * start.blueF() + t * end.blueF(),
      (1 - t) * start.alphaF() + t * end.alphaF());
}

void ModelRenderer::drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data,
                             const QPointF &vd, const QRect &surface_rect, QColor marker_color, bool adjacent) {
  const float speedBuff = 10.;
  const float leadBuff = 40.;
  const float d_rel = lead_data.getDRel() + (adjacent ? fabs(lead_data.getYRel()) : 0);
  const float v_rel = lead_data.getVRel();

  float fillAlpha = 0;
  if (d_rel < leadBuff) {
    fillAlpha = 255 * (1.0 - (d_rel / leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255 * (-1 * (v_rel / speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp<float>(vd.x(), 0.f, surface_rect.width() - sz / 2);
  float y = std::min<float>(vd.y(), surface_rect.height() - sz * 0.6);

  float g_xo = sz / 5;
  float g_yo = sz / 10;

  QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_yo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};
  painter.setBrush(QColor(218, 202, 37, 255));
  painter.drawPolygon(glow, std::size(glow));

  // chevron
  QPointF chevron[] = {{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz}};
  painter.setBrush(QColor(marker_color.red(), marker_color.green(), marker_color.blue(), fillAlpha));
  painter.drawPolygon(chevron, std::size(chevron));

  // FrogPilot variables
  if (frogpilot_toggles.value("lead_info").toBool()) {
    frogpilot_nvg->paintLeadMetrics(painter, adjacent, chevron, lead_data);
  }
}

// Projects a point in car to space to the corresponding point in full frame image space.
bool ModelRenderer::mapToScreen(float in_x, float in_y, float in_z, QPointF *out) {
  Eigen::Vector3f input(in_x, in_y, in_z);
  auto pt = car_space_transform * input;
  *out = QPointF(pt.x() / pt.z(), pt.y() / pt.z());
  return clip_region.contains(*out);
}

void ModelRenderer::mapLineToPolygon(const cereal::XYZTData::Reader &line, float y_off, float z_off,
                                     QPolygonF *pvd, int max_idx, bool allow_invert) {
  const auto line_x = line.getX(), line_y = line.getY(), line_z = line.getZ();
  QPointF left, right;
  pvd->clear();
  for (int i = 0; i <= max_idx; i++) {
    // highly negative x positions  are drawn above the frame and cause flickering, clip to zy plane of camera
    if (line_x[i] < 0) continue;

    bool l = mapToScreen(line_x[i], line_y[i] - y_off, line_z[i] + z_off, &left);
    bool r = mapToScreen(line_x[i], line_y[i] + y_off, line_z[i] + z_off, &right);
    if (l && r) {
      // For wider lines the drawn polygon will "invert" when going over a hill and cause artifacts
      if (!allow_invert && pvd->size() && left.y() > pvd->back().y()) {
        continue;
      }
      pvd->push_back(left);
      pvd->push_front(right);
    }
  }
}

// FrogPilot variables
void ModelRenderer::mapAveragedLineToPolygon(const cereal::XYZTData::Reader &line1, const cereal::XYZTData::Reader &line2, float y_off, float z_off,
                                             QPolygonF *pvd, int max_idx, float height, bool allow_invert) {
  const auto line_x1 = line1.getX(), line_y1 = line1.getY(), line_z1 = line1.getZ();
  const auto line_y2 = line2.getY();
  QPointF left, right;
  pvd->clear();
  for (int i = 0; i <= max_idx; i++) {
    // highly negative x positions  are drawn above the frame and cause flickering, clip to zy plane of camera
    if (line_x1[i] < 0) continue;

    bool l = mapToScreen(line_x1[i], ((line_y1[i] + line_y2[i]) / 2.0f) - y_off, line_z1[i] + z_off, &left);
    bool r = mapToScreen(line_x1[i], ((line_y1[i] + line_y2[i]) / 2.0f) + y_off, line_z1[i] + z_off, &right);
    if (l && r) {
      // For wider lines the drawn polygon will "invert" when going over a hill and cause artifacts
      if (!allow_invert && pvd->size() && left.y() > pvd->back().y()) {
        continue;
      }
      pvd->push_back(left);
      pvd->push_front(right);
    }
  }

  // Ground the path
  if (pvd->size() >= 4) {
    int mid = pvd->size() / 2;

    std::function<void(int, int)> extendToBottom = [&](int idx1, int idx2) {
      QPointF &p0 = (*pvd)[idx1];
      QPointF &p1 = (*pvd)[idx2];

      float dy = p0.y() - p1.y();
      if (std::abs(dy) > 0.1f) {
        float slope = (p0.x() - p1.x()) / dy;
        p0.setX(p0.x() + (height - p0.y()) * slope);
        p0.setY(height);
      }
    };

    extendToBottom(mid, mid + 1);
    extendToBottom(mid - 1, mid - 2);
  }
}

void ModelRenderer::updateAdjacentLeads(const cereal::FrogPilotRadarState::Reader &radar_state, const cereal::XYZTData::Reader &line) {
  for (int i = 0; i < 2; ++i) {
    const auto &lead_data = (i == 0) ? radar_state.getLeadLeft() : radar_state.getLeadRight();
    if (lead_data.getStatus()) {
      float z = line.getZ()[get_path_length_idx(line, lead_data.getDRel())];
      mapToScreen(lead_data.getDRel(), -lead_data.getYRel(), z + path_offset_z, &adjacent_lead_vertices[i]);
    }
  }
}

void ModelRenderer::updateRadarTracks(const cereal::XYZTData::Reader &line) {
  std::vector<QPointF> &radar_tracks = frogpilot_nvg->radar_tracks;
  radar_tracks.clear();

  SubMaster &fpsm = *(frogpilotUIState()->sm);
  capnp::List<cereal::RadarData::RadarPoint>::Reader radar_points = fpsm["liveTracks"].getLiveTracks().getPoints();
  radar_tracks.reserve(radar_points.size());

  capnp::List<float>::Reader line_z = line.getZ();

  for (cereal::RadarData::RadarPoint::Reader point : radar_points) {
    float d_rel = point.getDRel();
    float z = line_z[get_path_length_idx(line, d_rel)];

    QPointF calibrated_point;
    if (mapToScreen(d_rel, -point.getYRel(), z + path_offset_z, &calibrated_point)) {
      radar_tracks.push_back(calibrated_point);
    }
  }
}
