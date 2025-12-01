#pragma once

#include "selfdrive/ui/qt/onroad/buttons.h"

class DrivingPersonalityButton : public QPushButton {
  Q_OBJECT

public:
  explicit DrivingPersonalityButton(QWidget *parent = 0);

  void updateState(const UIState &s, const FrogPilotUIState &fs);

private:
  void hideEvent(QHideEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void updateTheme();

  QSharedPointer<QMovie> currentGif;

  QPixmap currentImg;

  QString currentIcon;
};

class InstantReplayButton : public QPushButton {
  Q_OBJECT

public:
  explicit InstantReplayButton(QWidget *parent = 0);

private:
  void paintEvent(QPaintEvent *event) override;
  void updateState(const UIState &s, const FrogPilotUIState &fs);
};
