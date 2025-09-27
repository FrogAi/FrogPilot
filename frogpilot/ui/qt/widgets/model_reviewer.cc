#include "frogpilot/ui/qt/widgets/model_reviewer.h"

static QLabel *addLabel(QWidget *parent, QVBoxLayout *layout, const QString &text, int fontSize=50) {
  QLabel *label = new QLabel(text, parent);
  label->setAlignment(Qt::AlignCenter);
  label->setStyleSheet(QString(R"(
    QLabel {
      color: #FFFFFF;
      font-size: %2px;
      font-weight: bold;
    }
  )").arg(fontSize));
  layout->addWidget(label);
  return label;
}

static QLabel *addTitleLabel(QWidget *parent, QVBoxLayout *layout, const QString &text) {
  QLabel *label = new QLabel(text, parent);
  label->setAlignment(Qt::AlignCenter);
  label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  label->setStyleSheet(R"(
    QLabel {
      background-color: #444444;
      border-radius: 12px;
      color: #FFFFFF;
      font-size: 50px;
      font-weight: bold;
      padding: 12px 28px;
    }
  )");
  label->setMaximumHeight(label->sizeHint().height());
  layout->addWidget(label);
  layout->addSpacing(10);
  return label;
}

static QPushButton *createButton(QWidget *parent, const QString &text, const QString &name, int rating, int width, int height) {
  QPushButton *button = new QPushButton(text, parent);
  button->setFixedSize(width, height);
  button->setObjectName(name);
  button->setProperty("rating", rating);
  return button;
}

static QWidget *createStatBox(const QString &title, QLabel **valueLabel, QWidget *parent) {
  QWidget *box = new QWidget(parent);

  QVBoxLayout *layout = new QVBoxLayout(box);
  layout->setAlignment(Qt::AlignCenter);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(10);

  QLabel *statTitleLabel = new QLabel(title, box);
  statTitleLabel->setAlignment(Qt::AlignCenter);
  statTitleLabel->setStyleSheet(R"(
    QLabel {
      color: #AAAAAA;
      font-size: 40px;
      font-weight: 600;
    }
  )");
  layout->addWidget(statTitleLabel);

  QLabel *value = new QLabel("-", box);
  value->setAlignment(Qt::AlignCenter);
  value->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  value->setStyleSheet(R"(
    QLabel {
      color: #FFFFFF;
      font-size: 75px;
      font-weight: bold;
    }
  )");
  *valueLabel = value;
  layout->addWidget(value);

  return box;
}

FrogPilotModelReview::FrogPilotModelReview(QWidget *parent) : QFrame(parent) {
  mainLayout = new QStackedLayout(this);

  QVBoxLayout *ratingLayout = new QVBoxLayout();
  ratingLayout->setContentsMargins(50, 25, 50, 25);

  addTitleLabel(this, ratingLayout, tr("Drive Rating Selection"));
  ratingLayout->addStretch(1);

  QVBoxLayout *ratingGroup = new QVBoxLayout();
  ratingGroup->setAlignment(Qt::AlignCenter);
  addLabel(this, ratingGroup, tr("How would you rate that drive?"));

  QHBoxLayout *row = new QHBoxLayout();
  row->setSpacing(25);

  QList<QPair<QString, int>> ratings{
    {QStringLiteral("🤩"), 100},
    {QStringLiteral("🙂"), 80},
    {QStringLiteral("🤔"), 60},
    {QStringLiteral("🙁"), 40},
    {QStringLiteral("🤕"), 20}
  };

  for (const QPair<QString, int> &rating : ratings) {
    QPushButton *button = createButton(this, rating.first, "ratingButton", rating.second, 150, 150);
    QObject::connect(button, &QPushButton::clicked, this, &FrogPilotModelReview::onRatingButtonClicked);
    row->addWidget(button);
  }

  ratingGroup->addLayout(row);
  ratingLayout->addLayout(ratingGroup);
  ratingLayout->addStretch(1);

  QVBoxLayout *blacklistGroup = new QVBoxLayout();
  blacklistGroup->setAlignment(Qt::AlignCenter);
  addLabel(this, blacklistGroup, tr("Blacklist this model to remove it from rotation"), 40);

  blacklistButton = createButton(this, tr("Blacklist Model"), "blacklistButton", 0, 600, 100);
  QObject::connect(blacklistButton, &QPushButton::clicked, this, &FrogPilotModelReview::onBlacklistButtonClicked);
  blacklistGroup->addWidget(blacklistButton, 0, Qt::AlignCenter);

  ratingLayout->addLayout(blacklistGroup);

  QWidget *ratingWidget = new QWidget(this);
  ratingWidget->setLayout(ratingLayout);
  mainLayout->addWidget(ratingWidget);

  QVBoxLayout *modelInfoLayout = new QVBoxLayout();
  modelInfoLayout->setContentsMargins(50, 25, 50, 20);

  addLabel(this, modelInfoLayout, tr("Model used during that drive:"));

  modelLabel = new QLabel("", this);
  modelLabel->setAlignment(Qt::AlignCenter);
  modelLabel->setStyleSheet(R"(
    QLabel {
      background-color: #444444;
      border-radius: 12px;
      color: #FFFFFF;
      font-size: 65px;
      font-weight: bold;
      padding: 12px 24px;
    }
  )");
  modelInfoLayout->addWidget(modelLabel);

  modelInfoLayout->addSpacing(50);

  QVBoxLayout *bottomLayout = new QVBoxLayout();
  bottomLayout->addWidget(createStatBox(tr("Model Rank"), &modelRankLabel, this));
  bottomLayout->addWidget(createStatBox(tr("Model Rating"), &modelRatingLabel, this));
  bottomLayout->addWidget(createStatBox(tr("Model Drives"), &totalDrivesLabel, this));
  bottomLayout->addWidget(createStatBox(tr("Total Drives"), &totalOverallDrivesLabel, this));

  blacklistMessageLabel = new QLabel("", this);
  blacklistMessageLabel->setAlignment(Qt::AlignCenter);
  blacklistMessageLabel->setStyleSheet(R"(
    QLabel {
      color: #C92231;
      font-size: 50px;
      font-weight: bold;
    }
  )");
  bottomLayout->addWidget(blacklistMessageLabel);

  modelInfoLayout->addLayout(bottomLayout);

  QWidget *modelInfoWidget = new QWidget(this);
  modelInfoWidget->setLayout(modelInfoLayout);
  mainLayout->addWidget(modelInfoWidget);

  setStyleSheet(R"(
    FrogPilotModelReview {
      background-color: #333333;
    }
    QPushButton#blacklistButton {
      background-color: #444444;
      border-radius: 12px;
      color: #C92231;
      font-size: 45px;
      font-weight: bold;
      padding: 12px 24px;
    }
    QPushButton#ratingButton {
      background-color: #444444;
      border-radius: 12px;
      color: #FFFFFF;
      font-size: 100px;
      font-weight: bold;
      padding: 12px 24px;
    }
    QPushButton#ratingButton:hover {
      background-color: #666666;
    }
  )");

  QObject::connect(device(), &Device::interactiveTimeout, [this]() {
    if (isVisible()) {
      emit driveRated();
      modelRated = false;
    }
  });
  QObject::connect(uiState(), &UIState::offroadTransition, [this](bool offroad) {
    if (!offroad) {
      emit driveRated();
      modelRated = false;
    }
  });
}

void FrogPilotModelReview::mousePressEvent(QMouseEvent *e) {
  if (mainLayout->currentIndex() == 1) {
    emit driveRated();
  }
}

void FrogPilotModelReview::showEvent(QShowEvent *event) {
  QStringList availableModels = QString::fromStdString(params.get("AvailableModels")).split(",", QString::SkipEmptyParts);
  availableModelNames = QString::fromStdString(params.get("AvailableModelNames")).split(",", QString::SkipEmptyParts);
  blacklistedModels = QString::fromStdString(params.get("BlacklistedModels")).split(",", QString::SkipEmptyParts);

  blacklistButton->setVisible(!(QSet<QString>::fromList(availableModels) - QSet<QString>::fromList(blacklistedModels)).isEmpty());

  QMap<QString, QString> modelMap;
  for (int i = 0; i < qMin(availableModels.size(), availableModelNames.size()); ++i) {
    modelMap.insert(availableModels[i], processModelName(availableModelNames[i]));
  }
  currentModel = frogpilotUIState()->frogpilot_toggles.value("model").toString();
  currentModelFiltered = modelMap.value(currentModel);

  modelDrivesAndScores = QJsonDocument::fromJson(QString::fromStdString(params.get("ModelDrivesAndScores")).toUtf8()).object();
  currentModelData = modelDrivesAndScores.value(currentModelFiltered).toObject();

  mainLayout->setCurrentIndex(modelRated ? 1 : 0);
}

int FrogPilotModelReview::getModelRank() {
  QList<QPair<QString, int>> modelRatings;
  totalOverallDrives = 0;

  for (const QString &model : availableModelNames) {
    QString processedModel = processModelName(model);

    QJsonObject modelData = modelDrivesAndScores.value(processedModel).toObject();
    int modelDrives = modelData.value("Drives").toInt();
    int modelRating = modelData.value("Score").toInt();
    totalOverallDrives += modelDrives;

    if (modelRating > 0) {
      modelRatings.append(qMakePair(processedModel, modelRating));
    }
  }

  std::sort(modelRatings.begin(), modelRatings.end(), [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
    if (a.second == b.second) {
      return a.first < b.first;
    }
    return a.second > b.second;
  });
  for (int i = 0; i < modelRatings.size(); ++i) {
    if (modelRatings[i].first == currentModelFiltered) {
      return i + 1;
    }
  }
  return modelRatings.size();
}

void FrogPilotModelReview::onBlacklistButtonClicked() {
  totalDrives = currentModelData.value("Drives").toInt() + 1;
  currentModelData["Drives"] = totalDrives;
  currentModelData["Score"] = 0;
  modelDrivesAndScores[currentModelFiltered] = currentModelData;

  if (!blacklistedModels.contains(currentModel)) {
    blacklistedModels.append(currentModel);
    params.put("BlacklistedModels", blacklistedModels.join(",").toStdString());
  }

  params.put("ModelDrivesAndScores", QJsonDocument(modelDrivesAndScores).toJson(QJsonDocument::Compact).toStdString());

  blacklistMessageLabel->setText(tr("Model successfully blacklisted!"));

  updateLabel();
}

void FrogPilotModelReview::onRatingButtonClicked() {
  int modelDrives = currentModelData.value("Drives").toInt();
  int modelRating = currentModelData.value("Score").toInt();

  totalDrives = modelDrives + 1;
  finalRating = ((modelRating * modelDrives) + sender()->property("rating").toInt()) / totalDrives;

  currentModelData["Drives"] = totalDrives;
  currentModelData["Score"] = finalRating;
  modelDrivesAndScores[currentModelFiltered] = currentModelData;

  params.put("ModelDrivesAndScores", QJsonDocument(modelDrivesAndScores).toJson(QJsonDocument::Compact).toStdString());

  modelRated = true;

  updateLabel();
}

void FrogPilotModelReview::updateLabel() {
  modelLabel->setText(currentModelFiltered);
  modelRankLabel->setText(tr("#%1").arg(getModelRank()));
  modelRatingLabel->setText(tr("%1%").arg(finalRating));
  totalDrivesLabel->setText(tr("%1 %2").arg(totalDrives).arg(totalDrives == 1 ? tr("Drive") : tr("Drives")));
  totalOverallDrivesLabel->setText(tr("%1 Total %2").arg(totalOverallDrives).arg(totalOverallDrives == 1 ? tr("Drive") : tr("Drives")));

  mainLayout->setCurrentIndex(1);
}
