#pragma once

#include <QMovie>

#include "selfdrive/ui/qt/onroad/buttons.h"

class DistanceButton : public QPushButton {
  Q_OBJECT

public:
  explicit DistanceButton(QWidget *parent = 0);

  void updateState(const UIScene &scene, const FrogPilotUIScene &frogpilot_scene);

protected:
  void showEvent(QShowEvent *event) override;

private:
  void paintEvent(QPaintEvent *event) override;

  bool traffic_mode_active;

  int personality;

  Params params_memory{"/dev/shm/params"};

  QMap<int, QPair<QPixmap, QMovie*>> icon_map;
};
