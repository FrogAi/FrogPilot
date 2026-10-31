#pragma once

#include "selfdrive/ui/ui.h"

class FrogPilotDriveSummary : public QFrame {
  Q_OBJECT

public:
  explicit FrogPilotDriveSummary(QWidget *parent = nullptr, bool randomEvents = false);

signals:
  void panelClosed();

protected:
  void hideEvent(QHideEvent *event) override;
  void mousePressEvent(QMouseEvent *e) override;
  void showEvent(QShowEvent *event) override;

private:
  void updateStats();

  QWidget *createStatBox(const QString &title, QLabel **valueLabel);

  bool displayRandomEvents = false;

  Params params;

  QJsonObject previousStats;

  QLabel *experimentalModeTimeValue = nullptr;
  QLabel *frogPilotMetersValue = nullptr;
  QLabel *engagementValue = nullptr;
  QLabel *trackedTimeValue = nullptr;

  QVBoxLayout *eventsListLayout = nullptr;
};
