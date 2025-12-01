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
  QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }

  QPoint dmIconPosition;
  QPoint experimentalButtonPosition;

  QRect setSpeedRect;

  QSize defaultSize;

protected:
  void hideEvent(QHideEvent *event) override;

private:
  float distanceConversion = 0.0f;
  float speedConversion = 0.0f;
  float speedConversionMetrics = 0.0f;

  QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }

  QString leadDistanceUnit;
  QString leadSpeedUnit;
  QString speedUnit;
};
