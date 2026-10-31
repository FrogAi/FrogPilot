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
  void hideEvent(QHideEvent *event);
  void showEvent(QShowEvent *event) override;

private:
  void mousePressEvent(QMouseEvent *event);
  void updateButtons();
  void updateState(const UIState &s, const FrogPilotUIState &fs);
  void updateStep();

  bool forceOpenDescriptions = false;
  bool mapboxPublicKeySet = false;
  bool mapboxSecretKeySet = false;
  bool setupCompleted = false;

  std::map<QString, AbstractControl*> toggles;

  ButtonControl *amapKeyControl = nullptr;
  ButtonControl *setupButton = nullptr;

  ParamControl *pairingToggle = nullptr;
  ParamControl *speedLimitFillerToggle = nullptr;

  FrogPilotButtonsControl *publicMapboxKeyControl = nullptr;
  FrogPilotButtonsControl *searchProviderControl = nullptr;
  FrogPilotButtonsControl *secretMapboxKeyControl = nullptr;

  FrogPilotSettingsWindow *parent = nullptr;

  LabelControl *ipLabel = nullptr;

  Params params;

  QLabel *imageLabel = nullptr;

  QNetworkAccessManager *networkManager = nullptr;

  QStackedLayout *primelessLayout = nullptr;

  QString currentStep;
};
