#pragma once

#include <map>
#include <memory>
#include <string>

#include <QObject>
#include <QTimer>
#include <QColor>
#include <QFuture>
#include <QPolygonF>
#include <QTransform>

#include "cereal/messaging/messaging.h"
#include "common/mat.h"
#include "common/params.h"
#include "common/timing.h"
#include "selfdrive/ui/qt/network/wifi_manager.h"
#include "system/hardware/hw.h"

const int UI_BORDER_SIZE = 30;
const int UI_HEADER_HEIGHT = 420;

const int UI_FREQ = 20; // Hz
const int BACKLIGHT_OFFROAD = 50;
typedef cereal::CarControl::HUDControl::AudibleAlert AudibleAlert;

const float MIN_DRAW_DISTANCE = 10.0;
const float MAX_DRAW_DISTANCE = 100.0;
constexpr mat3 DEFAULT_CALIBRATION = {{ 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0 }};
constexpr mat3 FCAM_INTRINSIC_MATRIX = (mat3){{2648.0, 0.0, 1928.0 / 2,
                                           0.0, 2648.0, 1208.0 / 2,
                                           0.0, 0.0, 1.0}};
// tici ecam focal probably wrong? magnification is not consistent across frame
// Need to retrain model before this can be changed
constexpr mat3 ECAM_INTRINSIC_MATRIX = (mat3){{567.0, 0.0, 1928.0 / 2,
                                           0.0, 567.0, 1208.0 / 2,
                                           0.0, 0.0, 1.0}};


constexpr vec3 default_face_kpts_3d[] = {
  {-5.98, -51.20, 8.00}, {-17.64, -49.14, 8.00}, {-23.81, -46.40, 8.00}, {-29.98, -40.91, 8.00}, {-32.04, -37.49, 8.00},
  {-34.10, -32.00, 8.00}, {-36.16, -21.03, 8.00}, {-36.16, 6.40, 8.00}, {-35.47, 10.51, 8.00}, {-32.73, 19.43, 8.00},
  {-29.30, 26.29, 8.00}, {-24.50, 33.83, 8.00}, {-19.01, 41.37, 8.00}, {-14.21, 46.17, 8.00}, {-12.16, 47.54, 8.00},
  {-4.61, 49.60, 8.00}, {4.99, 49.60, 8.00}, {12.53, 47.54, 8.00}, {14.59, 46.17, 8.00}, {19.39, 41.37, 8.00},
  {24.87, 33.83, 8.00}, {29.67, 26.29, 8.00}, {33.10, 19.43, 8.00}, {35.84, 10.51, 8.00}, {36.53, 6.40, 8.00},
  {36.53, -21.03, 8.00}, {34.47, -32.00, 8.00}, {32.42, -37.49, 8.00}, {30.36, -40.91, 8.00}, {24.19, -46.40, 8.00},
  {18.02, -49.14, 8.00}, {6.36, -51.20, 8.00}, {-5.98, -51.20, 8.00},
};

struct Alert {
  QString text1;
  QString text2;
  QString type;
  cereal::ControlsState::AlertSize size;
  cereal::ControlsState::AlertStatus status;
  AudibleAlert sound;

  bool equal(const Alert &a2) {
    return text1 == a2.text1 && text2 == a2.text2 && type == a2.type && sound == a2.sound;
  }

  static Alert get(const SubMaster &sm, uint64_t started_frame) {
    const cereal::ControlsState::Reader &cs = sm["controlsState"].getControlsState();
    const uint64_t controls_frame = sm.rcv_frame("controlsState");

    Alert alert = {};
    if (controls_frame >= started_frame) {  // Don't get old alert.
      alert = {cs.getAlertText1().cStr(), cs.getAlertText2().cStr(),
               cs.getAlertType().cStr(), cs.getAlertSize(),
               cs.getAlertStatus(),
               cs.getAlertSound()};
    }

    if (!sm.updated("controlsState") && (sm.frame - started_frame) > 5 * UI_FREQ) {
      const int CONTROLS_TIMEOUT = 5;
      const int controls_missing = (nanos_since_boot() - sm.rcv_time("controlsState")) / 1e9;

      // Handle controls timeout
      if (std::ifstream("/data/community/crashes/error.txt")) {
        alert = {"openpilot crashed", "Please post the error log in the FrogPilot Discord!",
                 "controlsWaiting", cereal::ControlsState::AlertSize::MID,
                 cereal::ControlsState::AlertStatus::NORMAL,
                 AudibleAlert::NONE};
      } else if (controls_frame < started_frame) {
        // car is started, but controlsState hasn't been seen at all
        alert = {"openpilot Unavailable", "Waiting for controls to start",
                 "controlsWaiting", cereal::ControlsState::AlertSize::MID,
                 cereal::ControlsState::AlertStatus::NORMAL,
                 AudibleAlert::NONE};
      } else if (controls_missing > CONTROLS_TIMEOUT && !Hardware::PC()) {
        // car is started, but controls is lagging or died
        if (cs.getEnabled() && (controls_missing - CONTROLS_TIMEOUT) < 10) {
          alert = {"TAKE CONTROL IMMEDIATELY", "Controls Unresponsive",
                   "controlsUnresponsive", cereal::ControlsState::AlertSize::FULL,
                   cereal::ControlsState::AlertStatus::CRITICAL,
                   AudibleAlert::WARNING_IMMEDIATE};
        } else {
          alert = {"Controls Unresponsive", "Reboot Device",
                   "controlsUnresponsivePermanent", cereal::ControlsState::AlertSize::MID,
                   cereal::ControlsState::AlertStatus::NORMAL,
                   AudibleAlert::NONE};
        }
      }
    }
    return alert;
  }
};

typedef enum UIStatus {
  STATUS_DISENGAGED,
  STATUS_OVERRIDE,
  STATUS_ENGAGED,

  // FrogPilot statuses
  STATUS_LATERAL_ACTIVE,
} UIStatus;

enum PrimeType {
  UNKNOWN = -1,
  NONE = 0,
  MAGENTA = 1,
  LITE = 2,
  BLUE = 3,
  MAGENTA_NEW = 4,
  PURPLE = 5,
};

const QColor bg_colors [] = {
  [STATUS_DISENGAGED] = QColor(0x17, 0x33, 0x49, 0xc8),
  [STATUS_OVERRIDE] = QColor(0x91, 0x9b, 0x95, 0xf1),
  [STATUS_ENGAGED] = QColor(0x17, 0x86, 0x44, 0xf1),

  // FrogPilot colors
  [STATUS_LATERAL_ACTIVE] = QColor(0x0a, 0xba, 0xb5, 0xf1),
};

static std::map<cereal::ControlsState::AlertStatus, QColor> alert_colors = {
  {cereal::ControlsState::AlertStatus::NORMAL, QColor(0x15, 0x15, 0x15, 0xf1)},
  {cereal::ControlsState::AlertStatus::USER_PROMPT, QColor(0xDA, 0x6F, 0x25, 0xf1)},
  {cereal::ControlsState::AlertStatus::CRITICAL, QColor(0xC9, 0x22, 0x31, 0xf1)},
  {cereal::ControlsState::AlertStatus::FROGPILOT, QColor(0x17, 0x86, 0x44, 0xf1)},
};

typedef struct UIScene {
  bool calibration_valid = false;
  bool calibration_wide_valid  = false;
  bool wide_cam = true;
  mat3 view_from_calib = DEFAULT_CALIBRATION;
  mat3 view_from_wide_calib = DEFAULT_CALIBRATION;
  cereal::PandaState::PandaType pandaType;

  // modelV2
  float lane_line_probs[4];
  float road_edge_stds[2];
  QPolygonF track_vertices;
  QPolygonF lane_line_vertices[4];
  QPolygonF road_edge_vertices[2];

  // lead
  QPointF lead_vertices[2];

  // DMoji state
  float driver_pose_vals[3];
  float driver_pose_diff[3];
  float driver_pose_sins[3];
  float driver_pose_coss[3];
  vec3 face_kpts_draw[std::size(default_face_kpts_3d)];

  bool navigate_on_openpilot = false;

  float light_sensor;
  bool started, ignition, is_metric, map_on_left, longitudinal_control;
  bool world_objects_visible = false;
  uint64_t started_frame;

  // FrogPilot variables
  bool acceleration_path;
  bool active_alert;
  bool adjacent_path;
  bool adjacent_path_metrics;
  bool always_on_lateral;
  bool always_on_lateral_active;
  bool blind_spot_left;
  bool blind_spot_path;
  bool blind_spot_right;
  bool compass;
  bool conditional_experimental;
  bool disable_smoothing_mtsc;
  bool driver_camera;
  bool dynamic_path_width;
  bool enabled;
  bool experimental_mode;
  bool experimental_mode_via_screen;
  bool fahrenheit;
  bool fps_counter;
  bool full_map;
  bool hide_alerts;
  bool hide_aol_status_bar;
  bool hide_cem_status_bar;
  bool hide_lead_marker;
  bool hide_map_icon;
  bool hide_max_speed;
  bool hide_speed;
  bool hide_speed_ui;
  bool holiday_themes;
  bool lead_info;
  bool map_open;
  bool model_ui;
  bool numerical_temp;
  bool parked;
  bool pedals_on_ui;
  bool reverse_cruise;
  bool reverse_cruise_ui;
  bool right_hand_drive;
  bool road_name_ui;
  bool rotating_wheel;
  bool screen_recorder;
  bool show_driver_camera;
  bool show_slc_offset;
  bool show_slc_offset_ui;
  bool speed_limit_changed;
  bool speed_limit_controller;
  bool speed_limit_overridden;
  bool standby_mode;
  bool standstill;
  bool status_changed;
  bool tethering_enabled;
  bool turn_signal_left;
  bool turn_signal_right;
  bool unlimited_road_ui_length;
  bool use_si;
  bool use_vienna_slc_sign;

  float acceleration;
  float adjusted_cruise;
  float lane_line_width;
  float lane_width_left;
  float lane_width_right;
  float path_edge_width;
  float path_width;
  float road_edge_width;
  float speed_limit;
  float speed_limit_offset;
  float speed_limit_overridden_speed;
  float unconfirmed_speed_limit;

  int bearing_deg;
  int camera_view;
  int conditional_speed;
  int conditional_speed_lead;
  int conditional_status;
  int current_holiday_theme;
  int custom_colors;
  int custom_icons;
  int custom_signals;
  int desired_follow;
  int obstacle_distance;
  int obstacle_distance_stock;
  int screen_brightness;
  int screen_brightness_onroad;
  int screen_timeout;
  int screen_timeout_onroad;
  int steering_angle_deg;
  int stopped_equivalence;

  QPolygonF track_adjacent_vertices[6];
  QPolygonF track_edge_vertices;

} UIScene;

class UIState : public QObject {
  Q_OBJECT

public:
  UIState(QObject* parent = 0);
  void updateStatus();
  inline bool engaged() const {
    return scene.started && (*sm)["controlsState"].getControlsState().getEnabled();
  }

  void setPrimeType(PrimeType type);
  inline PrimeType primeType() const { return prime_type; }
  inline bool hasPrime() const { return prime_type != PrimeType::UNKNOWN && prime_type != PrimeType::NONE; }

  int fb_w = 0, fb_h = 0;

  std::unique_ptr<SubMaster> sm;

  UIStatus status;
  UIScene scene = {};

  QString language;

  QTransform car_space_transform;

  // FrogPilot variables
  WifiManager *wifi = nullptr;

  UIStatus previous_status;

signals:
  void uiUpdate(const UIState &s);
  void offroadTransition(bool offroad);
  void primeChanged(bool prime);
  void primeTypeChanged(PrimeType prime_type);

private slots:
  void update();

private:
  QTimer *timer;
  bool started_prev = false;
  PrimeType prime_type = PrimeType::UNKNOWN;
};

UIState *uiState();

// device management class
class Device : public QObject {
  Q_OBJECT

public:
  Device(QObject *parent = 0);
  bool isAwake() { return awake; }
  void setOffroadBrightness(int brightness) {
    offroad_brightness = std::clamp(brightness, 0, 100);
  }

private:
  bool awake = false;
  int interactive_timeout = 0;
  bool ignition_on = false;

  int offroad_brightness = BACKLIGHT_OFFROAD;
  int last_brightness = 0;
  FirstOrderFilter brightness_filter;
  QFuture<void> brightness_future;

  void updateBrightness(const UIState &s);
  void updateWakefulness(const UIState &s);
  void setAwake(bool on);

signals:
  void displayPowerChanged(bool on);
  void interactiveTimeout();

public slots:
  void resetInteractiveTimeout(int timeout = -1, int timeout_onroad = -1);
  void update(const UIState &s);
};

Device *device();

void ui_update_params(UIState *s);
int get_path_length_idx(const cereal::XYZTData::Reader &line, const float path_height);
void update_model(UIState *s,
                  const cereal::ModelDataV2::Reader &model,
                  const cereal::UiPlan::Reader &plan);
void update_dmonitoring(UIState *s, const cereal::DriverStateV2::Reader &driverstate, float dm_fade_state, bool is_rhd);
void update_leads(UIState *s, const cereal::RadarState::Reader &radar_state, const cereal::XYZTData::Reader &line);
void update_line_data(const UIState *s, const cereal::XYZTData::Reader &line,
                      float y_off, float z_off, QPolygonF *pvd, int max_idx, bool allow_invert);

// FrogPilot functions
void ui_update_frogpilot_params(UIState *s);
