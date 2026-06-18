#pragma once

#include <QPainter>
#include <QPolygonF>

#include "selfdrive/ui/ui.h"

#include "frogpilot/ui/qt/onroad/frogpilot_annotated_camera.h"

class ModelRenderer {
public:
  ModelRenderer() {}
  void setTransform(const Eigen::Matrix3f &transform) { car_space_transform = transform; }
  void draw(QPainter &painter, const QRect &surface_rect);

  // FrogPilot variables
  FrogPilotAnnotatedCameraWidget *frogpilot_nvg;

  FrogPilotUIScene frogpilot_scene = {};

  QJsonObject frogpilot_toggles;

private:
  bool mapToScreen(float in_x, float in_y, float in_z, QPointF *out);
  void mapLineToPolygon(const cereal::XYZTData::Reader &line, float y_off, float z_off,
                        QPolygonF *pvd, int max_idx, bool allow_invert = true);
  void drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd, const QRect &surface_rect, QColor marker_color, bool adjacent=false);
  void update_leads(const cereal::RadarState::Reader &radar_state, const cereal::XYZTData::Reader &line);
  void update_model(const cereal::ModelDataV2::Reader &model, const cereal::RadarState::LeadData::Reader &lead, float height);
  void drawLaneLines(QPainter &painter);
  void drawPath(QPainter &painter, const cereal::ModelDataV2::Reader &model, int height);
  void updatePathGradient(QLinearGradient &bg);
  QColor blendColors(const QColor &start, const QColor &end, float t);

  bool longitudinal_control = false;
  bool experimental_mode = false;
  float blend_factor = 1.0f;
  bool prev_allow_throttle = true;
  float lane_line_probs[4] = {};
  float road_edge_stds[2] = {};
  float path_offset_z = 1.22f;
  QPolygonF track_vertices;
  QPolygonF lane_line_vertices[4] = {};
  QPolygonF road_edge_vertices[2] = {};
  QPointF lead_vertices[2] = {};
  Eigen::Matrix3f car_space_transform = Eigen::Matrix3f::Zero();
  QRectF clip_region;

  // FrogPilot variables
  void mapAveragedLineToPolygon(const cereal::XYZTData::Reader &line1, const cereal::XYZTData::Reader &line2, float y_off, float z_off,
                                QPolygonF *pvd, int max_idx, float height, bool allow_invert = true);
  void updateAdjacentLeads(const cereal::FrogPilotRadarState::Reader &radar_state, const cereal::XYZTData::Reader &line);
  void updateRadarTracks(const cereal::XYZTData::Reader &line);

  QPointF adjacent_lead_vertices[2] = {};
};
