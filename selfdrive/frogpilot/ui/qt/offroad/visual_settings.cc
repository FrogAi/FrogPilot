#include "selfdrive/frogpilot/ui/qt/offroad/visual_settings.h"

FrogPilotVisualsPanel::FrogPilotVisualsPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  const std::vector<std::tuple<QString, QString, QString, QString>> visualToggles {
    {"CustomUI", tr("Onroad UI Widgets"), tr("Custom FrogPilot widgets used in the onroad user interface."), "../assets/offroad/icon_road.png"},
    {"Compass", tr("Compass"), tr("A compass in the onroad UI to show the current driving direction."), ""},
    {"DynamicPathWidth", tr("Dynamic Path Width"), tr("Automatically adjust the width of the driving path display based on the current engagement state:\n\nFully engaged = 100%\nAlways On Lateral Active = 75%\nFully disengaged = 50%"), ""},
    {"PedalsOnUI", tr("Gas/Brake Pedal Indicators"), tr("Pedal indicators in the onroad UI that change opacity based on the pressure applied."), ""},
    {"CustomPaths", tr("Paths"), tr("Projected acceleration path, detected lanes, and vehicles in the blind spot."), ""},
    {"RoadNameUI", tr("Road Name"), tr("The current road name is displayed at the bottom of the screen using data from 'OpenStreetMap'."), ""},
    {"RotatingWheel", tr("Rotating Steering Wheel"), tr("The steering wheel in the onroad UI rotates along with your steering wheel movements."), ""},

    {"QOLVisuals", tr("Quality of Life Improvements"), tr("Miscellaneous visual focused features to improve your overall openpilot experience."), "../frogpilot/assets/toggle_icons/quality_of_life.png"},
    {"BigMap", tr("Larger Map Display"), tr("A larger size of the map in the onroad UI for easier navigation readings."), ""},
    {"MapStyle", tr("Map Style"), tr("Custom map styles for the map used during navigation."), ""},
    {"StandbyMode", tr("Screen Standby Mode"), tr("The screen is turned off after it times out when driving, but it automatically wakes up if engagement state changes or important alerts occur."), ""},
    {"DriverCamera", tr("Show Driver Camera When In Reverse"), tr("The driver camera feed is displayed when the vehicle is in reverse."), ""},
    {"StoppedTimer", tr("Stopped Timer"), tr("A timer on the onroad UI to indicate how long the vehicle has been stopped."), ""}
  };

  for (const auto &[param, title, desc, icon] : visualToggles) {
    AbstractControl *visualToggle;

    if (param == "CustomUI") {
      FrogPilotParamManageControl *customUIToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(customUIToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        customPathsBtn->setVisibleButton(0, hasBSM);

        std::set<QString> modifiedCustomOnroadUIKeys = customOnroadUIKeys;

        showToggles(modifiedCustomOnroadUIKeys);
      });
      visualToggle = customUIToggle;
    } else if (param == "CustomPaths") {
      std::vector<QString> pathToggles{"AccelerationPath", "AdjacentPath", "BlindSpotPath"};
      std::vector<QString> pathToggleNames{tr("Acceleration"), tr("Adjacent"), tr("Blind Spot")};
      customPathsBtn = new FrogPilotButtonToggleControl(param, title, desc, pathToggles, pathToggleNames);
      visualToggle = customPathsBtn;
    } else if (param == "PedalsOnUI") {
      std::vector<QString> pedalsToggles{"DynamicPedalsOnUI", "StaticPedalsOnUI"};
      std::vector<QString> pedalsToggleNames{tr("Dynamic"), tr("Static")};
      FrogPilotButtonToggleControl *pedalsToggle = new FrogPilotButtonToggleControl(param, title, desc, pedalsToggles, pedalsToggleNames, true);
      QObject::connect(pedalsToggle, &FrogPilotButtonToggleControl::buttonClicked, [this](int index) {
        if (index == 0) {
          params.putBool("StaticPedalsOnUI", false);
        } else if (index == 1) {
          params.putBool("DynamicPedalsOnUI", false);
        }
      });
      visualToggle = pedalsToggle;

    } else if (param == "QOLVisuals") {
      FrogPilotParamManageControl *qolToggle = new FrogPilotParamManageControl(param, title, desc, icon);
      QObject::connect(qolToggle, &FrogPilotParamManageControl::manageButtonClicked, [this]() {
        showToggles(qolKeys);
      });
      visualToggle = qolToggle;
    } else if (param == "BigMap") {
      std::vector<QString> mapToggles{"FullMap"};
      std::vector<QString> mapToggleNames{tr("Full Map")};
      visualToggle = new FrogPilotButtonToggleControl(param, title, desc, mapToggles, mapToggleNames);
    } else if (param == "MapStyle") {
      QMap<int, QString> styleMap = {
        {0, tr("Stock openpilot")},
        {1, tr("Mapbox Streets")},
        {2, tr("Mapbox Outdoors")},
        {3, tr("Mapbox Light")},
        {4, tr("Mapbox Dark")},
        {5, tr("Mapbox Satellite")},
        {6, tr("Mapbox Satellite Streets")},
        {7, tr("Mapbox Navigation Day")},
        {8, tr("Mapbox Navigation Night")},
        {9, tr("Mapbox Traffic Night")},
        {10, tr("mike854's (Satellite hybrid)")},
      };

      QStringList styles = styleMap.values();
      ButtonControl *mapStyleButton = new ButtonControl(title, tr("SELECT"), desc);
      QObject::connect(mapStyleButton, &ButtonControl::clicked, [=]() {
        QStringList styles = styleMap.values();
        QString selection = MultiOptionDialog::getSelection(tr("Select a map style"), styles, "", this);
        if (!selection.isEmpty()) {
          int selectedStyle = styleMap.key(selection);
          params.putIntNonBlocking("MapStyle", selectedStyle);
          mapStyleButton->setValue(selection);
          updateFrogPilotToggles();
        }
      });

      int currentStyle = params.getInt("MapStyle");
      mapStyleButton->setValue(styleMap[currentStyle]);

      visualToggle = mapStyleButton;

    } else {
      visualToggle = new ParamControl(param, title, desc, icon);
    }

    addItem(visualToggle);
    toggles[param] = visualToggle;

    makeConnections(visualToggle);

    if (FrogPilotParamManageControl *frogPilotManageToggle = qobject_cast<FrogPilotParamManageControl*>(visualToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotParamManageControl::manageButtonClicked, this, &FrogPilotVisualsPanel::openParentToggle);
    }

    QObject::connect(visualToggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  QObject::connect(parent, &FrogPilotSettingsWindow::closeParentToggle, this, &FrogPilotVisualsPanel::hideToggles);
  QObject::connect(parent, &FrogPilotSettingsWindow::updateCarToggles, this, &FrogPilotVisualsPanel::updateCarToggles);
}

void FrogPilotVisualsPanel::updateCarToggles() {
  hasBSM = parent->hasBSM;

  hideToggles();
}

void FrogPilotVisualsPanel::showToggles(const std::set<QString> &keys) {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    toggle->setVisible(keys.find(key) != keys.end());
  }

  setUpdatesEnabled(true);
  update();
}

void FrogPilotVisualsPanel::hideToggles() {
  setUpdatesEnabled(false);

  for (auto &[key, toggle] : toggles) {
    bool subToggles = customOnroadUIKeys.find(key) != customOnroadUIKeys.end() ||
                      qolKeys.find(key) != qolKeys.end();

    toggle->setVisible(!subToggles);
  }

  setUpdatesEnabled(true);
  update();
}
