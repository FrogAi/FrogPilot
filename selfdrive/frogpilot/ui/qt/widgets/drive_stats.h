#pragma once

#include "selfdrive/ui/ui.h"

class DriveStats : public QFrame {
  Q_OBJECT

public:
  explicit DriveStats(QWidget *parent = 0);

private:
  inline QString getDistanceUnit() const { return metric ? tr("KM") : tr("Miles"); }

  void showEvent(QShowEvent *event) override;
  void updateStats();

  Params params;
  Params paramsTracking{"/persist/tracking"};

  bool metric;

  QJsonDocument stats;

  struct StatsLabels {
    QLabel *routes, *distance, *distance_unit, *hours;
  } all, week, frogPilot;

private slots:
  void parseResponse(const QString &response, bool success);
};
