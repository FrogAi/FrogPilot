#include "selfdrive/frogpilot/ui/qt/widgets/model_reviewer.h"

ModelReview::ModelReview(QWidget *parent) : QFrame(parent) {
  mainLayout = new QStackedLayout(this);

  QVBoxLayout *ratingLayout = new QVBoxLayout();
  ratingLayout->setContentsMargins(50, 25, 50, 20);

  questionLabel = addLabel(ratingLayout, "What would you rate that drive?", "question");

  QHBoxLayout *ratingButtonsLayout = new QHBoxLayout();
  QStringList emojis = {"🤩", "🙂", "🤔", "🙁", "🤕"};
  QList<int> scores = {100, 80, 60, 40, 20};

  for (int i = 0; i < emojis.size(); ++i) {
    QPushButton *ratingButton = createButton(emojis[i], "rating_button", scores[i], 150, 150);
    ratingButtonsLayout->addWidget(ratingButton);
    QObject::connect(ratingButton, &QPushButton::clicked, this, &ModelReview::onRatingButtonClicked);
  }

  ratingLayout->addLayout(ratingButtonsLayout);

  blacklistButton = createButton("Blacklist this model", "blacklist_button", 0, 600, 100);
  QObject::connect(blacklistButton, &QPushButton::clicked, this, &ModelReview::onBlacklistButtonClicked);
  ratingLayout->addWidget(blacklistButton, 0, Qt::AlignCenter);

  QWidget *ratingWidget = new QWidget(this);
  ratingWidget->setLayout(ratingLayout);
  mainLayout->addWidget(ratingWidget);

  QVBoxLayout *modelInfoLayout = new QVBoxLayout();
  modelInfoLayout->setContentsMargins(50, 25, 50, 20);

  titleLabel = addLabel(modelInfoLayout, "The model used during that drive was:", "title");
  modelLabel = addLabel(modelInfoLayout, "", "model");

  modelInfoLayout->addItem(new QSpacerItem(20, 75, QSizePolicy::Minimum, QSizePolicy::Fixed));

  QVBoxLayout *bottomLayout = new QVBoxLayout();
  modelScoreLabel = addLabel(bottomLayout, "Current Model Score: 0", "score");
  modelRankLabel = addLabel(bottomLayout, "Current Model Rank: 0", "rank");
  totalDrivesLabel = addLabel(bottomLayout, "Total Model Drives: 0", "drives");
  totalOverallDrivesLabel = addLabel(bottomLayout, "Total Overall Model Drives: 0", "drives");
  blacklistMessageLabel = addLabel(bottomLayout, "", "blacklist_message");

  modelInfoLayout->addLayout(bottomLayout);

  QWidget *modelInfoWidget = new QWidget(this);
  modelInfoWidget->setLayout(modelInfoLayout);
  mainLayout->addWidget(modelInfoWidget);

  setStyleSheet(R"(
    ModelReview {
      background-color: #333333;
      border-radius: 5px;
    }
    QLabel[type="drives"], QLabel[type="question"], QLabel[type="rank"], QLabel[type="score"], QLabel[type="title"] {
      font-size: 50px;
      font-weight: semi-bold;
      color: #FFFFFF;
    }
    QLabel[type="model"] {
      font-size: 75px;
      font-weight: bold;
      color: #FFFFFF;
    }
    QLabel[type="blacklist_message"] {
      font-size: 40px;
      font-weight: bold;
      color: #C92231;
    }
    QPushButton[type="rating_button"] {
      font-size: 75px;
      font-weight: bold;
      padding: 10px;
      color: #FFFFFF;
      background-color: #555555;
      border: 2px solid #FFFFFF;
      border-radius: 5px;
    }
    QPushButton[type="rating_button"]:hover {
      background-color: #777777;
    }
    QPushButton[type="blacklist_button"] {
      font-size: 50px;
      font-weight: bold;
      padding: 10px;
      color: #C92231;
      background-color: #000000;
      border: 2px solid #FFFFFF;
      border-radius: 5px;
    }
  )");
}

QLabel *ModelReview::addLabel(QVBoxLayout *layout, const QString &text, const QString &type) {
  QLabel *label = new QLabel(text, this);
  label->setProperty("type", type);
  label->setAlignment(Qt::AlignCenter);
  layout->addWidget(label);
  return label;
}

QPushButton *ModelReview::createButton(const QString &text, const QString &type, int rating, int width, int height) {
  QPushButton *button = new QPushButton(text, this);
  button->setProperty("type", type);
  button->setProperty("rating", rating);
  button->setFixedSize(width, height);
  return button;
}

void ModelReview::showEvent(QShowEvent *event) {
  currentModel = QString::fromStdString(paramsMemory.get("CurrentModelName"));
  currentModelFiltered = processModelName(currentModel);

  mainLayout->setCurrentIndex(modelRated ? 1 : 0);

  checkBlacklistButtonVisibility();
}

void ModelReview::mousePressEvent(QMouseEvent *e) {
  if (mainLayout->currentIndex() != 1) {
    return;
  }

  paramsMemory.putBool("DriveRated", true);
}

void ModelReview::updateLabel() {
  totalDrivesLabel->setText(QString("Total Model Drives: %1").arg(totalDrives));
  modelLabel->setText(currentModel.remove(QRegularExpression("[🗺️👀📡]")).remove(QRegularExpression(" \\(Default\\)")));
  modelRankLabel->setText(QString("Current Model Rank: %1").arg(getModelRank()));
  modelScoreLabel->setText(QString("Current Model Score: %1").arg(finalRating));
  totalOverallDrivesLabel->setText(QString("Total Overall Drives: %1").arg(totalOverallDrives));

  mainLayout->setCurrentIndex(1);

  QTimer::singleShot(30000, [this]() {
    paramsMemory.putBool("DriveRated", true);
    modelRated = false;
  });
}

void ModelReview::onRatingButtonClicked() {
  int newRating = qobject_cast<QPushButton*>(sender())->property("rating").toInt();

  totalDrives = params.getInt((QString("%1Drives").arg(currentModelFiltered)).toStdString()) + 1;

  int currentRating = params.getInt((QString("%1Score").arg(currentModelFiltered)).toStdString());
  finalRating = (currentRating * (totalDrives - 1) + newRating) / totalDrives;

  params.putInt((QString("%1Drives").arg(currentModelFiltered)).toStdString(), totalDrives);
  params.putInt((QString("%1Score").arg(currentModelFiltered)).toStdString(), finalRating);

  modelRated = true;
  updateLabel();
}

void ModelReview::onBlacklistButtonClicked() {
  params.putInt(QString("%1Score").arg(currentModelFiltered).toStdString(), 0);

  totalDrives = params.getInt((QString("%1Drives").arg(currentModelFiltered)).toStdString()) + 1;

  params.putInt((QString("%1Drives").arg(currentModelFiltered)).toStdString(), totalDrives);

  if (!blacklistedModels.contains(currentModel)) {
    blacklistedModels.append(currentModel);
    params.put("BlacklistedModels", blacklistedModels.join(",").toStdString());
  }

  blacklistMessageLabel->setText("Model successfully blacklisted!");
  updateLabel();
}

void ModelReview::checkBlacklistButtonVisibility() {
  QStringList availableModels = QString::fromStdString(params.get("AvailableModels")).split(",");
  blacklistedModels = QString::fromStdString(params.get("BlacklistedModels")).split(",", QString::SkipEmptyParts);

  blacklistButton->setVisible(availableModels.size() > blacklistedModels.size());
}

int ModelReview::getModelRank() {
  QStringList availableModels = QString::fromStdString(params.get("AvailableModelsNames")).split(",");
  QList<QPair<QString, int>> modelScores;
  totalOverallDrives = 0;

  for (QString &model : availableModels) {
    QString processedModel = processModelName(model);
    int modelDrives = params.getInt((QString("%1Drives").arg(processedModel)).toStdString());
    int modelScore = params.getInt((QString("%1Score").arg(processedModel)).toStdString());

    totalOverallDrives += modelDrives;

    if (modelScore > 0) {
      modelScores.append(qMakePair(processedModel, modelScore));
    }
  }

  std::sort(modelScores.begin(), modelScores.end(), [](QPair<QString, int> &a, QPair<QString, int> &b) {
    return a.second > b.second;
  });

  for (int i = 0; i < modelScores.size(); ++i) {
    if (modelScores[i].first == currentModelFiltered) {
      return i + 1;
    }
  }

  return 1;
}
