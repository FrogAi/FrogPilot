#pragma once

#include "frogpilot/ui/qt/onroad/frogpilot_buttons.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"

const int widget_size = img_size + (UI_BORDER_SIZE / 2);

class FrogPilotAnnotatedCameraWidget : public QWidget {
  Q_OBJECT

public:
  explicit FrogPilotAnnotatedCameraWidget(CameraWidget *nvg, QWidget *parent = 0);

  void mousePressEvent(QMouseEvent *mouseEvent) override;
  bool needsAdjacentPaths() const;
  void paintAdjacentPaths(QPainter &p);
  void paintBlindSpotPath(QPainter &p);
  void paintFrogPilotWidgets(QPainter &p);
  void paintLeadMetrics(QPainter &p, bool adjacent, QPointF *chevron, const cereal::RadarState::LeadData::Reader &lead_data);
  void paintPathEdges(QPainter &p);
  void paintRainbowPath(QLinearGradient &bg, float lin_grad_point);
  void updateState(const UIState &s, const FrogPilotUIState &fs);

  bool hideBottomIcons = false;
  bool isCruiseSet = false;
  bool rightHandDM;

  int alertHeight;
  int speedLimitHeight = 0;
  int speedLimitSignHeight = 0;
  int standstillDuration = 0;

  float speed = 0;

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
  QRect setSpeedRect;

  QSize defaultSize;

  QString signalStyle;

  QVector<QRect> leadTextRects;

protected:
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void drawOutlinedText(QPainter &p, const QPointF &position, const QString &text);
  void paintCEMStatus(QPainter &p, const QPoint &position);
  void paintCompass(QPainter &p, const QPoint &position);
  void paintCurveSpeedControl(QPainter &p);
  void paintPausedIcon(QPainter &p, const QPoint &position, const QPixmap &icon);
  void paintPedalIcons(QPainter &p);
  void paintPendingSpeedLimit(QPainter &p);
  void paintRadarTracks(QPainter &p);
  void paintRoadName(QPainter &p);
  void paintSignFrame(QPainter &p, const QRect &rect, const QPen &pen, bool vienna);
  void paintSpeedLimit(QPainter &p);
  void paintSpeedLimitSources(QPainter &p);
  void paintStandstillTimer(QPainter &p);
  void paintStoppingPoint(QPainter &p);
  void paintTurnSignals(QPainter &p);
  void updateCEMIcon();
  void updateIcon(const QString &path, QSharedPointer<QMovie> &icon, QString &iconPath);
  void updateSignals();

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

  int desiredFollowDistance;
  int signalAnimationLength = 0;
  int signalHeight = 0;
  int signalWidth = 0;
  int totalFrames = 0;

  float accelerationEgo;
  float cameraSpeedLimit;
  float cscSpeed;
  float dashboardSpeedLimit;
  float distanceConversion;
  float gpsBearing;
  float hueOffset = 0.0f;
  float laneWidthLeft;
  float laneWidthRight;
  float mapboxSpeedLimit;
  float mapSpeedLimit;
  float nextSpeedLimit;
  float roadCurvature;
  float slcOverriddenSpeed;
  float speedConversion;
  float speedConversionMetrics;
  float speedLimit;
  float stoppingDistance;
  float unconfirmedSpeedLimit;
  float vEgo;

  std::string speedLimitSource;

  DrivingPersonalityButton *personalityButton;
  InstantReplayButton *instantReplayButton;
  ScreenRecorderButton *screenRecorderButton;

  QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }
  QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }

  QElapsedTimer glowTimer;
  QElapsedTimer pendingLimitTimer;
  QElapsedTimer signalTimer;
  QElapsedTimer standstillTimer;

  QPixmap brakePedalImg;
  QPixmap cameraIcon;
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

  QRect newSpeedLimitRect;
  QRect roadNameRect;
  QRect sourcesRect;
  QRect speedLimitRect;
  QRect standstillTimerRect;

  QSharedPointer<QMovie> cemIcon;

  QString cemIconPath;
  QString leadDistanceUnit;
  QString leadSpeedUnit;
  QString roadName;
  QString speedLimitOffsetStr;
  QString speedUnit;

  QVector<QPixmap> blindspotImages;
  QVector<QPixmap> signalImages;
};
