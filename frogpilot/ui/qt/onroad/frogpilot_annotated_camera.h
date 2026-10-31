#pragma once

#include "frogpilot/ui/qt/onroad/frogpilot_buttons.h"

const int widget_size = img_size + (UI_BORDER_SIZE / 2);

class FrogPilotAnnotatedCameraWidget : public QWidget {
  Q_OBJECT

public:
  explicit FrogPilotAnnotatedCameraWidget(QWidget *parent = 0);

  static void drawOutlinedText(QPainter &p, const QPointF &position, const QString &text);

  void paintAdjacentPaths(QPainter &p);
  void paintBlindSpotPath(QPainter &p);
  void paintFrogPilotWidgets(QPainter &p);
  void paintLeadMetrics(QPainter &p, bool adjacent, QPointF *chevron, float leadDistance, float vLead);
  void paintPathEdges(QPainter &p);
  void paintRainbowPath(QLinearGradient &bg, float lin_grad_point);
  void paintWorldWidgets(QPainter &p);
  void updateState(const UIState &s, const FrogPilotUIState &fs);

  bool bigMapOpen = false;
  bool driverView = false;
  bool hideBottomIcons = false;
  bool isCruiseSet = false;
  bool mapButtonEnabled = false;
  bool mutcdSpeedLimit = false;
  bool rightHandDM = false;
  bool viennaSpeedLimit = false;

  int alertHeight = 0;
  int standstillDuration = 0;

  float speed = 0.0f;
  float speedConversion = 0.0f;

  FrogPilotUIScene frogpilot_scene = {};

  QColor blueColor(int alpha = 255) { return QColor(0, 0, 255, alpha); }
  QColor purpleColor(int alpha = 255) { return QColor(128, 0, 128, alpha); }
  QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }

  QJsonObject frogpilot_toggles;

  QPoint dmIconPosition;
  QPoint experimentalButtonPosition;

  QRect adjacentLeadTextRect;
  QRect newSpeedLimitRect;
  QRect setSpeedRect;
  QRect speedLimitRect;

  QSize defaultSize;

  QString signalStyle;
  QString speedLimitOffsetStr;
  QString speedUnit;

  QVector<QRect> leadTextRects;

protected:
  void hideEvent(QHideEvent *event) override;

private:
  QLinearGradient adjacentPathGradient(float hue);
  void paintCEMStatus(QPainter &p, const QPoint &position);
  void paintCompass(QPainter &p, const QPoint &position);
  void paintCurveSpeedControl(QPainter &p);
  void paintPausedIcon(QPainter &p, const QPoint &position, const QPixmap &icon);
  void paintPedalIcons(QPainter &p);
  void paintPendingSpeedLimit(QPainter &p);
  void paintRadarTracks(QPainter &p);
  void paintRoadName(QPainter &p);
  void paintSignFrame(QPainter &p, const QRect &rect, const QPen &pen, bool vienna);
  void paintSpeedLimitSources(QPainter &p);
  void paintStandstillTimer(QPainter &p);
  void paintStoppingPoint(QPainter &p);
  void paintTurnSignals(QPainter &p);
  void paintWeather(QPainter &p, const QPoint &position);
  void paintWidgetFrame(QPainter &p, const QRect &rect, const QColor &borderColor);
  void updateCEMIcon();
  void updateIcon(const QString &path, QSharedPointer<FrogPilotMovie> &icon, QString &iconPath);
  void updateSignals();
  void updateWeatherIcon();

  bool blindspotLeft = false;
  bool blindspotRight = false;
  bool blinkerLeft = false;
  bool blinkerRight = false;
  bool brakeLights = false;
  bool cscActive = false;
  bool cscTraining = false;
  bool experimentalMode = false;
  bool forceCoast = false;
  bool lateralPaused = false;
  bool longitudinalPaused = false;
  bool redLight = false;
  bool signalIsGif = false;
  bool speedLimitChanged = false;
  bool weatherDaytime = false;

  int desiredFollowDistance = 0;
  int signalAnimationLength = 0;
  int signalHeight = 0;
  int signalWidth = 0;
  int weatherId = 0;

  float accelerationEgo = 0.0f;
  float cameraSpeedLimit = 0.0f;
  float cscSpeed = 0.0f;
  float dashboardSpeedLimit = 0.0f;
  float distanceConversion = 0.0f;
  float gpsBearing = 0.0f;
  float hueOffset = 0.0f;
  float laneWidthLeft = 0.0f;
  float laneWidthRight = 0.0f;
  float maneuverDistance = 0.0f;
  float mapboxSpeedLimit = 0.0f;
  float mapSpeedLimit = 0.0f;
  float navigationSpeedLimit = 0.0f;
  float nextSpeedLimit = 0.0f;
  float roadCurvature = 0.0f;
  float speedConversionMetrics = 0.0f;
  float stoppingDistance = 0.0f;
  float unconfirmedSpeedLimit = 0.0f;
  float vEgo = 0.0f;

  std::string speedLimitSource;

  DrivingPersonalityButton *personalityButton;
  InstantReplayButton *instantReplayButton;
  ScreenRecorderButton *screenRecorderButton;

  Params params_memory{"/dev/shm/params"};

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
  QPixmap mapDataIcon;
  QPixmap navigationIcon;
  QPixmap nextMapsIcon;
  QPixmap pausedIcon;
  QPixmap speedIcon;
  QPixmap stopSignImg;
  QPixmap turnIcon;

  QRect roadNameRect;
  QRect sourcesRect;
  QRect standstillTimerRect;
  QRect staticSignalRect;

  QSharedPointer<FrogPilotMovie> cemIcon;
  QSharedPointer<FrogPilotMovie> weatherIcon;

  QString cemIconPath;
  QString leadDistanceUnit;
  QString leadSpeedUnit;
  QString roadName;
  QString weatherIconPath;

  QVector<QPixmap> blindspotImages;
  QVector<QPixmap> signalImages;
};
