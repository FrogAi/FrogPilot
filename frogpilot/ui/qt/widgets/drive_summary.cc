#include "frogpilot/ui/qt/widgets/drive_summary.h"

FrogPilotDriveSummary::FrogPilotDriveSummary(QWidget *parent) : QFrame(parent) {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, 20, 20, 20);
  mainLayout->setSpacing(15);

  QLabel *titleLabel = new QLabel(tr("Drive Summary"), this);
  titleLabel->setAlignment(Qt::AlignCenter);
  titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  titleLabel->setStyleSheet(R"(
    QLabel {
      background-color: #444444;
      border-radius: 12px;
      color: #FFFFFF;
      font-size: 50px;
      font-weight: bold;
      padding: 12px 28px;
    }
  )");
  titleLabel->setWordWrap(true);

  mainLayout->addWidget(titleLabel);
  mainLayout->addSpacing(10);

  QWidget *containerWidget = new QWidget(this);
  QVBoxLayout *listLayout = new QVBoxLayout(containerWidget);
  listLayout->setAlignment(Qt::AlignTop);
  listLayout->setSpacing(20);

  listLayout->addWidget(createStatBox(tr("% of Drive With openpilot Engaged"), &engagementValue));
  listLayout->addWidget(createStatBox(tr("Drive Distance"), &frogPilotMetersValue));
  listLayout->addWidget(createStatBox(tr("Drive Time"), &trackedTimeValue));
  listLayout->addWidget(createStatBox(tr("% of Drive In \"Experimental Mode\""), &experimentalModeTimeValue));

  mainLayout->addWidget(containerWidget, 1);

  setStyleSheet(R"(
    QFrame {
      background-color: #333333;
    }
  )");

  QObject::connect(device(), &Device::interactiveTimeout, this, &FrogPilotDriveSummary::panelClosed);
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::statsSaved, this, [this]() {
    if (isVisible()) {
      updateStats();
    }
  });
  QObject::connect(uiState(), &UIState::offroadTransition, [this](bool offroad) {
    if (!offroad) {
      previousStats = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("FrogPilotStats"))).object();
    }
  });
}

void FrogPilotDriveSummary::mousePressEvent(QMouseEvent *e) {
  emit panelClosed();
}

void FrogPilotDriveSummary::showEvent(QShowEvent *event) {
  updateStats();
}

void FrogPilotDriveSummary::updateStats() {
  bool isMetric = params.getBool("IsMetric");

  QJsonObject currentStats = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("FrogPilotStats"))).object();

  std::function<double(const QString &)> diffDouble = [currentStats, this](const QString &key) -> double {
    return currentStats.value(key).toDouble() - previousStats.value(key).toDouble();
  };

  int engagedTime = diffDouble("EngagedTime");
  int experimentalTime = diffDouble("ExperimentalModeTime");
  int trackedTime = diffDouble("TrackedTime");

  engagementValue->setText(QLocale().toString((trackedTime > 0) ? (engagedTime * 100 / trackedTime) : 0) + "%");
  experimentalModeTimeValue->setText(QLocale().toString((trackedTime > 0) ? (experimentalTime * 100 / trackedTime) : 0) + "%");
  frogPilotMetersValue->setText(formatDistance(diffDouble("FrogPilotMeters"), isMetric));
  trackedTimeValue->setText(formatDuration(trackedTime));
}

void FrogPilotDriveSummary::hideEvent(QHideEvent *event) {
  emit panelClosed();
}

QWidget *FrogPilotDriveSummary::createStatBox(const QString &title, QLabel **valueLabel) {
  QWidget *box = new QWidget(this);

  QVBoxLayout *layout = new QVBoxLayout(box);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(8);

  QLabel *statTitleLabel = new QLabel(title, box);
  statTitleLabel->setAlignment(Qt::AlignCenter);
  statTitleLabel->setWordWrap(true);
  statTitleLabel->setStyleSheet(R"(
    QLabel {
      color: #AAAAAA;
      font-size: 40px;
      font-weight: bold;
    }
  )");

  QLabel *value = new QLabel("-", box);
  value->setAlignment(Qt::AlignCenter);
  value->setStyleSheet(R"(
    QLabel {
      color: #FFFFFF;
      font-size: 75px;
      font-weight: bold;
    }
  )");

  *valueLabel = value;

  layout->addWidget(statTitleLabel);
  layout->addWidget(value);

  return box;
}
