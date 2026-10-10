#pragma once

#include "common/params.h"

#include <QFrame>

class FrogPilotTelemetryPage : public QFrame {
  Q_OBJECT

public:
  explicit FrogPilotTelemetryPage(QWidget *parent = nullptr) : QFrame(parent) {}

  static QString description();

signals:
  void answered();

private:
  void answer(bool share);
  void showEvent(QShowEvent *event) override;

  Params params;
};
