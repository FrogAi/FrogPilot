#pragma once

#include "selfdrive/ui/qt/onroad/buttons.h"

class DrivingPersonalityButton : public QPushButton {
  Q_OBJECT

public:
  explicit DrivingPersonalityButton(QWidget *parent = 0);

  void updateState(const UIState &s, const FrogPilotUIState &fs);

private:
  void paintEvent(QPaintEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void updateTheme();

  bool theme_updated = true;
  bool traffic_mode_active = false;

  int personality = 0;

  Params params_memory{"", true};

  QSharedPointer<QMovie> currentGif;

  QPixmap currentImg;
};

class ScreenRecorderButton : public QPushButton {
  Q_OBJECT

public:
  explicit ScreenRecorderButton(QWidget *parent = 0);

private:
  void paintEvent(QPaintEvent *event) override;
};
