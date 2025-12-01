#pragma once

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class FrogPilotNavigationPanel : public FrogPilotListWidget {
  Q_OBJECT

public:
  explicit FrogPilotNavigationPanel(FrogPilotSettingsWindow *parent, bool forceOpen = false);

signals:
  void closeSubPanel();
  void openSubPanel();

protected:
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void mousePressEvent(QMouseEvent *event) override;
  void testMapboxKey(FrogPilotButtonsControl *control, const std::string &paramKey, const QString &urlTemplate);
  void updateButtons();
  void updateNetworkState(const FrogPilotUIState &fs);
  void updateState(const UIState &s, const FrogPilotUIState &fs);
  void updateStep();

  bool forceOpenDescriptions;
  bool mapboxPublicKeySet = false;
  bool mapboxSecretKeySet = false;

  ButtonControl *setupButton;

  ParamControl *speedLimitFillerToggle;

  FrogPilotButtonsControl *publicMapboxKeyControl;
  FrogPilotButtonsControl *secretMapboxKeyControl;

  FrogPilotSettingsWindow *parent;

  LabelControl *ipLabel;

  QLabel *imageLabel;

  QNetworkAccessManager *networkManager;

  QStackedLayout *primelessLayout;
};
