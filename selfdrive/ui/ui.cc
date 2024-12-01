#include "selfdrive/ui/ui.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include <QtConcurrent>

#include "common/transformations/orientation.hpp"
#include "common/params.h"
#include "common/swaglog.h"
#include "common/util.h"
#include "common/watchdog.h"
#include "system/hardware/hw.h"

#define BACKLIGHT_DT 0.05
#define BACKLIGHT_TS 10.00

// Projects a point in car to space to the corresponding point in full frame
// image space.
static bool calib_frame_to_full_frame(const UIState *s, float in_x, float in_y, float in_z, QPointF *out) {
  const float margin = 500.0f;
  const QRectF clip_region{-margin, -margin, s->fb_w + 2 * margin, s->fb_h + 2 * margin};

  const vec3 pt = (vec3){{in_x, in_y, in_z}};
  const vec3 Ep = matvecmul3(s->scene.wide_cam ? s->scene.view_from_wide_calib : s->scene.view_from_calib, pt);
  const vec3 KEp = matvecmul3(s->scene.wide_cam ? ECAM_INTRINSIC_MATRIX : FCAM_INTRINSIC_MATRIX, Ep);

  // Project.
  QPointF point = s->car_space_transform.map(QPointF{KEp.v[0] / KEp.v[2], KEp.v[1] / KEp.v[2]});
  if (clip_region.contains(point)) {
    *out = point;
    return true;
  }
  return false;
}

int get_path_length_idx(const cereal::XYZTData::Reader &line, const float path_height) {
  const auto line_x = line.getX();
  int max_idx = 0;
  for (int i = 1; i < line_x.size() && line_x[i] <= path_height; ++i) {
    max_idx = i;
  }
  return max_idx;
}

void update_leads(UIState *s, const cereal::RadarState::Reader &radar_state, const cereal::XYZTData::Reader &line) {
  SubMaster &sm = *(s->sm);
  float path_offset_z = sm["liveCalibration"].getLiveCalibration().getHeight()[0];

  cereal::RadarState::LeadData::Reader (cereal::RadarState::Reader::*get_lead_data[7])() const = {
    &cereal::RadarState::Reader::getLeadOne,
    &cereal::RadarState::Reader::getLeadTwo,
    &cereal::RadarState::Reader::getLeadFar,
    &cereal::RadarState::Reader::getLeadLeft,
    &cereal::RadarState::Reader::getLeadRight,
    &cereal::RadarState::Reader::getLeadLeftFar,
    &cereal::RadarState::Reader::getLeadRightFar
  };

  for (int i = 0; i < 7; ++i) {
    auto lead_data = (radar_state.*get_lead_data[i])();
    if (lead_data.getStatus()) {
      float z = line.getZ()[get_path_length_idx(line, lead_data.getDRel())];
      calib_frame_to_full_frame(s, lead_data.getDRel(), -lead_data.getYRel(), z + path_offset_z, &s->scene.lead_vertices[i]);
    }
  }
}

void update_line_data(const UIState *s, const cereal::XYZTData::Reader &line,
                      float y_off, float z_off, QPolygonF *pvd, int max_idx, bool allow_invert=true) {
  const auto line_x = line.getX(), line_y = line.getY(), line_z = line.getZ();
  QPointF left, right;
  pvd->clear();
  for (int i = 0; i <= max_idx; i++) {
    // highly negative x positions  are drawn above the frame and cause flickering, clip to zy plane of camera
    if (line_x[i] < 0) continue;

    bool l = calib_frame_to_full_frame(s, line_x[i], line_y[i] - y_off, line_z[i] + z_off, &left);
    bool r = calib_frame_to_full_frame(s, line_x[i], line_y[i] + y_off, line_z[i] + z_off, &right);
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

void update_model(UIState *s,
                  const cereal::ModelDataV2::Reader &model,
                  const cereal::UiPlan::Reader &plan) {
  SubMaster &sm = *(s->sm);
  UIScene &scene = s->scene;
  scene.left_curve = model.getOrientationRate().getZ()[33 - 1] < 0;
  float path_offset_z = sm["liveCalibration"].getLiveCalibration().getHeight()[0];
  auto plan_position = plan.getPosition();
  scene.model_length = model.getPosition().getX()[33 - 1];
  if (plan_position.getX().size() < model.getPosition().getX().size()) {
    plan_position = model.getPosition();
  }
  float max_distance = scene.unlimited_road_ui_length ? *(plan_position.getX().end() - 1) :
                       std::clamp(*(plan_position.getX().end() - 1),
                                  MIN_DRAW_DISTANCE, MAX_DRAW_DISTANCE);

  // update lane lines
  const auto lane_lines = model.getLaneLines();
  const auto lane_line_probs = model.getLaneLineProbs();
  int max_idx = get_path_length_idx(lane_lines[0], max_distance);
  for (int i = 0; i < std::size(scene.lane_line_vertices); i++) {
    scene.lane_line_probs[i] = lane_line_probs[i];
    update_line_data(s, lane_lines[i], (scene.model_ui ? scene.lane_line_width : 0.025) * scene.lane_line_probs[i], 0, &scene.lane_line_vertices[i], max_idx);
  }

  // update road edges
  const auto road_edges = model.getRoadEdges();
  const auto road_edge_stds = model.getRoadEdgeStds();
  for (int i = 0; i < std::size(scene.road_edge_vertices); i++) {
    scene.road_edge_stds[i] = road_edge_stds[i];
    update_line_data(s, road_edges[i], scene.model_ui ? scene.road_edge_width : 0.025, 0, &scene.road_edge_vertices[i], max_idx);
  }

  // Update adjacent paths
  for (int i = 4; i <= 5; i++) {
    update_line_data(s, lane_lines[i], (i == 4 ? scene.lane_width_left : scene.lane_width_right) / 2.0f, 0, &scene.track_adjacent_vertices[i], max_idx, false);
  }

  // update path
  float path_width;
  if (scene.dynamic_path_width) {
    float multiplier = scene.enabled ? 1.0f : scene.always_on_lateral_active ? 0.75f : 0.50f;
    path_width = scene.path_width * multiplier;
  } else {
    path_width = scene.path_width;
  }

  if (scene.radarless_model) {
    auto lead_count = model.getLeadsV3().size();
    if (lead_count > 0) {
      auto lead_one = model.getLeadsV3()[0];
      scene.has_lead = lead_one.getProb() > scene.lead_detection_probability;

      if (scene.has_lead) {
        const float lead_d = lead_one.getX()[0] * 2.;
        max_distance = std::clamp((float)(lead_d - fmin(lead_d * 0.35, 10.)), 0.0f, max_distance);
      }
    } else {
      scene.has_lead = false;
    }
  } else {
    auto lead_one = (*s->sm)["radarState"].getRadarState().getLeadOne();
    scene.has_lead = lead_one.getModelProb() > scene.lead_detection_probability;

    if (scene.has_lead) {
      const float lead_d = lead_one.getDRel() * 2.;
      max_distance = std::clamp((float)(lead_d - fmin(lead_d * 0.35, 10.)), 0.0f, max_distance);
    }
  }
  max_idx = get_path_length_idx(plan_position, max_distance);
  update_line_data(s, plan_position, scene.model_ui ? path_width * (1 - (scene.path_edge_width / 100.0f)) : 0.9, path_offset_z, &scene.track_vertices, max_idx, false);

  // Update path edges
  update_line_data(s, plan_position, scene.model_ui ? path_width : 0, path_offset_z, &scene.track_edge_vertices, max_idx, false);
}

void update_dmonitoring(UIState *s, const cereal::DriverStateV2::Reader &driverstate, float dm_fade_state, bool is_rhd) {
  UIScene &scene = s->scene;
  const auto driver_orient = is_rhd ? driverstate.getRightDriverData().getFaceOrientation() : driverstate.getLeftDriverData().getFaceOrientation();
  for (int i = 0; i < std::size(scene.driver_pose_vals); i++) {
    float v_this = (i == 0 ? (driver_orient[i] < 0 ? 0.7 : 0.9) : 0.4) * driver_orient[i];
    scene.driver_pose_diff[i] = fabs(scene.driver_pose_vals[i] - v_this);
    scene.driver_pose_vals[i] = 0.8 * v_this + (1 - 0.8) * scene.driver_pose_vals[i];
    scene.driver_pose_sins[i] = sinf(scene.driver_pose_vals[i]*(1.0-dm_fade_state));
    scene.driver_pose_coss[i] = cosf(scene.driver_pose_vals[i]*(1.0-dm_fade_state));
  }

  auto [sin_y, sin_x, sin_z] = scene.driver_pose_sins;
  auto [cos_y, cos_x, cos_z] = scene.driver_pose_coss;

  const mat3 r_xyz = (mat3){{
    cos_x * cos_z,
    cos_x * sin_z,
    -sin_x,

    -sin_y * sin_x * cos_z - cos_y * sin_z,
    -sin_y * sin_x * sin_z + cos_y * cos_z,
    -sin_y * cos_x,

    cos_y * sin_x * cos_z - sin_y * sin_z,
    cos_y * sin_x * sin_z + sin_y * cos_z,
    cos_y * cos_x,
  }};

  // transform vertices
  for (int kpi = 0; kpi < std::size(default_face_kpts_3d); kpi++) {
    vec3 kpt_this = matvecmul3(r_xyz, default_face_kpts_3d[kpi]);
    scene.face_kpts_draw[kpi] = (vec3){{kpt_this.v[0], kpt_this.v[1], (float)(kpt_this.v[2] * (1.0-dm_fade_state) + 8 * dm_fade_state)}};
  }
}

static void update_sockets(UIState *s) {
  s->sm->update(0);
}

static void update_state(UIState *s) {
  Params params = Params();
  SubMaster &sm = *(s->sm);
  UIScene &scene = s->scene;

  if (sm.updated("liveCalibration")) {
    auto live_calib = sm["liveCalibration"].getLiveCalibration();
    auto rpy_list = live_calib.getRpyCalib();
    auto wfde_list = live_calib.getWideFromDeviceEuler();
    Eigen::Vector3d rpy;
    Eigen::Vector3d wfde;
    if (rpy_list.size() == 3) rpy << rpy_list[0], rpy_list[1], rpy_list[2];
    if (wfde_list.size() == 3) wfde << wfde_list[0], wfde_list[1], wfde_list[2];
    Eigen::Matrix3d device_from_calib = euler2rot(rpy);
    Eigen::Matrix3d wide_from_device = euler2rot(wfde);
    Eigen::Matrix3d view_from_device;
    view_from_device << 0, 1, 0,
                        0, 0, 1,
                        1, 0, 0;
    Eigen::Matrix3d view_from_calib = view_from_device * device_from_calib;
    Eigen::Matrix3d view_from_wide_calib = view_from_device * wide_from_device * device_from_calib;
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        scene.view_from_calib.v[i*3 + j] = view_from_calib(i, j);
        scene.view_from_wide_calib.v[i*3 + j] = view_from_wide_calib(i, j);
      }
    }
    scene.calibration_valid = live_calib.getCalStatus() == cereal::LiveCalibrationData::Status::CALIBRATED;
    scene.calibration_wide_valid = wfde_list.size() == 3;
  }
  if (sm.updated("pandaStates")) {
    auto pandaStates = sm["pandaStates"].getPandaStates();
    if (pandaStates.size() > 0) {
      scene.pandaType = pandaStates[0].getPandaType();

      if (scene.pandaType != cereal::PandaState::PandaType::UNKNOWN) {
        scene.ignition = false;
        for (const auto& pandaState : pandaStates) {
          scene.ignition |= pandaState.getIgnitionLine() || pandaState.getIgnitionCan();
        }
      }
    }
  } else if ((s->sm->frame - s->sm->rcv_frame("pandaStates")) > 5*UI_FREQ) {
    scene.pandaType = cereal::PandaState::PandaType::UNKNOWN;
  }
  if (sm.updated("carControl")) {
    auto carControl = sm["carControl"].getCarControl();
    scene.steer = carControl.getActuators().getSteer();
  }
  if (sm.updated("carParams")) {
    scene.longitudinal_control = sm["carParams"].getCarParams().getOpenpilotLongitudinalControl();
  }
  if (sm.updated("carState")) {
    auto carState = sm["carState"].getCarState();
    scene.acceleration = carState.getAEgo();
    scene.blind_spot_left = carState.getLeftBlindspot();
    scene.blind_spot_right = carState.getRightBlindspot();
    scene.parked = carState.getGearShifter() == cereal::CarState::GearShifter::PARK;
    scene.reverse = carState.getGearShifter() == cereal::CarState::GearShifter::REVERSE;
    scene.standstill = carState.getStandstill() && !scene.reverse;
    scene.steering_angle_deg = -carState.getSteeringAngleDeg();
    scene.turn_signal_left = carState.getLeftBlinker();
    scene.turn_signal_right = carState.getRightBlinker();
  }
  if (sm.updated("controlsState")) {
    auto controlsState = sm["controlsState"].getControlsState();
    scene.enabled = controlsState.getEnabled();
    scene.experimental_mode = scene.enabled && controlsState.getExperimentalMode();
  }
  if (sm.updated("deviceState")) {
    auto deviceState = sm["deviceState"].getDeviceState();
    scene.online = deviceState.getNetworkType() != cereal::DeviceState::NetworkType::NONE;
  }
  if (sm.updated("frogpilotCarControl")) {
    auto frogpilotCarControl = sm["frogpilotCarControl"].getFrogpilotCarControl();
    scene.always_on_lateral_active = !scene.enabled && frogpilotCarControl.getAlwaysOnLateralActive();
  }
  if (sm.updated("frogpilotCarState")) {
    auto frogpilotCarState = sm["frogpilotCarState"].getFrogpilotCarState();
    scene.brake_lights_on = frogpilotCarState.getBrakeLights();
    scene.dashboard_speed_limit = frogpilotCarState.getDashboardSpeedLimit();
    scene.traffic_mode_active = frogpilotCarState.getTrafficModeActive();
  }
  if (sm.updated("frogpilotNavigation")) {
    auto frogpilotNavigation = sm["frogpilotNavigation"].getFrogpilotNavigation();
    scene.navigation_speed_limit = frogpilotNavigation.getNavigationSpeedLimit();
  }
  if (sm.updated("frogpilotPlan")) {
    auto frogpilotPlan = sm["frogpilotPlan"].getFrogpilotPlan();
    scene.acceleration_jerk = frogpilotPlan.getAccelerationJerk();
    scene.acceleration_jerk_difference = frogpilotPlan.getAccelerationJerkStock() - scene.acceleration_jerk;
    scene.desired_follow = frogpilotPlan.getDesiredFollowDistance();
    scene.lane_width_left = frogpilotPlan.getLaneWidthLeft();
    scene.lane_width_right = frogpilotPlan.getLaneWidthRight();
    scene.mtsc_speed = frogpilotPlan.getMtscSpeed();
    scene.obstacle_distance = frogpilotPlan.getSafeObstacleDistance();
    scene.obstacle_distance_stock = frogpilotPlan.getSafeObstacleDistanceStock();
    scene.red_light = frogpilotPlan.getRedLight();
    scene.speed_jerk = frogpilotPlan.getSpeedJerk();
    scene.speed_jerk_difference = frogpilotPlan.getSpeedJerkStock() - scene.speed_jerk;
    scene.speed_limit = frogpilotPlan.getSlcSpeedLimit();
    scene.speed_limit_changed = scene.speed_limit_controller && frogpilotPlan.getSpeedLimitChanged();
    scene.speed_limit_map = frogpilotPlan.getSlcMapSpeedLimit();
    scene.speed_limit_offset = frogpilotPlan.getSlcSpeedLimitOffset();
    scene.speed_limit_overridden = frogpilotPlan.getSlcOverridden();
    scene.speed_limit_overridden_speed = frogpilotPlan.getSlcOverriddenSpeed();
    scene.speed_limit_source = frogpilotPlan.getSlcSpeedLimitSource().cStr();
    scene.stopped_equivalence = frogpilotPlan.getStoppedEquivalenceFactor();
    scene.unconfirmed_speed_limit = frogpilotPlan.getUnconfirmedSlcSpeedLimit();
    scene.upcoming_speed_limit = frogpilotPlan.getUpcomingSLCSpeedLimit();
    scene.vtsc_controlling_curve = frogpilotPlan.getVtscControllingCurve();
    scene.vtsc_speed = frogpilotPlan.getVtscSpeed();
    if (frogpilotPlan.getTogglesUpdated()) {
      scene.frogpilot_toggles = QJsonDocument::fromJson(QString::fromStdString(s->params_memory.get("FrogPilotToggles", true)).toUtf8()).object();
      ui_update_params(s);
      if (frogpilotPlan.getThemeUpdated()) {
        ui_update_theme(s);
      }
    }
  }
  if (sm.updated("liveLocationKalman")) {
    auto liveLocationKalman = sm["liveLocationKalman"].getLiveLocationKalman();
    auto orientation = liveLocationKalman.getCalibratedOrientationNED();
    if (orientation.getValid()) {
      scene.bearing_deg = RAD2DEG(orientation.getValue()[2]);
    }
  }
  if (sm.updated("liveTorqueParameters")) {
    auto liveTorqueParameters = sm["liveTorqueParameters"].getLiveTorqueParameters();
    scene.friction = liveTorqueParameters.getFrictionCoefficientFiltered();
    scene.lat_accel = liveTorqueParameters.getLatAccelFactorFiltered();
    scene.live_valid = liveTorqueParameters.getLiveValid();
  }
  if (sm.updated("wideRoadCameraState")) {
    auto cam_state = sm["wideRoadCameraState"].getWideRoadCameraState();
    float scale = (cam_state.getSensor() == cereal::FrameData::ImageSensor::AR0231) ? 6.0f : 1.0f;
    scene.light_sensor = std::max(100.0f - scale * cam_state.getExposureValPercent(), 0.0f);
  } else if (!sm.allAliveAndValid({"wideRoadCameraState"})) {
    scene.light_sensor = -1;
  }
  scene.started = sm["deviceState"].getDeviceState().getStarted() && scene.ignition;

  scene.world_objects_visible = scene.world_objects_visible ||
                                (scene.started &&
                                 sm.rcv_frame("liveCalibration") > scene.started_frame &&
                                 sm.rcv_frame("modelV2") > scene.started_frame &&
                                 sm.rcv_frame("uiPlan") > scene.started_frame);
}

void ui_update_params(UIState *s) {
  auto params = Params();
  s->scene.is_metric = params.getBool("IsMetric");
  s->scene.map_on_left = params.getBool("NavSettingLeftSide");

  ui_update_frogpilot_params(s);
}

void ui_update_frogpilot_params(UIState *s) {
  UIScene &scene = s->scene;

  scene.acceleration_path = scene.frogpilot_toggles.value("acceleration_path").toBool();
  scene.adjacent_path = scene.frogpilot_toggles.value("adjacent_paths").toBool();
  scene.adjacent_path_metrics = scene.frogpilot_toggles.value("adjacent_path_metrics").toBool();
  scene.always_on_lateral = scene.frogpilot_toggles.value("always_on_lateral").toBool();
  scene.aol_status_bar = scene.frogpilot_toggles.value("always_on_lateral_status_bar").toBool();
  scene.big_map = scene.frogpilot_toggles.value("big_map").toBool();
  scene.blind_spot_path = scene.frogpilot_toggles.value("blind_spot_path").toBool();
  scene.camera_view = scene.frogpilot_toggles.value("camera_view").toDouble();
  scene.cem_status_bar = scene.frogpilot_toggles.value("conditional_status_bar").toBool();
  scene.compass = scene.frogpilot_toggles.value("compass").toBool();
  scene.conditional_experimental = scene.frogpilot_toggles.value("conditional_experimental_mode").toBool();
  scene.conditional_limit = scene.frogpilot_toggles.value("conditional_limit").toDouble();
  scene.conditional_limit_lead = scene.frogpilot_toggles.value("conditional_limit_lead").toDouble();
  scene.cpu_metrics = scene.frogpilot_toggles.value("cpu_metrics").toBool();
  scene.driver_camera_in_reverse = scene.frogpilot_toggles.value("driver_camera_in_reverse").toBool();
  scene.dynamic_path_width = scene.frogpilot_toggles.value("dynamic_path_width").toBool();
  scene.dynamic_pedals_on_ui = scene.frogpilot_toggles.value("dynamic_pedals_on_ui").toBool();
  scene.experimental_mode_via_tap = scene.frogpilot_toggles.value("experimental_mode_via_tap").toBool();
  scene.fahrenheit = scene.frogpilot_toggles.value("fahrenheit").toBool();
  scene.frogs_go_moo = scene.frogpilot_toggles.value("frogs_go_moo").toBool();
  scene.full_map = scene.frogpilot_toggles.value("full_map").toBool();
  scene.gpu_metrics = scene.frogpilot_toggles.value("gpu_metrics").toBool();
  scene.hide_alerts = scene.frogpilot_toggles.value("hide_alerts").toBool();
  scene.hide_csc_ui = scene.frogpilot_toggles.value("hide_csc_ui").toBool();
  scene.hide_lead_marker = scene.frogpilot_toggles.value("hide_lead_marker").toBool();
  scene.hide_map_icon = scene.frogpilot_toggles.value("hide_map_icon").toBool();
  scene.hide_max_speed = scene.frogpilot_toggles.value("hide_max_speed").toBool();
  scene.hide_speed = scene.frogpilot_toggles.value("hide_speed").toBool();
  scene.hide_speed_limit = scene.frogpilot_toggles.value("hide_speed_limit").toBool();
  scene.ip_metrics = scene.frogpilot_toggles.value("ip_metrics").toBool();
  scene.jerk_metrics = scene.frogpilot_toggles.value("jerk_metrics").toBool();
  scene.lateral_tuning_metrics = scene.frogpilot_toggles.value("lateral_tuning_metrics").toBool();
  scene.lane_detection_width = scene.frogpilot_toggles.value("lane_detection_width").toDouble();
  scene.lane_line_width = scene.frogpilot_toggles.value("lane_line_width").toDouble();
  scene.lead_detection_probability = scene.frogpilot_toggles.value("lead_detection_probability").toDouble();
  scene.lead_metrics = scene.frogpilot_toggles.value("lead_metrics").toBool();
  scene.map_style = scene.frogpilot_toggles.value("map_style").toDouble();
  scene.memory_metrics = scene.frogpilot_toggles.value("memory_metrics").toBool();
  scene.minimum_lane_change_speed = scene.frogpilot_toggles.value("minimum_lane_change_speed").toDouble();
  scene.model_name = scene.frogpilot_toggles.value("model_name").toString();
  scene.model_randomizer = scene.frogpilot_toggles.value("model_randomizer").toBool();
  scene.model_ui = scene.frogpilot_toggles.value("model_ui").toBool();
  scene.mtsc_enabled = scene.frogpilot_toggles.value("map_turn_speed_controller").toBool();
  scene.no_logging = scene.frogpilot_toggles.value("no_logging").toBool();
  scene.no_uploads = scene.frogpilot_toggles.value("no_uploads").toBool();
  scene.numerical_temp = scene.frogpilot_toggles.value("numerical_temp").toBool();
  scene.onroad_distance_button = scene.frogpilot_toggles.value("onroad_distance_button").toBool();
  scene.path_edge_width = scene.frogpilot_toggles.value("path_edge_width").toDouble();
  scene.path_width = scene.frogpilot_toggles.value("path_width").toDouble();
  scene.pedals_on_ui = scene.frogpilot_toggles.value("pedals_on_ui").toBool();
  scene.radarless_model = scene.frogpilot_toggles.value("radarless_model").toBool();
  scene.random_events = scene.frogpilot_toggles.value("random_events").toBool();
  scene.rainbow_path = scene.frogpilot_toggles.value("rainbow_path").toBool();
  scene.road_edge_width = scene.frogpilot_toggles.value("road_edge_width").toDouble();
  scene.road_name_ui = scene.frogpilot_toggles.value("road_name_ui").toBool();
  scene.rotating_wheel = scene.frogpilot_toggles.value("rotating_wheel").toBool();
  if (scene.screen_brightness == -1) {
    scene.screen_brightness = scene.frogpilot_toggles.value("screen_brightness").toDouble();
  }
  if (scene.screen_brightness_onroad == -1) {
    scene.screen_brightness_onroad = scene.frogpilot_toggles.value("screen_brightness_onroad").toDouble();
  }
  scene.screen_recorder = scene.frogpilot_toggles.value("screen_recorder").toBool();
  scene.screen_timeout = scene.frogpilot_toggles.value("screen_timeout").toDouble();
  scene.screen_timeout_onroad = scene.frogpilot_toggles.value("screen_timeout_onroad").toDouble();
  scene.show_blind_spot = scene.frogpilot_toggles.value("blind_spot_metrics").toBool();
  scene.show_fps = scene.frogpilot_toggles.value("show_fps").toBool();
  scene.show_speed_limit_offset = scene.frogpilot_toggles.value("show_speed_limit_offset").toBool();
  scene.show_speed_limits = scene.frogpilot_toggles.value("show_speed_limits").toBool();
  scene.show_stopping_point = scene.frogpilot_toggles.value("show_stopping_point").toBool();
  scene.show_stopping_point_metrics = scene.frogpilot_toggles.value("show_stopping_point_metrics").toBool();
  scene.sidebar_metrics = scene.frogpilot_toggles.value("sidebar_metrics").toBool();
  scene.signal_metrics = scene.frogpilot_toggles.value("signal_metrics").toBool();
  scene.speed_limit_controller = scene.frogpilot_toggles.value("speed_limit_controller").toBool();
  scene.speed_limit_sources = scene.frogpilot_toggles.value("speed_limit_sources").toBool();
  scene.speed_limit_vienna = scene.frogpilot_toggles.value("speed_limit_vienna").toBool();
  scene.standby_mode = scene.frogpilot_toggles.value("standby_mode").toBool();
  scene.static_pedals_on_ui = scene.frogpilot_toggles.value("static_pedals_on_ui").toBool();
  scene.steering_metrics = scene.frogpilot_toggles.value("steering_metrics").toBool();
  scene.stopped_timer = scene.frogpilot_toggles.value("stopped_timer").toBool();
  scene.storage_left_metrics = scene.frogpilot_toggles.value("storage_left_metrics").toBool();
  scene.storage_used_metrics = scene.frogpilot_toggles.value("storage_used_metrics").toBool();
  scene.tethering_config = scene.frogpilot_toggles.value("tethering_config").toDouble();
  scene.unlimited_road_ui_length = scene.frogpilot_toggles.value("unlimited_road_ui_length").toBool();
  scene.use_si_metrics = scene.frogpilot_toggles.value("use_si_metrics").toBool();
  scene.use_wheel_speed = scene.frogpilot_toggles.value("use_wheel_speed").toBool();
  scene.vtsc_enabled = scene.frogpilot_toggles.value("vision_turn_controller").toBool();

  if (scene.tethering_config == 1) {
    s->wifi->setTetheringEnabled(true);
  }

  emit s->togglesUpdated();
}

void ui_update_theme(UIState *s) {
  UIScene &scene = s->scene;

  scene.use_stock_colors = scene.frogpilot_toggles.value("color_scheme").toString() == "stock" && scene.frogpilot_toggles.value("current_holiday_theme").toString() == "stock";
  scene.use_stock_wheel = scene.frogpilot_toggles.value("wheel_image").toString() == "stock" && scene.frogpilot_toggles.value("current_holiday_theme").toString() == "stock";

  if (!scene.use_stock_colors) {
    scene.use_stock_colors |= !loadThemeColors("", true).isValid();

    scene.lane_lines_color = loadThemeColors("LaneLines");
    scene.lead_marker_color = loadThemeColors("LeadMarker");
    scene.path_color = loadThemeColors("Path");
    scene.path_edges_color = loadThemeColors("PathEdge");
    scene.sidebar_color1 = loadThemeColors("Sidebar1");
    scene.sidebar_color2 = loadThemeColors("Sidebar2");
    scene.sidebar_color3 = loadThemeColors("Sidebar3");
  }

  emit s->themeUpdated();
}

void UIState::updateStatus() {
  if (scene.started && sm->updated("controlsState")) {
    auto controls_state = (*sm)["controlsState"].getControlsState();
    auto state = controls_state.getState();
    auto previous_status = status;
    if (state == cereal::ControlsState::OpenpilotState::PRE_ENABLED || state == cereal::ControlsState::OpenpilotState::OVERRIDING) {
      status = STATUS_OVERRIDE;
    } else if (scene.always_on_lateral_active) {
      status = STATUS_ALWAYS_ON_LATERAL_ACTIVE;
    } else if (scene.traffic_mode_active && scene.enabled) {
      status = STATUS_TRAFFIC_MODE_ACTIVE;
    } else {
      status = scene.enabled ? STATUS_ENGAGED : STATUS_DISENGAGED;
    }

    scene.wake_up_screen = controls_state.getAlertStatus() != cereal::ControlsState::AlertStatus::NORMAL || status != previous_status;
  }

  scene.started |= scene.force_onroad;
  scene.started &= !params_memory.getBool("ForceOffroad");

  // Handle onroad/offroad transition
  if (scene.started != started_prev || sm->frame == 1) {
    if (scene.started) {
      status = STATUS_DISENGAGED;
      scene.started_frame = sm->frame;
    } else if (scene.started_timer > 15*60*UI_FREQ && scene.model_randomizer) {
      emit reviewModel();
    }
    started_prev = scene.started;
    scene.world_objects_visible = false;
    emit offroadTransition(!scene.started);
    if (scene.tethering_config == 2) {
      wifi->setTetheringEnabled(scene.started);
    }
  }
}

UIState::UIState(QObject *parent) : QObject(parent) {
  sm = std::make_unique<SubMaster, const std::initializer_list<const char *>>({
    "modelV2", "controlsState", "liveCalibration", "radarState", "deviceState",
    "pandaStates", "carParams", "driverMonitoringState", "carState", "liveLocationKalman", "driverStateV2",
    "wideRoadCameraState", "managerState", "navInstruction", "navRoute", "uiPlan", "clocks",
    "carControl", "liveTorqueParameters", "frogpilotCarControl", "frogpilotCarState", "frogpilotDeviceState",
    "frogpilotNavigation", "frogpilotPlan",
  });

  Params params;
  language = QString::fromStdString(params.get("LanguageSetting"));
  auto prime_value = params.get("PrimeType");
  if (!prime_value.empty()) {
    prime_type = static_cast<PrimeType>(std::atoi(prime_value.c_str()));
  }

  // update timer
  timer = new QTimer(this);
  QObject::connect(timer, &QTimer::timeout, this, &UIState::update);
  timer->start(1000 / UI_FREQ);

  // FrogPilot variables
  wifi = new WifiManager(this);

  scene.frogpilot_toggles = QJsonDocument::fromJson(QString::fromStdString(params_memory.get("FrogPilotToggles", true)).toUtf8()).object();
  ui_update_params(this);
  ui_update_theme(this);
}

void UIState::update() {
  update_sockets(this);
  update_state(this);
  updateStatus();

  if (sm->frame % UI_FREQ == 0) {
    watchdog_kick(nanos_since_boot());
  }
  emit uiUpdate(*this);

  // FrogPilot variables
  scene.conditional_status = scene.conditional_experimental && scene.enabled ? params_memory.getInt("CEStatus") : 0;
  scene.driver_camera_timer = scene.driver_camera_in_reverse && scene.reverse ? scene.driver_camera_timer + 1 : 0;
  scene.force_onroad = params_memory.getBool("ForceOnroad");
  scene.started_timer = scene.started || started_prev ? scene.started_timer + 1 : 0;

  if (scene.keep_screen_on) {
    device()->resetInteractiveTimeout(scene.screen_timeout, scene.screen_timeout_onroad);
  }
}

void UIState::setPrimeType(PrimeType type) {
  if (type != prime_type) {
    bool prev_prime = hasPrime();

    prime_type = type;
    Params().put("PrimeType", std::to_string(prime_type));
    emit primeTypeChanged(prime_type);

    bool prime = hasPrime();
    if (prev_prime != prime) {
      emit primeChanged(prime);
    }
  }
}

Device::Device(QObject *parent) : brightness_filter(BACKLIGHT_OFFROAD, BACKLIGHT_TS, BACKLIGHT_DT), QObject(parent) {
  setAwake(true);
  resetInteractiveTimeout();

  QObject::connect(uiState(), &UIState::uiUpdate, this, &Device::update);
}

void Device::update(const UIState &s) {
  updateBrightness(s);
  updateWakefulness(s);
}

void Device::setAwake(bool on) {
  if (on != awake) {
    awake = on;
    Hardware::set_display_power(awake);
    LOGD("setting display power %d", awake);
    emit displayPowerChanged(awake);
  }
}

void Device::resetInteractiveTimeout(int timeout, int timeout_onroad) {
  if (timeout == -1) {
    timeout = (ignition_on ? 10 : 30);
  } else {
    timeout = (ignition_on ? timeout_onroad : timeout);
  }
  interactive_timeout = timeout * UI_FREQ;
}

void Device::updateBrightness(const UIState &s) {
  float clipped_brightness = offroad_brightness;
  if (s.scene.started && s.scene.light_sensor >= 0) {
    clipped_brightness = s.scene.light_sensor;

    // CIE 1931 - https://www.photonstophotos.net/GeneralTopics/Exposure/Psychometric_Lightness_and_Gamma.htm
    if (clipped_brightness <= 8) {
      clipped_brightness = (clipped_brightness / 903.3);
    } else {
      clipped_brightness = std::pow((clipped_brightness + 16.0) / 116.0, 3.0);
    }

    // Scale back to 10% to 100%
    clipped_brightness = std::clamp(100.0f * clipped_brightness, 10.0f, 100.0f);
  }

  int brightness = brightness_filter.update(clipped_brightness);
  if (!awake) {
    brightness = 0;
  } else if (s.scene.started && s.scene.standby_mode && !s.scene.wake_up_screen && interactive_timeout == 0) {
    brightness = 0;
  } else if (s.scene.started && s.scene.screen_brightness_onroad != 101) {
    brightness = interactive_timeout > 0 ? fmax(5, s.scene.screen_brightness_onroad) : s.scene.screen_brightness_onroad;
  } else if (s.scene.screen_brightness != 101) {
    brightness = s.scene.screen_brightness;
  }

  if (brightness != last_brightness) {
    if (!brightness_future.isRunning()) {
      brightness_future = QtConcurrent::run(Hardware::set_brightness, brightness);
      last_brightness = brightness;
    }
  }
}

void Device::updateWakefulness(const UIState &s) {
  bool ignition_state_changed = s.scene.ignition != ignition_on;
  ignition_on = s.scene.ignition;

  if (ignition_on && s.scene.standby_mode) {
    if (s.scene.wake_up_screen) {
      resetInteractiveTimeout(s.scene.screen_timeout, s.scene.screen_timeout_onroad);
    }
  }

  if (ignition_state_changed) {
    if (ignition_on && s.scene.screen_brightness_onroad == 0 && !s.scene.standby_mode) {
      resetInteractiveTimeout(0, 0);
    } else {
      resetInteractiveTimeout(s.scene.screen_timeout, s.scene.screen_timeout_onroad);
    }
  } else if (interactive_timeout > 0 && --interactive_timeout == 0) {
    emit interactiveTimeout();
  }

  if (s.scene.screen_brightness_onroad != 0) {
    setAwake(s.scene.ignition || interactive_timeout > 0);
  } else {
    setAwake(interactive_timeout > 0);
  }
}

UIState *uiState() {
  static UIState ui_state;
  return &ui_state;
}

Device *device() {
  static Device _device;
  return &_device;
}
