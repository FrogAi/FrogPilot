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
  void showEvent(QShowEvent *event) override;

private:
  void rateDrive(int rating);

  bool driveRandomized = false;

  Params params;

  QElapsedTimer driveTimer;

  QLabel *blacklistLabel;
  QLabel *modelDrivesLabel;
  QLabel *modelLabel;
  QLabel *modelRankLabel;
  QLabel *modelRatingLabel;
  QLabel *resultLabel;
  QLabel *totalDrivesLabel;

  QPushButton *blacklistButton;

  QStackedLayout *mainLayout;

  QString driveModel;
  QString driveModelName;

  QWidget *statsWidget;
};
