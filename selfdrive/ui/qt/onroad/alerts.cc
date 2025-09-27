#include "selfdrive/ui/qt/onroad/alerts.h"

#include <QPainter>
#include <map>

#include "selfdrive/ui/qt/util.h"

void OnroadAlerts::updateState(const UIState &s, const FrogPilotUIState &fs) {
  Alert a = getAlert(*(s.sm), *(fs.sm), s.scene.started_frame, fs.frogpilot_toggles);
  if (!alert.equal(a)) {
    if (alert.status == cereal::ControlsState::AlertStatus::NORMAL && fs.frogpilot_toggles.value("hide_alerts").toBool()) {
      clear();
    } else {
      alert = a;

      update();
    }
  }

  // FrogPilot variables
  sidebarsOpen = fs.frogpilot_scene.sidebars_open;
}

void OnroadAlerts::clear() {
  alertHeight = 0;

  alert = {};
  update();
}

OnroadAlerts::Alert OnroadAlerts::getAlert(const SubMaster &sm, const SubMaster &fpsm, uint64_t started_frame, QJsonObject &frogpilot_toggles) {
  const cereal::ControlsState::Reader &cs = sm["controlsState"].getControlsState();
  const cereal::FrogPilotControlsState::Reader &fpcs = fpsm["frogpilotControlsState"].getFrogpilotControlsState();
  const uint64_t controls_frame = sm.rcv_frame("controlsState");

  Alert a = {};
  static QString crash_log_path = "/data/error_logs/error.txt";
  if (QFile::exists(crash_log_path)) {
    if (frogpilot_toggles.value("random_events").toBool()) {
      if (enableFerg) {
        displayFerg = true;
      } else {
        a = {tr("openpilot crashed 💩"),
             tr("Please post the \"Error Log\" in the FrogPilot Discord!"),
             "openpilotCrashedRandomEvent",
             cereal::ControlsState::AlertSize::MID,
             cereal::ControlsState::AlertStatus::CRITICAL};
      }
    } else {
      a = {tr("openpilot crashed"),
           tr("Please post the \"Error Log\" in the FrogPilot Discord!"),
           "openpilotCrashed",
           cereal::ControlsState::AlertSize::MID,
           cereal::ControlsState::AlertStatus::CRITICAL};
    }
    return a;
  } else if (controls_frame >= started_frame) {  // Don't get old alert.
    a = {cs.getAlertText1().cStr(), cs.getAlertText2().cStr(),
         cs.getAlertType().cStr(), cs.getAlertSize(), cs.getAlertStatus()};

    if (a.size == cereal::ControlsState::AlertSize::NONE) {
      a = {fpcs.getAlertText1().cStr(), fpcs.getAlertText2().cStr(),
           fpcs.getAlertType().cStr(), static_cast<cereal::ControlsState::AlertSize>(fpcs.getAlertSize()), static_cast<cereal::ControlsState::AlertStatus>(fpcs.getAlertStatus())};
    }
  }

  if (!sm.updated("controlsState") && (sm.frame - started_frame) > 5 * UI_FREQ && !frogpilot_toggles.value("force_onroad").toBool()) {
    const int CONTROLS_TIMEOUT = 5;
    const int controls_missing = (nanos_since_boot() - sm.rcv_time("controlsState")) / 1e9;

    // Handle controls timeout
    if (controls_frame < started_frame) {
      // car is started, but controlsState hasn't been seen at all
      a = {tr("openpilot Unavailable"), tr("Waiting for controls to start"),
           "controlsWaiting", cereal::ControlsState::AlertSize::MID,
           cereal::ControlsState::AlertStatus::NORMAL};
    } else if (controls_missing > CONTROLS_TIMEOUT && !Hardware::PC()) {
      // car is started, but controls is lagging or died
      if (cs.getEnabled() && (controls_missing - CONTROLS_TIMEOUT) < 10) {
        a = {tr("TAKE CONTROL IMMEDIATELY"), tr("Controls Unresponsive"),
             "controlsUnresponsive", cereal::ControlsState::AlertSize::FULL,
             cereal::ControlsState::AlertStatus::CRITICAL};
      } else {
        a = {tr("Controls Unresponsive"), tr("Reboot Device"),
             "controlsUnresponsivePermanent", cereal::ControlsState::AlertSize::MID,
             cereal::ControlsState::AlertStatus::NORMAL};
      }
    }
  }
  return a;
}

void OnroadAlerts::paintEvent(QPaintEvent *event) {
  if (displayFerg) {
    QPainter p(this);
    p.drawPixmap(QPoint((width() - ferg.width()) / 2, (height() - ferg.height()) / 2), ferg);
    return;
  }
  if (alert.size == cereal::ControlsState::AlertSize::NONE) {
    alertHeight = 0;
    return;
  }
  static std::map<cereal::ControlsState::AlertSize, const int> alert_heights = {
    {cereal::ControlsState::AlertSize::SMALL, 271},
    {cereal::ControlsState::AlertSize::MID, 420},
    {cereal::ControlsState::AlertSize::FULL, height()},
  };
  alertHeight = alert_heights[alert.size];
  int h = alertHeight;

  int margin = 40;
  int radius = 30;
  if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    margin = 0;
    radius = 0;
  }
  alertHeight -= margin;
  QRect r = QRect(0 + margin, height() - h + margin, width() - margin*2, h - margin*2);

  QPainter p(this);

  // draw background + gradient
  p.setPen(Qt::NoPen);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  p.setBrush(QBrush(frogpilot_alert_colors[static_cast<cereal::FrogPilotControlsState::AlertStatus>(alert.status)]));
  p.drawRoundedRect(r, radius, radius);

  QLinearGradient g(0, r.y(), 0, r.bottom());
  g.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.05));
  g.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0.35));

  p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
  p.setBrush(QBrush(g));
  p.drawRoundedRect(r, radius, radius);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  // text
  const QPoint c = r.center();
  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setRenderHint(QPainter::TextAntialiasing);
  if (alert.size == cereal::ControlsState::AlertSize::SMALL) {
    bool long_alert1 = alert.text1.length() > 40;
    p.setFont(InterFont(long_alert1 && sidebarsOpen ? 64 : 74, QFont::DemiBold));
    p.drawText(r, Qt::AlignCenter, alert.text1);
  } else if (alert.size == cereal::ControlsState::AlertSize::MID) {
    bool long_alert1 = alert.text1.length() > 30;
    p.setFont(InterFont(long_alert1 && sidebarsOpen ? 78 : 88, QFont::Bold));
    p.drawText(QRect(0, c.y() - 125, width(), 150), Qt::AlignHCenter | Qt::AlignTop, alert.text1);
    bool long_alert2 = alert.text2.length() > 40;
    p.setFont(InterFont(long_alert2 && sidebarsOpen ? 56 : 66));
    p.drawText(QRect(0, c.y() + 21, width(), 90), Qt::AlignHCenter, alert.text2);
  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    bool l = alert.text1.length() > 15;
    p.setFont(InterFont(l ? 132 : 177, QFont::Bold));
    p.drawText(QRect(0, r.y() + (l ? 240 : 270), width(), 600), Qt::AlignHCenter | Qt::TextWordWrap, alert.text1);
    p.setFont(InterFont(88));
    p.drawText(QRect(0, r.height() - (l ? 361 : 420), width(), 300), Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);
  }
}
