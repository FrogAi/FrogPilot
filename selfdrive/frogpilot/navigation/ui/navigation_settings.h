#pragma once

#include "selfdrive/ui/qt/network/wifi_manager.h"
#include "selfdrive/ui/qt/offroad/settings.h"
#include "selfdrive/ui/qt/widgets/scrollview.h"
#include "selfdrive/ui/ui.h"

class Primeless : public QWidget {
  Q_OBJECT

public:
  explicit Primeless(QWidget *parent = nullptr);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void hideEvent(QHideEvent *event) override;

private:
  void createMapboxKeyControl(ButtonControl *&control, const QString &label, const std::string &paramKey, const QString &prefix);
  void updateState();
  void updateStep();

  QVBoxLayout *mainLayout;
  FrogPilotListWidget *list;

  QPushButton *backButton;
  QLabel *imageLabel;

  ButtonControl *publicMapboxKeyControl;
  ButtonControl *secretMapboxKeyControl;
  LabelControl *ipLabel;

  WifiManager *wifi;

  bool mapboxPublicKeySet;
  bool mapboxSecretKeySet;
  bool setupCompleted;
  QPixmap pixmap;
  QString currentStep = "../assets/images/setup_completed.png";

  Params params;

signals:
  void backPress();
};

class FrogPilotNavigationPanel : public QFrame {
  Q_OBJECT

public:
  explicit FrogPilotNavigationPanel(QWidget *parent = 0);

private:
  QStackedLayout *mainLayout;
  QWidget *navigationWidget;

  Params params;
  Params paramsMemory{"/dev/shm/params"};
  UIScene &scene;
};
