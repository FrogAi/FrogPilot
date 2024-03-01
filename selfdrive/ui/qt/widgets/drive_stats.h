#pragma once

#include <QJsonDocument>
#include <QLabel>

#include "common/params.h"

class DriveStats : public QFrame {
  Q_OBJECT

public:
  explicit DriveStats(QWidget* parent = 0);

private:
  void showEvent(QShowEvent *event) override;
  void updateStats();
  inline QString getDistanceUnit() const { return metric_ ? tr("KM") : tr("Miles"); }

  bool metric_;
  Params params;
  QJsonDocument stats_;
  struct StatsLabels {
    QLabel *routes, *distance, *distance_unit, *hours;
  } all_, week_, frogPilot_;

private slots:
  void parseResponse(const QString &response, bool success);
};
