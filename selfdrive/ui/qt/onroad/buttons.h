#pragma once

#include <QPushButton>

#include "selfdrive/ui/ui.h"

const int btn_size = 192;
const int img_size = (btn_size / 4) * 3;

class ExperimentalButton : public QPushButton {
  Q_OBJECT

public:
  explicit ExperimentalButton(QWidget *parent = 0);
  void updateState(const UIState &s);

private:
  void paintEvent(QPaintEvent *event) override;
  void changeMode();

  Params params;
  QPixmap engage_img;
  QPixmap experimental_img;
  bool experimental_mode;
  bool engageable;

  // FrogPilot variables
  bool conditionalExperimental;

  int conditionalStatus;

  Params paramsMemory{"/dev/shm/params"};
};


class MapSettingsButton : public QPushButton {
  Q_OBJECT

public:
  explicit MapSettingsButton(QWidget *parent = 0);

private:
  void paintEvent(QPaintEvent *event) override;

  QPixmap settings_img;
};

void drawIcon(QPainter &p, const QPoint &center, const QPixmap &img, const QBrush &bg, float opacity);

// FrogPilot buttons
class DistanceButton : public QPushButton {
  Q_OBJECT

public:
  explicit DistanceButton(QWidget *parent = 0);
  void updateState(const UIScene &scene);

private:
  void buttonPressed();
  void buttonReleased();
  void paintEvent(QPaintEvent *event) override;

  bool trafficModeActive;

  int elapsed;
  int personality;

  QElapsedTimer transitionTimer;

  QPixmap profileImage;
  QString profileText;

  Params paramsMemory{"/dev/shm/params"};

  const QVector<std::pair<QPixmap, QString>> profileData = {
    {QPixmap("../frogpilot/assets/other_images/traffic.png"), "Traffic"},
    {QPixmap("../frogpilot/assets/other_images/aggressive.png"), "Aggressive"},
    {QPixmap("../frogpilot/assets/other_images/standard.png"), "Standard"},
    {QPixmap("../frogpilot/assets/other_images/relaxed.png"), "Relaxed"}
  };

  const QVector<std::pair<QPixmap, QString>> profileDataKaofui = {
    {QPixmap("../frogpilot/assets/other_images/traffic_kaofui.png"), "Traffic"},
    {QPixmap("../frogpilot/assets/other_images/aggressive_kaofui.png"), "Aggressive"},
    {QPixmap("../frogpilot/assets/other_images/standard_kaofui.png"), "Standard"},
    {QPixmap("../frogpilot/assets/other_images/relaxed_kaofui.png"), "Relaxed"}
  };
};
