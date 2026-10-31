#pragma once

#include "selfdrive/ui/qt/widgets/input.h"
#include "selfdrive/ui/ui.h"

// FrogPilot.com account QR code
class FrogPilotAccountQRWidget : public QWidget {
  Q_OBJECT

public:
  explicit FrogPilotAccountQRWidget(const QString &purpose, QWidget* parent = 0);
  void paintEvent(QPaintEvent*) override;

private:
  QPixmap img;
  QTimer *timer;
  void updateQrCode(const QString &text);
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void updateState(const UIState &s, const FrogPilotUIState &fs);

  bool waiting = false;

  QString purpose;

private slots:
  void refresh();
};


// FrogPilot.com account popup widget
class FrogPilotAccountPopup : public DialogBase {
  Q_OBJECT

public:
  explicit FrogPilotAccountPopup(const QString &purpose, QWidget* parent);
  int exec() override;
};
