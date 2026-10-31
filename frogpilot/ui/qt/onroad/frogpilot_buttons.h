#pragma once

#include "selfdrive/ui/qt/onroad/buttons.h"

const int personality_button_width = btn_size + UI_BORDER_SIZE;

class DrivingPersonalityButton : public QPushButton {
  Q_OBJECT

public:
  explicit DrivingPersonalityButton(QWidget *parent = 0);

  void updateState(const UIState &s, const FrogPilotUIState &fs);

  bool rightHandDM = false;

private:
  void hideEvent(QHideEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void updateTheme();

  QSharedPointer<FrogPilotMovie> currentGif;

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

class ScreenRecorderButton : public QPushButton {
  Q_OBJECT

public:
  explicit ScreenRecorderButton(QWidget *parent = 0);

private:
  void paintEvent(QPaintEvent *event) override;
  void updateState();

  bool recording = false;
};
