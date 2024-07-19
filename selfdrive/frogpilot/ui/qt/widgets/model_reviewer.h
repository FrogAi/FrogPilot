#pragma once

#include <QComboBox>

#include "selfdrive/ui/ui.h"

class ModelReview : public QFrame {
  Q_OBJECT

public:
  explicit ModelReview(QWidget *parent = nullptr);

private slots:
  void onBlacklistButtonClicked();
  void onRatingButtonClicked();

private:
  int getModelRank();

  QLabel *addLabel(QVBoxLayout *layout, const QString &text, const QString &type);

  QPushButton *createButton(const QString &text, const QString &type, int rating, int width, int height);

  QString processModelName(const QString &modelName);

  void checkBlacklistButtonVisibility();
  void mousePressEvent(QMouseEvent *e) override;
  void showEvent(QShowEvent *event) override;
  void updateLabel();

  QStackedLayout *mainLayout;

  QVBoxLayout *ratingLayout;
  QVBoxLayout *modelInfoLayout;

  Params params;
  Params paramsMemory{"/dev/shm/params"};

  QLabel *blacklistMessageLabel;
  QLabel *modelLabel;
  QLabel *modelRankLabel;
  QLabel *modelScoreLabel;
  QLabel *questionLabel;
  QLabel *titleLabel;
  QLabel *totalDrivesLabel;
  QLabel *totalOverallDrivesLabel;

  QString currentModel;
  QString currentModelFiltered;

  QStringList blacklistedModels;

  QPushButton *blacklistButton;

  QList<QPushButton*> ratingButtons;

  bool modelRated;

  int finalRating;
  int totalDrives;
  int totalOverallDrives;
};
