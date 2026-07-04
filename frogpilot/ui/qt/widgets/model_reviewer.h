#pragma once

#include "selfdrive/ui/ui.h"

class FrogPilotModelReview : public QFrame {
  Q_OBJECT

public:
  explicit FrogPilotModelReview(QWidget *parent = nullptr);

signals:
  void driveRated();

protected:
  void mousePressEvent(QMouseEvent *e) override;
  void showEvent(QShowEvent *event) override;

private slots:
  void onBlacklistButtonClicked();
  void onRatingButtonClicked();

private:
  int getModelRank();

  void updateLabel();

  bool modelRated;

  int finalRating;
  int totalDrives;
  int totalOverallDrives;

  Params params;

  QJsonObject currentModelData;
  QJsonObject modelDrivesAndScores;

  QLabel *blacklistMessageLabel;
  QLabel *modelLabel;
  QLabel *modelRankLabel;
  QLabel *modelRatingLabel;
  QLabel *totalDrivesLabel;
  QLabel *totalOverallDrivesLabel;

  QPushButton *blacklistButton;

  QStackedLayout *mainLayout;

  QString currentModel;
  QString currentModelFiltered;

  QStringList availableModelNames;
  QStringList blacklistedModels;
};
