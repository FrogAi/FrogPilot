#pragma once

#include "frogpilot/ui/qt/onroad/frogpilot_buttons.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"

const int widget_size = img_size + (UI_BORDER_SIZE / 2);

class FrogPilotAnnotatedCameraWidget : public QWidget {
  Q_OBJECT

public:
  explicit FrogPilotAnnotatedCameraWidget(CameraWidget *nvg, QWidget *parent = 0);

  static void drawOutlinedText(QPainter &p, const QPointF &position, const QString &text);

  void mousePressEvent(QMouseEvent *mouseEvent) override;
  bool needsAdjacentPaths() const;
  void paintAdjacentPaths(QPainter &p);
  void paintFrogPilotWidgets(QPainter &p);
  void paintWorldWidgets(QPainter &p);
  void updateState(const UIState &s, const FrogPilotUIState &fs);

  bool hideBottomIcons = false;
  bool isCruiseSet = false;
  bool rightHandDM = false;

  int alertHeight = 0;

  float speed = 0;

  FrogPilotUIScene frogpilot_scene = {};

  QColor blueColor(int alpha = 255) { return QColor(0, 0, 255, alpha); }
  QColor purpleColor(int alpha = 255) { return QColor(128, 0, 128, alpha); }
  QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }

  QJsonObject frogpilot_toggles;

  QPoint dmIconPosition;
  QPoint experimentalButtonPosition;

  QPolygonF track_adjacent_vertices[2];

  QRect setSpeedRect;

  QSize defaultSize;

protected:
  void hideEvent(QHideEvent *event) override;

private:
  QLinearGradient adjacentPathGradient(float hue);

  bool blindspotLeft = false;
  bool blindspotRight = false;
  bool blinkerLeft = false;
  bool blinkerRight = false;
  bool brakeLights = false;
  bool experimentalMode = false;

  float accelerationEgo = 0.0f;
  float dashboardSpeedLimit = 0.0f;
  float distanceConversion = 0.0f;
  float speedConversion = 0.0f;
  float speedConversionMetrics = 0.0f;
  float stoppingDistance = 0.0f;
  float vEgo = 0.0f;

  QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }

  QString leadDistanceUnit;
  QString leadSpeedUnit;
  QString speedUnit;
};
