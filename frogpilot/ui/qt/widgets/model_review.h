#pragma once

#include "selfdrive/ui/ui.h"

class FrogPilotModelReview : public QFrame {
  Q_OBJECT

public:
  explicit FrogPilotModelReview(QWidget *parent = nullptr);

  bool reviewReady();

signals:
  void driveRated();

protected:
  void mousePressEvent(QMouseEvent *e) override;

private:
  void rateDrive(int rating);

  bool driveRandomized = false;

  Params params;

  QElapsedTimer driveTimer;

  QLabel *modelLabel;
  QLabel *resultLabel;

  QStackedLayout *mainLayout;

  QString driveModel;
  QString driveModelName;
};
