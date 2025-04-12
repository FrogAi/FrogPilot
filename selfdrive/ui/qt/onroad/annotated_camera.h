#pragma once

#include <QVBoxLayout>
#include <memory>

#include "selfdrive/ui/qt/onroad/buttons.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"

#include "selfdrive/frogpilot/screenrecorder/screenrecorder.h"

class PedalIcons : public QWidget {
  Q_OBJECT

public:
  explicit PedalIcons(QWidget *parent = 0);
  void updateState(const UIScene &scene);

private:
  void paintEvent(QPaintEvent *event) override;

  QPixmap brake_pedal_img;
  QPixmap gas_pedal_img;

  bool accelerating;
  bool brakeLightOn;
  bool decelerating;
  bool dynamicPedals;
  bool standstill;
  bool staticPedals;

  float acceleration;
};

class AnnotatedCameraWidget : public CameraWidget {
  Q_OBJECT

public:
  explicit AnnotatedCameraWidget(VisionStreamType type, QWidget* parent = 0);
  void updateState(int alert_height, const UIState &s);

  MapSettingsButton *map_settings_btn;

  // FrogPilot variables
  QRect newSpeedLimitRect;

  QString accelerationUnit;

  float accelerationConversion;

private:
  void drawText(QPainter &p, int x, int y, const QString &text, int alpha = 255, bool overridePen = false);

  QVBoxLayout *main_layout;
  ExperimentalButton *experimental_btn;
  QPixmap dm_img;
  float speed;
  QString speedUnit;
  float setSpeed;
  float speedLimit;
  bool is_cruise_set = false;
  bool is_metric = false;
  bool dmActive = false;
  bool hideBottomIcons = false;
  bool rightHandDM = false;
  float dm_fade_state = 1.0;
  bool has_us_speed_limit = false;
  bool has_eu_speed_limit = false;
  bool v_ego_cluster_seen = false;
  int status = STATUS_DISENGAGED;
  std::unique_ptr<PubMaster> pm;

  int skip_frame_count = 0;
  bool wide_cam_requested = false;

  // FrogPilot widgets
  void drawCEMStatus(QPainter &p);
  void drawLateralPaused(QPainter &p);
  void drawLongitudinalPaused(QPainter &p);
  void drawRadarTracks(QPainter &p);
  void drawRoadName(QPainter &p);
  void drawTurnSignals(QPainter &p);
  void initializeFrogPilotWidgets();
  void paintFrogPilotWidgets(QPainter &painter);
  void updateFrogPilotVariables(int alert_height, const UIScene &scene);
  void updateSignals();

  // FrogPilot variables
  Params params_memory{"/dev/shm/params"};

  DistanceButton *distance_btn;
  PedalIcons *pedal_icons;
  ScreenRecorder *screenRecorder;

  QPixmap chillModeIcon;
  QPixmap curveIcon;
  QPixmap curveSpeedLeftIcon;
  QPixmap curveSpeedRightIcon;
  QPixmap dashboardIcon;
  QPixmap experimentalModeIcon;
  QPixmap leadIcon;
  QPixmap lightIcon;
  QPixmap mapDataIcon;
  QPixmap navigationIcon;
  QPixmap pausedIcon;
  QPixmap speedIcon;
  QPixmap stopSignImg;
  QPixmap turnIcon;
  QPixmap upcomingMapsIcon;

  QPoint cemIconPosition;
  QPoint dmIconPosition;
  QPoint lateralIconPosition;

  QRect leadTextRect;

  QString leadDistanceUnit;
  QString leadSpeedUnit;
  QString signalStyle;

  QTimer *animationTimer;

  QVector<QPixmap> blindspotImages;
  QVector<QPixmap> signalImages;

  bool bigMapOpen;
  bool blindSpotLeft;
  bool blindSpotRight;
  bool cemStatus;
  bool cscStatus;
  bool compass;
  bool experimentalMode;
  bool hideMapIcon;
  bool hideMaxSpeed;
  bool hideSpeed;
  bool hideSpeedLimit;
  bool lateralPaused;
  bool leadInfo;
  bool leftCurve;
  bool longitudinalPaused;
  bool mapOpen;
  bool mtscEnabled;
  bool onroadDistanceButton;
  bool radarTracks;
  bool roadNameUI;
  bool showSLCOffset;
  bool slcOverridden;
  bool speedLimitChanged;
  bool speedLimitSources;
  bool stscControllingCurve;
  bool stscEnabled;
  bool trafficMode;
  bool turnSignalAnimation;
  bool turnSignalLeft;
  bool turnSignalRight;
  bool useStockColors;
  bool useSI;
  bool useVienna;
  bool vtscControllingCurve;
  bool vtscEnabled;

  float dashboardSpeedLimit;
  float distanceConversion;
  float laneDetectionWidth;
  float mapboxSpeedLimit;
  float mapsSpeedLimit;
  float mtscSpeed;
  float navigationSpeedLimit;
  float slcSpeedLimitOffset;
  float speedConversion;
  float speedConversionMetrics;
  float stscSpeed;
  float unconfirmedSpeedLimit;
  float upcomingSpeedLimit;
  float vtscSpeed;

  int alertHeight;
  int animationFrameIndex;
  int cameraView;
  int conditionalStatus;
  int desiredFollow;
  int modelLength;
  int signalAnimationLength;
  int signalHeight;
  int signalMovement;
  int signalWidth;
  int standstillDuration;
  int totalFrames;

  QColor blueColor(int alpha = 255) { return QColor(0, 0, 255, alpha); }
  QColor greenColor(int alpha = 242) { return QColor(23, 134, 68, alpha); }
  QColor orangeColor(int alpha = 255) { return QColor(255, 165, 0, alpha); }
  QColor purpleColor(int alpha = 255) { return QColor(128, 0, 128, alpha); }
  QColor yellowColor(int alpha = 255) { return QColor(255, 255, 0, alpha); }

  QString speedLimitSource;

protected:
  void paintGL() override;
  void initializeGL() override;
  void showEvent(QShowEvent *event) override;
  void updateFrameMat() override;
  void drawLaneLines(QPainter &painter, const UIState *s, float v_ego);
  void drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd, float v_ego, const QColor &lead_marker_color, bool adjacent = false);
  void drawHud(QPainter &p);
  void drawDriverState(QPainter &painter, const UIState *s);
  void paintEvent(QPaintEvent *event) override;
  inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  inline QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }
  inline QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }

  double prev_draw_t = 0;
  FirstOrderFilter fps_filter;
};
