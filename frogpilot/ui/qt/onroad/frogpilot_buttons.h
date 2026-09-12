#pragma once

#include <QMovie>

#include "selfdrive/ui/qt/onroad/buttons.h"

class DistanceButton : public QPushButton {
  Q_OBJECT

public:
  explicit DistanceButton(QWidget *parent = 0);

  void updateState(const UIScene &scene, const FrogPilotUIScene &frogpilot_scene);

private:
  void paintEvent(QPaintEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void updateTheme();

  bool traffic_mode_active = false;

  int personality = 0;

  Params params_memory{"/dev/shm/params"};

  QPixmap icon_img;

  QSharedPointer<QMovie> icon_gif;
};

class ScreenRecorderButton : public QPushButton {
  Q_OBJECT

public:
  explicit ScreenRecorderButton(QWidget *parent = 0);

private:
  void paintEvent(QPaintEvent *event) override;
};
