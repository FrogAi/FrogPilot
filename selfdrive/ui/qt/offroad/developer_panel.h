#pragma once

#include "selfdrive/ui/qt/offroad/settings.h"

#include "frogpilot/ui/qt/offroad/frogpilot_settings.h"

class DeveloperPanel : public QFrame {
  Q_OBJECT
public:
  explicit DeveloperPanel(SettingsWindow *parent);
  void showEvent(QShowEvent *event) override;

// FrogPilot variables
signals:
  void openSubPanel();
  void openSubSubPanel();

private:
  Params params;
  ParamControl* adbToggle;
  ParamControl* joystickToggle;
  ParamControl* longManeuverToggle;
  ParamControl* experimentalLongitudinalToggle;
  bool is_release;
  bool offroad = false;

  // FrogPilot variables
  QMap<int, QString> developerMetricOptions();

  bool forceOpenDescriptions = false;
  bool hasBSM = true;
  bool hasOpenpilotLongitudinal = true;
  bool hasRadar = true;

  int tuningLevel;

  std::map<QString, AbstractControl*> toggles;

  std::vector<QString> sidebarMetricsToggles;

  FrogPilotButtonsControl *sidebarMetricsToggle;

  FrogPilotButtonToggleControl *borderMetricsButton;

  FrogPilotManageControl *developerUIToggle;

  QJsonObject frogpilotToggleLevels;

  QSet<QString> developerMetricKeys = {"AdjacentPathMetrics", "BorderMetrics", "FPSCounter", "LeadInfo", "NumericalTemp", "SidebarMetrics", "UseSI"};
  QSet<QString> developerSidebarKeys = {"DeveloperSidebarMetric1", "DeveloperSidebarMetric2", "DeveloperSidebarMetric3", "DeveloperSidebarMetric4", "DeveloperSidebarMetric5", "DeveloperSidebarMetric6", "DeveloperSidebarMetric7"};
  QSet<QString> developerUIKeys = {"DeveloperMetrics", "DeveloperSidebar", "DeveloperWidgets"};
  QSet<QString> developerWidgetKeys = {"AdjacentLeadsUI", "RadarTracksUI", "ShowStoppingPoint"};

  QSet<QString> parentKeys;

  QStackedLayout *mainLayout;

  QWidget *mainWidget;

  ScrollView *developerMetricPanel;
  ScrollView *developerSidebarPanel;
  ScrollView *developerUIPanel;
  ScrollView *developerWidgetPanel;

private slots:
  void updateToggles(bool _offroad);
};
