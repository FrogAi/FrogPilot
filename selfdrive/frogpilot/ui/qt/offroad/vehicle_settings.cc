#include "selfdrive/frogpilot/ui/qt/offroad/vehicle_settings.h"

FrogPilotVehiclesPanel::FrogPilotVehiclesPanel(SettingsWindow *parent) : FrogPilotListWidget(parent) {
  std::vector<std::tuple<QString, QString, QString, QString>> vehicleToggles {
    {"LongitudinalTune", tr("Longitudinal Tune"), tr("Use a custom Toyota longitudinal tune.\n\nCydia = More focused on TSS-P vehicles but works for all Toyotas\n\nDragonPilot = Focused on TSS2 vehicles\n\nFrogPilot = Takes the best of both worlds with some personal tweaks focused around FrogsGoMoo's 2019 Lexus ES 350"), ""},
  };

  for (const auto &[param, title, desc, icon] : vehicleToggles) {
    ParamControl *toggle;

    if (param == "LongitudinalTune") {
      std::vector<std::pair<QString, QString>> tuneOptions{
        {"StockTune", tr("Stock")},
      };
      toggle = new FrogPilotButtonsParamControl(param, title, desc, icon, tuneOptions);

      QObject::connect(static_cast<FrogPilotButtonsParamControl*>(toggle), &FrogPilotButtonsParamControl::buttonClicked, [this]() {
        if (started) {
          if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
            Hardware::reboot();
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

  std::set<QString> rebootKeys = {};
  for (const QString &key : rebootKeys) {
    QObject::connect(toggles[key.toStdString().c_str()], &ToggleControl::toggleFlipped, [this]() {
      if (started) {
        if (FrogPilotConfirmationDialog::toggle(tr("Reboot required to take effect."), tr("Reboot Now"), this)) {
          Hardware::reboot();
        }
      }
    });
  }

  QObject::connect(uiState(), &UIState::offroadTransition, this, &FrogPilotVehiclesPanel::updateCarToggles);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &FrogPilotVehiclesPanel::updateState);
}

void FrogPilotVehiclesPanel::updateState(const UIState &s) {
  if (!isVisible()) return;

  started = s.scene.started;
}

void FrogPilotVehiclesPanel::updateToggles() {
  std::thread([this]() {
    paramsMemory.putBool("FrogPilotTogglesUpdated", true);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    paramsMemory.putBool("FrogPilotTogglesUpdated", false);
  }).detach();
}

void FrogPilotVehiclesPanel::updateCarToggles() {
  std::set<std::string> evCars = {
    "CHEVROLET BOLT EUV 2022",
    "CHEVROLET BOLT EV NO ACC",
    "CHEVROLET VOLT NO ACC",
    "CHEVROLET VOLT PREMIER 2017",
  };

  auto carParams = params.get("CarParamsPersistent");
  if (!carParams.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(carParams.data(), carParams.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();

    auto carFingerprint = CP.getCarFingerprint();

    hasExperimentalOpenpilotLongitudinal = CP.getExperimentalLongitudinalAvailable();
    hasOpenpilotLongitudinal = CP.getOpenpilotLongitudinalControl();
    hasSNG = CP.getMinEnableSpeed() <= 0;
    isEVCar = evCars.count(carFingerprint) > 0;
    isGMTruck = carFingerprint == "CHEVROLET SILVERADO 1500 2020";
    isImpreza = carFingerprint == "SUBARU IMPREZA LIMITED 2019";
  } else {
    hasExperimentalOpenpilotLongitudinal = false;
    hasOpenpilotLongitudinal = true;
    hasSNG = false;
    isEVCar = true;
    isGMTruck = true;
    isImpreza = true;
  }

  hideToggles();
}

void FrogPilotVehiclesPanel::hideToggles() {
  bool gm = carMake == "Buick" || carMake == "Cadillac" || carMake == "Chevrolet" || carMake == "GM" || carMake == "GMC";
  bool subaru = carMake == "Subaru";
  bool toyota = carMake == "Lexus" || carMake == "Toyota";

  std::set<QString> evCarKeys = {};
  std::set<QString> gmTruckKeys = {};
  std::set<QString> imprezaKeys = {};
  std::set<QString> longitudinalKeys = {"LongitudinalTune"};
  std::set<QString> sngKeys = {};

  for (auto &[key, toggle] : toggles) {
    if (toggle) {
      toggle->setVisible(false);

      if (!hasOpenpilotLongitudinal && longitudinalKeys.find(key.c_str()) != longitudinalKeys.end()) {
        continue;
      }

      if (hasSNG && sngKeys.find(key.c_str()) != sngKeys.end()) {
        continue;
      }

      if (!isEVCar && evCarKeys.find(key.c_str()) != evCarKeys.end()) {
        continue;
      }

      if (!isGMTruck && gmTruckKeys.find(key.c_str()) != gmTruckKeys.end()) {
        continue;
      }

      if (!isImpreza && imprezaKeys.find(key.c_str()) != imprezaKeys.end()) {
        continue;
      }

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
