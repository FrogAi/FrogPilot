#include "selfdrive/ui/qt/sidebar.h"

#include <QMouseEvent>

#include "selfdrive/ui/qt/util.h"

void Sidebar::drawMetric(QPainter &p, const QPair<QString, QString> &label, QColor c, int y) {
  const QRect rect = {30, y, 240, 126};

  p.setPen(Qt::NoPen);
  p.setBrush(QBrush(c));
  p.setClipRect(rect.x() + 4, rect.y(), 18, rect.height(), Qt::ClipOperation::ReplaceClip);
  p.drawRoundedRect(QRect(rect.x() + 4, rect.y() + 4, 100, 118), 18, 18);
  p.setClipping(false);

  QPen pen = QPen(QColor(0xff, 0xff, 0xff, 0x55));
  pen.setWidth(2);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(rect, 20, 20);

  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setFont(fitInterFont(35, QFont::DemiBold, rect.width() - 22, {label.first, label.second}));
  p.drawText(rect.adjusted(22, 0, 0, 0), Qt::AlignCenter, label.first + "\n" + label.second);
}

Sidebar::Sidebar(QWidget *parent) : QFrame(parent), onroad(false), flag_pressed(false), settings_pressed(false) {
  connect(this, &Sidebar::valueChanged, [=] { update(); });

  setAttribute(Qt::WA_OpaquePaintEvent);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setFixedWidth(300);

  QObject::connect(uiState(), &UIState::uiUpdate, this, &Sidebar::updateState);

  pm = std::make_unique<PubMaster, const std::initializer_list<const char *>>({"userFlag"});

  // FrogPilot variables
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::togglesUpdated, this, [this] {
    update();
  });
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::cameraFrameReceived, this, [this] {
    QPair<int, int> frames = {flag_gif ? flag_gif->currentFrameNumber() : -1, settings_gif ? settings_gif->currentFrameNumber() : -1};
    if (frames != gif_frames) {
      gif_frames = frames;
      update();
    }
  });
  QObject::connect(frogpilotUIState(), &FrogPilotUIState::themeUpdated, this, &Sidebar::updateTheme);
  QObject::connect(device(), &Device::displayPowerChanged, this, [this](bool on) {
    if (on) {
      updateTheme();
    } else {
      flag_gif.reset();
      home_gif.reset();
      settings_gif.reset();
    }
  });
}

// FrogPilot variables
void Sidebar::updateTheme() {
  if (!isVisible() || !device()->isAwake()) {
    return;
  }

  QJsonObject &frogpilot_toggles = frogpilotUIState()->frogpilot_toggles;

  flag_gif.reset();
  home_gif.reset();
  settings_gif.reset();

  if (onroad) {
    loadImage("../../frogpilot/assets/active_theme/icons/button_flag", flag_img, flag_gif, home_btn.size(), this, false);
  } else if (util::random_int(0, 100) == 69 && frogpilot_toggles.value("random_events").toBool()) {
    loadImage("../../frogpilot/assets/random_events/icons/button_home", home_img, home_gif, home_btn.size(), this);
  } else {
    loadImage("../../frogpilot/assets/active_theme/icons/button_home", home_img, home_gif, home_btn.size(), this);
  }

  loadImage("../../frogpilot/assets/active_theme/icons/button_settings", settings_img, settings_gif, settings_btn.size(), this, !onroad);
}

// FrogPilot variables
void Sidebar::showEvent(QShowEvent *event) {
  onroad = uiState()->scene.started;

  updateTheme();
}

// FrogPilot variables
void Sidebar::hideEvent(QHideEvent *event) {
  flag_gif.reset();
  home_gif.reset();
  settings_gif.reset();
}

void Sidebar::mousePressEvent(QMouseEvent *event) {
  // FrogPilot variables
  const QJsonObject &frogpilot_toggles = frogpilotUIState()->frogpilot_scene.frogpilot_toggles;

  QPoint pos = event->pos();

  static constexpr QRect cpu_rect = {30, 496, 240, 126};
  static constexpr QRect memory_rect = {30, 654, 240, 126};
  static constexpr QRect temp_rect = {30, 338, 240, 126};

  if ((cpu_rect.contains(pos) || memory_rect.contains(pos) || temp_rect.contains(pos)) && frogpilot_toggles.value(QLatin1String("developer_metrics")).toBool() && !frogpilot_toggles.value(QLatin1String("debug_mode")).toBool()) {
    if (cpu_rect.contains(pos)) {
      const bool show_cpu = params.getBool("ShowCPU");
      params.putBool("ShowCPU", !show_cpu && !params.getBool("ShowGPU"));
      params.putBool("ShowGPU", show_cpu);
    } else if (memory_rect.contains(pos)) {
      const bool show_memory_usage = params.getBool("ShowMemoryUsage");
      const bool show_storage_left = params.getBool("ShowStorageLeft");
      params.putBool("ShowMemoryUsage", !show_memory_usage && !show_storage_left && !params.getBool("ShowStorageUsed"));
      params.putBool("ShowStorageLeft", show_memory_usage);
      params.putBool("ShowStorageUsed", show_storage_left);
    } else {
      const bool numerical_temp = params.getBool("NumericalTemp");
      const bool fahrenheit = params.getBool("Fahrenheit");
      params.putBool("NumericalTemp", !(numerical_temp && fahrenheit));
      params.putBool("Fahrenheit", numerical_temp && !fahrenheit);
    }
    frogpilotUIState()->updateToggles();
    return;
  }

  if (onroad && home_btn.contains(event->pos())) {
    flag_pressed = true;
    update();
  } else if (settings_btn.contains(event->pos())) {
    settings_pressed = true;
    update();
  }
}

void Sidebar::mouseReleaseEvent(QMouseEvent *event) {
  if (flag_pressed || settings_pressed) {
    flag_pressed = settings_pressed = false;
    update();
  }
  if (onroad && home_btn.contains(event->pos())) {
    MessageBuilder msg;
    msg.initEvent().initUserFlag();
    pm->send("userFlag", msg);
  } else if (settings_btn.contains(event->pos())) {
    emit openSettings();
  }
}

void Sidebar::offroadTransition(bool offroad) {
  onroad = !offroad;

  // FrogPilot variables
  updateTheme();

  update();
}

void Sidebar::updateState(const UIState &s, const FrogPilotUIState &fs) {
  if (!isVisible()) return;

  auto &sm = *(s.sm);

  // FrogPilot variables
  const QJsonObject &frogpilot_toggles = fs.frogpilot_toggles;

  bool cpu_metrics = frogpilot_toggles.value(QLatin1String("cpu_metrics")).toBool();
  bool gpu_metrics = frogpilot_toggles.value(QLatin1String("gpu_metrics")).toBool();
  bool memory_metrics = frogpilot_toggles.value(QLatin1String("memory_metrics")).toBool();
  bool numerical_temp = frogpilot_toggles.value(QLatin1String("numerical_temp")).toBool();
  bool storage_left_metrics = frogpilot_toggles.value(QLatin1String("storage_left_metrics")).toBool();
  bool storage_used_metrics = frogpilot_toggles.value(QLatin1String("storage_used_metrics")).toBool();

  QColor sidebar_color1 = QColor(frogpilot_toggles.value(QLatin1String("sidebar_color1")).toString());
  QColor sidebar_color2 = QColor(frogpilot_toggles.value(QLatin1String("sidebar_color2")).toString());
  QColor sidebar_color3 = QColor(frogpilot_toggles.value(QLatin1String("sidebar_color3")).toString());

  const SubMaster &fpsm = *(fs.sm);

  const cereal::FrogPilotDeviceState::Reader &frogpilotDeviceState = fpsm["frogpilotDeviceState"].getFrogpilotDeviceState();

  auto deviceState = sm["deviceState"].getDeviceState();
  setProperty("netType", network_type[deviceState.getNetworkType()]);
  int strength = (int)deviceState.getNetworkStrength();
  setProperty("netStrength", strength > 0 ? strength + 1 : 0);

  ItemStatus connectStatus;
  auto last_ping = deviceState.getLastAthenaPingTime();
  if (last_ping == 0) {
    connectStatus = ItemStatus{{tr("CONNECT"), tr("OFFLINE")}, warning_color};
  } else {
    connectStatus = nanos_since_boot() - last_ping < 80e9
                        ? ItemStatus{{tr("CONNECT"), tr("ONLINE")}, sidebar_color3}
                        : ItemStatus{{tr("CONNECT"), tr("ERROR")}, danger_color};
  }

  // FrogPilot variables
  if (memory_metrics) {
    int memory_usage = deviceState.getMemoryUsagePercent();

    connectStatus = {{tr("MEMORY"), QString::number(memory_usage) + "%"}, sidebar_color3};
    if (memory_usage >= 85) {
      connectStatus.second = danger_color;
    } else if (memory_usage >= 70) {
      connectStatus.second = warning_color;
    }
  } else if (storage_left_metrics || storage_used_metrics) {
    int free_space = deviceState.getFreeSpacePercent();
    int storage = storage_left_metrics ? frogpilotDeviceState.getFreeSpace() : frogpilotDeviceState.getUsedSpace();

    connectStatus = {{storage_left_metrics ? tr("LEFT") : tr("USED"), QString::number(storage) + tr(" GB")}, sidebar_color3};
    if (free_space < 10) {
      connectStatus.second = danger_color;
    } else if (free_space < 25) {
      connectStatus.second = warning_color;
    }
  }

  setProperty("connectStatus", QVariant::fromValue(connectStatus));

  ItemStatus tempStatus = {{tr("TEMP"), tr("HIGH")}, danger_color};
  auto ts = deviceState.getThermalStatus();
  if (ts == cereal::DeviceState::ThermalStatus::GREEN) {
    tempStatus = {{tr("TEMP"), tr("GOOD")}, sidebar_color1};
  } else if (ts == cereal::DeviceState::ThermalStatus::YELLOW) {
    tempStatus = {{tr("TEMP"), tr("OK")}, warning_color};
  }

  // FrogPilot variables
  if (numerical_temp) {
    float max_temp_c = deviceState.getMaxTempC();
    if (frogpilot_toggles.value(QLatin1String("fahrenheit")).toBool()) {
      tempStatus.first.second = QString::number(qRound(max_temp_c * 9 / 5 + 32)) + "°F";
    } else {
      tempStatus.first.second = QString::number(qRound(max_temp_c)) + "°C";
    }
  }

  setProperty("tempStatus", QVariant::fromValue(tempStatus));

  ItemStatus pandaStatus = {{tr("VEHICLE"), tr("ONLINE")}, sidebar_color2};
  if (s.scene.pandaType == cereal::PandaState::PandaType::UNKNOWN) {
    pandaStatus = {{tr("NO"), tr("PANDA")}, danger_color};
  } else if (s.scene.started && !sm["liveLocationKalman"].getLiveLocationKalman().getGpsOK()) {
    pandaStatus = {{tr("GPS"), tr("SEARCH")}, warning_color};
  }

  // FrogPilot variables
  if (cpu_metrics || gpu_metrics) {
    capnp::List<int8_t>::Reader cpu_loads = deviceState.getCpuUsagePercent();
    int cpu_usage = cpu_loads.size() != 0 ? std::accumulate(cpu_loads.begin(), cpu_loads.end(), 0) / cpu_loads.size() : 0;
    int gpu_usage = deviceState.getGpuUsagePercent();
    int usage = gpu_metrics ? gpu_usage : cpu_usage;

    pandaStatus = {{gpu_metrics ? tr("GPU") : tr("CPU"), QString::number(usage) + "%"}, sidebar_color2};
    if (usage >= 85) {
      pandaStatus.second = danger_color;
    } else if (usage >= 70) {
      pandaStatus.second = warning_color;
    }
  }

  setProperty("pandaStatus", QVariant::fromValue(pandaStatus));

  // FrogPilot variables
  setProperty("ipAddress", frogpilotUIState()->wifi->ipv4_address);
}

void Sidebar::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setPen(Qt::NoPen);
  p.setRenderHint(QPainter::Antialiasing);

  p.fillRect(rect(), QColor(57, 57, 57));

  // buttons
  p.setOpacity(settings_pressed ? 0.65 : 1.0);

  // FrogPilot variables
  if (settings_gif) {
    p.drawPixmap(settings_btn.x(), settings_btn.y(), settings_gif->currentPixmap());
  } else {
    p.drawPixmap(settings_btn.x(), settings_btn.y(), settings_img);
  }

  p.setOpacity(onroad && flag_pressed ? 0.65 : 1.0);

  // FrogPilot variables
  if (onroad) {
    if (flag_gif) {
      p.drawPixmap(home_btn.x(), home_btn.y(), flag_gif->currentPixmap());
    } else {
      p.drawPixmap(home_btn.x(), home_btn.y(), flag_img);
    }
  } else {
    if (home_gif) {
      p.drawPixmap(home_btn.x(), home_btn.y(), home_gif->currentPixmap());
    } else {
      p.drawPixmap(home_btn.x(), home_btn.y(), home_img);
    }
  }

  p.setOpacity(1.0);

  // FrogPilot variables
  const QJsonObject &frogpilot_toggles = frogpilotUIState()->frogpilot_scene.frogpilot_toggles;

  // network
  // FrogPilot variables
  if (frogpilot_toggles.value(QLatin1String("ip_metrics")).toBool()) {
    p.setPen(QColor(0xff, 0xff, 0xff));
    p.setFont(InterFont(30));
    p.drawText(QRect(50, 196, 225, 27), Qt::AlignLeft | Qt::AlignVCenter, ip_address);
  } else {
    int x = 58;
    const QColor gray(0x54, 0x54, 0x54);
    for (int i = 0; i < 5; ++i) {
      p.setBrush(i < net_strength ? Qt::white : gray);
      p.drawEllipse(x, 196, 27, 27);
      x += 37;
    }
  }

  p.setFont(InterFont(35));
  p.setPen(QColor(0xff, 0xff, 0xff));
  const QRect r = QRect(50, 247, 100, 50);
  p.drawText(r, Qt::AlignCenter, net_type);

  // metrics
  drawMetric(p, temp_status.first, temp_status.second, 338);
  drawMetric(p, panda_status.first, panda_status.second, 496);
  drawMetric(p, connect_status.first, connect_status.second, 654);
}
