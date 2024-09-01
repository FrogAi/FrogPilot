#pragma once

#include "selfdrive/ui/ui.h"

class ModelReview : public QFrame {
  Q_OBJECT

public:
  explicit ModelReview(QWidget *parent = nullptr);

protected:
  void mousePressEvent(QMouseEvent *e) override;
  void showEvent(QShowEvent *event) override;

private slots:
  void onBlacklistButtonClicked();
  void onRatingButtonClicked();

private:
  int getModelRank();

  QLabel *addLabel(QVBoxLayout *layout, const QString &text, const QString &type);

  QPushButton *createButton(const QString &text, const QString &type, int rating, int width, int height);

  void checkBlacklistButtonVisibility();
  void setupModelInfoLayout();
  void setupRatingLayout();
  void updateLabel();

  QStackedLayout *mainLayout;

  QVBoxLayout *ratingLayout;
  QVBoxLayout *modelInfoLayout;

  QLabel *blacklistMessageLabel;
  QLabel *modelLabel;
  QLabel *modelRankLabel;
  QLabel *modelScoreLabel;
  QLabel *questionLabel;
  QLabel *titleLabel;
  QLabel *totalDrivesLabel;
  QLabel *totalOverallDrivesLabel;

  QPushButton *blacklistButton;

  QList<QPushButton*> ratingButtons;

  Params params;
  Params paramsMemory{"/dev/shm/params"};

  QString currentModel;
  QString currentModelFiltered;

  QStringList blacklistedModels;

  bool modelRated;

  int finalRating;
  int totalDrives;
  int totalOverallDrives;
};
