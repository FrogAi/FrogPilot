#pragma once

#include "selfdrive/ui/qt/onroad/buttons.h"

class InstantReplayButton : public QPushButton {
  Q_OBJECT

public:
  explicit InstantReplayButton(QWidget *parent = 0);

private:
  void paintEvent(QPaintEvent *event) override;
  void updateState(const UIState &s, const FrogPilotUIState &fs);
};
