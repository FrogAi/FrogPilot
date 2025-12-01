#include "selfdrive/ui/ui.h"

#include <algorithm>
#include <cmath>

#include <QtConcurrent>

#include "common/transformations/orientation.hpp"
#include "common/swaglog.h"
#include "common/util.h"
#include "common/watchdog.h"
#include "system/hardware/hw.h"

#define BACKLIGHT_DT 0.05
#define BACKLIGHT_TS 10.00

static void update_sockets(UIState *s) {
  s->sm->update(0);
}

static void update_state(UIState *s) {
  SubMaster &sm = *(s->sm);
  UIScene &scene = s->scene;

  if (sm.updated("liveCalibration")) {
    auto list2rot = [](const capnp::List<float>::Reader &rpy_list) ->Eigen::Matrix3f {
      return euler2rot({rpy_list[0], rpy_list[1], rpy_list[2]}).cast<float>();
    };

    auto live_calib = sm["liveCalibration"].getLiveCalibration();
    if (live_calib.getCalStatus() == cereal::LiveCalibrationData::Status::CALIBRATED) {
      auto device_from_calib = list2rot(live_calib.getRpyCalib());
      auto wide_from_device = list2rot(live_calib.getWideFromDeviceEuler());
      s->scene.view_from_calib = VIEW_FROM_DEVICE * device_from_calib;
      s->scene.view_from_wide_calib = VIEW_FROM_DEVICE * wide_from_device * device_from_calib;
    } else {
      s->scene.view_from_calib = s->scene.view_from_wide_calib = VIEW_FROM_DEVICE;
    }
  }
  if (sm.updated("pandaStates")) {
    auto pandaStates = sm["pandaStates"].getPandaStates();
    if (pandaStates.size() > 0) {
      scene.pandaType = pandaStates[0].getPandaType();

      if (scene.pandaType != cereal::PandaState::PandaType::UNKNOWN) {
        scene.ignition = false;
        for (const auto& pandaState : pandaStates) {
          scene.ignition |= pandaState.getIgnitionLine() || pandaState.getIgnitionCan();
        }
      }
    }
  } else if ((s->sm->frame - s->sm->rcv_frame("pandaStates")) > 5*UI_FREQ) {
    scene.pandaType = cereal::PandaState::PandaType::UNKNOWN;
  }
  if (sm.updated("wideRoadCameraState")) {
    auto cam_state = sm["wideRoadCameraState"].getWideRoadCameraState();
    float scale = (cam_state.getSensor() == cereal::FrameData::ImageSensor::AR0231) ? 6.0f : 1.0f;
    scene.light_sensor = std::max(100.0f - scale * cam_state.getExposureValPercent(), 0.0f);
  } else if (!sm.allAliveAndValid({"wideRoadCameraState"})) {
    scene.light_sensor = -1;
  }
  scene.started = sm["deviceState"].getDeviceState().getStarted() && scene.ignition;

  auto params = Params();
  scene.recording_audio = params.getBool("RecordAudio") && scene.started;

  // FrogPilot variables
  FrogPilotUIScene &frogpilot_scene = frogpilotUIState()->frogpilot_scene;

  scene.started |= frogpilot_scene.frogpilot_toggles.value(QLatin1String("force_onroad")).toBool();
  scene.started &= !frogpilot_scene.frogpilot_toggles.value(QLatin1String("force_offroad")).toBool();
}

void ui_update_params(UIState *s) {
  auto params = Params();
  s->scene.is_metric = params.getBool("IsMetric");
}

void UIState::updateStatus() {
  // FrogPilot variables
  FrogPilotUIScene &frogpilot_scene = frogpilotUIState()->frogpilot_scene;

  if (scene.started && sm->updated("selfdriveState")) {
    auto ss = (*sm)["selfdriveState"].getSelfdriveState();
    auto state = ss.getState();

    // FrogPilot variables

    if (state == cereal::SelfdriveState::OpenpilotState::PRE_ENABLED || state == cereal::SelfdriveState::OpenpilotState::OVERRIDING) {
      status = STATUS_OVERRIDE;
    // FrogPilot variables
    } else if (frogpilot_scene.always_on_lateral_active) {
      status = STATUS_ALWAYS_ON_LATERAL_ACTIVE;
    } else {
      status = ss.getEnabled() ? STATUS_ENGAGED : STATUS_DISENGAGED;
    }

    // FrogPilot variables
    const cereal::FrogPilotSelfdriveState::AlertStatus frogpilot_alert_status = (*frogpilotUIState()->sm)["frogpilotSelfdriveState"].getFrogpilotSelfdriveState().getAlertStatus();
  }

  if (engaged() != engaged_prev) {
    engaged_prev = engaged();
    emit engagedChanged(engaged());
  }

  // Handle onroad/offroad transition
  if (scene.started != started_prev || sm->frame == 1) {
    if (scene.started) {
      status = STATUS_DISENGAGED;
      scene.started_frame = sm->frame;
    }
    started_prev = scene.started;
    emit offroadTransition(!scene.started);
  }
}

UIState::UIState(QObject *parent) : QObject(parent) {
  sm = std::make_unique<SubMaster>(std::vector<const char*>{
    "modelV2", "controlsState", "liveCalibration", "radarState", "deviceState",
    "pandaStates", "carParams", "driverMonitoringState", "carState", "driverStateV2",
    "wideRoadCameraState", "managerState", "selfdriveState", "longitudinalPlan",
  });
  prime_state = new PrimeState(this);
  language = QString::fromStdString(Params().get("LanguageSetting"));

  // update timer
  timer = new QTimer(this);
  QObject::connect(timer, &QTimer::timeout, this, &UIState::update);
  timer->start(1000 / UI_FREQ);
}

void UIState::update() {
  // FrogPilot variables
  FrogPilotUIState *fs = frogpilotUIState();
  update_sockets(this);
  // FrogPilot variables
  fs->update();
  update_state(this);
  updateStatus();

  if (sm->frame % UI_FREQ == 0) {
    watchdog_kick(nanos_since_boot());
  }
  emit uiUpdate(*this, *fs);
}

Device::Device(QObject *parent) : brightness_filter(BACKLIGHT_OFFROAD, BACKLIGHT_TS, BACKLIGHT_DT), QObject(parent) {
  setAwake(true);
  resetInteractiveTimeout();

  QObject::connect(uiState(), &UIState::uiUpdate, this, &Device::update);
}

void Device::update(const UIState &s) {
  updateBrightness(s);
  updateWakefulness(s);
}

void Device::setAwake(bool on) {
  if (on != awake) {
    awake = on;
    Hardware::set_display_power(awake);
    LOGD("setting display power %d", awake);
    emit displayPowerChanged(awake);
  }
}

void Device::resetInteractiveTimeout(int timeout) {
  // FrogPilot variables
  if (timeout == -1) {
    const QJsonObject &frogpilot_toggles = frogpilotUIState()->frogpilot_scene.frogpilot_toggles;
    timeout = frogpilot_toggles.value(QLatin1String("screen_timeout")).toInt(-1);
    const int timeout_onroad = frogpilot_toggles.value(QLatin1String("screen_timeout_onroad")).toInt(-1);
    if (timeout != -1 && ignition_on && timeout_onroad != -1) {
      timeout = timeout_onroad;
    }
  }

  if (timeout == -1) {
    timeout = (ignition_on ? 10 : 30);
  }
  interactive_timeout = timeout * UI_FREQ;
}

void Device::updateBrightness(const UIState &s) {
  // FrogPilot variables
  const FrogPilotUIScene &frogpilot_scene = frogpilotUIState()->frogpilot_scene;
  const QJsonObject &frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  float clipped_brightness = offroad_brightness;
  if (s.scene.started && s.scene.light_sensor >= 0) {
    clipped_brightness = s.scene.light_sensor;

    // CIE 1931 - https://www.photonstophotos.net/GeneralTopics/Exposure/Psychometric_Lightness_and_Gamma.htm
    if (clipped_brightness <= 8) {
      clipped_brightness = (clipped_brightness / 903.3);
    } else {
      clipped_brightness = std::pow((clipped_brightness + 16.0) / 116.0, 3.0);
    }

    // Scale back to 10% to 100%
    clipped_brightness = std::clamp(100.0f * clipped_brightness, 10.0f, 100.0f);
  }

  int brightness = brightness_filter.update(clipped_brightness);
  if (!awake) {
    brightness = 0;
  // FrogPilot variables
  } else if (s.scene.started) {
    const int screen_brightness_onroad = frogpilot_toggles.value(QLatin1String("screen_brightness_onroad")).toInt();
    if (screen_brightness_onroad != 101) {
      brightness = interactive_timeout > 0 ? std::max(5, screen_brightness_onroad) : screen_brightness_onroad;
    }
  } else if (offroad_brightness == BACKLIGHT_OFFROAD) {
    const int screen_brightness = frogpilot_toggles.value(QLatin1String("screen_brightness")).toInt();
    if (screen_brightness != 101) {
      brightness = screen_brightness;
    }
  }

  // FrogPilot variables
  dark = awake && brightness == 0;

  if (brightness != last_brightness) {
    if (!brightness_future.isRunning()) {
      brightness_future = QtConcurrent::run(Hardware::set_brightness, brightness);
      last_brightness = brightness;
    }
  }
}

void Device::updateWakefulness(const UIState &s) {
  // FrogPilot variables
  const FrogPilotUIScene &frogpilot_scene = frogpilotUIState()->frogpilot_scene;
  const QJsonObject &frogpilot_toggles = frogpilot_scene.frogpilot_toggles;

  bool ignition_just_turned_on = s.scene.ignition && !ignition_on;
  bool ignition_just_turned_off = !s.scene.ignition && ignition_on;
  ignition_on = s.scene.ignition;

  // FrogPilot variables
  if (frogpilot_scene.downloading_update || frogpilot_scene.frogpilot_panel_active) {
    resetInteractiveTimeout();
  } else if (ignition_just_turned_on && frogpilot_toggles.value(QLatin1String("screen_brightness_onroad")).toInt() == 0) {
    resetInteractiveTimeout(0);
  } else if (ignition_just_turned_off) {
    resetInteractiveTimeout();
  } else if (interactive_timeout > 0 && --interactive_timeout == 0) {
    emit interactiveTimeout();
  }

  setAwake(s.scene.ignition || interactive_timeout > 0);
}

UIState *uiState() {
  static UIState ui_state;
  return &ui_state;
}

Device *device() {
  static Device _device;
  return &_device;
}
