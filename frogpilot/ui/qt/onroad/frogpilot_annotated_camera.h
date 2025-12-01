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

protected:
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void drawOutlinedText(QPainter &p, const QPointF &position, const QString &text);
  bool blindspotLeft;
  bool blindspotRight;

  float distanceConversion;
  float speedConversion;
  float speedConversionMetrics;

  QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }
  QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }

  QString leadDistanceUnit;
  QString leadSpeedUnit;
  QString speedUnit;
};
