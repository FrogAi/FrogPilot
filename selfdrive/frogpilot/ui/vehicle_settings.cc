#include "selfdrive/frogpilot/ui/vehicle_settings.h"

FrogPilotVehiclesPanel::FrogPilotVehiclesPanel(SettingsWindow *parent) : FrogPilotListWidget(parent) {
  ParamControl *disableOpenpilotLong = new ParamControl("DisableOpenpilotLongitudinal", "Disable Openpilot Longitudinal Control", "Disables openpilot longitudinal control to use stock ACC.", "", this);
  addItem(disableOpenpilotLong);

  QObject::connect(disableOpenpilotLong, &ToggleControl::toggleFlipped, [=]() {
    if (started) {
      if (FrogPilotConfirmationDialog::toggle("Reboot required to take effect.", "Reboot Now", this)) {
        Hardware::reboot();
      }
    }
  });

  std::vector<std::tuple<QString, QString, QString, QString>> vehicleToggles {
    {"LongitudinalTune", "Longitudinal Tune", "Use a custom Toyota longitudinal tune.\n\nCydia = More focused on TSS-P vehicles but works for all Toyotas\n\nDragonPilot = Focused on TSS2 vehicles\n\nFrogPilot = Takes the best of both worlds with some personal tweaks focused around my 2019 Lexus ES 350", ""},
  };

  for (const auto &[param, title, desc, icon] : vehicleToggles) {
    ParamControl *toggle;

    if (param == "LongitudinalTune") {
      std::vector<std::pair<QString, QString>> tuneOptions{
        {"StockTune", tr("Stock")},
        {"CydiaTune", tr("Cydia's")},
        {"DragonPilotTune", tr("DragonPilot's")},
      };
      toggle = new FrogPilotButtonsParamControl(param, title, desc, icon, tuneOptions);

      QObject::connect(static_cast<FrogPilotButtonsParamControl*>(toggle), &FrogPilotButtonsParamControl::buttonClicked, [this]() {
        if (started) {
          if (FrogPilotConfirmationDialog::toggle("Reboot required to take effect.", "Reboot Now", this)) {
            Hardware::soft_reboot();
          }
        }
      });

    } else {
      toggle = new ParamControl(param, title, desc, icon, this);
    }

    toggle->setVisible(false);
    addItem(toggle);
    toggles[param.toStdString()] = toggle;

    QObject::connect(toggle, &ToggleControl::toggleFlipped, [this]() {
      updateToggles();
    });

    QObject::connect(toggle, &AbstractControl::showDescriptionEvent, [this]() {
      update();
    });
  }

  std::set<std::string> rebootKeys = {};
  for (const std::string &key : rebootKeys) {
    QObject::connect(toggles[key], &ToggleControl::toggleFlipped, [this]() {
      if (started) {
        if (FrogPilotConfirmationDialog::toggle("Reboot required to take effect.", "Reboot Now", this)) {
          Hardware::soft_reboot();
        }
      }
    });
  }

  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotVehiclesPanel::updateState);
}

void FrogPilotVehiclesPanel::updateState(const UIState &s) {
  started = s.scene.started;
}

void FrogPilotVehiclesPanel::updateToggles() {
  std::thread([this]() {
    paramsMemory.putBool("FrogPilotTogglesUpdated", true);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    paramsMemory.putBool("FrogPilotTogglesUpdated", false);
  }).detach();
}

void FrogPilotVehiclesPanel::setToggles() {
  bool gm = false;
  bool subaru false;
  bool toyota = false;

  for (auto &[key, toggle] : toggles) {
    if (toggle) {
      toggle->setVisible(false);

      if (gm) {
        toggle->setVisible(gmKeys.find(key.c_str()) != gmKeys.end());
      } else if (subaru) {
        toggle->setVisible(subaruKeys.find(key.c_str()) != subaruKeys.end());
      } else if (toyota) {
        toggle->setVisible(toyotaKeys.find(key.c_str()) != toyotaKeys.end());
      }
    }
  }

  update();
}
