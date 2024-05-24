#pragma once

#include "selfdrive/frogpilot/ui/qt/widgets/frogpilot_controls.h"
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

class SelectMaps : public QWidget {
  Q_OBJECT

public:
  explicit SelectMaps(QWidget *parent = nullptr);

  QFrame *horizontalLine(QWidget *parent = nullptr) const;

private:
  void hideEvent(QHideEvent *event);

  ScrollView *countriesScrollView;
  ScrollView *statesScrollView;
  QStackedLayout *mapsLayout;

  QPushButton *backButton;
  QPushButton *statesButton;
  QPushButton *countriesButton;

  static QString activeButtonStyle;
  static QString normalButtonStyle;

signals:
  void backPress();
  void setMaps();
};

class FrogPilotNavigationPanel : public QFrame {
  Q_OBJECT

public:
  explicit FrogPilotNavigationPanel(QWidget *parent = 0);

private:
  void cancelDownload(QWidget *parent);
  void downloadMaps();
  void hideEvent(QHideEvent *event);
  void removeMaps(QWidget *parent);
  void setMaps();
  void showEvent(QShowEvent *event);
  void updateDownloadedLabel();
  void updateState();
  void updateStatuses();
  void updateVisibility(bool visibility);

  QStackedLayout *mainLayout;
  QWidget *navigationWidget;

  ButtonControl *cancelDownloadButton;
  ButtonControl *downloadOfflineMapsButton;
  ButtonControl *redownloadOfflineMapsButton;
  ButtonControl *removeOfflineMapsButton;

  LabelControl *lastMapsDownload;
  LabelControl *offlineMapsSize;
  LabelControl *offlineMapsStatus;
  LabelControl *offlineMapsETA;
  LabelControl *offlineMapsElapsed;

  bool downloadActive;
  bool previousDownloadActive;
  QString elapsedTime;
  QString offlineFolderPath = "/data/media/0/osm/offline";
  std::string osmDownloadProgress;
  std::string previousOSMDownloadProgress;

  Params params;
  Params paramsMemory{"/dev/shm/params"};
  UIScene &scene;
};
