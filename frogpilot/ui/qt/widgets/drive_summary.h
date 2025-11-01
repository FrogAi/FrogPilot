#pragma once

#include "selfdrive/ui/ui.h"

class FrogPilotDriveSummary : public QFrame {
  Q_OBJECT

public:
  explicit FrogPilotDriveSummary(QWidget *parent = nullptr, bool random_events = false);

signals:
  void panelClosed();

protected:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void mousePressEvent(QMouseEvent *e);

private:
  QWidget *createStatBox(const QString &title, QLabel **valueLabel, QWidget *parent);

  bool displayRandomEvents;

  Params params;

  QJsonObject previousStats;

  QLabel *experimentalModeTimeValue;
  QLabel *frogPilotMetersValue;
  QLabel *engagementValue;
  QLabel *titleLabel;
  QLabel *trackedTimeValue;

  QMap<QString, QLabel*> randomEventLabels;

  QMap<QString, QString> randomEventsMap;

  QVBoxLayout *eventsListLayout;
};
