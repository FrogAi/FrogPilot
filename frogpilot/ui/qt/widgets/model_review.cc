#include "frogpilot/ui/qt/widgets/model_review.h"

static QLabel *addLabel(QVBoxLayout *layout, const QString &text, int fontSize, QWidget *parent) {
  QLabel *label = new QLabel(text, parent);
  label->setAlignment(Qt::AlignCenter);
  label->setStyleSheet(QString("QLabel { color: #FFFFFF; font-size: %1px; font-weight: bold; }").arg(fontSize));
  label->setWordWrap(true);
  layout->addWidget(label);
  return label;
}

FrogPilotModelReview::FrogPilotModelReview(QWidget *parent) : QFrame(parent) {
  mainLayout = new QStackedLayout(this);

  QWidget *ratingWidget = new QWidget(this);
  QVBoxLayout *ratingLayout = new QVBoxLayout(ratingWidget);
  ratingLayout->setContentsMargins(50, 25, 50, 25);

  ratingLayout->addStretch(1);
  addLabel(ratingLayout, tr("How would you rate that drive?"), 50, this);

  QHBoxLayout *ratingButtons = new QHBoxLayout();
  ratingButtons->setSpacing(25);

  const QList<QPair<QString, int>> ratings {
    {QStringLiteral("🤩"), 100},
    {QStringLiteral("🙂"), 80},
    {QStringLiteral("🤔"), 60},
    {QStringLiteral("🙁"), 40},
    {QStringLiteral("🤕"), 20}
  };

  for (const auto &[emoji, rating] : ratings) {
    QPushButton *ratingButton = new QPushButton(emoji, this);
    ratingButton->setFixedSize(150, 150);
    ratingButton->setObjectName("ratingButton");
    QObject::connect(ratingButton, &QPushButton::clicked, [score = rating, this]() {
      rateDrive(score);
    });
    ratingButtons->addWidget(ratingButton);
  }

  ratingLayout->addLayout(ratingButtons);
  ratingLayout->addStretch(1);

  addLabel(ratingLayout, tr("Blacklist this model to remove it from rotation"), 40, this);

  QPushButton *blacklistButton = new QPushButton(tr("Blacklist Model"), this);
  blacklistButton->setFixedSize(600, 100);
  blacklistButton->setObjectName("blacklistButton");
  QObject::connect(blacklistButton, &QPushButton::clicked, [this]() {
    QStringList blacklistedModels = QString::fromStdString(params.get("BlacklistedModels")).split(",", QString::SkipEmptyParts);
    if (!blacklistedModels.contains(driveModel)) {
      blacklistedModels.append(driveModel);
      params.put("BlacklistedModels", blacklistedModels.join(",").toStdString());

      frogpilotUIState()->updateToggles();
    }

    resultLabel->setText(tr("Model successfully blacklisted!"));
    mainLayout->setCurrentIndex(1);
  });
  ratingLayout->addWidget(blacklistButton, 0, Qt::AlignCenter);

  mainLayout->addWidget(ratingWidget);

  QWidget *resultWidget = new QWidget(this);
  QVBoxLayout *resultLayout = new QVBoxLayout(resultWidget);
  resultLayout->setContentsMargins(50, 25, 50, 25);

  resultLayout->addStretch(1);
  addLabel(resultLayout, tr("Model used during that drive:"), 50, this);
  modelLabel = addLabel(resultLayout, "", 65, this);
  resultLabel = addLabel(resultLayout, "", 50, this);
  resultLayout->addStretch(1);

  mainLayout->addWidget(resultWidget);

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
    }
    QPushButton#ratingButton {
      background-color: #444444;
      border-radius: 12px;
      font-size: 100px;
    }
  )");

  QObject::connect(device(), &Device::interactiveTimeout, [this]() {
    if (isVisible()) {
      emit driveRated();
    }
  });
  QObject::connect(uiState(), &UIState::offroadTransition, [this](bool offroad) {
    if (!offroad) {
      const QJsonObject &frogpilot_toggles = frogpilotUIState()->frogpilot_scene.frogpilot_toggles;

      driveModel = frogpilot_toggles.value("model").toString();
      driveModelName = frogpilot_toggles.value("model_name").toString();
      driveRandomized = frogpilot_toggles.value("model_randomizer").toBool();
      driveTimer.start();

      modelLabel->setText(driveModelName);
      mainLayout->setCurrentIndex(0);
    }
  });
}

bool FrogPilotModelReview::reviewReady() {
  return driveTimer.isValid() && driveTimer.elapsed() > 15 * 60 * 1000 && driveRandomized;
}

void FrogPilotModelReview::mousePressEvent(QMouseEvent *e) {
  if (mainLayout->currentIndex() == 1) {
    emit driveRated();
  }
}

void FrogPilotModelReview::rateDrive(int rating) {
  QJsonObject modelDrivesAndScores = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("ModelDrivesAndScores"))).object();
  QJsonObject modelData = modelDrivesAndScores.value(driveModelName).toObject();

  int drives = modelData.value("Drives").toInt() + 1;
  int score = qRound(double(modelData.value("Score").toInt() * (drives - 1) + rating) / drives);

  modelData["Drives"] = drives;
  modelData["Score"] = score;
  modelDrivesAndScores[driveModelName] = modelData;

  params.put("ModelDrivesAndScores", QJsonDocument(modelDrivesAndScores).toJson(QJsonDocument::Compact).toStdString());

  resultLabel->setText(drives == 1 ? tr("Rating: %1% over %2 drive").arg(score).arg(drives) : tr("Rating: %1% over %2 drives").arg(score).arg(drives));
  mainLayout->setCurrentIndex(1);
}
