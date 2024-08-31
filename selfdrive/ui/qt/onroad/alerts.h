#pragma once

#include <QWidget>

#include "selfdrive/ui/ui.h"

class OnroadAlerts : public QWidget {
  Q_OBJECT

public:
  OnroadAlerts(QWidget *parent = 0) : QWidget(parent) {}
  void updateState(const UIState &s);
  void clear();

  // FrogPilot variables
  int alert_height;

protected:
  struct Alert {
    QString text1;
    QString text2;
    QString type;
    cereal::ControlsState::AlertSize size;
    cereal::ControlsState::AlertStatus status;

    bool equal(const Alert &other) const {
      return text1 == other.text1 && text2 == other.text2 && type == other.type;
    }
  };

  const QMap<cereal::ControlsState::AlertStatus, QColor> alert_colors = {
    {cereal::ControlsState::AlertStatus::NORMAL, QColor(0x15, 0x15, 0x15, 0xf1)},
    {cereal::ControlsState::AlertStatus::USER_PROMPT, QColor(0xDA, 0x6F, 0x25, 0xf1)},
    {cereal::ControlsState::AlertStatus::CRITICAL, QColor(0xC9, 0x22, 0x31, 0xf1)},

    // FrogPilot alert colors
    {cereal::ControlsState::AlertStatus::FROGPILOT, QColor(0x17, 0x86, 0x44, 0xf1)},
  };

  void paintEvent(QPaintEvent*) override;
  OnroadAlerts::Alert getAlert(const SubMaster &sm, uint64_t started_frame, bool force_onroad);

  QColor bg;
  Alert alert = {};

  // FrogPilot variables
  bool hide_alerts;
  bool road_name_ui;
  bool show_aol_status_bar;
  bool show_cem_status_bar;
};
