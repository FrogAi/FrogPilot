#pragma once

#include "frogpilot/ui/qt/onroad/frogpilot_buttons.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"

const int widget_size = img_size + (UI_BORDER_SIZE / 2);

class FrogPilotAnnotatedCameraWidget : public QWidget {
  Q_OBJECT

public:
  explicit FrogPilotAnnotatedCameraWidget(CameraWidget *nvg, QWidget *parent = 0);

  void mousePressEvent(QMouseEvent *mouseEvent) override;
  void paintBlindSpotPath(QPainter &p);
  void paintFrogPilotWidgets(QPainter &p);
  void updateState(const UIState &s, const FrogPilotUIState &fs);

  bool hideBottomIcons = false;
  bool isCruiseSet = false;
  bool rightHandDM;

  int alertHeight;
  int standstillDuration = 0;

  float speed = 0;

  FrogPilotUIScene frogpilot_scene = {};

  QColor blueColor(int alpha = 255) { return QColor(0, 0, 255, alpha); }
  QColor purpleColor(int alpha = 255) { return QColor(128, 0, 128, alpha); }
  QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }

  QJsonObject frogpilot_toggles;

  QPoint dmIconPosition;
  QPoint experimentalButtonPosition;

  QRect setSpeedRect;

  QSize defaultSize;

  QString signalStyle;

protected:
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void drawOutlinedText(QPainter &p, const QPointF &position, const QString &text);
  void paintCEMStatus(QPainter &p, const QPoint &position);
  void paintCompass(QPainter &p, const QPoint &position);
  void paintCurveSpeedControl(QPainter &p);
  void paintRoadName(QPainter &p);
  void paintStandstillTimer(QPainter &p);
  void paintTurnSignals(QPainter &p);
  void updateCEMIcon();
  void updateIcon(const QString &path, QSharedPointer<QMovie> &icon, QString &iconPath);
  void updateSignals();

  bool assetsLoaded = false;
  bool blindspotLeft;
  bool blindspotRight;
  bool blinkerLeft;
  bool blinkerRight;
  bool cscActive;
  bool cscTraining;
  bool experimentalMode;

  int signalAnimationLength = 0;
  int signalHeight = 0;
  int signalWidth = 0;
  int totalFrames = 0;

  float cscSpeed;
  float distanceConversion;
  float gpsBearing;
  float roadCurvature;
  float speedConversion;
  float speedConversionMetrics;

  InstantReplayButton *instantReplayButton;

  QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }
  QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }

  QElapsedTimer glowTimer;
  QElapsedTimer signalTimer;
  QElapsedTimer standstillTimer;

  QPixmap curveSpeedIcon;
  QPixmap curveSpeedIconFlipped;

  QRect roadNameRect;
  QRect standstillTimerRect;

  QSharedPointer<QMovie> cemIcon;

  QString cemIconPath;
  QString leadDistanceUnit;
  QString leadSpeedUnit;
  QString roadName;
  QString speedUnit;

  QVector<QPixmap> blindspotImages;
  QVector<QPixmap> signalImages;
};
