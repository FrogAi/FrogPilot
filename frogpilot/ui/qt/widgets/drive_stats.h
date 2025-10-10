#pragma once

#include "selfdrive/ui/ui.h"

struct StatsLabels {
  QLabel *distance;
  QLabel *distance_unit;
  QLabel *hours;
  QLabel *routes;
};

class DriveStats : public QFrame {
  Q_OBJECT

public:
  explicit DriveStats(QWidget *parent = 0);

private:
  void addStatsLayouts(const QString &title, StatsLabels &labels, bool FrogPilot = false);
  void showEvent(QShowEvent *event) override;
  void updateStats();
  void updateStatsForLabel(const QJsonObject &obj, StatsLabels &labels);
  void updateFrogPilotStatsForLabel(StatsLabels &labels);

  bool isMetric;
  bool konik;

  Params params;

  QJsonDocument stats;

  StatsLabels all;
  StatsLabels frogPilot;
  StatsLabels week;

private slots:
  void parseResponse(const QString &response, bool success);
};
