#pragma once

#include "selfdrive/ui/qt/onroad/buttons.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"

const int widget_size = img_size + (UI_BORDER_SIZE / 2);

class FrogPilotAnnotatedCameraWidget : public QWidget {
  Q_OBJECT

public:
  explicit FrogPilotAnnotatedCameraWidget(QWidget *parent = 0);

  void mousePressEvent(QMouseEvent *mouseEvent) override;
  void paintAdjacentPaths(QPainter &p);
  void paintBlindSpotPath(QPainter &p);
  void paintFrogPilotWidgets(QPainter &p, UIState &s);
  void paintLeadMetrics(QPainter &p, bool adjacent, QPointF *chevron, const cereal::RadarState::LeadData::Reader &lead_data);
  void paintPathEdges(QPainter &p, int height);
  void paintRainbowPath(QPainter &p, QLinearGradient &bg, float lin_grad_point);
  void updateState(const UIState &s, const FrogPilotUIState &fs);

  bool hideBottomIcons = false;
  bool isCruiseSet;
  bool rightHandDM;

  int alertHeight;
  int speedLimitHeight = 0;
  int standstillDuration = 0;

  float speed;

  std::vector<QPointF> radar_tracks;

  FrogPilotUIScene frogpilot_scene = {};

  QColor blueColor(int alpha = 255) { return QColor(0, 0, 255, alpha); }
  QColor purpleColor(int alpha = 255) { return QColor(128, 0, 128, alpha); }
  QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }

  QJsonObject frogpilot_toggles;

  QPoint dmIconPosition;
  QPoint experimentalButtonPosition;

  QPolygonF track_adjacent_vertices[2];
  QPolygonF track_edge_vertices;
  QPolygonF track_vertices;

  QRect adjacentLeadTextRect;
  QRect leadTextRect;
  QRect setSpeedRect;

  QSize defaultSize;

  QString signalStyle;

protected:
  void showEvent(QShowEvent *event) override;

private:
  void paintCEMStatus(QPainter &p);
  void paintCompass(QPainter &p);
  void paintCurveSpeedControl(QPainter &p);
  void paintCurveSpeedControlTraining(QPainter &p);
  void paintLateralPaused(QPainter &p);
  void paintLongitudinalPaused(QPainter &p);
  void paintPedalIcons(QPainter &p);
  void paintPendingSpeedLimit(QPainter &p);
  void paintRadarTracks(QPainter &p);
  void paintRoadName(QPainter &p);
  void paintSpeedLimit(QPainter &p);
  void paintSpeedLimitSources(QPainter &p);
  void paintStandstillTimer(QPainter &p);
  void paintStoppingPoint(QPainter &p);
  void paintTurnSignals(QPainter &p);
  void paintWeather(QPainter &p);
  void updateCEMIcon();
  void updateSignals();
  void updateWeatherIcon();

  bool assetsLoaded = false;
  bool blindspotLeft;
  bool blindspotRight;
  bool blinkerLeft;
  bool blinkerRight;
  bool brakeLights;
  bool cscActive;
  bool cscTraining;
  bool experimentalMode;
  bool forceCoast;
  bool lateralPaused;
  bool longitudinalPaused;
  bool redLight;
  bool speedLimitChanged;
  bool weatherDaytime;

  int animationFrameIndex = 0;
  int desiredFollowDistance;
  int frogHopCount = 0;
  int signalAnimationLength = 0;
  int signalHeight = 0;
  int signalMovement = 0;
  int signalWidth = 0;
  int totalFrames = 0;
  int weatherId;

  float accelerationEgo;
  float cscSpeed;
  float dashboardSpeedLimit;
  float distanceConversion;
  float laneWidthLeft;
  float laneWidthRight;
  float mapSpeedLimit;
  float mapboxSpeedLimit;
  float nextSpeedLimit;
  float roadCurvature;
  float setSpeed;
  float slcOverriddenSpeed;
  float speedConversion;
  float speedConversionMetrics;
  float speedLimit;
  float stoppingDistance;
  float unconfirmedSpeedLimit;

  std::string speedLimitSource;

  Params params;
  Params params_memory{"", true};

  QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }
  QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }

  QElapsedTimer glowTimer;
  QElapsedTimer pendingLimitTimer;
  QElapsedTimer standstillTimer;

  QPixmap brakePedalImg;
  QPixmap curveSpeedIcon;
  QPixmap curveSpeedIconFlipped;
  QPixmap dashboardIcon;
  QPixmap gasPedalImg;
  QPixmap mapboxIcon;
  QPixmap mapDataIcon;
  QPixmap nextMapsIcon;
  QPixmap pausedIcon;
  QPixmap speedIcon;
  QPixmap stopSignImg;
  QPixmap turnIcon;

  QPoint cemStatusPosition;
  QPoint compassPosition;
  QPoint lateralPausedPosition;

  QRect newSpeedLimitRect;
  QRect speedLimitRect;

  QSharedPointer<QMovie> cemIcon;
  QSharedPointer<QMovie> weatherIcon;

  QString cemIconPath;
  QString leadDistanceUnit;
  QString leadSpeedUnit;
  QString roadName;
  QString speedLimitOffsetStr;
  QString speedUnit;
  QString weatherIconPath;

  QTimer *animationTimer;

  QVector<QPixmap> blindspotImages;
  QVector<QPixmap> blindspotImagesRight;
  QVector<QPixmap> signalImages;
  QVector<QPixmap> signalImagesRight;
};
